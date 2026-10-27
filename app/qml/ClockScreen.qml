import QtQuick
import Nightstand.Core

// Standby clock. Tap anywhere to return to the player (or library).
Item {
    id: root

    readonly property var info: { Library.revision; return Library.book(Player.bookId) }

    MouseArea {
        anchors.fill: parent
        onClicked: Ui.go(Player.bookId ? "player" : "library")
    }

    Column {
        anchors.centerIn: parent
        anchors.horizontalCenterOffset: Ui.burnShift.x
        anchors.verticalCenterOffset: -30 + Ui.burnShift.y

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 10
            Text {
                id: time
                text: Ui.clockText
                // Warm and softer at night.
                color: Ui.night ? "#e0b48c" : "white"
                font.pixelSize: 180
                font.features: { "tnum": 1 }
                font.letterSpacing: -5
                lineHeightMode: Text.FixedHeight
                lineHeight: 180
            }
            Text {
                visible: Ui.ampm !== ""
                anchors.baseline: time.baseline
                text: Ui.ampm
                color: Qt.rgba(1, 1, 1, 0.7)
                font.pixelSize: 24
            }
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            topPadding: 10
            text: Ui.dateText
            color: Theme.dim
            font.pixelSize: Theme.textTitle
        }
    }

    Rectangle {
        visible: Player.bookId !== ""
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.horizontalCenterOffset: Ui.burnShift.x
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.margin - Ui.burnShift.y / 2
        width: nowRow.implicitWidth + 10 + 26
        height: 84
        radius: 20
        color: Qt.rgba(1, 1, 1, 0.07)

        Row {
            id: nowRow
            x: 10
            anchors.verticalCenter: parent.verticalCenter
            spacing: 16
            Cover {
                width: 64
                height: 64
                decodeSize: 128
                bookId: Player.bookId
                title: root.info.title || ""
                hasCover: !!root.info.hasCover
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Text {
                    width: Math.min(implicitWidth, 560)
                    text: root.info.title || ""
                    color: "white"
                    font.pixelSize: Theme.textBody
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                }
                Text {
                    text: {
                        let s = (Player.playing ? qsTr("Playing") : qsTr("Paused"))
                        if (Player.chapterCount > 0)
                            s += qsTr(" · Chapter %1").arg(Player.chapterIndex + 1)
                        if (Player.sleepMode === "min")
                            s += qsTr(" · Sleep %1").arg(Theme.fmt(Player.sleepRemaining))
                        else if (Player.sleepMode === "eoc")
                            s += qsTr(" · Sleep end of ch.")
                        return s
                    }
                    color: Theme.dim
                    font.pixelSize: Theme.textSmall
                    font.features: { "tnum": 1 }
                }
            }
        }
    }
}
