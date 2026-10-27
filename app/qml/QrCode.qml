import QtQuick

// Crisp QR code on a white tile.
Rectangle {
    id: root

    property string text

    radius: 14
    color: "white"

    Image {
        anchors.fill: parent
        anchors.margins: 8
        source: root.text ? "image://qr/" + encodeURIComponent(root.text) : ""
        fillMode: Image.PreserveAspectFit
        smooth: false
        cache: false
    }
}
