import QtQuick
import Nightstand.Core

// Black screen (backlight off). With tap-to-wake, the first tap shows a dim clock for a few
// seconds and a second tap wakes the device.
Rectangle {
    color: "black"

    MouseArea {
        anchors.fill: parent
        onClicked: {
            if (Player.sleepFading) {
                Player.extendSleep(5)
                return
            }
            Ui.wakeTimer.stop()
            if (Ui.wakeClock || !Settings.tapToWake) {
                Ui.wake()
                return
            }
            Ui.wakeClock = true
            Ui.wakeTimer.restart()
        }
    }

    Item {
        anchors.fill: parent
        visible: Ui.wakeClock

        Column {
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: Ui.burnShift.x
            anchors.verticalCenterOffset: Ui.burnShift.y
            spacing: 10
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: Ui.clockText
                color: "#6f6a63"
                font.pixelSize: 150
                font.features: { "tnum": 1 }
                font.letterSpacing: -4
                lineHeightMode: Text.FixedHeight
                lineHeight: 150
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: Player.sleepMode === "min" ? qsTr("Sleep %1").arg(Theme.fmt(Player.sleepRemaining))
                      : Player.sleepMode === "eoc" ? qsTr("Sleep end of ch.")
                      : Player.playing ? qsTr("Playing") : ""
                color: "#6f6a63"
                font.pixelSize: Theme.textBody
            }
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Theme.margin + 8
            text: qsTr("Tap again to wake")
            color: "#4d4944"
            font.pixelSize: Theme.textSmall
        }
    }
}
