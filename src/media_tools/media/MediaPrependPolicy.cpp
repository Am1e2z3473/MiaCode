#include "media_tools/media/MediaPrependPolicy.h"


#include <algorithm>

namespace miacode::media {
namespace {

QString fixedSeconds(double seconds)
{
    return QString::number(std::max(0.0, seconds), 'f', 6);
}

void appendAudioEncoderArguments(QStringList& args, const QString& suffix)
{
    if (suffix.compare(QStringLiteral("mp3"), Qt::CaseInsensitive) == 0) {
        args << QStringLiteral("-c:a") << QStringLiteral("libmp3lame")
             << QStringLiteral("-q:a") << QStringLiteral("2");
    } else if (suffix.compare(QStringLiteral("wav"), Qt::CaseInsensitive) == 0) {
        args << QStringLiteral("-c:a") << QStringLiteral("pcm_s16le");
    } else if (suffix.compare(QStringLiteral("flac"), Qt::CaseInsensitive) == 0) {
        args << QStringLiteral("-c:a") << QStringLiteral("flac");
    } else if (suffix.compare(QStringLiteral("ogg"), Qt::CaseInsensitive) == 0) {
        args << QStringLiteral("-c:a") << QStringLiteral("libvorbis")
             << QStringLiteral("-q:a") << QStringLiteral("6");
    }
}

}  // namespace

QStringList makeAudioPrependArguments(
    const QString& inputPath,
    const QString& outputPath,
    const QString& outputSuffix,
    double silenceSeconds)
{
    const QString duration = fixedSeconds(silenceSeconds);
    const QString audioFormat = QStringLiteral(
        "aformat=sample_fmts=fltp:sample_rates=44100:channel_layouts=stereo");
    QStringList args{
        QStringLiteral("-hide_banner"),
        QStringLiteral("-nostdin"),
        QStringLiteral("-y"),
        QStringLiteral("-f"), QStringLiteral("lavfi"),
        QStringLiteral("-i"),
        QStringLiteral("anullsrc=channel_layout=stereo:sample_rate=44100:d=%1").arg(duration),
        QStringLiteral("-i"), inputPath,
        QStringLiteral("-filter_complex"),
        QStringLiteral(
            "[0:a]atrim=duration=%1,%2,asetpts=PTS-STARTPTS[s];"
            "[1:a]aresample=44100,%2,asetpts=PTS-STARTPTS[a];"
            "[s][a]concat=n=2:v=0:a=1[out]")
            .arg(duration, audioFormat),
        QStringLiteral("-map"), QStringLiteral("[out]"),
    };
    appendAudioEncoderArguments(args, outputSuffix);
    args << outputPath;
    return args;
}

}  // namespace miacode::media
