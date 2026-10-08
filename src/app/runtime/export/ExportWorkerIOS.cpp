#include "app/runtime/export/VideoExportHost.h"
#include "app/services/JobProgressService.h"

namespace miacode::runtime {

bool VideoExportHost::startVideoExportWorkerProcess(
    QProcess*, const VideoExportSnapshot&, QString* errorMessage, bool)
{
    if (errorMessage) *errorMessage = miacode::localizedText("platform.external_process_unavailable").text();
    return false;
}

bool VideoExportHost::runVideoExportWorkerSync(
    const VideoExportSnapshot& snapshot, bool* canceledByUser, QString* errorMessage,
    const std::function<void(int, const QString&)>&,
    const std::function<bool()>&, const std::function<void()>&)
{
    if (canceledByUser) *canceledByUser = false;
    return startVideoExportWorkerProcess(nullptr, snapshot, errorMessage);
}

bool VideoExportHost::launchVideoExportWorker(const VideoExportSnapshot& snapshot, QString* errorMessage)
{
    return startVideoExportWorkerProcess(nullptr, snapshot, errorMessage);
}

void VideoExportHost::handleVideoExportWorkerStdout() {}
void VideoExportHost::handleVideoExportWorkerStderr() {}
void VideoExportHost::handleVideoExportWorkerEvent(const QJsonObject&) {}
void VideoExportHost::handleVideoExportWorkerProcessFinished(int, int) {}
void VideoExportHost::cancelVideoExportWorker() {}
void VideoExportHost::clearVideoExportWorkerState()
{
    session_.videoExportWorkerProcess_ = nullptr;
    endExportProgress();
}

} // namespace miacode::runtime

// The export worker owns the shared progress surface between begin and end.
// A negative percent means the stage has no measurable progress.
void miacode::runtime::VideoExportHost::reportExportProgress(int percent, const miacode::LocalizedText& label)
{
    miacode::JobProgressService* const jobProgress = session_.jobProgressService();
    if (jobProgress == nullptr || jobProgress->token() != session_.videoExportJobToken_) {
        return;
    }
    if (percent < 0) {
        jobProgress->reportIndeterminate(label);
    } else {
        jobProgress->report(percent, label);
    }
}

void miacode::runtime::VideoExportHost::endExportProgress()
{
    miacode::JobProgressService* const jobProgress = session_.jobProgressService();
    if (jobProgress == nullptr || session_.videoExportJobToken_ == 0) {
        return;
    }
    // Only clear our own job: a later job may already own the surface.
    if (jobProgress->token() == session_.videoExportJobToken_) {
        jobProgress->end();
    }
    session_.videoExportJobToken_ = 0;
}
