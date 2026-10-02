#pragma once

#include "AndroidDocumentSession.h"
#include "preview/runtime/PreviewRuntime.h"
#include "core/scene/PreviewProgressStatsCache.h"
#include "audio/PreviewAudioSettings.h"
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QElapsedTimer>
#include <QTimer>
#include <QVideoSink>
#include <QPointer>

namespace miacode::android {

// Android transport adapter. The chart parser, offset rules, skin loader,
// scene graph and statistics are the production v2 implementations.
class MobilePreview final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* runtime READ runtime CONSTANT)
    Q_PROPERTY(QObject* mediaHost READ mediaHost CONSTANT)
    Q_PROPERTY(double positionSeconds READ positionSeconds WRITE setPositionSeconds NOTIFY positionChanged)
    Q_PROPERTY(double durationSeconds READ durationSeconds NOTIFY transportChanged)
    Q_PROPERTY(double lowerBoundSeconds READ lowerBoundSeconds NOTIFY transportChanged)
    Q_PROPERTY(double rate READ rate WRITE setRate NOTIFY transportChanged)
    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)
    Q_PROPERTY(QString renderModeLabel READ renderModeLabel NOTIFY renderModeChanged)
    Q_PROPERTY(bool muriCheckEnabled READ muriCheckEnabled NOTIFY renderModeChanged)
    Q_PROPERTY(bool smoothStarErase READ smoothStarErase NOTIFY renderModeChanged)
    Q_PROPERTY(int muriHandRadiusPx READ muriHandRadiusPx NOTIFY muriParametersChanged)
    Q_PROPERTY(int muriTapOnSlideThresholdMs READ muriTapOnSlideThresholdMs NOTIFY muriParametersChanged)
    Q_PROPERTY(QVariantMap muriParameterRanges READ muriParameterRanges CONSTANT)
    Q_PROPERTY(double canvasAspectRatio READ canvasAspectRatio CONSTANT)
    Q_PROPERTY(QVariantList statistics READ statistics NOTIFY positionChanged)
    Q_PROPERTY(bool statisticsAvailable READ statisticsAvailable NOTIFY transportChanged)
    Q_PROPERTY(bool hasResolvedMedia READ hasResolvedMedia NOTIFY mediaChanged)
    Q_PROPERTY(bool hasVideoMedia READ hasVideoMedia NOTIFY mediaChanged)
    Q_PROPERTY(bool chartHasVideoBackground READ hasVideoMedia NOTIFY mediaChanged)
    Q_PROPERTY(bool hasVideoFrame READ hasVideoFrame NOTIFY mediaChanged)
    Q_PROPERTY(bool mediaVisible READ mediaVisible NOTIFY mediaChanged)
    Q_PROPERTY(bool hidePv READ hidePv WRITE setHidePv NOTIFY mediaChanged)
    Q_PROPERTY(int backgroundScaleMode READ backgroundScaleMode NOTIFY mediaLayoutChanged)
    Q_PROPERTY(double layoutSquareScale READ layoutSquareScale NOTIFY mediaLayoutChanged)
    Q_PROPERTY(QUrl imageSource READ imageSource NOTIFY mediaChanged)
