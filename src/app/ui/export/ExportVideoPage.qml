pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

Rectangle {
    id: root

    required property var pages
    required property var previewSession
    required property var previewSettings
    readonly property var session: pages && pages.exportSession ? pages.exportSession : null
    readonly property alias cornerSourceItem: heading
    readonly property bool introSettingsEnabled: !!root.session
                                                  && root.session.introEnabled
                                                  && (root.session.activeTab === "batch"
                                                      || root.session.fullRangeExport)

    // The batch-only inputs used to sit above the settings tab row and eat the
    // height the tab body needed. They are a settings tab of their own now, so
    // this list simply gains one entry while batch mode is active.
    readonly property var settingsTabs: {
        const tabs = []
        if (root.session && root.session.activeTab === "batch")
            tabs.push({ id: "batch", label: qsTrId("qml.batch") })
        tabs.push({ id: "output", label: qsTrId("video_export.output") })
        tabs.push({ id: "video", label: qsTrId("dialog.render_settings.visual_group") })
        tabs.push({ id: "gameplay", label: qsTrId("dialog.render_settings.gameplay_group") })
        tabs.push({ id: "skin", label: qsTrId("dialog.render_settings.skin_group") })
        tabs.push({ id: "intro", label: qsTrId("video_export.intro") })
        return tabs
    }

    function fontIndexForPath(options, path) {
        if (!options)
            return 0
        for (let index = 0; index < options.length; ++index) {
            if (options[index].path === path)
                return index
        }
        return 0
    }

    function fontFamilyForPath(options, path) {
        const index = fontIndexForPath(options, path)
        return options && options.length > index ? options[index].family : ""
    }

    FontLoader {
        id: defaultDisplayFont
        source: "qrc:/intro/assets/fonts/ResourceHanRoundedCN-Heavy.ttf"
    }
    FontLoader {
        id: defaultBodyFont
        source: "qrc:/intro/assets/fonts/ResourceHanRoundedCN-Bold.ttf"
    }

    readonly property int tabInset: 6
    readonly property int formInset: Theme.dialogMargin + Theme.dialogPadding

    color: Theme.surfaceColor(Theme.colors.background.panel)
    clip: true

    Component {
        id: appearanceForm
        PreviewAppearancePages {
            width: appearanceLoader.width
            previewSettings: root.previewSettings
            currentIndex: root.session && root.session.settingsTab === "gameplay"
                       ? 1
                       : root.session && root.session.settingsTab === "skin" ? 2 : 0
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.bottomMargin: Theme.dialogPadding
        spacing: 0

        PanelHeader {
            id: heading
            Layout.fillWidth: true
            title: qsTrId("export_page.export_video")
            sidebarTitle: true
            showMore: false

            AppChoiceButton {
                text: qsTrId("action.batch_export")
                enabled: root.session !== null
                checked: root.session && root.session.activeTab === "batch"
                onClicked: {
                    if (root.session)
                        root.session.activeTab = checked ? "batch" : "export"
                    checked = Qt.binding(() => root.session && root.session.activeTab === "batch")
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: Theme.workspaceSectionTopMargin
            spacing: 2

            Text {
                Layout.fillWidth: true
                Layout.leftMargin: root.tabInset
                Layout.rightMargin: root.tabInset
                visible: !!(root.session && root.session.unavailableReason)
                text: root.session ? root.session.unavailableReason : ""
                color: Theme.colors.text.secondary
                font.family: Theme.uiFont
                font.pixelSize: Theme.uiFontSize
                wrapMode: Text.WordWrap
            }

            AppTabBar {
                Layout.fillWidth: true
                Layout.leftMargin: root.tabInset
                visible: root.session && !root.session.unavailableReason
                tabs: root.settingsTabs
                selectedId: root.session ? root.session.settingsTab : "output"
                buttonObjectNamePrefix: "exportSettingsTab_"
                onTabSelected: function(tabId) { if (root.session) root.session.settingsTab = tabId }
            }

            ButtonGroup { id: difficultyGroup }

            Flow {
                id: difficultyRow
                Layout.fillWidth: true
                Layout.leftMargin: root.tabInset
                Layout.preferredHeight: Theme.controlMinHeight
                spacing: 4
                Repeater {
                    model: root.session ? root.session.difficulties : []
                    delegate: AppChoiceButton {
                        required property var modelData
                        ButtonGroup.group: difficultyGroup
                        text: modelData.name
                        difficultyId: modelData.id
                        checked: root.session && root.session.selectedDifficultyId === modelData.id
                        onClicked: if (root.session) root.session.selectDifficulty(modelData.id)
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: root.tabInset
                Layout.rightMargin: root.tabInset
                Layout.topMargin: Theme.panelPadding
                Layout.preferredHeight: 1
                color: Theme.colors.border.normal
            }

            Flickable {
                id: settingsFlickable
                objectName: "exportSettingsFlickable"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.topMargin: Theme.panelPadding
                Layout.minimumHeight: 200
                visible: root.session && !(root.session && root.session.unavailableReason)
                clip: true
                contentWidth: width
                contentHeight: settingsBody.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: AppScrollBar {
                    policy: settingsFlickable.contentHeight > settingsFlickable.height
                        ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
                }

                AppTabPages {
                    id: settingsBody
                    x: root.formInset
                    width: settingsFlickable.width - 2 * root.formInset
                    currentIndex: root.session && root.session.settingsTab === "batch" ? 0
                                  : root.session && root.session.settingsTab === "output" ? 1
                                  : root.session && root.session.settingsTab === "intro" ? 3 : 2

                    // Batch (only reachable while batch export is the active mode)
                    ColumnLayout {
                        objectName: "exportBatchSettingsPage"
                        Layout.fillHeight: false
                        Layout.alignment: Qt.AlignTop
                        spacing: 10
                        Layout.fillWidth: true

                        SettingsSection {
                            title: qsTrId("dialog.batch_export.difficulty")
                            first: true

                            Flow {
                                Layout.fillWidth: true
                                spacing: 8
                                Repeater {
                                    model: root.session ? root.session.batchDifficultyChecks : []
                                    delegate: AppSwitch {
                                        required property var modelData
                                        text: modelData.name
                                        checked: modelData.checked
                                        onToggled: if (root.session) root.session.setBatchDifficultyChecked(modelData.id, checked)
                                    }
                                }
                            }
                        }

                        SettingsSection {
                            title: qsTrId("qml.output_folder")

                            RowLayout {
                                Layout.fillWidth: true
                                AppTextField {
                                    objectName: "batchOutputDirectoryField"
                                    Layout.fillWidth: true
                                    text: root.session ? root.session.batchOutputDirectory : ""
                                    onEditingFinished: if (root.session) root.session.batchOutputDirectory = text
                                }
                                AppButton {
                                    text: qsTrId("action.browse")
                                    onClicked: if (root.session) root.session.browseBatchOutputDirectory()
                                }
                            }
                        }

                        SettingsSection {
                            title: qsTrId("dialog.batch_export.chart_folders")

                            // SettingsSection's title row has no slot for trailing
                            // controls, so Add/Clear sit on their own right-aligned
                            // row directly above the list instead of riding the
                            // caption itself.
                            RowLayout {
                                Layout.fillWidth: true
                                Item { Layout.fillWidth: true }
                                AppButton {
                                    text: qsTrId("qml.add")
                                    onClicked: if (root.session) root.session.addChartDirectories()
                                }
                                AppButton {
                                    text: qsTrId("dialog.batch_export.clear")
                                    onClicked: if (root.session) root.session.clearChartDirectories()
                                }
                            }

                            // This was a ListView capped at 112px because it shared
                            // a column with the settings tabs and would otherwise
                            // starve them. On its own tab there is nothing left to
                            // starve, so the rows lay out at full height and the
                            // tab's own Flickable scrolls them — which also keeps a
                            // second scrollable from nesting inside that one.
                            Rectangle {
                                id: chartDirectoryGroove
                                objectName: "batchChartDirectoryList"
                                Layout.fillWidth: true
                                color: Theme.overlayColor(Theme.colors.background.surface)
                                radius: Theme.controlRadius
                                border.width: 1
                                border.color: Theme.colors.border.normal

                                readonly property var directories: root.session ? root.session.chartDirectories : []
                                // An empty groove that collapsed to zero height would
                                // read as the section vanishing, so it holds a floor
                                // and stays visible as an empty list.
                                implicitHeight: chartDirectoryGroove.directories.length > 0
                                                 ? directoryColumn.implicitHeight
                                                 : Theme.controlMinHeight * 3

                                ColumnLayout {
                                    id: directoryColumn
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    spacing: 0

                                    Repeater {
                                        model: chartDirectoryGroove.directories
                                        delegate: ColumnLayout {
                                            id: chartDirectoryDelegate
                                            required property int index
                                            required property string modelData
                                            Layout.fillWidth: true
                                            spacing: 0

                                            Rectangle {
                                                // Inset to match the row's own padding rather
                                                // than running edge-to-edge, so it reads as a
                                                // row separator and not a second groove border.
                                                visible: chartDirectoryDelegate.index > 0
                                                Layout.fillWidth: true
                                                Layout.leftMargin: Theme.rowPaddingX
                                                Layout.rightMargin: Theme.rowPaddingX
                                                height: 1
                                                color: Theme.colors.border.normal
                                            }

                                            ChromeRow {
                                                id: chartDirectoryRow
                                                Layout.fillWidth: true
                                                implicitHeight: Theme.controlMinHeight

                                                contentItem: RowLayout {
                                                    spacing: 8

                                                    Text {
                                                        id: chartDirectoryPath
                                                        Layout.fillWidth: true
                                                        text: chartDirectoryDelegate.modelData
                                                        elide: Text.ElideMiddle
                                                        color: Theme.colors.text.active
                                                        font.family: Theme.uiFont
                                                        font.pixelSize: Theme.uiFontSize
                                                        verticalAlignment: Text.AlignVCenter

                                                        HoverHandler { id: chartDirectoryPathHover }
                                                        Tooltip {
                                                            requestedVisible: chartDirectoryPathHover.hovered
                                                            text: chartDirectoryDelegate.modelData
                                                        }
                                                    }

                                                    IconButton {
                                                        compact: true
                                                        iconSource: Qt.resolvedUrl("icons/remove.svg")
                                                        tooltip: qsTrId("qml.remove")
                                                        // Left to IconButton's own resting/hover
                                                        // glyphColor: `active` would be wrong here
                                                        // because it also drives HoverChrome's
                                                        // selected background, so brightening the
                                                        // glyph on row hover would paint the
                                                        // button as if it were toggled on.
                                                        onClicked: if (root.session)
                                                            root.session.removeChartDirectory(chartDirectoryDelegate.index)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Output (single-export range lives on this tab)
                    ColumnLayout {
                        Layout.fillHeight: false
                        Layout.alignment: Qt.AlignTop
                        spacing: Theme.panelPadding
                        Layout.fillWidth: true

                        ColumnLayout {
                            visible: root.session && root.session.activeTab === "export"
                            spacing: Theme.chromePadding
                            Layout.fillWidth: true

                            Text {
                                text: qsTrId("video_export.filename")
                                color: Theme.colors.text.secondary
                                font.family: Theme.uiFont
                                font.pixelSize: Theme.uiFontSize
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                AppTextField {
                                    Layout.fillWidth: true
                                    text: root.session ? root.session.outputPath : ""
                                    onEditingFinished: if (root.session) root.session.outputPath = text
                                }
                                AppButton {
                                    text: qsTrId("action.browse")
                                    onClicked: if (root.session) root.session.browseOutputPath()
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 0
                                spacing: Theme.chromePadding
                                Text {
                                    text: qsTrId("dialog.video_export.resolution")
                                    color: Theme.colors.text.secondary
                                    font.family: Theme.uiFont
                                    font.pixelSize: Theme.uiFontSize
                                }
                                AppComboBox {
                                    Layout.fillWidth: true
                                    model: root.session ? root.session.resolutionOptions : []
                                    textRole: "label"
                                    currentIndex: root.session ? root.session.resolutionIndex : 0
                                    onActivated: if (root.session) root.session.resolutionIndex = currentIndex
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 0
                                spacing: Theme.chromePadding
                                Text {
                                    text: qsTrId("dialog.video_export.fps")
                                    color: Theme.colors.text.secondary
                                    font.family: Theme.uiFont
                                    font.pixelSize: Theme.uiFontSize
                                }
                                AppComboBox {
                                    Layout.fillWidth: true
                                    model: root.session ? root.session.fpsOptions : []
                                    currentIndex: {
                                        if (!root.session) return 1
                                        const opts = root.session.fpsOptions
                                        for (let i = 0; i < opts.length; ++i)
                                            if (opts[i] === root.session.fps) return i
                                        return 1
                                    }
                                    displayText: root.session ? (root.session.fps + " FPS") : ""
                                    onActivated: if (root.session) root.session.fps = model[currentIndex]
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 0
                                spacing: Theme.chromePadding
                                Text {
                                    text: qsTrId("dialog.video_export.audio_bitrate")
                                    color: Theme.colors.text.secondary
                                    font.family: Theme.uiFont
                                    font.pixelSize: Theme.uiFontSize
                                }
                                AppComboBox {
                                    Layout.fillWidth: true
                                    model: root.session ? root.session.audioBitrateOptions : []
                                    currentIndex: {
                                        if (!root.session) return 2
                                        const opts = root.session.audioBitrateOptions
                                        for (let i = 0; i < opts.length; ++i)
                                            if (opts[i] === root.session.audioBitrateKbps) return i
                                        return 2
                                    }
                                    displayText: root.session ? (root.session.audioBitrateKbps + " kbps") : ""
                                    onActivated: if (root.session) root.session.audioBitrateKbps = model[currentIndex]
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 0
                                spacing: Theme.chromePadding
                                Text {
                                    text: qsTrId("dialog.video_export.preset")
                                    color: Theme.colors.text.secondary
                                    font.family: Theme.uiFont
                                    font.pixelSize: Theme.uiFontSize
                                }
                                AppComboBox {
                                    Layout.fillWidth: true
                                    model: root.session ? root.session.presetOptions : []
                                    currentIndex: root.session ? root.session.presetIndex : 1
                                    onActivated: if (root.session) root.session.presetIndex = currentIndex
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 0
                                spacing: Theme.chromePadding
                                Text {
                                    text: qsTrId("dialog.video_export.size_preset")
                                    color: Theme.colors.text.secondary
                                    font.family: Theme.uiFont
                                    font.pixelSize: Theme.uiFontSize
                                }
                                AppComboBox {
                                    Layout.fillWidth: true
                                    model: root.session ? root.session.sizePresetOptions : []
                                    currentIndex: root.session ? root.session.sizePresetIndex : 0
                                    onActivated: if (root.session) root.session.sizePresetIndex = currentIndex
                                }
                            }
                        }

                        ColumnLayout {
                            visible: root.session && root.session.activeTab === "export"
                            spacing: Theme.panelPadding
                            Layout.fillWidth: true

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Theme.colors.border.normal
                        }

                        ColumnLayout {
                            spacing: Theme.chromePadding
                            Layout.fillWidth: true

                            Text {
                                text: qsTrId("video_export.export_range")
                                color: Theme.colors.text.secondary
                                font.family: Theme.uiFont
                                font.pixelSize: Theme.uiFontSize
                            }

                            ExportRangeSelector {
                                id: exportRangeSelector
                                objectName: "exportRangeSelector"
                                Layout.fillWidth: true
                                exportSession: root.session
                                previewSession: root.previewSession
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.chromePadding

                            Text {
                                text: qsTrId("dialog.video_export.range.start")
                                color: Theme.colors.text.secondary
                                font.family: Theme.uiFont
                                font.pixelSize: Theme.uiFontSize
                                Layout.alignment: Qt.AlignVCenter
                            }
                            AppTextField {
                                id: exportRangeStartField

                                objectName: "exportRangeStartField"
                                Layout.preferredWidth: 100
                                Layout.alignment: Qt.AlignVCenter
                                text: root.session ? root.session.exportStartSeconds.toFixed(3) : "0"
                                onEditingFinished: {
                                    if (!root.session)
                                        return
                                    text = root.session.setExportStartText(text)
                                    exportRangeSelector.seekToSelectedStart()
                                }
                            }
                            Item { Layout.fillWidth: true }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.chromePadding

                            Text {
                                text: qsTrId("dialog.video_export.range.end")
                                color: Theme.colors.text.secondary
                                font.family: Theme.uiFont
                                font.pixelSize: Theme.uiFontSize
                                Layout.alignment: Qt.AlignVCenter
                            }
                            AppTextField {
                                id: exportRangeEndField

                                objectName: "exportRangeEndField"
                                Layout.preferredWidth: 100
                                Layout.alignment: Qt.AlignVCenter
                                text: root.session ? root.session.exportEndSeconds.toFixed(3) : "0"
                                onEditingFinished: if (root.session) text = root.session.setExportEndText(text)
                            }
                            Item { Layout.fillWidth: true }
                        }
                        Text {
                            text: root.session
                                  ? qsTrId("qml.total_duration_1_s").arg(
                                        (root.session.exportEndSeconds - root.session.exportStartSeconds).toFixed(3))
                                  : ""
                            color: Theme.colors.text.secondary
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Theme.colors.border.normal
                        }
                        }

                        AppSwitch {
                            text: qsTrId("dialog.video_export.option.show_object_stats")
                            checked: root.session ? root.session.showObjectStatsHud : false
                            onToggled: if (root.session) root.session.showObjectStatsHud = checked
                        }
                        AppSwitch {
                            text: qsTrId("qml.show_chart_information")
                            checked: root.session ? root.session.showChartInfoHud : false
                            onToggled: if (root.session) root.session.showChartInfoHud = checked
                        }
                        AppSwitch {
                            text: qsTrId("qml.fix_hud_text_layout")
                            checked: root.session ? root.session.fixHudTextLayout : false
                            onToggled: if (root.session) root.session.fixHudTextLayout = checked
                        }
                        AppSwitch {
                            text: qsTrId("qml.enable_clock_count")
                            checked: root.session ? root.session.clockCountEnabled : false
                            onToggled: if (root.session) root.session.clockCountEnabled = checked
                        }
                    }

                    Loader {
                        id: appearanceLoader
                        Layout.fillWidth: true
                        Layout.fillHeight: false
                        Layout.alignment: Qt.AlignTop
                        active: !!root.session && StackLayout.isCurrentItem
                        sourceComponent: appearanceForm
                    }

                    // Intro
                    ColumnLayout {
                        Layout.fillHeight: false
                        Layout.alignment: Qt.AlignTop
                        spacing: 10
                        Layout.fillWidth: true

                        // Kept outside every section: gating this on `introEnabled`
                        // itself would let it disable its own switch.
                        AppSwitch {
                            text: qsTrId("video_export.add_intro")
                            sectionTitle: true
                            checked: root.session ? root.session.introEnabled : false
                            enabled: root.session
                                     ? root.session.activeTab === "batch" || root.session.fullRangeExport
                                     : false
                            onToggled: if (root.session) root.session.introEnabled = checked
                        }

                        SettingsSection {
                            title: qsTrId("qml.visuals")
                            enabled: root.introSettingsEnabled

                            RowLayout {
                                Text {
                                    text: qsTrId("dialog.preferences.background_group")
                                    color: Theme.colors.text.secondary
                                    Layout.preferredWidth: 120
                                }
                                AppComboBox {
                                    Layout.fillWidth: true
                                    model: [qsTrId("cover.jacket"), qsTrId("qml.custom")]
                                    currentIndex: root.session ? root.session.introBackgroundModeIndex : 0
                                    onActivated: if (root.session) root.session.introBackgroundModeIndex = currentIndex
                                }
                            }
                            RowLayout {
                                visible: root.session && root.session.introBackgroundModeIndex === 1
                                AppTextField {
                                    Layout.fillWidth: true
                                    text: root.session ? root.session.introCustomBackgroundPath : ""
                                    onEditingFinished: if (root.session) root.session.introCustomBackgroundPath = text
                                }
                                AppButton {
                                    text: qsTrId("action.browse")
                                    onClicked: if (root.session) root.session.browseIntroBackground()
                                }
                            }
                            AppSwitch {
                                text: qsTrId("cover.blur_background")
                                checked: root.session ? root.session.introBlurBackground : true
                                onToggled: if (root.session) root.session.introBlurBackground = checked
                            }
                            RowLayout {
                                Text {
                                    text: qsTrId("cover.chart_type")
                                    color: Theme.colors.text.secondary
                                    Layout.preferredWidth: 120
                                }
                                AppComboBox {
                                    Layout.fillWidth: true
                                    model: [qsTrId("qml.automatic"), "DX", "SD"]
                                    currentIndex: root.session ? root.session.introModeIndex : 0
                                    onActivated: if (root.session) root.session.introModeIndex = currentIndex
                                }
                            }
                            AppSwitch {
                                text: qsTrId("cover.card_drop_shadow")
                                checked: root.session ? root.session.introCardShadow : false
                                onToggled: if (root.session) root.session.introCardShadow = checked
                            }
                            AppSwitch {
                                text: qsTrId("qml.render_level_as_text")
                                checked: root.session ? root.session.introLevelTextRender : false
                                onToggled: if (root.session) root.session.introLevelTextRender = checked
                            }
                        }

                        SettingsSection {
                            title: qsTrId("qml.difficulty_card_fonts")
                            enabled: root.introSettingsEnabled

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: qsTrId("card_font.title")
                                    color: Theme.colors.text.secondary
                                    Layout.preferredWidth: 120
                                }
                                AppComboBox {
                                    id: introDisplayFontCombo
                                    fontFamilyRole: "family"
                                    defaultFontFamily: defaultDisplayFont.name
                                    objectName: "introDisplayFontCombo"
                                    Layout.fillWidth: true
                                    model: root.session ? root.session.fontLibraryOptions : []
                                    textRole: "label"
                                    currentIndex: root.fontIndexForPath(
                                                      model, root.session ? root.session.introFontDisplayPath : "")
                                    Accessible.name: qsTrId("qml.intro_title_font")
                                    onActivated: if (root.session)
                                        root.session.introFontDisplayPath = model[currentIndex].path
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: qsTrId("card_font.body")
                                    color: Theme.colors.text.secondary
                                    Layout.preferredWidth: 120
                                }
                                AppComboBox {
                                    id: introBodyFontCombo
                                    fontFamilyRole: "family"
                                    defaultFontFamily: defaultBodyFont.name
                                    objectName: "introBodyFontCombo"
                                    Layout.fillWidth: true
                                    model: root.session ? root.session.fontLibraryOptions : []
                                    textRole: "label"
                                    currentIndex: root.fontIndexForPath(
                                                      model, root.session ? root.session.introFontBodyPath : "")
                                    Accessible.name: qsTrId("qml.intro_body_font")
                                    onActivated: if (root.session)
                                        root.session.introFontBodyPath = model[currentIndex].path
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: introFontPreviewColumn.implicitHeight + 20
                                radius: Theme.controlRadius
                                color: Theme.overlayColor(Theme.colors.background.surface)
                                Column {
                                    id: introFontPreviewColumn
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 3
                                    Text {
                                        id: introFontSample
                                        width: parent.width
                                        text: qsTrId("qml.title_font_preview")
                                        color: Theme.colors.text.primary
                                        font.family: root.fontFamilyForPath(
                                                         root.session ? root.session.fontLibraryOptions : [],
                                                         root.session ? root.session.introFontDisplayPath : "") || defaultDisplayFont.name
                                        font.pixelSize: Theme.uiFontSize
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        width: parent.width
                                        text: qsTrId("qml.body_font_preview")
                                        color: Theme.colors.text.secondary
                                        font.family: root.fontFamilyForPath(
                                                         root.session ? root.session.fontLibraryOptions : [],
                                                         root.session ? root.session.introFontBodyPath : "") || defaultBodyFont.name
                                        font.pixelSize: Theme.secondaryFontSize
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                AppButton {
                                    id: introFontImportButton
                                    objectName: "introFontImportButton"
                                    text: qsTrId("card_font.import")
                                    Accessible.name: qsTrId("qml.import_intro_difficulty_card_fonts")
                                    onClicked: if (root.session) root.session.importIntroFont()
                                }
                                AppButton {
                                    id: introFontResetButton
                                    objectName: "introFontResetButton"
                                    text: qsTrId("card_font.reset")
                                    Accessible.name: qsTrId("qml.reset_intro_difficulty_card_fonts")
                                    onClicked: if (root.session) root.session.resetIntroFonts()
                                }
                                Item { Layout.fillWidth: true }
                            }
                        }

                        SettingsSection {
                            title: qsTrId("qml.sound_effects")
                            enabled: root.introSettingsEnabled

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: root.session ? root.session.introSoundLabel : ""
                                    color: Theme.colors.text.secondary
                                    Layout.preferredWidth: 120
                                }
                                AppComboBox {
                                    id: introSoundCombo
                                    objectName: "introSoundCombo"
                                    Layout.fillWidth: true
                                    model: root.session ? root.session.introSoundOptions : []
                                    textRole: "label"
                                    currentIndex: root.session ? root.session.introSoundIndex : 0
                                    focusPolicy: Qt.StrongFocus
                                    Accessible.name: root.session ? root.session.introSoundLabel : ""
                                    onActivated: if (root.session) root.session.introSoundIndex = currentIndex
                                }
                                AppButton {
                                    id: introSoundImportButton
                                    objectName: "introSoundImportButton"
                                    text: root.session ? root.session.introSoundImportLabel : ""
                                    focusPolicy: Qt.StrongFocus
                                    Accessible.name: root.session
                                                     ? root.session.introSoundImportLabel + " "
                                                       + root.session.introSoundLabel
                                                     : ""
                                    onClicked: if (root.session) root.session.importIntroSound()
                                }
                            }
                            LabeledSlider {
                                id: introSoundVolumeSlider
                                objectName: "introSoundVolumeSlider"
                                label: root.session ? root.session.introSoundVolumeLabel : ""
                                from: 0
                                to: 200
                                stepSize: 1
                                suffix: "%"
                                value: root.session ? root.session.introSoundVolume * 100 : 100
                                focusPolicy: Qt.StrongFocus
                                onMoved: function(v) { if (root.session) root.session.introSoundVolume = v / 100 }
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.panelPadding
                Layout.leftMargin: root.formInset
                Layout.rightMargin: root.formInset
                Item { Layout.fillWidth: true }
                AppButton {
                    text: root.session && root.session.exportRunning ? qsTrId("video_export.cancel_export") : qsTrId("video_export.start_export")
                    emphasized: !(root.session && root.session.exportRunning)
                    onClicked: {
                        if (!root.session) return
                        if (root.session.exportRunning)
                            root.session.cancelExport()
                        else
                            root.session.startExport()
                    }
                }
            }
        }
    }

    Connections {
        target: root.session

        function onRangeChanged() {
            if (!exportRangeStartField.activeFocus)
                exportRangeStartField.text = root.session.exportStartSeconds.toFixed(3)
            if (!exportRangeEndField.activeFocus)
                exportRangeEndField.text = root.session.exportEndSeconds.toFixed(3)
        }
    }
}
