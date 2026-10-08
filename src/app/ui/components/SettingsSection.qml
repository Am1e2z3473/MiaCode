import QtQuick
import QtQuick.Layouts
import MiaCode.UI

// A settings page is a stack of these: a divider, a bold caption, then the
// controls underneath. Every page in ExportVideoPage hand-rolled this same
// three-piece shape, so it moved here once the third copy showed up.
ColumnLayout {
    id: root

    required property string title
    // The page's own leading divider (if any) already separates the tab bar
    // from the first section, so the first section skips its own to avoid a
    // doubled-up line.
    property bool first: false
    // Regular-weight title followed by a hairline, for sections that sit under
    // a regular-weight tab row: a bold title there would outrank its own tab.
    property bool inlineRule: false
    // Optional count shown after the title, e.g. how many items a list holds.
    property string badge: ""
    default property alias content: contentColumn.data

    spacing: 8

    Rectangle {
        visible: !root.first && !root.inlineRule
        Layout.fillWidth: true
        Layout.topMargin: 6
        height: 1
        color: Theme.colors.border.normal
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: root.inlineRule && !root.first ? 8 : 0
        spacing: 10

        Text {
            text: root.title
            color: Theme.colors.text.section
            font.family: Theme.uiFont
            font.pixelSize: Theme.sectionTitleFontSize
            font.weight: root.inlineRule ? Font.Normal : Theme.sectionTitleFontWeight
        }
        Rectangle {
            visible: root.badge.length > 0
            implicitWidth: Math.max(implicitHeight, badgeText.implicitWidth + 12)
            implicitHeight: 18
            radius: height / 2
            color: Theme.colors.popupState.selected

            Text {
                id: badgeText
                anchors.centerIn: parent
                text: root.badge
                color: Theme.colors.text.secondary
                font.family: Theme.uiFont
                font.pixelSize: Theme.secondaryFontSize
            }
        }
        Rectangle {
            visible: root.inlineRule
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            implicitHeight: 1
            color: Theme.colors.border.normal
        }
    }

    ColumnLayout {
        id: contentColumn
        Layout.fillWidth: true
        spacing: 10
    }
}
