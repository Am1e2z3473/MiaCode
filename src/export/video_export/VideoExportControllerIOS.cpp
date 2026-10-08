#include "export/video_export/VideoExportController.h"
#include "common/LocalizedText.h"

VideoExportResult VideoExportController::exportFullPreview(const VideoExportTask&)
{
    return {false, miacode::localizedText("platform.external_process_unavailable").text(), {}};
}

VideoExportResult VideoExportController::exportPreparedTask(
    const VideoExportTask& task, const VideoExportProgressCallback&)
{
    return exportFullPreview(task);
}
