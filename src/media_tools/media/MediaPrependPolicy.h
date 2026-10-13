#pragma once

#include <QString>
#include <QStringList>

namespace miacode::media {

QStringList makeAudioPrependArguments(
    const QString& inputPath,
    const QString& outputPath,
    const QString& outputSuffix,
    double silenceSeconds);

}  // namespace miacode::media
