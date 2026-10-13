#include "media_tools/media/MediaPrependPolicy.h"

#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <cmath>
#include <cstring>

namespace {

bool run(const QString& executable, const QStringList& arguments, QByteArray* output = nullptr)
{
    QProcess process;
    process.start(executable, arguments);
    if (!process.waitForStarted(5000) || !process.waitForFinished(30000)
        || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QTextStream(stderr) << process.errorString() << '\n' << process.readAllStandardError() << Qt::endl;
        return false;
    }
    if (output != nullptr) {
        *output = process.readAllStandardOutput();
    }
    return true;
}

bool checkAudio(const QString& ffmpeg, const QString& directory, const QString& suffix)
{
    const QString source = QDir(directory).filePath(QStringLiteral("source.%1").arg(suffix));
    const QString output = QDir(directory).filePath(QStringLiteral("padded.%1").arg(suffix));
    if (!run(ffmpeg, {"-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
                     "sine=frequency=1000:sample_rate=44100:duration=0.2", "-ac", "2", source})
        || !run(ffmpeg, miacode::media::makeAudioPrependArguments(source, output, suffix, 0.15))) {
        return false;
    }
    QByteArray pcm;
    if (!run(ffmpeg, {"-hide_banner", "-loglevel", "error", "-i", output,
                     "-ar", "44100", "-ac", "2", "-f", "f32le", "-"}, &pcm)) {
        return false;
    }
    const double duration = static_cast<double>(pcm.size()) / (44100 * 2 * sizeof(float));
    if (std::abs(duration - 0.35) > 0.06) {
        QTextStream(stderr) << suffix << ": padded duration " << duration << Qt::endl;
        return false;
    }
    for (int offset = 0; offset < 4410 * 2 * static_cast<int>(sizeof(float)); offset += sizeof(float)) {
        float sample = 0.0f;
        std::memcpy(&sample, pcm.constData() + offset, sizeof(sample));
        if (std::abs(sample) > 0.001f) {
            QTextStream(stderr) << suffix << ": initial audio is silence" << Qt::endl;
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QString ffmpeg = qEnvironmentVariable("MIACODE_FFMPEG_PATH");
    QTemporaryDir directory;
    if (ffmpeg.isEmpty() || !directory.isValid()) {
        QTextStream(stderr) << "Set MIACODE_FFMPEG_PATH to the export FFmpeg executable." << Qt::endl;
        return 1;
    }
    for (const auto& suffix : {QStringLiteral("mp3"), QStringLiteral("wav"),
                              QStringLiteral("flac"), QStringLiteral("ogg")}) {
        if (!checkAudio(ffmpeg, directory.path(), suffix)) {
            return 1;
        }
    }
    return 0;
}
