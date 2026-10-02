#pragma once

#include "tools/video_export/VideoExportAudioRenderPlan.h"
#include <QString>
#include <atomic>

namespace miacode::android {
// Uses the production v2 audio schedule; decoding/mixing is local Qt Multimedia.
QString renderExportWav(const video_export::VideoExportAudioRenderPlan& plan,
                       const QString& introSound, double introVolume, const QString& jobDirectory,
                       const std::atomic_bool& cancelled);
}
