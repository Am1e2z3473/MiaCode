#include "MobilePreview.h"
#include "timeline/TimelineMarkerOffset.h"
#include "common/AssetPaths.h"
#include "tools/muri/MuriAnalyzer.h"
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QDebug>
#include <QUrl>

namespace miacode::android {
MobilePreview::MobilePreview(AndroidDocumentSession* document, QObject* parent)
    : QObject(parent), document_(document), runtime_(this), audio_(this), audioOutput_(this), video_(this),
      statisticsCache_(std::make_shared<miacode::preview::scene::PreviewProgressStatsCache>())
{
    audio_.setAudioOutput(&audioOutput_);
    audioOutput_.setVolume(1);
    runtime_.setSkinDirectory(miacode::assets::assetPath("skin/skinDX"));
    runtime_.setProgressStatsCache(statisticsCache_);
    runtime_.setStageMediaPresentationMode(miacode::preview::scene::PreviewStageMediaPresentationMode::ExternalQuickMediaItem);
    timer_.setInterval(16);
    timer_.setTimerType(Qt::PreciseTimer);
    connect(&timer_, &QTimer::timeout, this, &MobilePreview::tick);
    connect(document_, &AndroidDocumentSession::documentStateChanged, this, &MobilePreview::refreshDocument);
    connect(document_, &AndroidDocumentSession::mediaAssetsChanged, this, &MobilePreview::refreshMedia);
    connect(&audio_, &QMediaPlayer::positionChanged, this, [this](qint64 ms) {
        audioAnchor_ = ms / 1000.0;
        audioClockAge_.restart();
    });
    connect(&audio_, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString& message) {
        setPlaying(false);
        emit mediaError(message);
    });
    connect(&audio_, &QMediaPlayer::durationChanged, this, [this](qint64 ms) {
        duration_ = qMax(duration_, ms / 1000.0);
        emit transportChanged();
    });
    refreshDocument();
}

void MobilePreview::refreshDocument()
{
    const auto& document = document_->workspace().document();
    first_ = miacode::timeline::offset::parsedFirstSeconds(document.first);
    const auto parsed = SimaiNativeParser::parseForTimeline(document_->chartText(), miacode::simai::buildTimingMetadata(document));
    const auto markers = miacode::timeline::offset::shiftedNoteMarkers(parsed.noteMarkers, first_,
        miacode::timeline::offset::NonFiniteHandling::PassThrough);
    runtime_.setNoteMarkers(markers);
    runtime_.setMuriRenderOptions(renderOptions_);
    // AnalysisService publishes revision-gated results asynchronously.
    statisticsCache_->rebuild(markers);
    duration_ = qMax(qMax(0.0, parsed.durationSeconds + first_), audio_.duration() / 1000.0);
    const auto* difficulty = document.difficulty(document_->activeDifficulty());
    runtime_.setChartInfo(document.title, document.artist,
        SimaiDocument::difficultyName(document_->activeDifficulty()) + " " + (difficulty ? difficulty->level : QString()),
        difficulty && !difficulty->designer.trimmed().isEmpty() ? difficulty->designer : document.designer);
    const QString path = QFileInfo(document_->currentFilePath()).absolutePath();
    if (path != projectPath_) {
        setPlaying(false);
        projectPath_ = path;
        publishPosition(lowerBoundSeconds());
    }
    refreshMedia();
    publishPosition(qBound(lowerBoundSeconds(), position_, duration_));
    emit transportChanged();
}

void MobilePreview::refreshMedia()
{
    const auto localUrl = [](const QString& path) { return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path); };
    const QUrl audioSource = localUrl(document_->previewAssetPath("audio"));
    if (audio_.source() != audioSource) {
        setPlaying(false);
        audioClockAge_.invalidate();
        audio_.setSource(audioSource);
        audio_.setPosition(qMax<qint64>(0, qRound64(position_ * 1000)));
    }
    const QUrl videoSource = localUrl(document_->previewAssetPath("video"));
    if (video_.source() != videoSource) {
        video_.pause();
        hasVideoFrame_ = false;
        video_.setSource(videoSource);
        video_.setPosition(qMax<qint64>(0, qRound64(position_ * 1000)));
        if (playing_ && mediaVisible()) video_.play();
    }
    const QUrl imageSource = localUrl(document_->previewAssetPath("image"));
    if (imageSource_ != imageSource) {
        QImageReader reader(imageSource.toLocalFile());
        const bool readable = !imageSource.isEmpty() && reader.canRead();
        imageSource_ = readable ? imageSource : QUrl();
        if (readable) qInfo() << "Mobile preview image:" << reader.format() << reader.size();
        else if (!imageSource.isEmpty()) emit mediaError(reader.errorString());
    }
    runtime_.setStageMediaAvailable(mediaVisible());
    emit mediaChanged();
}

