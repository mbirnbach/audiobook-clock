import QtQuick
import Nightstand.Core

Rectangle {
    width: 76
    height: 280
    radius: 38
    color: Qt.rgba(18 / 255, 18 / 255, 22 / 255, 0.92)

    Rectangle {
        id: track
        anchors.horizontalCenter: parent.horizontalCenter
        y: 18
        width: 12
        height: parent.height - 18 - 14 - 12 - label.height
        radius: 5
        color: Qt.rgba(1, 1, 1, 0.2)
        clip: true
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: parent.height * Player.volume / 100
            radius: 5
            color: "white"
        }
    }
    Text {
        id: label
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 14
        text: Player.volume
        color: "white"
        font.pixelSize: Theme.textBody
        font.weight: Font.DemiBold
        font.features: { "tnum": 1 }
    }
}
