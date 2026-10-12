import QtQuick
import QtQuick.Controls
import QtQuick.Window

// 放在 SplitView 的事件过滤范围之外，以被动观察指针驱动尺寸约束。
Item {
    id: root

    required property SplitView splitView
    required property Item resizedItem
    // 分割槽之前的面板使用 +1，之后的面板使用 -1。
    required property int resizeDirection
    required property real minimumSize
    required property real maximumSize
    required property real snapEnterDistance
    required property real snapLeaveDistance
    required property real targetSize
    property real releaseSize: targetSize
    property bool snapAvailable: true
    signal finished(real finalSize, real sizeAtPress)

    width: splitView !== null ? splitView.width : 0
    height: splitView !== null ? splitView.height : 0
    z: 1
    visible: dragAvailable

    readonly property bool horizontal: splitView !== null && splitView.orientation === Qt.Horizontal
    readonly property bool dragAvailable: splitView !== null && resizedItem !== null
        && splitView.visible && resizedItem.visible && width > 0 && height > 0
    readonly property real snapAlignmentTolerance: 0.5 / root.Screen.devicePixelRatio
    readonly property bool targetReachable: snapAvailable && isFinite(targetSize)
        && targetSize >= minimumSize && targetSize <= maximumSize
    readonly property bool releaseReachable: snapAvailable && isFinite(releaseSize)
        && releaseSize >= minimumSize && releaseSize <= maximumSize
    readonly property bool constrained: tracking && splitView !== null && splitView.resizing
        && snapped && targetReachable
    property bool tracking: false
    property bool snapped: false
    property bool horizontalAtPress: false
    property real pressPosition: 0
    property real sizeAtPress: 0
    property real rawSize: 0

    onDragAvailableChanged: {
        if (!dragAvailable)
            finishDrag(false)
    }
    onResizeDirectionChanged: finishDrag(false)
    onHorizontalChanged: finishDrag(false)
    onSnapAvailableChanged: updateSnap()
    onTargetSizeChanged: updateSnap()
    onTargetReachableChanged: updateSnap()

    Connections {
        target: root.splitView
        function onResizingChanged() {
            if (root.splitView.resizing)
                root.beginDrag(root.pointerPosition())
            else
                root.finishDrag(false)
        }
    }

    PointHandler {
        id: pointer
        target: null
        acceptedButtons: Qt.LeftButton
        onPointChanged: root.updateDrag(root.pointerPosition())
        onGrabChanged: transition => {
            if (transition === PointerHandler.CancelGrabPassive)
                root.finishDrag(false)
        }
    }

    function pointerPosition() {
        return horizontal ? pointer.point.position.x : pointer.point.position.y
    }

    function currentSize() {
        if (resizedItem === null)
            return NaN
        const sizeIsHorizontal = tracking ? horizontalAtPress : horizontal
        return sizeIsHorizontal ? resizedItem.width : resizedItem.height
    }

    function beginDrag(position) {
        if (!dragAvailable || !isFinite(position))
            return
        horizontalAtPress = horizontal
        pressPosition = position
        sizeAtPress = currentSize()
        rawSize = sizeAtPress
        tracking = true
        snapped = targetReachable && Math.abs(rawSize - targetSize) <= snapAlignmentTolerance
    }

    function updateSnap() {
        if (!tracking || splitView === null || !splitView.resizing)
            return
        const distance = snapped ? snapLeaveDistance : snapEnterDistance
        snapped = targetReachable && Math.abs(rawSize - targetSize) <= distance
    }

    function updateDrag(position) {
        if (!tracking || splitView === null || !splitView.resizing || !isFinite(position))
            return
        // 原始指针位移决定脱离吸附，面板尺寸由原生 SplitView 管理。
        rawSize = sizeAtPress + (position - pressPosition) * resizeDirection
        updateSnap()
    }

    function finishDrag(alignRelease) {
        if (!tracking)
            return
        const finalSize = alignRelease && constrained && releaseReachable
            ? releaseSize : currentSize()
        if (isFinite(finalSize))
            finished(finalSize, sizeAtPress)
        snapped = false
        tracking = false
    }
}
