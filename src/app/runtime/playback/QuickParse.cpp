#include "runtime/playback/PlaybackCoordinator.h"
#include "common/ContentDurationConfig.h"
#include "timeline/quick/TimelineQuickStateBridge.h"
#include "runtime/playback/TimelineFlow.Internal.h"

#include <QElapsedTimer>

using namespace miacode::runtime::preview_timeline_detail;

void miacode::runtime::PlaybackCoordinator::refreshTimelineQuickModelFromCurrentText()
{
    if (state_.timelineQuickStateBridge_ == nullptr || !hasActiveDifficulty()) {
        return;
    }
    QElapsedTimer timer;
    timer.start();
    state_.timelineQuickModel_.rebuildFromText(activeChartText(), parsedFirstSeconds(), currentTimingMetadata());
    invalidatePreviewFollowBindingCache();
    const auto& snapshot = state_.timelineQuickModel_.snapshot();
    if (state_.pendingDifficultySwitchPreviewRestore_
        && state_.pendingDifficultySwitchPreviewRestoreDifficultyId_ == activeDifficultyId()) {
        const double duration = miacode::content_duration::totalContentDurationSeconds(
            snapshot.durationSeconds, state_.previewTrackDurationSeconds_);
        const double restoredSecond = qBound(0.0, state_.pendingDifficultySwitchPreviewRestoreSecond_, duration);
        repositionSilently(restoredSecond, "switch_timeline_snapshot");
        state_.timelineQuickStateBridge_->replaceTimelineData(snapshot, restoredSecond, duration);
    } else {
        state_.timelineQuickStateBridge_->setTimelineData(snapshot);
    }
    if (state_.runtimeDebugOutputEnabled_) {
        appendTimelinePerfLog(
            QStringLiteral("edit/quick_timeline_perf"),
            QStringLiteral("mode=full revision=%1 lines=%2 elapsed_ms=%3")
                .arg(state_.timelineRevision_)
                .arg(state_.timelineQuickModel_.snapshot().lines.size())
                .arg(timer.nsecsElapsed() / 1000000.0, 0, 'f', 3)
        );
    }
}