public:
    explicit MobilePreview(AndroidDocumentSession* document, QObject* parent = nullptr);
    QObject* runtime() { return &runtime_; }
    QObject* mediaHost() { return this; }
    bool hasResolvedMedia() const { return hasVideoMedia() || !imageSource_.isEmpty(); }
    bool hasVideoMedia() const { return !video_.source().isEmpty(); }
    bool hasVideoFrame() const { return hasVideoFrame_; }
    bool mediaVisible() const { return hasResolvedMedia() && !hidePv_ && !pausedJudgeAreaView_; }
    bool hidePv() const { return hidePv_; }
    int backgroundScaleMode() const { return static_cast<int>(runtime_.frameState().render.backgroundScaleMode); }
    double layoutSquareScale() const { return runtime_.frameState().render.layoutSquareScale; }
    void setPausedJudgeAreaView(bool enabled);
    void refreshMediaLayout() { emit mediaLayoutChanged(); }
    QUrl imageSource() const { return imageSource_; }
    void setHidePv(bool value);
    Q_INVOKABLE void attachVideoOutputObject(QObject* output);
    Q_INVOKABLE void attachVideoOutputObjects(QObject* output, QObject* inner);
    Q_INVOKABLE void detachVideoOutputObject(QObject* output);
    Q_INVOKABLE void detachVideoOutputObjects(QObject* output, QObject*) { detachVideoOutputObject(output); }
    double positionSeconds() const { return position_; }
    double durationSeconds() const { return duration_; }
    double lowerBoundSeconds() const { return qMin(0.0, first_); }
    double rate() const { return rate_; }
    bool playing() const { return playing_; }
    QString renderModeLabel() const { return muriCheckEnabled() ? QStringLiteral("无理检测") : QStringLiteral("标准预览"); }
    bool muriCheckEnabled() const { return renderOptions_.renderMode == RenderMode::MaimuriDxStyle; }
    bool smoothStarErase() const { return renderOptions_.renderMode == RenderMode::EraseByArea; }
    int muriHandRadiusPx() const { return renderOptions_.handRadiusPx; }
    int muriTapOnSlideThresholdMs() const { return thresholdMs_; }
    QVariantMap muriParameterRanges() const;
    Q_INVOKABLE void setMuriCheckEnabled(bool enabled);
    Q_INVOKABLE void setSmoothStarErase(bool enabled);
    Q_INVOKABLE void setMuriHandRadiusPx(int value);
    Q_INVOKABLE void setMuriTapOnSlideThresholdMs(int value);
    double canvasAspectRatio() const { return 1.0; }
    QVariantList statistics() const;
    bool statisticsAvailable() const { return !document_->chartText().isEmpty(); }
    void setPositionSeconds(double second);
    void setRate(double rate);
    void applyAudioSettings(const PreviewAudioSettings& settings) { audioOutput_.setVolume(previewTrackVolume(settings)); }
    void setPlaying(bool playing);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void beginScrub();
    Q_INVOKABLE void updateScrub(double second) { setPositionSeconds(second); }
    Q_INVOKABLE void endScrub();
    Q_INVOKABLE void setPlaybackRangeEnabled(bool enabled, double start, double end);
    Q_INVOKABLE void logPreviewInteraction(const QString&, const QString&) const {}
    PreviewRuntime& sceneRuntime() { return runtime_; }
    const MuriRenderOptions& muriRenderOptions() const { return renderOptions_; }
    void setJudgeOverlayOptions(const MuriRenderOptions& options);
signals:
    void positionChanged();
    void transportChanged();
    void playingChanged();
    void renderModeChanged();
    void muriParametersChanged();
    void mediaError(const QString& message);
    void mediaChanged();
    void mediaLayoutChanged();
private:
    void refreshDocument();
    void refreshMedia();
    void tick();
    void publishPosition(double second);
    AndroidDocumentSession* document_;
    PreviewRuntime runtime_;
    QMediaPlayer audio_;
    QAudioOutput audioOutput_;
    QMediaPlayer video_;
    QPointer<QVideoSink> videoSink_;
    QMetaObject::Connection frameConnection_;
    QMetaObject::Connection innerFrameConnection_;
    QUrl imageSource_;
    QTimer timer_;
    QElapsedTimer clock_;
    QElapsedTimer audioClockAge_;
    std::shared_ptr<miacode::preview::scene::PreviewProgressStatsCache> statisticsCache_;
    QString projectPath_;
    double position_ = 0;
    double duration_ = 0;
    double first_ = 0;
    double rate_ = 1;
    double clockAnchor_ = 0;
    double audioAnchor_ = 0;
    bool playing_ = false;
    bool resumeAfterScrub_ = false;
    MuriRenderOptions renderOptions_;
    int thresholdMs_ = miacode::muri::kStaticTapOnSlideThresholdDefaultMs;
    bool hasVideoFrame_ = false;
    bool hidePv_ = false;
    bool pausedJudgeAreaView_ = false;
    bool rangeEnabled_ = false;
    double rangeStart_ = 0;
    double rangeEnd_ = 0;
};
}
