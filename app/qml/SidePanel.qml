import QtQuick
import Nightstand.Core

// Right-hand sheet for chapters, bookmarks and the sleep timer.
Item {
    id: root

    readonly property string bookId: Ui.overlayBook
    readonly property bool isCurrent: bookId === Player.bookId
    readonly property var pal: { Palette.revision; return Palette.forBook(bookId) }
    readonly property var chapters: { Library.revision; return Library.chapters(bookId) }
    readonly property real bookPos: {
        Library.revision
        return isCurrent ? Player.position : Library.progress(bookId).time
    }
    readonly property int currentChapter: isCurrent ? Player.chapterIndex : Library.chapterAt(bookId, bookPos)
    readonly property real chRemaining: Player.chapterDuration - (Player.position - Player.chapterStart)

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.5)
        MouseArea {
            anchors.fill: parent
            onClicked: Ui.overlay = ""
        }
    }

    Rectangle {
        id: sheet
        anchors.right: parent.right
        width: 520
        height: parent.height
        color: Theme.panel
        MouseArea { anchors.fill: parent } // swallow taps

        readonly property real inner: width - 2 * Theme.margin

        Rectangle {
            width: 1
            height: parent.height
            color: Qt.rgba(1, 1, 1, 0.08)
        }

        Item {
            id: header
            width: parent.width
            height: Theme.topBar
            Text {
                x: Theme.margin + 4
                anchors.verticalCenter: parent.verticalCenter
                text: ({ chapters: qsTr("Chapters"), bookmarks: qsTr("Bookmarks"), sleep: qsTr("Sleep timer"),
                         speed: qsTr("Speed") })[Ui.overlay] || ""
                color: "white"
                font.pixelSize: 28
                font.weight: Font.DemiBold
                font.letterSpacing: -0.3
            }
            CircleButton {
                anchors.right: parent.right
                anchors.rightMargin: Theme.margin
                anchors.verticalCenter: parent.verticalCenter
                icon: "close"
                iconSize: 28
                onClicked: Ui.overlay = ""
            }
        }

        // ------------------------------------------------------------ chapters
        ListView {
            id: chapterList
            visible: Ui.overlay === "chapters"
            anchors.top: header.bottom
            anchors.bottom: parent.bottom
            x: Theme.margin - 8
            width: parent.width - 2 * x
            bottomMargin: Theme.margin + Theme.cornerInset
            clip: true
            model: root.chapters
            boundsBehavior: Flickable.StopAtBounds
            onVisibleChanged: if (visible) positionLater.restart()
            onCountChanged: if (visible) positionLater.restart()

            Timer {
                id: positionLater
                interval: 0
                onTriggered: if (root.currentChapter >= 0) chapterList.positionViewAtIndex(root.currentChapter, ListView.Center)
            }

            delegate: Rectangle {
                readonly property bool cur: index === root.currentChapter && (root.bookPos > 0 || root.isCurrent)
                readonly property color fg: cur ? root.pal.acc : "white"
                width: chapterList.width
                height: 76
                radius: 16
                color: cur ? Theme.fill : "transparent"
                Text {
                    id: num
                    x: 16
                    width: 40
                    anchors.verticalCenter: parent.verticalCenter
                    text: index + 1
                    color: parent.fg
                    opacity: 0.75
                    font.pixelSize: Theme.textSmall
                    font.features: { "tnum": 1 }
                }
                Text {
                    anchors.left: num.right
                    anchors.right: durText.left
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.title
                    color: parent.fg
                    font.pixelSize: Theme.textBody
                    elide: Text.ElideRight
                }
                Text {
                    id: durText
                    anchors.right: parent.right
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: Theme.fmt(modelData.duration)
                    color: parent.fg
                    opacity: 0.75
                    font.pixelSize: Theme.textSmall
                    font.features: { "tnum": 1 }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: Ui.play(root.bookId, modelData.start)
                }
            }

            Text {
                visible: chapterList.count === 0
                x: 12
                y: 24
                text: qsTr("Loading chapters…")
                color: Theme.dim
                font.pixelSize: Theme.textBody
            }
        }

        // ------------------------------------------------------------ bookmarks
        Flickable {
            visible: Ui.overlay === "bookmarks"
            anchors.top: header.bottom
            anchors.bottom: parent.bottom
            x: Theme.margin
            width: sheet.inner
            contentHeight: bmCol.implicitHeight + Theme.margin + Theme.cornerInset
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: bmCol
                width: parent.width
                spacing: 8
                readonly property var marks: { Library.revision; return Library.bookmarks(root.bookId) }

                PillButton {
                    visible: root.isCurrent
                    width: parent.width
                    color: root.pal.acc
                    textColor: Theme.dark
                    fontWeight: Font.DemiBold
                    icon: "plus"
                    text: qsTr("Bookmark %1").arg(Theme.fmt(Player.position))
                    onClicked: Library.addBookmark(Player.bookId, Player.position)
                }
                Item { visible: root.isCurrent; width: 1; height: 4 }
                Text {
                    visible: bmCol.marks.length === 0
                    leftPadding: 8
                    topPadding: 20
                    width: parent.width
                    text: qsTr("No bookmarks in this book yet.")
                    color: Theme.dim
                    font.pixelSize: Theme.textBody
                    wrapMode: Text.WordWrap
                }
                Repeater {
                    model: bmCol.marks
                    Rectangle {
                        width: bmCol.width
                        height: 84
                        radius: 16
                        color: Qt.rgba(1, 1, 1, 0.06)
                        MouseArea {
                            anchors.fill: parent
                            onClicked: Ui.play(root.bookId, modelData.time)
                        }
                        Column {
                            x: 18
                            width: parent.width - 18 - Theme.touch - 12
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            Text {
                                text: Theme.fmt(modelData.time)
                                color: root.pal.acc
                                font.pixelSize: Theme.textBody
                                font.weight: Font.DemiBold
                                font.features: { "tnum": 1 }
                            }
                            Text {
                                width: parent.width
                                text: {
                                    const ci = Library.chapterAt(root.bookId, modelData.time)
                                    const title = ci >= 0 && root.chapters[ci] ? root.chapters[ci].title : modelData.title
                                    return title + " · " + Theme.relativeTime(modelData.createdAt)
                                }
                                color: Theme.dim
                                font.pixelSize: Theme.textSmall
                                elide: Text.ElideRight
                            }
                        }
                        CircleButton {
                            anchors.right: parent.right
                            anchors.rightMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            color: "transparent"
                            icon: "close"
                            iconSize: 24
                            iconColor: Qt.rgba(1, 1, 1, 0.75)
                            onClicked: Library.removeBookmark(root.bookId, modelData.time)
                        }
                    }
                }
            }
        }

        // ------------------------------------------------------------ sleep
        Column {
            visible: Ui.overlay === "sleep"
            anchors.top: header.bottom
            x: Theme.margin
            width: sheet.inner
            spacing: 12

            Text {
                width: parent.width
                text: Player.sleepMode === "" ? qsTr("Pauses playback and turns the screen off.")
                      : Player.sleepMode === "min" ? qsTr("Stopping in %1.").arg(Theme.fmt(Player.sleepRemaining))
                      : qsTr("Stopping at the end of this chapter.")
                color: Theme.dim
                font.pixelSize: Theme.textSmall
                wrapMode: Text.WordWrap
            }

            Row {
                spacing: 10
                Repeater {
                    model: [15, 30, 45, 60]
                    Rectangle {
                        readonly property bool on: Player.sleepMode === "min" && Player.sleepMinutes === modelData
                        width: (sheet.inner - 30) / 4
                        height: Theme.touchLarge
                        radius: 18
                        color: on ? root.pal.acc : Qt.rgba(1, 1, 1, 0.08)
                        Column {
                            anchors.centerIn: parent
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData
                                color: parent.parent.on ? Theme.dark : "white"
                                font.pixelSize: 34
                                font.weight: Font.DemiBold
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: qsTr("min")
                                color: parent.parent.on ? Theme.dark : Theme.dim
                                font.pixelSize: Theme.textSmall
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                Player.setSleepMinutes(modelData)
                                Ui.overlay = ""
                            }
                        }
                    }
                }
            }

            Rectangle {
                readonly property bool on: Player.sleepMode === "eoc"
                width: parent.width
                height: 84
                radius: 18
                color: on ? root.pal.acc : Qt.rgba(1, 1, 1, 0.08)
                Text {
                    x: 22
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("End of chapter")
                    color: parent.on ? Theme.dark : "white"
                    font.pixelSize: Theme.textBody
                    font.weight: Font.DemiBold
                }
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 22
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("in %1").arg(Theme.fmt(root.chRemaining))
                    color: parent.on ? Theme.dark : Theme.dim
                    font.pixelSize: Theme.textSmall
                    font.features: { "tnum": 1 }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        Player.setSleepEndOfChapter()
                        Ui.overlay = ""
                    }
                }
            }

            PillButton {
                visible: Player.sleepMode !== ""
                width: parent.width
                text: qsTr("Turn off sleep timer")
                onClicked: Player.cancelSleep()
            }
        }

        // ------------------------------------------------------------ speed
        Column {
            visible: Ui.overlay === "speed"
            anchors.top: header.bottom
            x: Theme.margin
            width: sheet.inner
            spacing: 12

            Text {
                width: parent.width
                text: qsTr("Applies to every book. Pitch stays natural.")
                color: Theme.dim
                font.pixelSize: Theme.textSmall
                wrapMode: Text.WordWrap
            }
            Grid {
                columns: 4
                spacing: 10
                Repeater {
                    model: [0.75, 0.9, 1.0, 1.1, 1.2, 1.25, 1.5, 2.0]
                    Rectangle {
                        readonly property bool on: Math.abs(Player.rate - modelData) < 0.001
                        width: (sheet.inner - 30) / 4
                        height: Theme.touchLarge
                        radius: 18
                        color: on ? root.pal.acc : Qt.rgba(1, 1, 1, 0.08)
                        Text {
                            anchors.centerIn: parent
                            text: Theme.fmtRate(modelData)
                            color: parent.on ? Theme.dark : "white"
                            font.pixelSize: Theme.textTitle
                            font.weight: Font.DemiBold
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: Player.rate = modelData
                        }
                    }
                }
            }
        }
    }
}