void MobilePreview::publishPosition(double second)
{
    position_ = second;
    runtime_.setPlayheadSeconds(second);
    emit positionChanged();
}

void MobilePreview::setPositionSeconds(double second)
{
    if (!qIsFinite(second)) return;
    second = qBound(lowerBoundSeconds(), second, duration_);
    clockAnchor_ = second;
    clock_.restart();
    audioClockAge_.invalidate();
    audio_.setPosition(qMax<qint64>(0, qRound64(second * 1000)));
    video_.setPosition(qMax<qint64>(0, qRound64(second * 1000)));
    publishPosition(second);
}

void MobilePreview::setRate(double rate)
{
    if (!qIsFinite(rate)) return;
    setPositionSeconds(position_);
    rate_ = qBound(0.25, rate, 2.0);
    audio_.setPlaybackRate(rate_);
    video_.setPlaybackRate(rate_);
    emit transportChanged();
}

void MobilePreview::setPlaying(bool playing)
{
    if (playing == playing_) return;
    playing_ = playing;
    if (playing) {
        if (rangeEnabled_ && (position_ < rangeStart_ || position_ >= rangeEnd_)) setPositionSeconds(rangeStart_);
        else if (position_ >= duration_) setPositionSeconds(lowerBoundSeconds());
        clockAnchor_ = position_;
        clock_.restart();
        audioClockAge_.invalidate();
        if (!audio_.source().isEmpty() && position_ >= 0) audio_.play();
        if (hasVideoMedia() && position_ >= 0 && mediaVisible()) video_.play();
        timer_.start();
    } else {
        timer_.stop();
        audio_.pause();
        video_.pause();
    }
    emit playingChanged();
}

void MobilePreview::tick()
{
    double second = clockAnchor_ + clock_.elapsed() / 1000.0 * rate_;
    if (hasVideoMedia() && second >= 0 && mediaVisible() && video_.playbackState() != QMediaPlayer::PlayingState) video_.play();
    if (!audio_.source().isEmpty() && second >= 0) {
        if (audio_.playbackState() != QMediaPlayer::PlayingState) audio_.play();
        if (audioClockAge_.isValid()) second = audioAnchor_ + qMin<qint64>(audioClockAge_.elapsed(), 150) / 1000.0 * rate_;
    }
    const double end = rangeEnabled_ ? rangeEnd_ : duration_;
    publishPosition(qMin(second, end));
    if (second >= end) setPlaying(false);
}

void MobilePreview::setPlaybackRangeEnabled(bool enabled, double start, double end)
{
    if (!enabled || !qIsFinite(start) || !qIsFinite(end) || end <= start) {
        rangeEnabled_ = false;
        return;
    }
    rangeEnabled_ = true;
    rangeStart_ = qBound(lowerBoundSeconds(), start, duration_);
    rangeEnd_ = qBound(rangeStart_, end, duration_);
    if (rangeEnabled_ && rangeEnd_ <= rangeStart_) rangeEnabled_ = false;
    if (rangeEnabled_ && (position_ < rangeStart_ || position_ > rangeEnd_)) setPositionSeconds(rangeStart_);
}

void MobilePreview::stop() { setPlaying(false); setPositionSeconds(lowerBoundSeconds()); }
void MobilePreview::beginScrub() { resumeAfterScrub_ = playing_; setPlaying(false); }
void MobilePreview::endScrub() { setPlaying(resumeAfterScrub_); resumeAfterScrub_ = false; }

QVariantList MobilePreview::statistics() const
{
    const auto s = statisticsCache_->snapshotAt(position_);
    QVariantList result;
    const auto row = [&result](const char* kind, const char* name, int played, int total) {
        result.append(QVariantMap{{"kind", kind}, {"name", name}, {"played", played}, {"total", total},
            {"value", QString::number(played) + "/" + QString::number(total)}, {"iconSource", QString()}});
    };
    row("tap", "Tap", s.tapPlayed, s.tapTotal);
    row("hold", "Hold", s.holdPlayed, s.holdTotal);
    row("slide", "Slide", s.slidePlayed, s.slideTotal);
    row("touch", "Touch", s.touchPlayed, s.touchTotal);
    row("break", "Break", s.breakPlayed, s.breakTotal);
    row("total", "Total", s.totalPlayed, s.totalCount);
    for (auto& value : result) {
        auto item = value.toMap();
        if (item.value("kind").toString() != "total") item.insert("iconSource", "image://noteicon/" + item.value("kind").toString());
        value = item;
    }
    return result;
}

