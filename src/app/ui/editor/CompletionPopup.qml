import QtQuick
import QtQuick.Controls
import MiaCode.UI

// Completion candidates for the QML source editor. Focus stays on the editor —
// SourceEditor routes ↑ ↓ Tab Enter Esc through the controller — so this popup
// is a pure projection of the controller session and never grabs input. Its job
// is to show *which* candidate those keys are acting on.
AppDropdownPanel {
    id: root

    required property var editor
    required property var controller
    readonly property bool pointerInside: popupHover.hovered

    parent: Overlay.overlay
    modal: false
    focus: false
    closePolicy: Popup.NoAutoClose
    visible: editor.navigationVisible && controller.completionActive && controller.completionCandidates.length > 0

    readonly property real maximumHeight: 260
    property real candidatesWidth: 0

    // A vertical ListView never computes contentWidth, so the previous
    // `implicitWidth: contentWidth` binding resolved to -1 and pinned the popup
    // at its minimum width no matter how wide the candidates were.
    function recomputeCandidateWidth() {
        const candidates = root.controller.completionCandidates
        let widest = 0
        for (let i = 0; i < candidates.length; ++i) {
            candidateMetrics.text = candidates[i]
            widest = Math.max(widest, candidateMetrics.advanceWidth)
        }
        root.candidatesWidth = widest
    }

    width: Math.max(120, Math.ceil(root.candidatesWidth) + root.leftPadding + root.rightPadding + 24)
    height: Math.min(root.maximumHeight,
                     Math.max(Theme.controlMinHeight, candidateList.contentHeight)
                         + root.topPadding + root.bottomPadding)

    function updateAnchor() {
        if (!root.visible || !root.parent || !root.editor)
            return
        const caret = root.editor.cursorRectangle
        const below = root.editor.mapToItem(root.parent, caret.x, caret.y + caret.height)
        const above = root.editor.mapToItem(root.parent, caret.x, caret.y)
        root.x = Math.max(0, Math.min(below.x, Math.max(0, root.parent.width - root.width)))
        // Flip above the caret rather than hang off the bottom of the overlay.
        root.y = (below.y + root.height <= root.parent.height) || (above.y - root.height < 0)
            ? below.y
            : above.y - root.height
    }

    onWidthChanged: root.updateAnchor()
    onHeightChanged: root.updateAnchor()
    onVisibleChanged: {
        if (root.visible) {
            root.recomputeCandidateWidth()
            root.updateAnchor()
        }
    }

    Connections {
        target: root.editor
        enabled: root.visible
        function onCursorRectangleChanged() { root.updateAnchor() }
        function onLayoutChanged() {
            root.recomputeCandidateWidth()
            root.updateAnchor()
            candidateList.positionViewAtIndex(root.controller.completionIndex, ListView.Contain)
        }
        function onScenePositionChanged() { root.updateAnchor() }
    }

    Connections {
        target: root.controller
        function onCompletionChanged() {
            root.recomputeCandidateWidth()
            // Keyboard navigation must stay visible even when the session has
            // more candidates than the popup can show at once.
            candidateList.positionViewAtIndex(root.controller.completionIndex, ListView.Contain)
        }
    }

    Connections {
        target: root.parent
        enabled: root.visible
        function onWidthChanged() { root.updateAnchor() }
        function onHeightChanged() { root.updateAnchor() }
    }

    contentItem: ListView {
        id: candidateList
        model: root.controller.completionCandidates
        currentIndex: root.controller.completionIndex
        clip: true
        interactive: contentHeight > height
        boundsBehavior: Flickable.StopAtBounds

        HoverHandler {
            id: popupHover
        }

        TextMetrics {
            id: candidateMetrics
            font: root.editor.effectiveFont
        }

        delegate: ChromeRow {
            stateColors: Theme.colors.popupState
            id: candidateRow

            // Declaring modelData as a required property switches the whole
            // delegate to required properties, which turns off context property
            // injection — so `index` has to be required too. Without it both
            // `highlighted` and the click handler died with "index is not
            // defined" and no candidate ever highlighted.
            required property string modelData
            required property int index

            width: candidateList.width
            implicitHeight: Math.max(Theme.controlMinHeight, root.editor.lineHeight,
                                     implicitContentHeight + 2 * (Theme.chromePadding + Theme.chromeInsetY))
            highlighted: candidateRow.index === root.controller.completionIndex
            text: candidateRow.modelData
            labelFont: root.editor.effectiveFont

            onClicked: {
                root.controller.selectCompletionIndex(candidateRow.index)
                root.editor.acceptCompletionFromPopup()
            }
        }
    }
}
