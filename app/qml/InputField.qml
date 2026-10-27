import QtQuick

// Tap-to-focus text box driven by the on-screen keyboard.
Column {
    id: root

    property string label
    property string value
    property bool masked: false
    property bool active: false
    signal tapped

    spacing: 8

    Text {
        text: root.label
        color: Theme.dim
        font.pixelSize: Theme.textSmall
    }

    Rectangle {
        width: root.width
        height: Theme.touch
        radius: 16
        color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 2
        border.color: root.active ? "white" : Qt.rgba(1, 1, 1, 0.16)
        clip: true

        Row {
            x: 18
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                text: root.masked ? "•".repeat(root.value.length) : root.value
                color: "white"
                font.pixelSize: 24
                font.letterSpacing: root.masked ? 3 : 0
                anchors.verticalCenter: parent.verticalCenter
            }
            Rectangle {
                width: 2
                height: 30
                color: "white"
                visible: root.active
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.tapped()
        }
    }
}
