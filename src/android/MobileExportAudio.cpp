#include "MobileExportAudio.h"
#include "common/PreviewSfxAssets.h"
#include "common/IntroConfig.h"
#include <QAudioDecoder>
#include <QAudioBuffer>
#include <QEventLoop>
#include <QSaveFile>
#include <QTimer>
#include <QUuid>
#include <QDataStream>
#include <QtEndian>
#include <QHash>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <cmath>
#include <stdexcept>

namespace miacode::android {
namespace {
constexpr int sampleRate = 48000, channels = 2, blockFrames = 4096;
struct Clip { QString path; qint64 frames = 0; };
struct Voice { Clip clip; qint64 start = 0, sourceStart = 0, length = 0; double gain = 0; };

void require(bool condition, const QString& message) {
    if (!condition) throw std::runtime_error(message.toUtf8().constData());
}

Clip decode(const QString& path, const QString& directory, const std::atomic_bool& cancelled) {
    require(!cancelled.load(), QStringLiteral("Export cancelled"));
    QString source = path;
    if (path.startsWith("assets:") || path.startsWith(":")) {
        source = directory + "/" + QUuid::createUuid().toString(QUuid::WithoutBraces) + "." + QFileInfo(path).suffix();
        require(QFile::copy(path, source), QStringLiteral("Cannot stage packaged audio: %1").arg(path));
    }
    Clip result{directory + "/" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".f32", 0};
    QSaveFile file(result.path);
    require(file.open(QIODevice::WriteOnly), file.errorString());
    QAudioFormat format;
    format.setSampleRate(sampleRate); format.setChannelCount(channels); format.setSampleFormat(QAudioFormat::Float);
    QAudioDecoder decoder;
    decoder.setAudioFormat(format); decoder.setSource(QUrl::fromLocalFile(source));
    QEventLoop loop;
    QTimer timeout, cancellation;
    timeout.setSingleShot(true); cancellation.setInterval(50);
    QString error;
    bool completed = false;
    QObject::connect(&decoder, &QAudioDecoder::bufferReady, &loop, [&] {
        const auto buffer = decoder.read();
        if (!buffer.isValid()) return;
        if (cancelled.load() || buffer.format() != format || buffer.sampleCount() % channels) {
            error = QStringLiteral("Audio decode cancelled or returned an unexpected PCM format"); loop.quit(); return;
        }
        const float* values = buffer.constData<float>();
        for (int i = 0; i < buffer.sampleCount(); ++i) if (!std::isfinite(values[i])) {
            error = QStringLiteral("Decoded audio contains non-finite samples"); loop.quit(); return;
        }
        if (file.write(buffer.constData<char>(), buffer.byteCount()) != buffer.byteCount()) {
            error = file.errorString(); loop.quit(); return;
        }
        result.frames += buffer.frameCount();
    });
    QObject::connect(&decoder, &QAudioDecoder::finished, &loop, [&] { completed = true; loop.quit(); });
    QObject::connect(&decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), &loop,
        [&](QAudioDecoder::Error) { error = decoder.errorString(); loop.quit(); });
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] { error = QStringLiteral("Audio decode timed out"); loop.quit(); });
    QObject::connect(&cancellation, &QTimer::timeout, &loop, [&] {
        if (cancelled.load()) { error = QStringLiteral("Export cancelled"); loop.quit(); }
    });
    timeout.start(120000); cancellation.start(); decoder.start(); loop.exec(); decoder.stop();
    require(completed && error.isEmpty() && !cancelled.load(), error.isEmpty() ? QStringLiteral("Audio decode incomplete") : error);
    require(file.commit(), file.errorString());
    return result;
}
}

