import QtQuick

// Rounded button with optional leading icon.
Rectangle {
    id: root

    property string text
    property string icon
    property int iconSize: 26
    property color textColor: "white"
    property int fontSize: Theme.textBody
    property int fontWeight: Font.Medium
    property real padLeft: 24
    property real padRight: 24
    property real spacing: 8
    signal clicked

    implicitHeight: Theme.touch
    implicitWidth: row.implicitWidth + padLeft + padRight
    radius: height / 2
    color: Qt.rgba(1, 1, 1, 0.12)

    Row {
        id: row
        x: (root.width - implicitWidth + root.padLeft - root.padRight) / 2
        anchors.verticalCenter: parent.verticalCenter
        spacing: root.icon && root.text ? root.spacing : 0

        Icon {
            visible: !!root.icon
            name: root.icon
            size: root.iconSize
            color: root.textColor
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            visible: !!root.text
            text: root.text
            color: root.textColor
            font.pixelSize: root.fontSize
            font.weight: root.fontWeight
            font.features: { "tnum": 1 }
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        color: "white"
        opacity: mouse.pressed ? 0.12 : 0
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
