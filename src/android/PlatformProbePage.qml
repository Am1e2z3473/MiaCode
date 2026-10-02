import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 440
    height: 820
    visible: true
    title: "MiaCode Android"
    color: "#17191e"
    readonly property bool wide: contentItem.width >= 840
    property int panel: 0
    property string requestedAction: ""
    palette.windowText: "#e8e8e8"
    palette.window: "#252b34"
    palette.text: "#e8e8e8"
    palette.base: "#22252c"
    palette.button: "#303640"
    palette.buttonText: "#e8e8e8"
    palette.highlight: "#478ac9"
    palette.light: "#3c4858"
    palette.midlight: "#384354"
    palette.dark: "#17191e"

    function requestAction(action) {
        if (androidSession.dirty || androidSession.recoveryAvailable) {
            requestedAction = action
            replaceDialog.open()
        } else performAction(action)
    }
    function performAction(action) {
        if (action === "open") androidSession.openProject()
        else if (action === "folder") androidSession.openProjectFolder()
        else androidSession.newProject()
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            Label {
                text: "MiaCode · Android P1"
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Label { text: androidSession.dirty ? "未保存" : "" }
            ToolButton {
                text: "文件"
                implicitHeight: 48
                onClicked: fileMenu.open()
                Menu {
                    id: fileMenu
                    MenuItem { text: "新建"; enabled: !androidSession.busy; onTriggered: window.requestAction("new") }
                    MenuItem { text: "打开谱面"; enabled: !androidSession.busy; onTriggered: window.requestAction("open") }
                    MenuItem { text: "打开工程文件夹"; enabled: !androidSession.busy; onTriggered: window.requestAction("folder") }
                    MenuItem { text: "保存"; enabled: !androidSession.busy; onTriggered: androidSession.save() }
                    MenuItem { text: "另存为"; enabled: !androidSession.busy; onTriggered: androidSession.save(true) }
                    MenuItem { text: "分享已保存谱面"; enabled: !androidSession.busy; onTriggered: androidSession.share() }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: titleField
                Layout.fillWidth: true
                implicitHeight: 48
                placeholderText: "作品标题"
                placeholderTextColor: "#858889"
                text: androidSession.title
                onTextEdited: androidSession.setTitle(text)
            }
            Button { text: "保存"; implicitHeight: 48; enabled: !androidSession.busy; onClicked: androidSession.save() }
        }
        TabBar {
            Layout.fillWidth: true
            visible: !window.wide
            currentIndex: window.panel
            onCurrentIndexChanged: window.panel = currentIndex
            TabButton { text: "编辑"; implicitHeight: 48 }
            TabButton { text: "素材与验证"; implicitHeight: 48 }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16
            ColumnLayout {
                visible: window.wide || window.panel === 0
                Layout.fillWidth: true
                Layout.fillHeight: true
                RowLayout {
                    Layout.fillWidth: true
                    ComboBox {
                        id: difficultyBox
                        Layout.fillWidth: true
                        implicitHeight: 48
                        model: androidSession.difficulties
                        textRole: "name"
                        valueRole: "id"
                        currentIndex: {
                            const entries = androidSession.difficulties
                            const active = androidSession.activeDifficulty
                            for (let i = 0; i < entries.length; ++i)
                                if (entries[i].id === active) return i
                            return -1
                        }
                        onActivated: androidSession.selectDifficulty(currentValue)
                    }
                    Button { text: "添加难度"; implicitHeight: 48; onClicked: addDialog.open() }
                }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    TextArea {
                        id: editor
                        objectName: "chartEditor"
                        property string historyKey: ""
                        text: androidSession.chartText
                        placeholderText: "输入 simai 谱面"
                        placeholderTextColor: "#858889"
                        wrapMode: TextEdit.Wrap
                        selectByMouse: true
                        persistentSelection: true
                        font.family: androidSession.editorFont || "monospace"
                        font.pixelSize: 18
                        inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                        onTextChanged: {
                            if (activeFocus && text !== androidSession.chartText)
                                androidSession.setChartText(text)
                        }
                        function updateHistoryKey() {
                            const key = androidSession.documentGeneration + ":" + androidSession.activeDifficulty
                            if (key === historyKey) return
                            historyKey = key
                            Qt.callLater(function() { editorTools.clearHistory(editor.textDocument) })
                        }
                        Component.onCompleted: updateHistoryKey()
                        Connections {
                            target: androidSession
                            function onChanged() { editor.updateHistoryKey() }
                        }
                    }
                }
                RowLayout {
                    Button { text: "撤销"; enabled: editor.canUndo; implicitHeight: 48; onClicked: editor.undo() }
                    Button { text: "重做"; enabled: editor.canRedo; implicitHeight: 48; onClicked: editor.redo() }
                    Item { Layout.fillWidth: true }
                    Button { text: "收起键盘"; implicitHeight: 48; onClicked: Qt.inputMethod.hide() }
                }
            }
            ScrollView {
                id: assetsScroll
                visible: window.wide || window.panel === 1
                Layout.preferredWidth: window.wide ? 320 : -1
                Layout.fillWidth: !window.wide
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: assetsScroll.availableWidth
                    spacing: 12
                    Label { text: "本机素材"; font.bold: true }
                    Label { text: "导入的素材保存为内部副本，重启后可继续离线使用。"; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        Repeater {
                            model: [{key: "audio", name: "音频"}, {key: "image", name: "图片"}, {key: "video", name: "视频"}, {key: "font", name: "字体"}]
                            Button {
                                required property var modelData
                                text: "导入" + modelData.name
                                implicitHeight: 48
                                enabled: !androidSession.busy
                                onClicked: androidSession.importAsset(modelData.key)
                            }
                        }
                    }
                    Repeater {
                        model: androidSession.assets
                        Label {
                            required property var modelData
                            text: modelData.kind + " · " + modelData.name
                            Layout.fillWidth: true
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                    Label { text: "P0 媒体探针"; font.bold: true }
                    Label { text: "检测 AVC 编码能力，并在本机生成短 MP4、WAV 和 PNG。此探针用于验证平台管线。"; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    Switch {
                        text: "允许探针在后台／锁屏继续"
                        checked: androidSession.backgroundExportAllowed
                        enabled: !androidSession.busy
                        onToggled: androidSession.backgroundExportAllowed = checked
                    }
                    Button { text: "运行离线媒体探针"; implicitHeight: 48; enabled: !androidSession.busy; onClicked: androidSession.runMediaProbe() }
                    Button { text: "导出探针结果"; implicitHeight: 48; enabled: !androidSession.busy; onClicked: androidSession.exportProbeResults() }
                    Item { Layout.fillHeight: true }
                }
            }
        }
        Label {
            Layout.fillWidth: true
            text: androidSession.status
            wrapMode: Text.WrapAnywhere
            maximumLineCount: 4
            elide: Text.ElideRight
        }
        BusyIndicator { running: androidSession.busy; visible: running; Layout.alignment: Qt.AlignHCenter; implicitHeight: 32 }
    }

    Dialog {
        id: replaceDialog
        anchors.centerIn: parent
        width: Math.min(window.width - 32, 420)
        modal: true
        title: "保留当前工程"
        standardButtons: Dialog.Discard | Dialog.Cancel
        Label { width: parent.width; text: "当前有编辑内容或恢复副本。继续将替换当前工程，请先保存需要保留的内容。"; wrapMode: Text.Wrap }
        onDiscarded: window.performAction(window.requestedAction)
    }
    Dialog {
        id: recoveryDialog
        anchors.centerIn: parent
        width: Math.min(window.width - 32, 420)
        modal: true
        title: "恢复工程"
        standardButtons: Dialog.Ok
        closePolicy: Popup.NoAutoClose
        Label { width: parent.width; text: "检测到上次工程的本机副本，点击确定恢复。恢复后可保存或新建工程。"; wrapMode: Text.Wrap }
        onAccepted: androidSession.recover()
    }
    Dialog {
        id: addDialog
        anchors.centerIn: parent
        width: Math.min(window.width - 32, 360)
        modal: true
        title: "添加难度"
        standardButtons: Dialog.Ok | Dialog.Cancel
        ComboBox { id: newDifficulty; width: parent.width; model: ["Easy", "Basic", "Advanced", "Expert", "Master", "Re:Master", "Utage"]; implicitHeight: 48 }
        onAccepted: { androidSession.addDifficulty(newDifficulty.currentIndex + 1); androidSession.selectDifficulty(newDifficulty.currentIndex + 1) }
    }
    Component.onCompleted: { if (androidSession.recoveryAvailable) recoveryDialog.open() }
}
