import QtQuick
import Nightstand.Core

// Full-screen password entry for one network, with the on-screen keyboard.
Rectangle {
    id: root

    property string ssid
    property string password: ""
    property bool reveal: false
    signal done(bool connected)

    color: "#101014"

    Binding {
        target: Device
        property: "textEntry"
        value: root.visible
    }

    function join() {
        Device.connectWifi(ssid, password)
    }

    Connections {
        target: Device
        function onWifiStateChanged() {
            if (Device.wifiState === "connected" && root.visible) {
                root.password = ""
                root.done(true)
            }
        }
    }

    focus: visible
    Keys.onPressed: event => {
        if (event.key === Qt.Key_Backspace)
            password = password.slice(0, -1)
        else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
            join()
        else if (event.text.length === 1 && event.text >= " ")
            password += event.text
        else
            return
        event.accepted = true
    }

    Item {
        id: top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: kb.top
        anchors.leftMargin: Theme.margin
        anchors.rightMargin: Theme.margin
        anchors.topMargin: 12
        anchors.bottomMargin: 8

        PillButton {
            id: cancel
            text: qsTr("Cancel")
            onClicked: { root.password = ""; root.done(false) }
        }
        PillButton {
            anchors.right: parent.right
            color: "white"
            textColor: Theme.dark
            fontWeight: Font.DemiBold
            text: Device.wifiState === "connecting" ? qsTr("Connecting…") : qsTr("Join")
            onClicked: if (Device.wifiState !== "connecting") root.join()
        }
        Text {
            anchors.left: cancel.right
            anchors.leftMargin: 16
            anchors.verticalCenter: cancel.verticalCenter
            width: parent.width - 2 * 150
            text: root.ssid
            color: "white"
            font.pixelSize: Theme.textBody
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
        }

        Rectangle {
            y: Theme.touch + 10
            width: parent.width
            height: Theme.touch
            radius: 16
            color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 2
            border.color: "white"
            clip: true

            Row {
                x: 18
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Text {
                    text: root.reveal ? root.password : "•".repeat(root.password.length)
                    color: "white"
                    font.pixelSize: 24
                    anchors.verticalCenter: parent.verticalCenter
                }
                Rectangle { width: 2; height: 30; color: "white"; anchors.verticalCenter: parent.verticalCenter }
            }
            PillButton {
                anchors.right: parent.right
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                height: Theme.touch - 8
                fontSize: Theme.textSmall
                text: root.reveal ? qsTr("Hide") : qsTr("Show")
                onClicked: root.reveal = !root.reveal
            }
        }
        Text {
            y: 2 * Theme.touch + 16
            width: parent.width
            text: Device.wifiState === "idle" || Device.wifiState === "connected" ? "" : Device.wifiMessage
            color: Device.wifiState === "failed" ? Theme.error : Theme.dim
            font.pixelSize: Theme.textSmall
            elide: Text.ElideRight
        }
    }

    Keyboard {
        id: kb
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        onKey: k => {
            if (k === "Done")
                root.join()
            else if (k === "⌫")
                root.password = root.password.slice(0, -1)
            else if (k === "space")
                root.password += " "
            else
                root.password += k
        }
    }
}
