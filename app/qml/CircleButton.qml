import QtQuick

Rectangle {
    id: root

    property string icon
    property int iconSize: 30
    property color iconColor: "white"
    property color pressedColor: Qt.rgba(1, 1, 1, 0.12)
    signal clicked

    implicitWidth: Theme.touch
    implicitHeight: Theme.touch
    radius: width / 2
    color: Qt.rgba(1, 1, 1, 0.1)

    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        color: root.pressedColor
        visible: mouse.pressed
    }

    Icon {
        anchors.centerIn: parent
        name: root.icon
        size: root.iconSize
        color: root.iconColor
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
