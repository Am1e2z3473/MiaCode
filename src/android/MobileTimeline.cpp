#include "MobileTimeline.h"
#include "common/AssetPaths.h"
#include "core/chart/document/SimaiTimingMetadata.h"
#include "timeline/TimelineMarkerOffset.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QPointer>

namespace miacode::android {
MobileTimeline::MobileTimeline(AndroidDocumentSession& document, MobilePreview& preview,
    EditorSyncController& editor, AnalysisService& analysis, QObject* parent)
    : QObject(parent), document_(document), preview_(preview), editor_(editor),
      bridge_(this), waveforms_(this)
{
    bridge_.setSkinDirectory(assets::assetPath("skin/skinDX"));
    connect(&document_, &AndroidDocumentSession::documentStateChanged, this, &MobileTimeline::rebuild);
    connect(&document_, &AndroidDocumentSession::mediaAssetsChanged, this, &MobileTimeline::rebuild);
    connect(&preview_, &MobilePreview::positionChanged, this, &MobileTimeline::publishPosition);
    connect(&preview_, &MobilePreview::transportChanged, this, [this] {
        bridge_.setPlayheadUpperLimitSeconds(preview_.durationSeconds());
    });
    connect(&preview_, &MobilePreview::playingChanged, this, [this] {
        bridge_.setPlaybackCadenceActive(preview_.playing());
        editor_.setPlaybackActive(preview_.playing());
        publishPosition();
    });
    connect(&editor_, &EditorSyncController::caretLocationPublished, this,
        [this](int difficulty, quint64 revision, int line, int column) {
            if (navigating_ || difficulty != document_.activeDifficulty()
                || revision != document_.documentRevision()) return;
            double second = 0;
            if (model_.resolveTimelineSecondForCursor(line, column, &second))
                bridge_.setCursorSeconds(second, !preview_.playing());
        });
    connect(&editor_, &EditorSyncController::previewSeekPublished, this,
        [this](int difficulty, int line, int column) {
            double second = 0;
            if (difficulty == document_.activeDifficulty()
                && model_.resolveTimelineSecondForCursor(line, column, &second))
                preview_.setPositionSeconds(second);
        });
    connect(&analysis, &AnalysisService::analysisReady, this,
        [this, &analysis](int difficulty, quint64 revision) {
            if (difficulty != document_.activeDifficulty() || revision != document_.documentRevision()) return;
            const auto result = analysis.snapshot();
            bridge_.setMuriAnalysisReport(result.muri);
            preview_.sceneRuntime().setMuriAnalysisReport(result.muri);
        });
    rebuild();
}

QString MobileTimeline::timelineTabLabel() const { return qtTrId("window.timeline"); }
QString MobileTimeline::validationTabLabel() const { return qtTrId("window.syntax"); }
QString MobileTimeline::muriTabLabel() const { return qtTrId("window.muri"); }
QString MobileTimeline::followCodeLabel() const { return qtTrId("shell.follow_code"); }
void MobileTimeline::setCurrentTabId(const QString& tab) {
    if (tab == tab_ || (tab != "timeline" && tab != "validation" && tab != "muri")) return;
    tab_ = tab;
    emit tabChanged();
}

void MobileTimeline::rebuild() {
    const auto& data = document_.workspace().document();
    model_.rebuildFromText(document_.chartText(), timeline::offset::parsedFirstSeconds(data.first),
        simai::buildTimingMetadata(data));
    bridge_.setTimelineData(model_.snapshot());
    bridge_.setPlaybackEntrySeconds(preview_.lowerBoundSeconds());
    bridge_.setPlayheadUpperLimitSeconds(preview_.durationSeconds());
    const QFileInfo track(document_.previewAssetPath("audio"));
    const QString identity = track.absoluteFilePath() + ':' + QString::number(track.size())
        + ':' + QString::number(track.lastModified().toMSecsSinceEpoch());
    if (identity != trackIdentity_) {
        trackIdentity_ = identity;
        const quint64 request = ++waveformRequest_;
        bridge_.setWaveformData({});
        if (track.isFile()) {
            QPointer<MobileTimeline> guard(this);
            // Cache lives beside the private project copy, never in the SAF provider.
            waveforms_.requestWaveform(track.absoluteFilePath(), track.absolutePath() + "/.waveform-cache",
                [guard, request](waveform::WaveformDataPtr waveform) {
                    if (guard && guard->waveformRequest_ == request) guard->bridge_.setWaveformData(waveform);
                });
        }
    }
    publishPosition();
}

void MobileTimeline::publishPosition() {
    bridge_.setPlayheadSeconds(preview_.positionSeconds(), preview_.playing() && bridge_.followProgressEnabled());
    EditorFollowState follow;
    follow.difficultyId = document_.activeDifficulty();
    follow.revision = document_.documentRevision();
    follow.playbackActive = preview_.playing();
    TimelineQuickModel::PreviewFollowSpan span;
    if (model_.resolvePreviewFollowSpan(preview_.positionSeconds(), &span)) {
        follow.active = true;
        follow.start = span.startPosition;
        follow.end = span.endPositionExclusive;
        follow.caret = span.cursorPosition;
        follow.reveal = bridge_.followPreviewEnabled();
    }
    editor_.publishFollow(follow);
}

void MobileTimeline::navigate(double second, bool center) {
    if (!qIsFinite(second)) return;
    navigating_ = true;
    preview_.setPositionSeconds(second);
    int line = 1, column = 1;
    double cursorSecond = 0;
    if (model_.resolveTimelineNavigateCursor(second, &line, &column, &cursorSecond)) {
        const int position = document_.chartPosition(line, column);
        editor_.requestNavigation(document_.activeDifficulty(), document_.documentRevision(), position, position, false, true);
        bridge_.setCursorSeconds(cursorSecond, center);
    }
    bridge_.setPlayheadSeconds(preview_.positionSeconds(), center);
    navigating_ = false;
}
void MobileTimeline::headerNavigate(double second) { navigate(second, false); }
void MobileTimeline::wheelNavigate(double second) { navigate(second, false); }
void MobileTimeline::centerNavigate(double second) { navigate(second, true); }
void MobileTimeline::dragStarted() { preview_.beginScrub(); }
void MobileTimeline::dragFinished(double second) { navigate(second, false); preview_.endScrub(); }
void MobileTimeline::userInteractionStarted() { bridge_.setFollowProgressEnabled(false); }
void MobileTimeline::surfaceReady() { publishPosition(); }
void MobileTimeline::followPreviewToggled(bool enabled) { bridge_.setFollowPreviewEnabled(enabled); publishPosition(); }
}
