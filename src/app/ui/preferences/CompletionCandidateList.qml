pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as ControlsImpl
import MiaCode.UI

Column {
    id: root
    required property var candidateModel
    required property string title
    required property string inputHint
    signal ensureVisibleRequested(Item item)
    property string draggingKey: ""
    property Item draggedItem: null
    property real draggedY: 0
    property int dropRow: -1
    readonly property real rowHeight: Math.max(36, Theme.controlMinHeight + 6)
    spacing: 8

    function cancelDrag() {
        root.draggingKey = ""
        root.draggedItem = null
        root.dropRow = -1
    }
    function focusFirst() {
        if (cards.count > 0) cards.itemAt(0).focusInput()
    }
    function destination(point) {
        const stride = root.rowHeight + rows.spacing
        return Math.max(0, Math.min(cards.count - 2, Math.round((point.y - root.rowHeight / 2) / stride)))
    }
    onVisibleChanged: if (!visible) root.cancelDrag()

    Text {
        text: root.title
        color: Theme.colors.text.section
        font.family: Theme.uiFont
        font.pixelSize: Theme.uiFontSize
        font.weight: Theme.sectionTitleFontWeight
    }

    Item {
        width: parent.width
        height: rows.implicitHeight

        Column {
            id: rows
            width: parent.width
            spacing: 6

            Repeater {
                id: cards
                model: root.candidateModel
                delegate: Rectangle {
                    id: card
                    required property int index
                    required property string candidateKey
                    required property string draftValue
                    required property bool placeholderRow
                    required property bool removable
                    required property string validationMessage
                    width: rows.width
                    height: root.rowHeight
                    radius: Theme.controlRadius
                    color: {
                        const panel = Qt.color(Theme.colors.background.panel)
                        return Theme.surfaceColor(Qt.tint(Theme.colors.background.surface,
                            Qt.rgba(panel.r, panel.g, panel.b, 0.62)))
                    }
                    opacity: card.placeholderRow ? 0.75 : 1
                    function focusInput() { valueInput.forceActiveFocus() }

                    Item {
                        id: grabArea
                        x: 5
                        y: 4
                        width: Math.max(0, card.width - 34)
                        height: card.height - 8
                        visible: !card.placeholderRow
                        activeFocusOnTab: true
                        Accessible.role: Accessible.Button
                        Accessible.name: qsTrId("preferences.completion.reorder")
                        Keys.onPressed: function(event) {
                            if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
                                root.candidateModel.moveCandidate(card.candidateKey,
                                    card.index + (event.key === Qt.Key_Up ? -1 : 1))
                                event.accepted = true
                            } else if (event.key === Qt.Key_Escape && root.draggingKey.length > 0) {
                                root.cancelDrag()
                                event.accepted = true
                            }
                        }
                        HoverHandler {
                            id: grabHover
                            cursorShape: drag.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                        }
                        Tooltip {
                            visible: grabHover.hovered && !drag.active
                            text: qsTrId("preferences.completion.reorder")
                        }
                        Rectangle {
                            anchors.fill: parent
                            visible: grabArea.activeFocus
                            color: "transparent"
                            radius: Theme.compactControlRadius
                            border.width: Theme.controlBorderWidth
                            border.color: Theme.colors.accent.focus
                        }
                        DragHandler {
                            id: drag
                            target: null
                            acceptedButtons: Qt.LeftButton
                            enabled: !card.placeholderRow
                            onActiveChanged: {
                                if (active) {
                                    grabArea.forceActiveFocus()
                                    root.draggingKey = card.candidateKey
                                    root.draggedItem = card
                                    root.draggedY = card.y
                                    root.dropRow = card.index
                                } else if (root.draggingKey === card.candidateKey) {
                                    const key = root.draggingKey
                                    const destination = root.dropRow
                                    root.cancelDrag()
                                    root.candidateModel.moveCandidate(key, destination)
                                }
                            }
                            onTranslationChanged: {
                                if (active && root.draggingKey === card.candidateKey) {
                                    root.draggedY = card.y + translation.y
                                    root.dropRow = root.destination(grabArea.mapToItem(rows, centroid.position))
                                }
                            }
                            onCanceled: root.cancelDrag()
                        }
                    }

                    AppTextField {
                        id: valueInput
                        x: 18
                        anchors.verticalCenter: parent.verticalCenter
                        height: Math.max(28, Theme.controlMinHeight - 2)
                        width: Math.max(0, Math.min(card.width * 0.75, card.width - 62,
                            Math.max(card.width * 0.375, valueMetrics.advanceWidth + leftPadding + rightPadding + 6)))
                        leftPadding: 4
                        rightPadding: 4
                        topPadding: 3
                        bottomPadding: 3
                        font.family: Theme.uiFont
                        font.pixelSize: Theme.uiFontSize
                        color: Theme.colors.text.primary
                        placeholderTextColor: Theme.colors.text.primary
                        placeholderText: card.placeholderRow ? root.inputHint : ""
                        text: card.draftValue
                        background: Item {}
                        Accessible.name: root.title
                        Accessible.description: card.validationMessage
                        onTextEdited: root.candidateModel.setDraft(card.candidateKey, text)
                        onAccepted: root.candidateModel.commit(card.candidateKey)
                        onActiveFocusChanged: {
                            if (activeFocus) {
                                root.ensureVisibleRequested(card)
                            } else {
                                const model = root.candidateModel
                                const key = card.candidateKey
                                Qt.callLater(function() { model.commit(key) })
                            }
                        }
                        TextMetrics {
                            id: valueMetrics
                            font: valueInput.font
                            text: valueInput.text.length > 0 ? valueInput.text : valueInput.placeholderText
                        }
                        Tooltip {
                            visible: valueInput.hovered && text.length > 0
                            text: card.validationMessage.length > 0 ? card.validationMessage
                                : valueMetrics.advanceWidth > valueInput.width - valueInput.leftPadding - valueInput.rightPadding
                                    ? valueInput.text : ""
                        }
                    }

                    Item {
                        anchors.right: parent.right
                        anchors.rightMargin: 5
                        anchors.verticalCenter: parent.verticalCenter
                        width: 24
                        height: 24
                        visible: !card.placeholderRow
                        HoverHandler { id: removeHover }
                        Tooltip {
                            visible: removeHover.hovered
                            text: card.removable ? qsTrId("preferences.completion.delete")
                                                 : qsTrId("preferences.completion.keep_one")
                        }
                        AbstractButton {
                            id: removeButton
                            anchors.fill: parent
                            enabled: card.removable
                            focusPolicy: Qt.TabFocus
                            Accessible.name: qsTrId("preferences.completion.delete")
                            onPressed: root.candidateModel.prepareRemoval(card.candidateKey)
                            onCanceled: root.candidateModel.cancelRemoval()
                            onClicked: root.candidateModel.removeCandidate(card.candidateKey)
                            contentItem: Item {
                                ControlsImpl.IconImage {
                                    anchors.centerIn: parent
                                    width: 12
                                    height: 12
                                    source: Qt.resolvedUrl("icons/close.svg")
                                    sourceSize: Qt.size(12, 12)
                                    color: card.removable ? Theme.colors.danger.primary : Theme.colors.text.disabled
                                }
                            }
                            background: Rectangle {
                                color: "transparent"
                                radius: Theme.compactControlRadius
                                border.width: removeButton.visualFocus ? Theme.controlBorderWidth : 0
                                border.color: Theme.colors.accent.focus
                            }
                        }
                    }
                }
            }
        }

        // Uses the existing layer-list insertion marker; sorting commits on release.
        Rectangle {
            width: rows.width
            height: 2
            y: root.dropRow * (root.rowHeight + rows.spacing)
            visible: root.draggingKey.length > 0 && root.dropRow >= 0
            color: Theme.colors.accent.primary
        }
        ShaderEffectSource {
            visible: root.draggingKey.length > 0 && root.draggedItem !== null
            sourceItem: root.draggedItem
            hideSource: visible
            live: visible
            width: root.draggedItem ? root.draggedItem.width : 0
            height: root.rowHeight
            x: 0
            y: root.draggedY
            smooth: true
            z: 2
        }
    }
    Connections {
        target: root.candidateModel
        function onModelAboutToBeReset() { root.cancelDrag() }
        function onRowsAboutToBeRemoved() { root.cancelDrag() }
    }
}