QString renderExportWav(const video_export::VideoExportAudioRenderPlan& plan,
                       const QString& introSound, double introVolume, const QString& directory,
                       const std::atomic_bool& cancelled) {
    require(QDir().mkpath(directory), QStringLiteral("Cannot create export audio directory"));
    const qint64 totalFrames = qRound64(plan.alignedTotalSeconds * sampleRate);
    require(totalFrames > 0 && totalFrames <= (0xffffffffLL - 36) / 4, QStringLiteral("WAV duration exceeds RIFF limits"));
    QHash<QString, Clip> clips;
    QVector<Voice> voices;
    const auto add = [&](const QString& path, double start, double sourceStart, double duration, double gain) {
        if (gain <= 0 || duration == 0) return;
        require(QFileInfo::exists(path), QStringLiteral("Missing export audio asset: %1").arg(path));
        if (!clips.contains(path)) clips.insert(path, decode(path, directory, cancelled));
        const auto clip = clips.value(path);
        const qint64 sourceFrame = qMax<qint64>(0, qRound64(sourceStart * sampleRate));
        const qint64 count = qMin(clip.frames - sourceFrame, duration < 0 ? clip.frames : qRound64(duration * sampleRate));
        if (count > 0) voices.append({clip, qRound64(start * sampleRate), sourceFrame, count, gain});
    };
    if (plan.backgroundTrack.enabled) {
        const auto& bg = plan.backgroundTrack;
        add(bg.path, bg.mixStartSecond, bg.sourceStartSecond, bg.durationSeconds, bg.gain);
    }
    for (const auto& sfx : plan.scheduledSfxPlaybacks)
        add(preview_sfx::assetFilePathForKind(plan.sfxDirectory, sfx.assetKind, introSound),
            sfx.mixSecond, 0, sfx.maxDurationSeconds, sfx.gain);
    for (const auto& span : plan.touchholdSpanPlaybacks)
        add(preview_sfx::assetFilePathForKind(plan.sfxDirectory, span.assetKind, introSound),
            span.mixSecond, span.sourceStartSecond, span.durationSeconds, span.gain);
    if (plan.introLeadSeconds > 0 && introVolume > 0) {
        QString path = preview_sfx::assetFilePathForKind(plan.sfxDirectory, QStringLiteral("track_start"), introSound);
        if (!QFileInfo::exists(path)) path = QString::fromLatin1(intro::kOpeningSfxResource);
        add(path, 0, 0, plan.introLeadSeconds, qBound(0.0, introVolume, 2.0));
    }

    const QString outputPath = directory + "/audio.wav";
    QSaveFile output(outputPath);
    require(output.open(QIODevice::WriteOnly), output.errorString());
    QDataStream stream(&output); stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData("RIFF", 4); stream << quint32(36 + totalFrames * 4);
    stream.writeRawData("WAVEfmt ", 8); stream << quint32(16) << quint16(1) << quint16(channels)
        << quint32(sampleRate) << quint32(sampleRate * 4) << quint16(4) << quint16(16);
    stream.writeRawData("data", 4); stream << quint32(totalFrames * 4);
    for (qint64 frame = 0; frame < totalFrames; frame += blockFrames) {
        require(!cancelled.load(), QStringLiteral("Export cancelled"));
        const qint64 count = qMin<qint64>(blockFrames, totalFrames - frame);
        QVector<double> mix(count * channels, 0.0);
        for (const auto& voice : voices) {
            const qint64 begin = qMax(frame, voice.start), end = qMin(frame + count, voice.start + voice.length);
            if (end <= begin) continue;
            QFile input(voice.clip.path);
            require(input.open(QIODevice::ReadOnly) && input.seek((voice.sourceStart + begin - voice.start) * channels * sizeof(float)),
                    QStringLiteral("Cannot read decoded export audio"));
            const QByteArray bytes = input.read((end - begin) * channels * sizeof(float));
            require(bytes.size() == (end - begin) * channels * sizeof(float), QStringLiteral("Decoded audio was truncated"));
            const auto* values = reinterpret_cast<const float*>(bytes.constData());
            for (qint64 i = 0; i < (end - begin) * channels; ++i)
                mix[(begin - frame) * channels + i] += values[i] * voice.gain;
        }
        QByteArray pcm(count * 4, Qt::Uninitialized);
        for (qsizetype i = 0; i < mix.size(); ++i)
            qToLittleEndian<qint16>(qRound(qBound(-1.0, mix[i], 1.0) * 32767.0), pcm.data() + i * 2);
        require(output.write(pcm) == pcm.size(), output.errorString());
    }
    require(output.commit(), output.errorString());
    return outputPath;
}
}
