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

    component BottomTab: AppTabButton {
        id: tab

        property int count: -1
        property color countColor: Theme.colors.accent.badge

        compact: true
        Layout.alignment: Qt.AlignVCenter
        accessory: count > 0 ? badge : null
        Accessible.description: count > 0 ? String(count) : ""

        Component {
            id: badge
            Rectangle {
                implicitWidth: Math.max(implicitHeight, countLabel.implicitWidth + 8)
                implicitHeight: 16
                radius: height / 2
                color: tab.countColor

                Text {
                    id: countLabel
                    anchors.fill: parent
                    text: tab.count
                    color: Theme.colors.text.onAccent
                    font.family: Theme.uiFont
                    font.pixelSize: 10
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    component AnalysisTab: BottomTab {
        required property var issueRows
        readonly property var issueCounts: root.countIssues(issueRows)
        count: issueCounts.errors > 0 ? issueCounts.errors : issueCounts.warnings
        countColor: issueCounts.errors > 0
            ? Theme.colors.danger.primary : Theme.colors.accent.badge
    }

    RowLayout {
        id: tabLayout
        anchors.fill: parent
        anchors.leftMargin: Theme.panelPadding - Theme.chromeInsetX
        anchors.rightMargin: Theme.panelPadding
        spacing: 4

        BottomTab {
            visible: root.timelineSession.timelineTabVisible
            text: root.timelineSession.timelineTabLabel
            checked: root.timelineSession.currentTabId === "timeline"
            onClicked: root.timelineSession.setCurrentTabId("timeline")
        }
        AnalysisTab {
            visible: root.timelineSession.validationTabVisible
            text: root.timelineSession.validationTabLabel
            issueRows: root.analysisSession.validationRows
            checked: root.timelineSession.currentTabId === "validation"
            onClicked: root.timelineSession.setCurrentTabId("validation")
        }
        AnalysisTab {
            visible: root.timelineSession.muriTabVisible
            text: root.timelineSession.muriTabLabel
            issueRows: root.analysisSession.muriRows
            checked: root.timelineSession.currentTabId === "muri"
            onClicked: root.timelineSession.setCurrentTabId("muri")
        }

        Item { Layout.fillWidth: true }

        AppCheckBox {
            Layout.alignment: Qt.AlignVCenter
            compact: true
            font.pixelSize: Theme.compactFontSize
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
