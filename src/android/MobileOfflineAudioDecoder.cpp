#include "audio/OfflineAudioDecoder.h"
#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QEventLoop>
#include <QTimer>
#include <QUrl>
#include <cmath>

namespace miacode::audio_decode {
DecodedMonoAudio decodeFileToMono(const QString& path, int targetSampleRate) {
    DecodedMonoAudio result;
    if (targetSampleRate <= 0) return result;
    QAudioDecoder decoder;
    QAudioFormat format;
    format.setSampleRate(targetSampleRate);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Float);
    decoder.setAudioFormat(format);
    decoder.setSource(QUrl::fromLocalFile(path));
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool complete = false, failed = false;
    QObject::connect(&decoder, &QAudioDecoder::bufferReady, &loop, [&] {
        const QAudioBuffer buffer = decoder.read();
        if (!buffer.isValid()) return;
        if (buffer.format() != format || result.samples.size() + buffer.sampleCount() > 24000ll * 3600) {
            failed = true;
            decoder.stop();
            loop.quit();
            return;
        }
        const auto* samples = buffer.constData<float>();
        for (int i = 0; i < buffer.sampleCount(); ++i) {
            if (!std::isfinite(samples[i])) { failed = true; loop.quit(); return; }
            result.samples.append(samples[i]);
        }
    });
    QObject::connect(&decoder, &QAudioDecoder::finished, &loop, [&] { complete = true; loop.quit(); });
    QObject::connect(&decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), &loop,
        [&](QAudioDecoder::Error) { failed = true; loop.quit(); });
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] { failed = true; loop.quit(); });
    timeout.start(120000);
    decoder.start();
    loop.exec();
    decoder.stop();
    if (!complete || failed) return {};
    result.sampleRate = targetSampleRate;
    result.durationSeconds = static_cast<double>(result.samples.size()) / targetSampleRate;
    return result;
}
double probeFileDurationSeconds(const QString& path) {
    return decodeFileToMono(path, 24000).durationSeconds;
}
}
