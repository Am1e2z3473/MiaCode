#include "media_tools/media/PvCompressionPolicy.h"

#include <QProcess>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace miacode::media {

PvCompressionPlan makePvCompressionPlan(double durationSeconds, qint64 targetBytes)
{
    PvCompressionPlan plan;
    plan.targetBytes = std::clamp<qint64>(targetBytes, 1, kPvCompressionWorkingTargetBytes);
    if (!(durationSeconds > 0.0)) {
        return plan;
    }

    const double targetBits = static_cast<double>(plan.targetBytes) * 8.0;
    plan.videoBitrateKbps = std::max(
        1,
        static_cast<int>(std::floor(targetBits / durationSeconds / 1000.0)));
    return plan;
}

PvCompressionPlan adjustedPvCompressionPlan(
    const PvCompressionPlan& previous,
    qint64 actualOutputBytes)
{
    PvCompressionPlan adjusted = previous;
    if (previous.videoBitrateKbps <= 0 || actualOutputBytes <= 0) {
        return adjusted;
    }

    const double corrected = static_cast<double>(previous.videoBitrateKbps)
        * static_cast<double>(previous.targetBytes)
        / static_cast<double>(actualOutputBytes)
        * kPvCompressionRetrySafetyRatio;
    adjusted.videoBitrateKbps = std::max(1, static_cast<int>(std::floor(corrected)));
    return adjusted;
}

bool isAcceptablePvCompressionOutput(qint64 originalBytes, qint64 outputBytes)
{
    return outputBytes > 0
        && outputBytes < kPvCompressionHardLimitBytes
        && (originalBytes <= 0 || outputBytes < originalBytes);
}

QString pvCompressionEncoderFromInputInfo(const QString& ffmpegInputInfo)
{
    static const QRegularExpression codecPattern(
        QStringLiteral(R"(Video:\s*([A-Za-z0-9_]+))"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = codecPattern.match(ffmpegInputInfo);
    if (!match.hasMatch()) {
        return {};
    }

    const QString codec = match.captured(1).toLower();
    if (codec == QStringLiteral("h264") || codec == QStringLiteral("mpeg2")
        || codec == QStringLiteral("mpeg2video") || codec == QStringLiteral("mpeg1")
        || codec == QStringLiteral("mpeg1video")) {
        return QStringLiteral("libx264");
    }
    if (codec == QStringLiteral("hevc") || codec == QStringLiteral("h265")) {
        return QStringLiteral("libx265");
    }
    if (codec == QStringLiteral("av1")) {
        return QStringLiteral("libaom-av1");
    }
    if (codec == QStringLiteral("mpeg4")) {
        return QStringLiteral("mpeg4");
    }
    if (codec == QStringLiteral("vp9")) {
        return QStringLiteral("libvpx-vp9");
    }
    return {};
}

QStringList makePvCompressionPassArguments(
    const QString& inputPath,
    const QString& outputPath,
    const QString& passLogPath,
    const QString& videoEncoder,
    const PvCompressionPlan& plan,
    int passNumber)
{
    QStringList args{
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-nostdin"),
        QStringLiteral("-y"),
        QStringLiteral("-i"), inputPath,
        QStringLiteral("-map"), QStringLiteral("0:v:0"),
        QStringLiteral("-map_metadata"), QStringLiteral("-1"),
        QStringLiteral("-c:v"), videoEncoder,
        QStringLiteral("-b:v"), QStringLiteral("%1k").arg(plan.videoBitrateKbps),
        QStringLiteral("-vf"), QStringLiteral("scale='min(1280,iw)':-2"),
        QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"),
        QStringLiteral("-fps_mode"), QStringLiteral("passthrough"),
        QStringLiteral("-an"),
        QStringLiteral("-pass"), QString::number(passNumber),
        QStringLiteral("-passlogfile"), passLogPath,
    };

if (videoEncoder == QStringLiteral("libx264")
        || videoEncoder == QStringLiteral("libx265")) {
        args << QStringLiteral("-preset") << QStringLiteral("fast");
    }
    if (videoEncoder == QStringLiteral("libx265")) {
        args << QStringLiteral("-x265-params")
             << QStringLiteral("pass=%1:stats=%2")
                    .arg(QString::number(passNumber), passLogPath);
    } else if (videoEncoder == QStringLiteral("libaom-av1")) {
        args << QStringLiteral("-cpu-used") << QStringLiteral("6")
             << QStringLiteral("-row-mt") << QStringLiteral("1");
        args << QStringLiteral("-movflags") << QStringLiteral("+faststart") << outputPath;
    }
    return args;
}

}  // namespace miacode::media