QVariantMap MobilePreview::muriParameterRanges() const
{
    using namespace miacode::muri;
    return {{"handRadiusDefault", kHandRadiusDefaultPx}, {"handRadiusMin", kHandRadiusMinPx},
        {"handRadiusMax", kHandRadiusMaxPx}, {"handRadiusStep", kHandRadiusStepPx},
        {"tapOnSlideThresholdMin", kStaticTapOnSlideThresholdMinMs},
        {"tapOnSlideThresholdMax", kStaticTapOnSlideThresholdMaxMs},
        {"tapOnSlideThresholdStep", kStaticTapOnSlideThresholdStepMs}};
}
void MobilePreview::setMuriCheckEnabled(bool enabled)
{
    renderOptions_.renderMode = enabled ? RenderMode::MaimuriDxStyle : RenderMode::Native;
    refreshDocument();
    emit renderModeChanged();
}
void MobilePreview::setSmoothStarErase(bool enabled)
{
    renderOptions_.renderMode = enabled ? RenderMode::EraseByArea : RenderMode::Native;
    refreshDocument();
    emit renderModeChanged();
}
void MobilePreview::setMuriHandRadiusPx(int value)
{
    renderOptions_.handRadiusPx = miacode::muri::normalizedHandRadiusPx(value);
    refreshDocument();
    emit muriParametersChanged();
}
void MobilePreview::setMuriTapOnSlideThresholdMs(int value)
{
    thresholdMs_ = qBound(miacode::muri::kStaticTapOnSlideThresholdMinMs, value, miacode::muri::kStaticTapOnSlideThresholdMaxMs);
    refreshDocument();
    emit muriParametersChanged();
}

void MobilePreview::setHidePv(bool value)
{
    if (hidePv_ == value) return;
    hidePv_ = value;
    if (!mediaVisible()) video_.pause();
    else if (playing_) video_.play();
    runtime_.setStageMediaAvailable(mediaVisible());
    emit mediaChanged();
}

void MobilePreview::setPausedJudgeAreaView(bool enabled)
{
    if (pausedJudgeAreaView_ == enabled) return;
    pausedJudgeAreaView_ = enabled;
    if (!mediaVisible()) video_.pause();
    else if (playing_) video_.play();
    runtime_.setStageMediaAvailable(mediaVisible());
    emit mediaChanged();
}

void MobilePreview::setJudgeOverlayOptions(const MuriRenderOptions& options)
{
    // Preserve the independently selected render mode and analysis parameters.
    renderOptions_.showChartReviewSlideJudgeOverlay = options.showChartReviewSlideJudgeOverlay;
    renderOptions_.showChartReviewTapJudgeOverlay = options.showChartReviewTapJudgeOverlay;
    renderOptions_.showChartReviewBreakJudgeOverlay = options.showChartReviewBreakJudgeOverlay;
    renderOptions_.showChartReviewTouchJudgeOverlay = options.showChartReviewTouchJudgeOverlay;
    runtime_.setMuriRenderOptions(renderOptions_);
}

void MobilePreview::attachVideoOutputObject(QObject* output)
{
    attachVideoOutputObjects(output, nullptr);
}

void MobilePreview::attachVideoOutputObjects(QObject* output, QObject* inner)
{
    if (!output) return;
    auto* sink = output->property("videoSink").value<QVideoSink*>();
    if (!sink) return;
    QObject::disconnect(frameConnection_);
    QObject::disconnect(innerFrameConnection_);
    videoSink_ = sink;
    video_.setVideoSink(sink);
    frameConnection_ = connect(sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame& frame) {
        if (frame.isValid() && !hasVideoFrame_) { hasVideoFrame_ = true; emit mediaChanged(); }
    });
    if (inner) {
        auto* innerSink = inner->property("videoSink").value<QVideoSink*>();
        if (innerSink) innerFrameConnection_ = connect(sink, &QVideoSink::videoFrameChanged, innerSink,
            [innerSink](const QVideoFrame& frame) { innerSink->setVideoFrame(frame); });
    }
}

void MobilePreview::detachVideoOutputObject(QObject* output)
{
    if (!output || output->property("videoSink").value<QVideoSink*>() != videoSink_) return;
    QObject::disconnect(frameConnection_);
    QObject::disconnect(innerFrameConnection_);
    video_.setVideoSink(nullptr);
    videoSink_.clear();
}
}
