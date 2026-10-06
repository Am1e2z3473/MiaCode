import QtQuick
import QtQuick.Controls
import MiaCode.UI

// The playback-rate HUD v1 drew over the preview: 当前倍速 + the rate as a
// percentage, held briefly and faded out. It is the only feedback a rate change
// made from the keyboard has — the transport's rate button is small, can be
// off-screen in fullscreen, and is not where the user is looking either way.
//
// It centres on whatever it is anchored to. In the workspace that is the
// timeline panel, where the eyes already are while editing and where nothing is
// obscured; fullscreen preview anchors it to the stage, having no timeline.
//
// The notification uses the shared popup surface and lets input reach the
// timeline or fullscreen stage underneath.
Item {
    id: root

    required property var previewSession
    // Keep the notification visible briefly after the latest rate change.
    property int holdMilliseconds: 900

    readonly property real rate: root.previewSession ? root.previewSession.rate : 1
    property int percent: 100
    property bool showing: false
    // The first evaluation of `rate` is the session's current speed, not a
    // change anyone asked for. Announcing it would flash the HUD every time the
    // pane is built — on page switches, on entering fullscreen.
    property bool armed: false

    onPreviewSessionChanged: root.armed = false

    onRateChanged: {
        const next = Math.round(root.rate * 100)
        if (!root.armed) {
            root.armed = true
            root.percent = next
            return
        }
        if (next === root.percent)
            return
        root.percent = next
        root.showing = true
        hideTimer.restart()
    }

    Timer {
        id: hideTimer
        interval: root.holdMilliseconds
        onTriggered: root.showing = false
    }

    AppDropdownPanel {
        id: card
        parent: root
        x: (root.width - width) / 2
        y: (root.height - height) / 2
        width: Math.min(Math.max(1, root.width - 16), Math.max(196, body.implicitWidth + 48))
        height: Math.min(Math.max(1, root.height - 16), Math.max(96, body.implicitHeight + 36))
        visible: root.showing && root.visible
        enabled: false
        modal: false
        dim: false
        focus: false
        closePolicy: Popup.NoAutoClose
        padding: Theme.panelPadding

        contentItem: Item {
            implicitWidth: body.implicitWidth
            implicitHeight: body.implicitHeight

            Column {
                id: body
                anchors.centerIn: parent
                spacing: 6

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTrId("timeline.playback_speed")
                    color: Theme.colors.text.primary
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.uiFontSize + 1
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.percent + "%"
                    color: Theme.colors.text.heading
                    font.family: Theme.uiFont
                    font.pixelSize: Theme.uiFontSize * 2
                    font.weight: Font.Bold
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }
}
