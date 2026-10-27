import QtQuick
import Nightstand.Core

Item {
    id: root

    readonly property var info: { Library.revision; return Library.book(Player.bookId) }
    readonly property var pal: { Palette.revision; return Palette.forBook(Player.bookId) }
    readonly property real chElapsed: Player.position - Player.chapterStart
    readonly property real chRemaining: Player.chapterDuration - chElapsed
    readonly property string sleepText: Player.sleepMode === "min" ? Theme.fmt(Player.sleepRemaining)
                                        : Player.sleepMode === "eoc" ? qsTr("End of ch.") : ""

    Item {
        width: parent.width
        height: Theme.topBar
        BackPill {
            x: Theme.margin
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Library")
            onClicked: Ui.go("library")
        }
        Row {
            anchors.right: parent.right
            anchors.rightMargin: Theme.margin
            anchors.verticalCenter: parent.verticalCenter
            spacing: 14

            Row {
                visible: Player.sleepMode !== ""
                spacing: 6
                anchors.verticalCenter: parent.verticalCenter
                Icon {
                    name: "moon"
                    size: 24
                    color: root.pal.acc
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: root.sleepText
                    color: root.pal.acc
                    font.pixelSize: Theme.textBody
                    font.features: { "tnum": 1 }
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            PillButton {
                anchors.verticalCenter: parent.verticalCenter
                padLeft: 20
                padRight: 20
                fontWeight: Font.DemiBold
                text: Theme.fmtRate(Player.rate)
                color: Player.rate !== 1 ? root.pal.acc : Theme.fill
                textColor: Player.rate !== 1 ? Theme.dark : "white"
                onClicked: Ui.openOverlay("speed", Player.bookId)
            }
            ClockLabel { anchors.verticalCenter: parent.verticalCenter }
        }
    }

    Shadow {
        x: cover.x
        y: cover.y
        width: cover.width
        height: cover.height
    }
    Cover {
        id: cover
        x: Theme.margin
        y: Theme.topBar + 8
        width: 240
        height: 240
        bookId: Player.bookId
        title: root.info.title || ""
        author: root.info.author || ""
        hasCover: !!root.info.hasCover
        MouseArea {
            anchors.fill: parent
            onClicked: Ui.openDetail(Player.bookId)
        }
    }

    Item {
        id: side
        x: cover.x + cover.width + 24
        y: Theme.topBar + 4
        width: parent.width - x - Theme.margin
        height: parent.height - y - Theme.margin

        Column {
            width: parent.width

            Text {
                text: Player.chapterCount > 0 ? qsTr("Chapter %1 of %2").arg(Player.chapterIndex + 1).arg(Player.chapterCount)
                                              : " "
                color: Theme.dim
                font.pixelSize: Theme.textSmall
            }
            Text {
                width: parent.width
                text: Player.chapterTitle || root.info.title || ""
                color: "white"
                font.pixelSize: 24
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: (root.info.title || "") + " · " + (root.info.author || "")
                color: Qt.rgba(1, 1, 1, 0.78)
                font.pixelSize: Theme.textSmall
                elide: Text.ElideRight
            }

            // Seek bar: the whole 52 px strip is touchable; drag or tap within the chapter.
            Item {
                id: seekBar
                width: parent.width
                height: 52
                property real preview: -1
                readonly property real frac: preview >= 0 ? preview
                                             : Math.max(0, Math.min(1, root.chElapsed / Player.chapterDuration))

                Rectangle {
                    y: parent.height / 2 - 4
                    width: parent.width
                    height: 8
                    radius: 4
                    color: Theme.faint
                    Rectangle {
                        width: parent.width * seekBar.frac
                        height: parent.height
                        radius: 4
                        color: root.pal.acc
                    }
                    Rectangle {
                        x: Math.max(0, Math.min(parent.width - width, parent.width * seekBar.frac - width / 2))
                        y: -10
                        width: 28
                        height: 28
                        radius: 14
                        color: "white"
                        border.width: 1
                        border.color: Qt.rgba(0, 0, 0, 0.25)
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    preventStealing: true
                    function fracAt(x) { return Math.max(0, Math.min(1, x / width)) }
                    onPressed: mouse => seekBar.preview = fracAt(mouse.x)
                    onPositionChanged: mouse => seekBar.preview = fracAt(mouse.x)
                    onReleased: mouse => {
                        Player.seek(Player.chapterStart + fracAt(mouse.x) * Player.chapterDuration)
                        seekBar.preview = -1
                    }
                    onCanceled: seekBar.preview = -1
                }
            }

            Item {
                width: parent.width
                height: 26
                readonly property real shownElapsed: seekBar.preview >= 0 ? seekBar.preview * Player.chapterDuration
                                                                          : root.chElapsed
                Text {
                    text: Theme.fmt(parent.shownElapsed)
                    color: Qt.rgba(1, 1, 1, 0.8)
                    font.pixelSize: Theme.textSmall
                    font.features: { "tnum": 1 }
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width - 170
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    text: Player.error ? Player.error
                          : Player.loading ? qsTr("Loading…")
                          // Listening time left at the current speed.
                          : qsTr("%1 left").arg(Theme.fmtDur((Player.duration - Player.position) / Player.rate))
                    color: Player.error ? Theme.error : Qt.rgba(1, 1, 1, 0.8)
                    font.pixelSize: Theme.textSmall
                    font.features: { "tnum": 1 }
                }
                Text {
                    anchors.right: parent.right
                    text: "−" + Theme.fmt(Player.chapterDuration - parent.shownElapsed)
                    color: Qt.rgba(1, 1, 1, 0.8)
                    font.pixelSize: Theme.textSmall
                    font.features: { "tnum": 1 }
                }
            }

            // Transport: chapter back, skip back, play/pause, skip forward, chapter forward.
            Item {
                id: transport
                width: parent.width
                height: Theme.touchLarge + 8
                readonly property real gap: (width - 2 * Theme.touch - 2 * 84 - 104) / 4

                CircleButton {
                    x: 0
                    anchors.verticalCenter: parent.verticalCenter
                    color: "transparent"
                    icon: "prev"
                    iconSize: 30
                    onClicked: Player.previousChapter()
                }
                CircleButton {
                    x: Theme.touch + transport.gap
                    anchors.verticalCenter: parent.verticalCenter
                    width: 84
                    height: 84
                    color: "transparent"
                    icon: "skipBack"
                    iconSize: 44
                    onClicked: Player.skip(-Settings.skipSecs)
                    Text {
                        anchors.centerIn: parent
                        anchors.horizontalCenterOffset: 0.5
                        anchors.verticalCenterOffset: 2
                        text: Settings.skipSecs
                        color: "white"
                        font.pixelSize: 14
                        font.weight: Font.Bold
                    }
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: 104
                    height: 104
                    radius: 52
                    color: "white"
                    scale: playMouse.pressed ? 0.96 : 1
                    Icon {
                        anchors.centerIn: parent
                        name: Player.playing ? "pause" : "play"
                        size: 44
                        color: Theme.dark
                    }
                    MouseArea {
                        id: playMouse
                        anchors.fill: parent
                        anchors.margins: -6
                        onClicked: Player.togglePlay()
                    }
                }
                CircleButton {
                    x: parent.width - Theme.touch - transport.gap - width
                    anchors.verticalCenter: parent.verticalCenter
                    width: 84
                    height: 84
                    color: "transparent"
                    icon: "skipFwd"
                    iconSize: 44
                    onClicked: Player.skip(Settings.skipSecs)
                    Text {
                        anchors.centerIn: parent
                        anchors.horizontalCenterOffset: -0.5
                        anchors.verticalCenterOffset: 2
                        text: Settings.skipSecs
                        color: "white"
                        font.pixelSize: 14
                        font.weight: Font.Bold
                    }
                }
                CircleButton {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    color: "transparent"
                    icon: "next"
                    iconSize: 30
                    onClicked: Player.nextChapter()
                }
            }
        }

        Row {
            anchors.bottom: parent.bottom
            width: parent.width
            spacing: 10
            readonly property real cell: (width - 20) / 3

            PillButton {
                width: parent.cell
                padLeft: 8
                padRight: 8
                icon: "moon"
                iconSize: 24
                fontSize: 20
                text: Player.sleepMode !== "" ? root.sleepText : qsTr("Sleep")
                color: Player.sleepMode !== "" ? root.pal.acc : Theme.fill
                textColor: Player.sleepMode !== "" ? Theme.dark : "white"
                onClicked: Ui.openOverlay("sleep", Player.bookId)
            }
            PillButton {
                width: parent.cell
                padLeft: 8
                padRight: 8
                icon: "list"
                iconSize: 24
                fontSize: 20
                text: qsTr("Chapters")
                onClicked: Ui.openOverlay("chapters", Player.bookId)
            }
            PillButton {
                width: parent.cell
                padLeft: 8
                padRight: 8
                icon: "bookmark"
                iconSize: 24
                fontSize: 20
                text: qsTr("Marks")
                onClicked: Ui.openOverlay("bookmarks", Player.bookId)
            }
        }
    }
}
