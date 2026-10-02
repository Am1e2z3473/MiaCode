#include "MobileExportComposition.h"
#include "ExportDestination.h"
#include "common/AssetPaths.h"
#include "common/ChartAssetPaths.h"
#include "common/IntroConfig.h"
#include "tools/video_export/VideoExportSettings.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

namespace miacode::android {
MobileExportComposition::MobileExportComposition(AndroidDocumentSession& document, MobilePreview& preview,
    MobileVideoExport& exporter, QObject* parent)
    : QObject(parent), document_(document), preview_(preview), exporter_(exporter),
      session_(notifications_, requests_, progress_, appearance_, engineSlot_, surfaceSlot_),
      settings_(notifications_, requests_, appearance_, surfaceSlot_) {
    appearance_.setSkinDirectory(QFileInfo(resolveSkinDir()).fileName());
    connect(&appearance_, &PreviewAppearanceState::changed, this, &MobileExportComposition::refreshSurfaces);
    connect(&preview_, &MobilePreview::playingChanged, this, &MobileExportComposition::applyEffectiveOutline);
    connect(&session_, &ui::ExportSession::pageSessionActiveChanged, this, &MobileExportComposition::applyEffectiveOutline);
    connect(&document_, &AndroidDocumentSession::documentReplaced, this, [this] {
        previousDifficulty_ = 0;
        session_.replaceDocument(document_.activeDifficulty());
    });
    connect(&exporter_, &MobileVideoExport::changed, this, [this] {
        emit notifications_.videoExportWorkerRunningChanged(exporter_.running());
        if (exporter_.running() && progress_.token() == jobToken_)
            progress_.report(exporter_.percent(), exportDestinationDisplayPath(exporter_.outputPath()));
    });
    connect(&exporter_, &MobileVideoExport::finished, this, [this](bool ok, const QString& output, const QString& error) {
        if (progress_.token() == jobToken_) progress_.end();
        requests_.postNotice(ok ? NoticeSeverity::Information : NoticeSeverity::Error,
            qtTrId("dialog.video_export.title"), ok ? exportDestinationDisplayPath(output) : error);
    });
    connect(&progress_, &JobProgressService::cancellationRequested, this, [this](quint64 token) {
        if (token == jobToken_) exporter_.cancel();
    });
    restoreAudioSettingsFromSoftwareDefault();
    applyEffectiveOutline();
}
MobileExportComposition::~MobileExportComposition() {
    session_.leave();
    engineSlot_ = nullptr; surfaceSlot_ = nullptr;
}
VideoExportTask MobileExportComposition::buildSeedTask(int id) {
    auto task = exporter_.buildTask(id);
    const auto& state = preview_.sceneRuntime().frameState();
    const auto& r = state.render;
    task.audioSettings = audioSettings_;
    task.backgroundBrightnessOuter = r.backgroundBrightnessOuter;
    task.backgroundBrightnessInner = r.backgroundBrightnessInner;
    task.layoutSquareScale = r.layoutSquareScale; task.backgroundScaleMode = r.backgroundScaleMode;
    task.smoothBrightness = r.smoothBrightness; task.tapFlowSpeed = r.tapFlowSpeed; task.touchFlowSpeed = r.touchFlowSpeed;
    task.showTimestamp = r.showTimestamp; task.showObjectStatsHud = r.showObjectStatsHud;
    task.showChartInfoHud = r.showChartInfoHud; task.fixHudTextLayout = r.fixHudTextLayout;
    task.outlineVariant = appearance_.outlineVariant();
    task.outlineImagePath = customOutline_.isEmpty() ? QString() : QDir(resolveCustomOutlineDir()).filePath(customOutline_);
    task.judgeEffectStyle = appearance_.judgeEffectStyle(); task.tapJudgeTextDistance = appearance_.tapJudgeTextDistance();
    task.slideEarlierSecondAndTextOnTop = appearance_.slideEarlierSecondAndTextOnTop();
    task.centerDisplayMode = appearance_.centerDisplayMode();
    task.muriAnalysisReport = state.muriAnalysisReport;
    return task;
}
void MobileExportComposition::applySharedTaskSettings(const VideoExportTask& task) {
    auto& runtime = preview_.sceneRuntime();
    runtime.setBackgroundBrightnessOuter(task.backgroundBrightnessOuter);
    runtime.setBackgroundBrightnessInner(task.backgroundBrightnessInner);
    runtime.setLayoutSquareScale(task.layoutSquareScale); runtime.setBackgroundScaleMode(task.backgroundScaleMode);
    runtime.setSmoothBrightness(task.smoothBrightness); runtime.setTapFlowSpeed(task.tapFlowSpeed);
    runtime.setTouchFlowSpeed(task.touchFlowSpeed); runtime.setShowTimestamp(task.showTimestamp);
    runtime.setShowObjectStatsHud(task.showObjectStatsHud); runtime.setShowChartInfoHud(task.showChartInfoHud);
    runtime.setFixHudTextLayout(task.fixHudTextLayout);
    preview_.refreshMediaLayout();
    emit notifications_.previewRenderSettingsChanged();
}
bool MobileExportComposition::startAudition(int id, const VideoExportTask& task) {
    if (!document_.workspace().document().difficulty(id)) return false;
    if (!previousDifficulty_) previousDifficulty_ = document_.activeDifficulty();
    preview_.setPlaying(false); document_.selectDifficulty(id); applySharedTaskSettings(task);
    return true;
}
void MobileExportComposition::stopAudition() {
    if (!previousDifficulty_) return;
    preview_.setPlaying(false);
    const int id = previousDifficulty_; previousDifficulty_ = 0;
    if (document_.workspace().document().difficulty(id)) document_.selectDifficulty(id);
    preview_.sceneRuntime().clearIntroOverlay();
}
bool MobileExportComposition::launchVideoExport(const VideoExportTask& requested, int id, QString* error) {
    auto task = buildSeedTask(id);
    video_export::copyVideoExportUserSettings(requested, &task);
    // The range is a command, not a persisted preference.
    task.exportStartSeconds = requested.exportStartSeconds;
    task.contentDurationSeconds = requested.contentDurationSeconds;
    task.fullRangeExport = requested.fullRangeExport;
    task.outputPath = requested.outputPath;
    jobToken_ = progress_.begin(qtTrId("dialog.video_export.title"), exportDestinationDisplayPath(task.outputPath), true, JobProgressService::TaskType::ChartExport);
    if (!exporter_.start(task, error)) { progress_.end(); return false; }
    return true;
}
bool MobileExportComposition::launchBatchExport(const VideoExportTask&, const QStringList&, const QList<int>&,
    const QString&, BatchResult*, const BatchCallbacks&, QString* error) {
    if (error) *error = QStringLiteral("Android batch export is not connected yet");
    return false;
}
QString MobileExportComposition::difficultyChartText(int id) const {
    const auto* difficulty = document_.workspace().document().difficulty(id);
    return difficulty ? difficulty->chart : QString();
}
void MobileExportComposition::refreshIntroState() {
    const auto spec = session_.previewIntroSpec();
    auto& runtime = preview_.sceneRuntime();
    if (!spec.enabled) { runtime.clearIntroOverlay(); return; }
    QFile file(QStringLiteral(":/intro/templates/maimai_banner.json"));
    if (!file.open(QIODevice::ReadOnly)) return;
    runtime.setIntroOverlayData(introBannerTrackMap(spec), QJsonDocument::fromJson(file.readAll()).object().toVariantMap(),
        chart_assets::displayBackgroundImageUrl(spec.jacketPath), QUrl(QString::fromLatin1(intro::kLogoFallbackUrl)), introBannerStyleMap(spec));
    runtime.setIntroOverlayFrame(intro::authoringFrameForOutputFrame(qRound(qMax(0.0, preview_.positionSeconds()) * 60), 60), false);
}
PlaybackTransportState MobileExportComposition::playbackTransportState() const {
    return preview_.playing() ? PlaybackTransportState::Playing : PlaybackTransportState::Paused;
}
QStringList MobileExportComposition::statsTexts() const {
    QStringList values;
    for (const auto& item : preview_.statistics()) values.append(item.toMap().value("value").toString());
    return values;
}
void MobileExportComposition::setMuriRenderMode(RenderMode mode) {
    if (mode == RenderMode::MaimuriDxStyle) preview_.setMuriCheckEnabled(true);
    else preview_.setSmoothStarErase(mode == RenderMode::EraseByArea);
}
QString MobileExportComposition::resolveSkinRootDir() const { return assets::assetPath("skin"); }
QStringList MobileExportComposition::availableSkinDirectoryNames() const {
    const QDir root(resolveSkinRootDir());
    const auto hasCoreAssets = [&root](const QString& name) {
        const QDir skin(root.filePath(name));
        return QFileInfo::exists(skin.filePath("tap.png")) && QFileInfo::exists(skin.filePath("hold.png"))
            && QFileInfo::exists(skin.filePath("star.png"));
    };
    QStringList names;
    const QStringList builtIns{QStringLiteral("skinSD"), QStringLiteral("skinDX")};
    for (const auto& name : builtIns) if (hasCoreAssets(name)) names.append(name);
    for (const auto& name : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase)) {
        bool builtIn = false;
        for (const auto& candidate : builtIns) builtIn |= name.compare(candidate, Qt::CaseInsensitive) == 0;
        if (!builtIn && hasCoreAssets(name)) names.append(name);
    }
    return names;
}
QString MobileExportComposition::skinDisplayName(const QString& name) const {
    const auto trimmed = name.trimmed();
    if (trimmed.compare("skinSD", Qt::CaseInsensitive) == 0 || trimmed.compare("skinSTD", Qt::CaseInsensitive) == 0)
        return qtTrId("dialog.render_settings.video.skin.standard");
    if (trimmed.compare("skinDX", Qt::CaseInsensitive) == 0) return qtTrId("dialog.render_settings.video.skin.dx");
    return trimmed;
}
QString MobileExportComposition::resolveCustomOutlineDir() const { return assets::customOutlineRootPath(); }
QStringList MobileExportComposition::availableCustomOutlineFileNames() const {
    return QDir(resolveCustomOutlineDir()).entryList({"*.png"}, QDir::Files, QDir::Name | QDir::IgnoreCase);
}
void MobileExportComposition::applyOutlineVariant(PreviewOutlineVariant value, bool, bool) {
    customOutline_.clear(); appearance_.setOutlineVariant(value);
    applyEffectiveOutline();
    emit notifications_.previewRenderSettingsChanged();
}
void MobileExportComposition::applyCustomOutlineFileName(const QString& name, bool) {
    if (!availableCustomOutlineFileNames().contains(name)) return;
    customOutline_ = name;
    applyEffectiveOutline();
    emit notifications_.previewRenderSettingsChanged();
}
QVariantMap MobileExportComposition::renderSettings() const {
    const auto& r = preview_.sceneRuntime().frameState().render;
    QVariantMap values;
    values.insert("brightnessOuter", r.backgroundBrightnessOuter * 100); values.insert("brightnessInner", r.backgroundBrightnessInner * 100);
    values.insert("layoutSquareScale", r.layoutSquareScale * 100);
    values.insert("layoutSquareScaleMin", preview_video::kLayoutSquareScaleMin * 100);
    values.insert("layoutSquareScaleMax", preview_video::kLayoutSquareScaleMax * 100);
    values.insert("layoutSquareScaleStep", preview_video::kLayoutSquareScaleStep * 100);
    values.insert("scaleMode", static_cast<int>(r.backgroundScaleMode)); values.insert("smoothBrightness", r.smoothBrightness);
    values.insert("showTimestamp", r.showTimestamp); values.insert("showDebugInfo", r.showDebugInfo);
    values.insert("tapFlowSpeed", r.tapFlowSpeed); values.insert("touchFlowSpeed", r.touchFlowSpeed);
    values.insert("flowSpeedMin", preview_gameplay::kPreviewTimingFlowSpeedMin);
    values.insert("flowSpeedMax", preview_gameplay::kPreviewTimingFlowSpeedMax);
    values.insert("flowSpeedStep", preview_gameplay::kPreviewTimingFlowSpeedStep);
    const auto& options = preview_.muriRenderOptions();
    values.insert("judgeEffectSlide", options.showChartReviewSlideJudgeOverlay);
    values.insert("judgeEffectTap", options.showChartReviewTapJudgeOverlay);
    values.insert("judgeEffectBreak", options.showChartReviewBreakJudgeOverlay);
    values.insert("judgeEffectTouch", options.showChartReviewTouchJudgeOverlay);
    values.insert("forceLabeledJudgeLineWhenPaused", forceLabeledJudgeLineWhenPaused_);
    values.insert("slideEarlierOnTop", appearance_.slideEarlierSecondAndTextOnTop());
    values.insert("centerDisplay", static_cast<int>(appearance_.centerDisplayMode()));
    values.insert("tapJudgeTextDistance", static_cast<int>(appearance_.tapJudgeTextDistance()));
    values.insert("touchPadAuthoringShortcut", preview_.sceneRuntime().touchPadAuthoringEnabled());
    return values;
}
void MobileExportComposition::setRenderSetting(const QString& key, const QVariant& value) {
    auto& runtime = preview_.sceneRuntime();
    if (key == "brightnessOuter") runtime.setBackgroundBrightnessOuter(qBound(0.0, value.toDouble() / 100, 1.0));
    else if (key == "brightnessInner") runtime.setBackgroundBrightnessInner(qBound(0.0, value.toDouble() / 100, 1.0));
    else if (key == "layoutSquareScale") runtime.setLayoutSquareScale(value.toDouble() / 100);
    else if (key == "scaleMode") runtime.setBackgroundScaleMode(static_cast<PreviewBackgroundScaleMode>(qBound(0, value.toInt(), 3)));
    else if (key == "smoothBrightness") runtime.setSmoothBrightness(value.toBool());
    else if (key == "showTimestamp") runtime.setShowTimestamp(value.toBool());
    else if (key == "showDebugInfo") runtime.setShowDebugInfo(value.toBool());
    else if (key == "touchPadAuthoringShortcut") runtime.setTouchPadAuthoringEnabled(value.toBool());
    else if (key == "tapFlowSpeed") runtime.setTapFlowSpeed(value.toDouble());
    else if (key == "touchFlowSpeed") runtime.setTouchFlowSpeed(value.toDouble());
    else if (key == "forceLabeledJudgeLineWhenPaused") {
        forceLabeledJudgeLineWhenPaused_ = value.toBool(); applyEffectiveOutline();
    }
    else if (key == "judgeEffectSlide" || key == "judgeEffectTap" || key == "judgeEffectBreak" || key == "judgeEffectTouch") {
        auto options = preview_.muriRenderOptions();
        if (key == "judgeEffectSlide") options.showChartReviewSlideJudgeOverlay = value.toBool();
        else if (key == "judgeEffectTap") options.showChartReviewTapJudgeOverlay = value.toBool();
        else if (key == "judgeEffectBreak") options.showChartReviewBreakJudgeOverlay = value.toBool();
        else options.showChartReviewTouchJudgeOverlay = value.toBool();
        preview_.setJudgeOverlayOptions(options);
    }
    else if (key == "slideEarlierOnTop") appearance_.setSlideEarlierSecondAndTextOnTop(value.toBool());
    else if (key == "centerDisplay") appearance_.setCenterDisplayMode(static_cast<preview_gameplay::CenterDisplayMode>(value.toInt()));
    else if (key == "tapJudgeTextDistance") appearance_.setTapJudgeTextDistance(static_cast<PreviewTapJudgeTextDistance>(value.toInt()));
    else return; // Unknown keys cannot pretend to change the live renderer.
    if (key == "scaleMode" || key == "layoutSquareScale") preview_.refreshMediaLayout();
    emit notifications_.previewRenderSettingsChanged();
}
void MobileExportComposition::refreshSurfaces() {
    auto& runtime = preview_.sceneRuntime();
    runtime.setSkinDirectory(QDir(resolveSkinRootDir()).filePath(appearance_.skinDirectoryName()));
    runtime.setJudgeEffectStyle(appearance_.judgeEffectStyle()); applyEffectiveOutline();
    runtime.setSlideEarlierSecondAndTextOnTop(appearance_.slideEarlierSecondAndTextOnTop());
    runtime.setTapJudgeTextDistance(appearance_.tapJudgeTextDistance()); runtime.setCenterDisplayMode(appearance_.centerDisplayMode());
    emit notifications_.previewSkinDirectoryChanged(); emit notifications_.previewRenderSettingsChanged();
}
void MobileExportComposition::applyEffectiveOutline() {
    const bool pausedJudgeArea = forceLabeledJudgeLineWhenPaused_ && !preview_.playing() && !session_.pageSessionActive();
    const auto variant = pausedJudgeArea ? PreviewOutlineVariant::JudgeAreaLabeled : appearance_.outlineVariant();
    const auto path = customOutline_.isEmpty() ? QString() : assets::customOutlinePathForFileName(customOutline_);
    const auto mode = pausedJudgeArea && !path.isEmpty()
        ? preview::runtime::PreviewOutlineImageMode::PausedJudgeAreaComposite : preview::runtime::PreviewOutlineImageMode::Direct;
    preview_.sceneRuntime().setOutlineSelection(variant, path, mode);
    preview_.setPausedJudgeAreaView(pausedJudgeArea);
}
void MobileExportComposition::applyAudioSettings(const PreviewAudioSettings& value) {
    audioSettings_ = value; audioSettings_.normalize(); preview_.applyAudioSettings(audioSettings_);
}
void MobileExportComposition::saveAudioSettingsAsSoftwareDefault() {
    QSettings settings; settings.setValue("mobile/audio", QJsonDocument(audioSettings_.toJson()).toJson(QJsonDocument::Compact));
}
void MobileExportComposition::restoreAudioSettingsFromSoftwareDefault() {
    QSettings settings; const auto json = QJsonDocument::fromJson(settings.value("mobile/audio").toByteArray());
    applyAudioSettings(json.isObject() ? PreviewAudioSettings::fromJson(json.object()) : PreviewAudioSettings{});
}
}
