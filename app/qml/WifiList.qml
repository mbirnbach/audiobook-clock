import QtQuick
import Nightstand.Core

// Scanned networks; tapping one asks the parent to join it.
ListView {
    id: root

    signal picked(string ssid, bool secure, bool saved)

    spacing: 4
    clip: true
    model: Device.wifiNetworks
    boundsBehavior: Flickable.StopAtBounds
    Component.onCompleted: Device.scanWifi()

    delegate: Rectangle {
        readonly property bool on: modelData.ssid === Device.wifiSsid
        width: root.width
        height: 84
        radius: 18
        color: on ? Theme.fill : "transparent"

        Icon {
            x: 18
            anchors.verticalCenter: parent.verticalCenter
            name: "wifi"
            size: 28
            color: Qt.rgba(1, 1, 1, 0.8)
        }
        Column {
            x: 64
            width: parent.width - 64 - 56
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1
            Text {
                width: parent.width
                text: modelData.ssid
                color: "white"
                font.pixelSize: Theme.textBody
                font.weight: Font.Medium
                elide: Text.ElideRight
            }
            Text {
                text: (parent.parent.on ? qsTr("Connected · ") : modelData.saved ? qsTr("Saved · ") : "") + modelData.detail
                color: Qt.rgba(1, 1, 1, 0.7)
                font.pixelSize: Theme.textSmall
            }
        }
        Icon {
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            name: parent.on ? "check" : modelData.secure ? "lock" : ""
            size: parent.on ? 28 : 22
            color: parent.on ? "white" : Qt.rgba(1, 1, 1, 0.6)
        }
        MouseArea {
            anchors.fill: parent
            onClicked: if (!parent.on) root.picked(modelData.ssid, !!modelData.secure, !!modelData.saved)
        }
    }

    Text {
        visible: root.count === 0
        width: root.width
        topPadding: 12
        leftPadding: 8
        text: Device.wifiAvailable ? qsTr("Looking for networks…") : qsTr("Wi-Fi is managed by the system on this machine.")
        color: Theme.dim
        font.pixelSize: Theme.textBody
        wrapMode: Text.WordWrap
    }
}
