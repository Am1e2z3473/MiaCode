import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import QtQuick.Layouts
import MiaCode.UI

Rectangle {
    id: root
    required property var preferencesModel
    property bool expanded: false
    signal ensureVisibleRequested(Item item)
    implicitHeight: heading.height + (contents.visible ? contents.height : 0)
    radius: Theme.popupRadius
    color: Theme.surfaceColor(Theme.colors.background.surface)
    border.width: Theme.controlBorderWidth
    border.color: Theme.colors.border.normal
    clip: true

    onExpandedChanged: if (!expanded) root.preferencesModel.finishCompletionEditing()
    onVisibleChanged: if (!visible) root.expanded = false

    function openGroup(group) {
        root.expanded = true
        Qt.callLater(function() {
            const list = group === "division" ? divisions : durations
            root.ensureVisibleRequested(list)
            list.focusFirst()
        })
    }

    Item {
        id: heading
        width: parent.width
        height: Math.max(46, Theme.controlMinHeight + 16)
        AbstractButton {
            id: toggle
            anchors.fill: parent
            hoverEnabled: true
            Accessible.name: qsTrId("preferences.completion.customize")
            Accessible.description: root.expanded ? qsTrId("preferences.completion.collapse")
                                                  : qsTrId("preferences.completion.expand")
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
                        font.pixelSize: Theme.uiFontSize
                    }
                }
            }
            background: Rectangle {
                color: "transparent"
                radius: Theme.popupRadius
                border.width: toggle.visualFocus ? Theme.controlBorderWidth : 0
                border.color: Theme.colors.accent.focus
            }
            Tooltip {
                visible: toggle.hovered && !resetButton.hovered
                text: toggle.Accessible.description
            }
        }
        AppButton {
            id: resetButton
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            visible: root.expanded
            text: qsTrId("preferences.completion.reset")
            onClicked: resetDialog.open()
        }
    }

    Item {
        id: contents
        y: heading.height
        width: parent.width
        height: groups.implicitHeight + 24
        opacity: 0
        visible: root.expanded || opacity > 0
        enabled: root.expanded
        state: root.expanded ? "expanded" : "collapsed"
        states: [
            State { name: "expanded"; PropertyChanges { target: contents; opacity: 1 } },
            State { name: "collapsed"; PropertyChanges { target: contents; opacity: 0 } }
        ]
        transitions: [
            FadeTransition { from: "collapsed"; to: "expanded"; appearing: true },
            FadeTransition { from: "expanded"; to: "collapsed"; appearing: false }
        ]
        Rectangle {
            width: parent.width
            height: 1
            color: Theme.separatorColor
        }
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
                onEnsureVisibleRequested: function(item) { root.ensureVisibleRequested(item) }
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
                onEnsureVisibleRequested: function(item) { root.ensureVisibleRequested(item) }
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
