import QtQuick
import QtQml.Models
import QtQuick.Controls
import QtQuick.Window
import MiaCode.UI

Rectangle {
    id: root
    required property var viewState
    required property var documentSession
    required property var editorController
    required property var syncController
    required property var analysisSession
    property var preferences: null
    property bool navigationVisible: false
    property int pendingBookmarkLine: -1
    signal normalizeChartRequested()
    readonly property var bookmarks: sourceArea.bookmarks
    readonly property bool canUndo: sourceArea.canUndo
    readonly property bool canRedo: sourceArea.canRedo
    readonly property bool canCut: !sourceArea.readonly && sourceArea.selectedText.length > 0
    readonly property bool canCopy: sourceArea.selectedText.length > 0
    readonly property bool canPaste: !sourceArea.readonly && sourceArea.canPaste
    readonly property bool canTransform: canCut
    readonly property bool canNormalize: !sourceArea.readonly && documentSession.currentDifficultyId > 0
    readonly property var selectionBeatSummary: preferences && preferences.editorSelectionBeatDisplay
        ? documentSession.selectionBeatSummary(sourceArea.text, sourceArea.selectionStart, sourceArea.selectionEnd)
        : ({ totalCommaCount: 0, parts: [], exact: true })
    function beatSummaryDetail() {
        return (selectionBeatSummary.parts || []).map(part => part.count + "/" + part.denominator).join(" + ")
    }
    readonly property string selectionBeatStatusText: {
        const total = Number(selectionBeatSummary.totalCommaCount || 0)
        if (total <= 0) return ""
        const detail = beatSummaryDetail()
        const value = (selectionBeatSummary.parts || []).length > 1 ? total + " (" + detail + ")" : detail
        return qsTrId("document.selection_beats").arg(selectionBeatSummary.exact ? value : "~ " + value)
    }
    readonly property string selectionBeatTooltipText: selectionBeatStatusText.length === 0 ? ""
        : selectionBeatSummary.exact ? beatSummaryDetail() : qsTrId("document.selection_beats_inexact").arg(beatSummaryDetail())
    color: Theme.surfaceColor(Theme.colors.background.surface)
    clip: true
    function undo() { sourceArea.undo() }
    function redo() { sourceArea.redo() }
    function cut() { sourceArea.cut() }
    function copy() { sourceArea.copy() }
    function paste() { sourceArea.paste() }
    function selectAll() { sourceArea.selectAll() }
    function selectCurrentLine() { sourceArea.selectCurrentLine() }
    function jumpToLine(line) { sourceArea.jumpToLine(line) }
    function centerCursorInView() { sourceArea.centerCursorInView() }
    function openFindReplace() { findReplaceBar.show() }
    function seekPreviewToCaret() { sourceArea.seekPreviewToCaret() }
    function exportSelectionRange() { sourceArea.exportSelectionRange() }
    function applyChartTransform(operation) { return sourceArea.applyChartTransform(operation) }
    function applyNormalization(options) { return sourceArea.applyNormalization(options) }
    function applyEditorTransaction(transaction) { return sourceArea.applyEditorTransaction(transaction) }
    function selectionDescription() {
        if (sourceArea.selectionStart === sourceArea.selectionEnd)
            return qsTrId("qml.normalize_the_entire_chart_source")
        const startLine = sourceArea.text.substring(0, sourceArea.selectionStart).split("\n").length
        const endLine = sourceArea.text.substring(0, sourceArea.selectionEnd - 1).split("\n").length
        return qsTrId("qml.normalize_selected_lines_1_2").arg(startLine).arg(endLine)
    }
    function createBookmarkAtLine(line) { return sourceArea.createBookmarkAtLine(line, qsTrId("qml.bookmarks")) }
    function deleteBookmarkAtLine(line) { return sourceArea.deleteBookmarkAtLine(line) }
    function renameBookmarkAtLine(line, title) { return sourceArea.renameBookmarkAtLine(line, title) }
    function promptRenameBookmark(line) {
        const bookmark = bookmarks.find(item => item.line === line)
        if (!bookmark) return
        pendingBookmarkLine = line
        bookmarkTitleField.text = bookmark.title
        bookmarkTitleDialog.open()
    }
    function openContextMenuAt(x, y) {
        sourceArea.forceActiveFocus()
        const overlay = editorContextMenu.parent
        const point = sourceArea.mapToItem(overlay, x, y)
        const line = sourceArea.mapToItem(overlay, sourceArea.cursorRectangle)
        editorContextMenu.prepareItems()
        editorContextMenu.placeAt(point, line)
        editorContextMenu.open()
    }
    AppDialog {
        id: bookmarkTitleDialog

        title: qsTrId("editor.bookmark.rename")
        footer: DialogFooter {
            acceptText: qsTrId("action.ok")
            cancelText: qsTrId("action.cancel")
            onAccepted: bookmarkTitleDialog.accept()
            onRejected: bookmarkTitleDialog.reject()
        }
        onAccepted: root.renameBookmarkAtLine(root.pendingBookmarkLine, bookmarkTitleField.text)
        body: AppTextField {
            id: bookmarkTitleField
            Accessible.name: qsTrId("qml.bookmark_name")
        }
    }

    AppMenu {
        id: editorContextMenu
        objectName: "editorContextMenu"
        parent: Overlay.overlay

        property rect placement: Qt.rect(0, 0, 0, 0)
        x: placement.x
        y: placement.y
        width: placement.width
        height: placement.height

        function placeAt(point, lineBounds) {
            // 点击位置决定水平锚点；当前行决定上下避让，选区长度不参与定位。
            contentItem.forceLayout()
            const viewportWidth = parent.width
            const viewportHeight = parent.height
            const menuWidth = Math.min(implicitWidth, viewportWidth)
            const menuHeight = Math.min(measuredHeight(), viewportHeight)
            const gap = Theme.menuPadding
            const rightX = point.x + gap
            const menuX = Math.max(0, Math.min(rightX, viewportWidth - menuWidth))
            const menuY = Math.max(0, Math.min(point.y, viewportHeight - menuHeight))
            if (rightX + menuWidth <= viewportWidth) {
                // 即使长菜单向上贴齐窗口，左边缘仍在点击位置右侧。
                placement = Qt.rect(menuX, menuY, menuWidth, menuHeight)
                return
            }

            // 右侧空间不足时保留菜单宽度，沿当前行的上下边缘放置。
            const top = Math.max(0, Math.min(lineBounds.y - gap, viewportHeight))
            const bottom = Math.max(0, Math.min(
                lineBounds.y + lineBounds.height + gap, viewportHeight))
            const belowSpace = viewportHeight - bottom
            const aboveSpace = top
            if (belowSpace >= menuHeight) {
                placement = Qt.rect(menuX, bottom, menuWidth, menuHeight)
            } else if (aboveSpace >= menuHeight) {
                placement = Qt.rect(menuX, top - menuHeight, menuWidth, menuHeight)
            } else if (belowSpace >= aboveSpace) {
                placement = Qt.rect(menuX, bottom, menuWidth, belowSpace)
            } else {
                placement = Qt.rect(menuX, 0, menuWidth, aboveSpace)
            }
        }

        readonly property var transformRows: root.documentSession.chartTransformMenu()
        // 每次打开时确定操作列表，条目与几何在关闭动画期间保持一致。
        property bool hasSelection: false
        property bool canTransform: false
        property bool canNormalize: false

        function prepareItems() {
            const selected = root.canCopy
            const transform = root.canTransform
            const normalize = root.canNormalize
            if (menuEntries.count > 0 && selected === hasSelection
                    && transform === canTransform && normalize === canNormalize)
                return
            hasSelection = selected
            canTransform = transform
            canNormalize = normalize

            const rows = []
            function action(labelKey, operation) {
                rows.push({ kind: "action", labelKey: labelKey, operation: operation })
            }
            function separator() {
                rows.push({ kind: "separator", labelKey: "", operation: "" })
            }
            action("action.cut", "cut")
            action("action.copy", "copy")
            action("action.paste", "paste")
            separator()
            action("net.select_all", "select_all")
            action("qml.find_and_replace", "find")
            if (selected || normalize)
                separator()
            if (selected)
                action("qml.export_selection", "export")
            if (transform) {
                separator()
                for (const section of [0, 2]) {
                    if (section > 0)
                        separator()
                    for (const row of transformRows.filter(row => row.section === section))
                        action(row.labelKey, row.id)
                }
            }
            if (normalize)
                action("qml.normalize_whole_chart", "normalize")
            if (transform)
                rows.push({ kind: "submenu", labelKey: "action.transform.more", operation: "" })
            menuEntries.clear()
            for (const row of rows)
                menuEntries.append(row)
        }

        function triggerOperation(operation) {
            switch (operation) {
            case "cut": root.cut(); break
            case "copy": root.copy(); break
            case "paste": root.paste(); break
            case "select_all": root.selectAll(); break
            case "find": root.openFindReplace(); break
            case "export": root.exportSelectionRange(); break
            case "normalize": root.normalizeChartRequested(); break
            default: root.applyChartTransform(operation); break
            }
        }

        ListModel { id: menuEntries }

        Instantiator {
            model: menuEntries
            delegate: DelegateChooser {
                role: "kind"
                DelegateChoice {
                    roleValue: "action"
                    delegate: AppMenuItem {
                        required property string labelKey
                        required property string operation
                        text: qsTrId(labelKey)
                        enabled: operation === "cut" ? root.canCut
                            : operation === "copy" ? root.canCopy
                            : operation === "paste" ? root.canPaste : true
                        onTriggered: editorContextMenu.triggerOperation(operation)
                    }
                }
                DelegateChoice {
                    roleValue: "separator"
                    delegate: AppMenuSeparator {}
                }
                DelegateChoice {
                    roleValue: "submenu"
                    delegate: AppMenu {
                        id: transformMoreMenu
                        title: qsTrId("action.transform.more")
                        readonly property var subdivisionRows: editorContextMenu.transformRows.filter(row => row.section === 1)
                        Instantiator {
                            model: transformMoreMenu.subdivisionRows
                            delegate: AppMenuItem {
                                required property var modelData
                                text: qsTrId(modelData.labelKey)
                                onTriggered: root.applyChartTransform(modelData.id)
                            }
                            onObjectAdded: (index, item) => transformMoreMenu.insertItem(index, item)
                            onObjectRemoved: (index, item) => transformMoreMenu.removeItem(item)
                        }
                        AppMenuSeparator {}
                        Instantiator {
                            model: editorContextMenu.transformRows.filter(row => row.section === 3)
                            delegate: AppMenuItem {
                                required property var modelData
                                text: qsTrId(modelData.labelKey)
                                onTriggered: root.applyChartTransform(modelData.id)
                            }
                            onObjectAdded: (index, item) => transformMoreMenu.insertItem(
                                transformMoreMenu.subdivisionRows.length + 1 + index, item)
                            onObjectRemoved: (index, item) => transformMoreMenu.removeItem(item)
                        }
                    }
                }
            }
            onObjectAdded: (index, item) => {
                if (item instanceof AppMenu)
                    editorContextMenu.insertMenu(index, item)
                else
                    editorContextMenu.insertItem(index, item)
            }
            onObjectRemoved: (index, item) => {
                if (item instanceof AppMenu)
                    editorContextMenu.removeMenu(item)
                else
                    editorContextMenu.removeItem(item)
            }
        }
    }


    FindReplaceBar {
        id: findReplaceBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        editor: sourceArea
    }
    ScintillaEditor {
        id: sourceArea
        objectName: "sourceArea"
        property bool reservesPlainSpace: true
        anchors.left: parent.left
        anchors.right: verticalBar.left
        anchors.top: findReplaceBar.bottom
        anchors.bottom: horizontalBar.top
        documentSession: root.documentSession
        controller: root.editorController
        syncController: root.syncController
        analysisSession: root.analysisSession
        navigationVisible: root.navigationVisible
        font: Theme.codeFont
        blockSpacing: root.preferences ? root.preferences.editorBlockSpacing : 0
        scrollPastEnd: root.preferences ? root.preferences.editorScrollPastEnd : true
        palette: ({ text: Theme.colors.text.editor,
                    background: Theme.surfaceColor(Theme.colors.background.surface),
                    lineNumber: Theme.colors.text.lineNumber, accent: Theme.colors.accent.primary,
                    followOpacity: Theme.followHighlightOpacity,
                    keyword: Theme.colors.syntax.keyword, duration: Theme.colors.syntax.duration,
                    comment: Theme.colors.syntax.comment, error: Theme.colors.syntax.error,
                    warning: Theme.colors.syntax.warning, follow: Theme.colors.state.followHighlight,
                    currentLine: Theme.overlayColor(Theme.colors.state.focusLine),
                    selection: Theme.overlayColor(Theme.colors.state.selectionHighlight) })
        Rectangle {
            x: sourceArea.followCursorRectangle.x
            y: sourceArea.followCursorRectangle.y
            width: 2
            height: sourceArea.followCursorRectangle.height
            color: Theme.colors.accent.primary
            visible: sourceArea.followCaretVisible
        }
        onSelectionChanged: {
            root.viewState.editorCursorLine = cursorLine
            root.viewState.editorCursorColumn = cursorColumn
        }
        onActiveFocusChanged: {
            root.Window.window.sourceEditorFocused = activeFocus
            if (!activeFocus && !completionPopup.pointerInside)
                root.editorController.closeCompletion()
        }
        onFindRequested: root.openFindReplace()
        onContextMenuRequested: (x, y) => root.openContextMenuAt(x, y)
        onBookmarkMenuRequested: (line, x, y) => {
            root.pendingBookmarkLine = line
            bookmarkMenu.popup(sourceArea, x, y)
        }
    }
    AppScrollBar {
        id: verticalBar
        anchors.top: sourceArea.top
        anchors.bottom: sourceArea.bottom
        anchors.right: parent.right
        orientation: Qt.Vertical
        onPressedChanged: if (pressed) sourceArea.beginViewportInteraction()
        size: sourceArea.vertical_scroll_page / Math.max(1, sourceArea.vertical_scroll_max + sourceArea.vertical_scroll_page)
        position: sourceArea.vertical_scroll_value / Math.max(1, sourceArea.vertical_scroll_max + sourceArea.vertical_scroll_page)
        onPositionChanged: if (pressed) sourceArea.scrollVertical(Math.round(position * (sourceArea.vertical_scroll_max + sourceArea.vertical_scroll_page)))
    }
    AppScrollBar {
        id: horizontalBar
        anchors.left: parent.left
        anchors.right: verticalBar.left
        anchors.bottom: parent.bottom
        orientation: Qt.Horizontal
        onPressedChanged: if (pressed) sourceArea.beginViewportInteraction()
        size: sourceArea.horizontal_scroll_page / Math.max(1, sourceArea.horizontal_scroll_max + sourceArea.horizontal_scroll_page)
        position: sourceArea.horizontal_scroll_value / Math.max(1, sourceArea.horizontal_scroll_max + sourceArea.horizontal_scroll_page)
        onPositionChanged: if (pressed) sourceArea.scrollHorizontal(Math.round(position * (sourceArea.horizontal_scroll_max + sourceArea.horizontal_scroll_page)))
    }
    CompletionPopup {
        id: completionPopup
        editor: sourceArea
        controller: root.editorController
        editorScrollY: sourceArea.vertical_scroll_value
    }
    Binding {
        target: root.Window.window
        property: "sourceEditorOverlayHeld"
        value: completionPopup.pointerInside
    }
    AppMenu {
        id: bookmarkMenu
        AppMenuItem { text: qsTrId("qml.create_bookmark"); onTriggered: root.createBookmarkAtLine(root.pendingBookmarkLine) }
        AppMenuItem { text: qsTrId("editor.bookmark.rename"); onTriggered: root.promptRenameBookmark(root.pendingBookmarkLine) }
        AppMenuItem { text: qsTrId("editor.bookmark.delete"); onTriggered: root.deleteBookmarkAtLine(root.pendingBookmarkLine) }
    }
    Connections {
        target: root.viewState
        function onEditorClosed(key) { sourceArea.dropDocument(key) }
    }
}
