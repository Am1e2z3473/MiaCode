import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import QtQuick.Layouts
import MiaCode.UI

Rectangle {
    id: root
    required property var preferencesModel
    property bool expanded: false
    required property Flickable scrollViewport
    signal ensureVisibleRequested(Item item, string alignment)
    signal scrollRequested(real distance)
    implicitHeight: heading.height + (root.expanded ? contents.height : 0)
    radius: Theme.popupRadius
    color: Theme.surfaceColor(Theme.colors.background.surface)
    border.width: Theme.controlBorderWidth
    border.color: Theme.colors.border.normal
    clip: true

    onExpandedChanged: {
        if (!expanded) {
            root.releaseInputFocus()
            root.preferencesModel.finishCompletionEditing()
        }
    }
    onVisibleChanged: {
        if (!visible) {
            root.releaseInputFocus()
            root.expanded = false
        }
    }

    function ensureContentLayout() {
        durations.ensureContentLayout()
        divisions.ensureContentLayout()
        groups.ensurePolished()
    }
    function releaseInputFocusOutside(origin, position) {
        durations.releaseInputFocusOutside(origin, position)
        divisions.releaseInputFocusOutside(origin, position)
    }
    function releaseInputFocus() {
        durations.releaseInputFocus()
        divisions.releaseInputFocus()
    }
    function openGroup(group) {
        root.expanded = true
        Qt.callLater(function() {
            if (root.visible && root.expanded) {
                root.ensureContentLayout()
                root.ensureVisibleRequested(group === "division" ? divisions : durations, "top")
            }
        })
    }

    Item {
        id: heading
        width: parent.width
        height: Theme.controlMinHeight + 10
        AbstractButton {
            id: toggle
            anchors.fill: parent
            Accessible.name: qsTrId("preferences.completion.customize")
            onClicked: root.expanded = !root.expanded
            contentItem: Item {
                Row {
                    x: 12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 7
                    ControlsImpl.IconImage {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 16
                        height: 16
                        source: Qt.resolvedUrl(root.expanded ? "icons/chevron-down.svg" : "icons/chevron-right.svg")
                        sourceSize: Qt.size(16, 16)
                        color: Theme.colors.text.secondary
                    }
                    Text {
                        text: qsTrId("preferences.completion.customize")
                        color: Theme.colors.text.primary
                        font.family: Theme.uiFont
                        font.pixelSize: Theme.uiFontSize + 1
                    }
                }
            }
            background: Rectangle {
                color: "transparent"
                radius: Theme.popupRadius
                border.width: toggle.visualFocus ? Theme.controlBorderWidth : 0
                border.color: Theme.colors.accent.focus
            }
        }
        AppButton {
            id: resetButton
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            visible: root.expanded
            text: qsTrId("preferences.completion.reset")
            Accessible.description: qsTrId("preferences.completion.reset_hint")
            onClicked: resetDialog.open()
            Tooltip {
                visible: resetButton.visible && resetButton.hovered
                text: qsTrId("preferences.completion.reset_hint")
            }
            background: Rectangle {
                color: "transparent"
                radius: Theme.controlRadius
                border.width: resetButton.visualFocus ? Theme.controlBorderWidth : 0
                border.color: Theme.colors.accent.focus
            }
        }
    }

    TapHandler {
        parent: root
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        gesturePolicy: TapHandler.DragThreshold
        onPressedChanged: {
            if (pressed) root.releaseInputFocusOutside(root, point.position)
        }
    }

    Item {
        id: contents
        y: heading.height
        width: parent.width
        height: groups.implicitHeight + 24
        visible: root.expanded
        enabled: root.expanded
        RowLayout {
            id: groups
            x: 12
            y: 12
            width: Math.max(0, parent.width - 24)
            spacing: 20
            CompletionCandidateList {
                id: durations
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignTop
                candidateModel: root.preferencesModel.completionDurations
                title: qsTrId("preferences.completion.duration")
                inputHint: qsTrId("preferences.completion.duration_hint")
                scrollViewport: root.scrollViewport
                onEnsureVisibleRequested: function(item) { root.ensureVisibleRequested(item, "contain") }
                onScrollRequested: function(distance) { root.scrollRequested(distance) }
            }
            CompletionCandidateList {
                id: divisions
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignTop
                candidateModel: root.preferencesModel.completionDivisions
                title: qsTrId("preferences.completion.division")
                inputHint: qsTrId("preferences.completion.division_hint")
                scrollViewport: root.scrollViewport
                onEnsureVisibleRequested: function(item) { root.ensureVisibleRequested(item, "contain") }
                onScrollRequested: function(distance) { root.scrollRequested(distance) }
            }
        }
    }

    ChoiceDialog {
        id: resetDialog
        title: qsTrId("preferences.completion.reset_question")
        dismissChoiceId: "cancel"
        choices: [
            { id: "cancel", label: qsTrId("action.cancel"), role: "reject" },
            { id: "reset", label: qsTrId("preferences.completion.reset"), role: "accept" }
        ]
        onChosen: function(choiceId) {
            if (choiceId === "reset") root.preferencesModel.resetCompletionCandidates()
        }
    }
}
