import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI
import "qrc:/preview/runtime/qml" as Preview

ApplicationWindow {
    id: window
    width: 1280
    height: 720
    visible: true
    title: "MiaCode Mobile"
    color: Theme.colors.background.surface
    property Item backdropSource: workspace
    property string activePage: "chart"
    onActivePageChanged: {
        if (activePage === "export") mobileExport.enter()
        else mobileExport.leave()
    }
    property bool showSidebar: true
    property bool showBottom: true
    property bool previewFullscreen: false
    property bool sourceEditorOverlayHeld: false
    // Fit the complete v2 workbench using the actual panes' minimum geometry.
    // Painting and touch hit testing share the same Item transform.
    readonly property real workbenchScale: Math.min(1,
        width / (activityBar.implicitWidth + 140 + Math.max(300, bottomPanel.minimumWidth) + previewPane.minimumWidth),
        height / (titleBar.implicitHeight + mainToolbar.implicitHeight + previewPane.stableMinimumHeight
            + statusBar.implicitHeight + (statusMessage.visible ? statusMessage.implicitHeight : 0)))

    // Popup.Item reparents its visual content to the window overlay. Keep that
    // layer in the same logical coordinate space as the fitted v2 workbench.
    Overlay.overlay.width: window.width / window.workbenchScale
    Overlay.overlay.height: window.height / window.workbenchScale
    Overlay.overlay.transform: Scale {
        xScale: window.workbenchScale
        yScale: window.workbenchScale
    }

    QtObject {
        id: mobilePreferences
        property bool darkTheme: true
        property string activeThemeToken: darkTheme ? "dark" : "light"
        property string uiFontFamily: Qt.application.font.family
        property font codeFont: Qt.font({ family: androidSession.editorFont || mobileCodeFontFamily, pixelSize: 15 })
        property int fontSize: 13
        property int editorBlockSpacing: 3
        property bool editorScrollPastEnd: true
        property bool editorSelectionBeatDisplay: true
        property bool previewCanvasFreeAspect: false
        property bool previewHidePv: false
        signal editorSettingsChanged()
    }
    QtObject {
        id: mobilePlatform
        property bool embeddedMenuInTitleBar: true
        property bool nativeMenuBar: false
        property bool captionButtons: false
    }
    QtObject { id: mobilePet; property bool visible: false }
    MainMenuCommands {
        id: menuCommands
        canUndo: v2EditorController.canUndo
        canRedo: v2EditorController.canRedo
        canCut: editorPane.canCut
        canCopy: editorPane.canCopy
        canPaste: editorPane.canPaste
        onNewDocumentRequested: window.confirmReplace("new")
        onOpenRequested: openProjectDialog.open()
        onSaveRequested: androidSession.save()
        onSaveWholeDocumentRequested: androidSession.save()
        onSaveAsRequested: androidSession.save(true)
        onUndoRequested: editorPane.undo()
        onRedoRequested: editorPane.redo()
        onCutRequested: editorPane.cut()
        onCopyRequested: editorPane.copy()
        onPasteRequested: editorPane.paste()
        onSelectAllRequested: editorPane.selectAll()
        onFindRequested: editorPane.openFindReplace()
        onSelectCurrentLineRequested: editorPane.selectCurrentLine()
        onChartTransformRequested: opId => editorPane.applyChartTransform(opId)
        onNormalizeChartRequested: normalizeDialog.open()
        onMetadataRequested: pages.activateMetadataPage()
        onPreferencesRequested: settings.open()
        onAudioSettingsRequested: settings.open()
        onPreviewSettingsRequested: settings.open()
        onPreviewRateStepRequested: direction => mobilePreview.rate = Math.max(0.25, Math.min(2, mobilePreview.rate + direction * 0.25))
        onCloseDocumentRequested: window.confirmReplace("new")
        onExitRequested: { androidSession.flushRecovery(); Qt.quit() }
        onAboutRequested: aboutDialog.open()
    }
    NormalizeOptionsDialog {
        id: normalizeDialog
        documentSession: androidSession
        selectionDescription: editorPane.normalizationSelectionDescription()
        onAccepted: Qt.callLater(() => editorPane.applyNormalization({
            reduceTo384Grid: reduceTo384Grid, sectionMeasureCount: sectionMeasureCount, syntax: syntax
        }))
    }
    ChoiceDialog {
        id: aboutDialog
        title: "MiaCode Mobile"
        message: "MiaCode v2 Android 迁移开发版"
        choices: [{ id: "close", label: "关闭" }]
        dismissChoiceId: "close"
    }
    QtObject {
        id: mobileRangePreview
        readonly property bool available: window.activePage === "export" && session.activeTab === "export" && session.settingsTab === "output"
        onAvailableChanged: if (!available) active = false
        property bool active: false
        onActiveChanged: mobilePreview.setPlaybackRangeEnabled(active && available, session.exportStartSeconds, session.exportEndSeconds)
        property bool armed: false
        property real startSeconds: 0
        readonly property var session: mobileExport.session
    }
    ViewState {
        id: state
        onDifficultyEditorActivationRequested: id => { window.activePage = "chart"; androidSession.selectDifficulty(id) }
    }
    QtObject {
        id: commands
        function addDifficulty(id) { androidSession.addDifficulty(id); return androidSession.currentDifficultyId === id }
        function removeDifficulty(id) { androidSession.removeDifficulty(id) }
        function applyDesignerSlots(slots, unified, name) { androidSession.applyDesignerSlots(slots, unified, name) }
        function newDocument() { window.confirmReplace("new") }
    }
    QtObject {
        id: pages
        readonly property string activePageId: window.activePage === "export" ? "export" : ""
        readonly property var exportSession: mobileExport.session
        function activateMetadataPage() { window.activePage = "chart"; state.openMetadataEditor(); return true }
        function openVideoExportPage() { window.activePage = "export" }
        function openCoverExport() { statusMessage.text = "封面导出尚未可用" }
        function packAsZip() { statusMessage.text = "工程打包尚未可用" }
        function openLatencyPage() { statusMessage.text = "延迟校准尚在移植中" }
    }
    Connections {
        target: mobileExport.session
        function onRangeChanged() {
            if (mobileRangePreview.active) mobilePreview.setPlaybackRangeEnabled(true, target.exportStartSeconds, target.exportEndSeconds)
        }
    }
    Component.onCompleted: {
        Theme.preferences = mobilePreferences
        state.resetEditorTabs(androidSession.currentDifficultyId)
        if (androidSession.recoveryAvailable) recovery.open()
    }
    Connections {
        target: androidSession
        function onDocumentReplaced() { state.resetEditorTabs(androidSession.currentDifficultyId) }
        function onDocumentStateChanged() { state.syncDifficultyEditors(androidSession.difficulties, androidSession.currentDifficultyId) }
        function onDifficultyCloseRequested(id) { state.closeEditor(state.difficultyEditorKey(id)) }
        function onBookmarkNavigationRequested(id, line) {
            editorSync.requestNavigation(id, androidSession.documentRevision,
                androidSession.chartPosition(line, 1), androidSession.chartPosition(line, 1), true, true)
        }
    }

    Item {
        id: workbench
        width: window.width / window.workbenchScale
        height: window.height / window.workbenchScale
        scale: window.workbenchScale
        transformOrigin: Item.TopLeft
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        WindowTitleBar {
            id: titleBar
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            hostWindow: window
            menuCommands: menuCommands
            shortcuts: mobileShortcuts
            documentSession: androidSession
            platform: mobilePlatform
            pet: mobilePet
            documentTitle: androidSession.title || "未命名谱面"
            saveEnabled: !androidSession.busy
            toolCommandsEnabled: false
        }
        MainToolBar {
            id: mainToolbar
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            hostWindow: window
            sidebarActive: window.showSidebar
            bottomActive: window.showBottom
            canUndo: v2EditorController.canUndo
            canRedo: v2EditorController.canRedo
            saveEnabled: !androidSession.busy
            onOpenRequested: openProjectDialog.open()
            onSaveRequested: androidSession.save()
            onUndoRequested: editorPane.undo()
            onRedoRequested: editorPane.redo()
            onToggleSidebarRequested: window.showSidebar = !window.showSidebar
            onToggleBottomRequested: window.showBottom = !window.showBottom
            onAudioSettingsRequested: settings.open()
            onPreviewSettingsRequested: settings.open()
        }
        RowLayout {
            id: workspace
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            ActivityBar {
                id: activityBar
                Layout.fillHeight: true
                Layout.preferredWidth: implicitWidth
                activeView: window.activePage === "export" ? "export" : "chart"
                normalizationEnabled: false
                onViewRequested: view => window.activePage = view
                onToolRequested: tool => settings.open()
                onSettingsRequested: settings.open()
            }
            SplitView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                orientation: Qt.Horizontal
                handle: SplitHandle {}
                Rectangle {
                    visible: window.showSidebar
                    SplitView.preferredWidth: Math.max(150, workspace.width * 0.17)
                    SplitView.minimumWidth: 140
                    SplitView.maximumWidth: workspace.width * 0.3
                    color: Theme.colors.background.panel
                    Column {
                        width: parent.width
                        visible: window.activePage === "chart"
                        PanelHeader { width: parent.width; title: "谱面"; sidebarTitle: true }
                        NavRow { width: parent.width; text: "谱面信息"; onClicked: pages.activateMetadataPage() }
                        DifficultyList { width: parent.width; viewState: state; documentSession: androidSession; commands: commands }
                    }
                    ExportSidebarPage { anchors.fill: parent; visible: window.activePage === "export"; pages: pages; documentAvailable: androidSession.hasDocument }
                }
                SplitView {
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 300
                    orientation: Qt.Vertical
                    handle: SplitHandle {}
                    Item {
                        SplitView.fillHeight: true
                        SplitView.minimumHeight: 130
                        EditorPane {
                            id: editorPane
                            anchors.fill: parent
                            visible: window.activePage === "chart"
                            viewState: state
                            documentSession: androidSession
                            commands: commands
                            editorController: v2EditorController
                            editorSync: editorSync
                            preferences: mobilePreferences
                            latency: null
                            pages: pages
                            onOpenRequested: openProjectDialog.open()
                        }
                        ExportVideoPage {
                            anchors.fill: parent
                            visible: window.activePage === "export"
                            pages: pages
                            previewSession: mobilePreview
                            previewSettings: mobileExport.settings
                        }
                    }
                    BottomPanel {
                        id: bottomPanel
                        visible: window.showBottom && window.activePage === "chart"
                        SplitView.preferredHeight: workspace.height * 0.30
                        SplitView.minimumHeight: minimumHeight
                        documentSession: androidSession
                        analysisSession: mobileAnalysis
                        preferences: mobilePreferences
                        timelineSession: mobileTimeline
                        previewSession: mobilePreview
                        onAnalysisRowActivated: (difficultyId, revision, line, column, endColumn, second) => {
                            if (!mobileAnalysis.completeRowActivation(difficultyId, revision, line, column, endColumn, second)) return
                            const start = androidSession.chartPosition(line, column)
                            const end = androidSession.chartPosition(line, endColumn)
                            editorSync.requestNavigation(difficultyId, revision, start, end, true, true)
                            if (second >= 0) mobilePreview.positionSeconds = second
                        }
                    }
                }
                PreviewPane {
                    id: previewPane
                    objectName: "v2PreviewPane"
                    minimumStageSize: minimumWidth
                    SplitView.preferredWidth: workspace.width * 0.32
                    SplitView.minimumWidth: 200
                    previewSession: mobilePreview
                    exportPageActive: window.activePage === "export"
                    preferences: mobilePreferences
                    rangePreviewState: mobileRangePreview
                    surfaceActive: !window.previewFullscreen
                    onFullscreenRequested: window.previewFullscreen = true
                }
            }
        }
        Label { id: statusMessage; Layout.fillWidth: true; visible: text.length > 0; text: androidSession.status; color: Theme.colors.text.secondary; font.pixelSize: 11; maximumLineCount: 1; elide: Text.ElideRight }
        StatusBar { id: statusBar; Layout.fillWidth: true; documentName: androidSession.currentFileName; cursorLine: state.editorCursorLine; cursorColumn: state.editorCursorColumn; difficultyActive: true; selectionBeatText: editorPane.selectionBeatStatusText }
    }
    }
    // Same stage / transport composition as v2 MainSplitView fullscreen.
    Rectangle {
        anchors.fill: parent
        visible: window.previewFullscreen
        color: Theme.surfaceColor(Theme.colors.background.panel)
        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: fullscreenTransport.top
            Loader {
                anchors.centerIn: parent
                width: Math.min(parent.width, parent.height * mobilePreview.canvasAspectRatio)
                height: width / mobilePreview.canvasAspectRatio
                active: window.previewFullscreen && width >= 64 && height >= 64
                sourceComponent: Preview.PreviewSurface {
                    runtime: mobilePreview.runtime
                    mediaHost: mobilePreview.mediaHost
                    logger: mobilePreview
                    surfaceRole: "fullscreen"
                    backgroundColor: "transparent"
                    hudTextColor: Theme.colors.previewHud.text
                    hudShadowColor: Theme.colors.previewHud.shadow
                }
            }
            PreviewRateToast { anchors.fill: parent; previewSession: mobilePreview }
            IconButton {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 12
                iconSource: "qrc:/icons/fullscreen.svg"
                tooltip: qsTrId("qml.exit_fullscreen_preview")
                onClicked: window.previewFullscreen = false
            }
        }
        PreviewTransport {
            id: fullscreenTransport
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            previewSession: mobilePreview
            preferences: mobilePreferences
            rangePreviewState: mobileRangePreview
            showCanvasMenuButton: false
        }
    }
    Shortcut { sequence: "Escape"; enabled: window.previewFullscreen; onActivated: window.previewFullscreen = false }
    AppMenu {
        id: fileMenu
        AppMenuItem { text: "新建"; onTriggered: confirmReplace("new") }
        AppMenuItem { text: "打开 maidata.txt"; onTriggered: confirmReplace("open") }
        AppMenuItem { text: "打开工程文件夹"; onTriggered: confirmReplace("folder") }
        AppMenuItem { text: "另存为"; onTriggered: androidSession.save(true) }
        AppMenuItem { text: "分享谱面"; onTriggered: androidSession.share() }
    }
    property string pendingReplace: ""
    ChoiceDialog {
        id: openProjectDialog
        title: "打开谱面"
        message: "工程文件夹可同时载入音频、背景与视频素材。"
        choices: [{ id: "folder", label: "工程文件夹" }, { id: "open", label: "maidata.txt" }, { id: "cancel", label: "取消" }]
        dismissChoiceId: "cancel"
        onChosen: id => { if (id !== "cancel") window.confirmReplace(id) }
    }
    function replaceProject() {
        if (pendingReplace === "new") androidSession.newProject()
        else if (pendingReplace === "folder") androidSession.openProjectFolder()
        else androidSession.openProject()
    }
    function confirmReplace(action) {
        pendingReplace = action
        if (androidSession.dirty) replaceDialog.open()
        else replaceProject()
    }
    ChoiceDialog {
        id: replaceDialog
        title: "切换工程"
        message: "当前谱面有未保存内容，继续切换？"
        choices: [{id:"cancel",label:"取消"},{id:"continue",label:"继续"}]
        dismissChoiceId: "cancel"
        onChosen: id => { if (id === "continue") { androidSession.flushRecovery(); window.replaceProject() } }
    }
    ChoiceDialog {
        id: recovery
        title: "恢复工程"
        message: "发现上次会话，是否恢复？"
        choices: [{id:"recover",label:"恢复"},{id:"new",label:"新建"}]
        dismissChoiceId: "recover"
        onChosen: id => { if (id === "recover") androidSession.recover(); else androidSession.newProject() }
    }
    AppDialog {
        id: settings
        title: "设置与素材"
        preferredWidth: Math.min(window.width - 48, 600)
        preferredHeight: Math.min(window.height - 48, 420)
        body: ScrollView {
            Column {
                spacing: 8
                AppSwitch { text: "深色主题"; checked: mobilePreferences.darkTheme; onToggled: mobilePreferences.darkTheme = checked }
                AppSwitch { text: "允许在后台导出"; checked: androidSession.backgroundExportAllowed; onToggled: androidSession.backgroundExportAllowed = checked }
                Row { spacing: 8; AppButton { text: "导入音频"; onClicked: androidSession.importAsset("audio") } AppButton { text: "导入图片"; onClicked: androidSession.importAsset("image") } }
                Row { spacing: 8; AppButton { text: "导入视频"; onClicked: androidSession.importAsset("video") } AppButton { text: "导入字体"; onClicked: androidSession.importAsset("font") } }
                Label { text: "开发验证"; color: Theme.colors.text.secondary }
                Row { spacing: 8; AppButton { text: "运行编码探针"; enabled: !androidSession.busy; onClicked: androidSession.runMediaProbe() } AppButton { text: "保存探针结果"; enabled: !androidSession.busy; onClicked: androidSession.exportProbeResults() } }
            }
        }
        footer: DialogFooter { acceptText: "关闭"; onAccepted: settings.close() }
    }
    UiRequestHost { requests: mobileExport.requests; externalFileDialogs: Qt.platform.os === "android" }
    JobProgressOverlay { progress: mobileExport.progress }
}
