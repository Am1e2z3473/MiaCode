import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI

Item {
    id: root

    required property var documentSession
    required property var analysisSession
    required property var timelineSession

    implicitHeight: Theme.compactControlHeight + 2
    readonly property real minimumWidth: tabLayout.implicitWidth
        + tabLayout.anchors.leftMargin + tabLayout.anchors.rightMargin

    function countIssues(rows) {
        let errors = 0
        let warnings = 0
        for (const row of rows) {
            if (row.severity === "error")
                ++errors
            else if (row.severity === "warning")
                ++warnings
        }
        return { errors: errors, warnings: warnings }
    }

    component AnalysisTab: TimelineTab {
        required property var issueRows
        readonly property var issueCounts: root.countIssues(issueRows)
        count: issueCounts.errors > 0 ? issueCounts.errors : issueCounts.warnings
        countColor: issueCounts.errors > 0
            ? Theme.colors.danger.primary : Theme.colors.accent.badge
    }

    component TimelineTab: AppTab {
        panelTab: true
        compact: true
        labelFontSize: Theme.compactFontSize
    }

    RowLayout {
        id: tabLayout
        anchors.fill: parent
        anchors.leftMargin: Theme.panelPadding - Theme.chromeInsetX
        anchors.rightMargin: Theme.panelPadding
        spacing: 0
        z: 1

        TimelineTab {
            Layout.alignment: Qt.AlignVCenter
            visible: root.timelineSession.timelineTabVisible
            text: root.timelineSession.timelineTabLabel
            active: root.timelineSession.currentTabId === "timeline"
            onClicked: root.timelineSession.setCurrentTabId("timeline")
        }
        AnalysisTab {
            Layout.alignment: Qt.AlignVCenter
            visible: root.timelineSession.validationTabVisible
            text: root.timelineSession.validationTabLabel
            Layout.leftMargin: 4
            issueRows: root.analysisSession.validationRows
            active: root.timelineSession.currentTabId === "validation"
            onClicked: root.timelineSession.setCurrentTabId("validation")
        }
        AnalysisTab {
            Layout.alignment: Qt.AlignVCenter
            visible: root.timelineSession.muriTabVisible
            text: root.timelineSession.muriTabLabel
            Layout.leftMargin: 4
            issueRows: root.analysisSession.muriRows
            active: root.timelineSession.currentTabId === "muri"
            onClicked: root.timelineSession.setCurrentTabId("muri")
        }

        Item { Layout.fillWidth: true }

        AppCheckBox {
            compact: true
            font.pixelSize: Theme.compactFontSize
            Layout.alignment: Qt.AlignVCenter
            visible: root.timelineSession.currentTabId === "timeline"
            text: root.timelineSession.followCodeLabel
            checked: root.timelineSession.stateBridge
                ? root.timelineSession.stateBridge.followPreviewEnabled
                : false
            Accessible.description: qsTrId("qml.follow_current_chart_source_position")
            onClicked: root.timelineSession.followPreviewToggled(checked)
        }
    }
}
