import QtQuick

// Labelled segmented control. options: [{ label, value }]
Column {
    id: root

    property string label
    property var options: []
    property var current
    signal picked(var value)

    spacing: 10

    Text {
        text: root.label
        color: Theme.dim
        font.pixelSize: Theme.textSmall
    }

    Rectangle {
        width: root.width
        height: Theme.touch + 8
        radius: 18
        color: Qt.rgba(1, 1, 1, 0.08)

        Row {
            id: segs
            anchors.fill: parent
            anchors.margins: 4
            spacing: 4

            Repeater {
                model: root.options
                Rectangle {
                    readonly property bool on: modelData.value === root.current
                    width: (segs.width - segs.spacing * (root.options.length - 1)) / root.options.length
                    height: Theme.touch
                    radius: 14
                    color: on ? "white" : "transparent"

                    Text {
                        anchors.centerIn: parent
                        width: parent.width - 12
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        text: modelData.label
                        color: parent.on ? "#141414" : "white"
                        font.pixelSize: Theme.textBody
                        font.weight: Font.Medium
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.picked(modelData.value)
                    }
                }
            }
        }
    }
}
