import QtQuick
import Nightstand.Core

Item {
    id: root

    readonly property string focusedId: {
        Library.revision
        return Library.books.idAt(Math.max(0, Math.min(Library.books.count - 1, Math.round(Ui.flowFocus))))
    }

    // ---------------------------------------------------------------- top bar
    Item {
        id: bar
        width: parent.width
        height: Theme.topBar

        Rectangle {
            x: Theme.margin
            anchors.verticalCenter: parent.verticalCenter
            width: tabs.implicitWidth + 8
            height: Theme.touch + 8
            radius: height / 2
            color: Qt.rgba(0, 0, 0, 0.25)

            Row {
                id: tabs
                x: 4
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Repeater {
                    model: [{ key: "books", label: qsTr("Books") }, { key: "series", label: qsTr("Series") },
                            { key: "authors", label: qsTr("Authors") }]
                    PillButton {
                        text: modelData.label
                        padLeft: 22
                        padRight: 22
                        color: Ui.tab === modelData.key ? "white" : "transparent"
                        textColor: Ui.tab === modelData.key ? Theme.dark : "white"
                        onClicked: {
                            Ui.tab = modelData.key
                            Ui.groupIdx = 0
                        }
                    }
                }
            }
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: Theme.margin
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10

            // Tapping the time opens the standby clock.
            Item {
                width: clock.implicitWidth
                height: Theme.touch
                ClockLabel {
                    id: clock
                    anchors.verticalCenter: parent.verticalCenter
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: Ui.go("clock")
                }
            }

            // Now playing: cover + play state, opens the player.
            Rectangle {
                visible: Player.bookId !== ""
                width: 6 + 60 + 12 + 28 + 20
                height: Theme.touch
                radius: height / 2
                color: Theme.fill
                Cover {
                    x: 6
                    anchors.verticalCenter: parent.verticalCenter
                    width: 60
                    height: 60
                    decodeSize: 120
                    round: true
                    bookId: Player.bookId
                    hasCover: { Library.revision; return !!Library.book(Player.bookId).hasCover }
                }
                Icon {
                    x: 6 + 60 + 12
                    anchors.verticalCenter: parent.verticalCenter
                    name: Player.playing ? "pause" : "play"
                    size: 28
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: Ui.go("player")
                }
            }

            CircleButton {
                icon: "sliders"
                onClicked: Ui.go("settings")
            }
        }
    }

    // ---------------------------------------------------------------- empty / loading
    Column {
        visible: Library.books.count === 0
        anchors.centerIn: parent
        spacing: 20
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Library.loading ? qsTr("Loading your library…")
                                  : (Library.error || qsTr("No books in this library yet."))
            color: Theme.dim
            font.pixelSize: Theme.textBody
        }
        PillButton {
            visible: !Library.loading
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Try again")
            onClicked: Library.refresh()
        }
    }

    // ---------------------------------------------------------------- books (cover flow)
    Item {
        anchors.fill: parent
        visible: Ui.tab === "books" && Library.books.count > 0

        CoverFlow {
            y: Theme.topBar
            width: parent.width
            height: 250
            coverSize: 224
            onOpenBook: id => Ui.openDetail(id)
        }

        Column {
            readonly property var info: { Library.revision; return Library.book(root.focusedId) }
            readonly property var prog: { Library.revision; return Theme.progress(Library.progress(root.focusedId)) }
            readonly property var pal: { Palette.revision; return Palette.forBook(root.focusedId) }
            // Narrower than the screen so it never runs under the A–Z button.
            x: 116
            y: 352
            width: parent.width - 232
            spacing: 4

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: parent.info.title || ""
                color: "white"
                font.pixelSize: Theme.textTitle
                font.weight: Font.DemiBold
                font.letterSpacing: -0.3
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: (parent.info.author || "") + " · " + parent.prog.label
                color: Qt.rgba(1, 1, 1, 0.78)
                font.pixelSize: Theme.textSmall
                elide: Text.ElideRight
            }
            Item { width: 1; height: 8 }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 200
                height: 6
                radius: 3
                color: Theme.faint
                Rectangle {
                    width: parent.width * parent.parent.prog.pct / 100
                    height: parent.height
                    radius: 3
                    color: parent.parent.pal.acc
                }
            }
        }
    }

    // ---------------------------------------------------------------- series / authors
    Item {
        anchors.fill: parent
        anchors.topMargin: Theme.topBar
        visible: Ui.tab !== "books" && Library.books.count > 0

        ListView {
            id: groupList
            x: Theme.margin
            width: 250
            // Ends above the A–Z button.
            height: parent.height - Theme.touch - Theme.margin - 8
            spacing: 4
            clip: true
            bottomMargin: 8
            model: Ui.groups
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                width: groupList.width
                height: Math.max(84, gcol.implicitHeight + 20)
                radius: 18
                color: index === Ui.groupIdx ? Theme.selected : "transparent"
                Column {
                    id: gcol
                    x: 18
                    width: parent.width - 36
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text {
                        width: parent.width
                        text: modelData.name
                        color: "white"
                        font.pixelSize: Theme.textBody
                        font.weight: Font.Medium
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                    Text {
                        text: modelData.count === 1 ? qsTr("1 book") : qsTr("%1 books").arg(modelData.count)
                        color: Theme.dim
                        font.pixelSize: Theme.textSmall
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        Ui.groupIdx = index
                        grid.contentY = grid.originY
                    }
                }
            }
        }

        GridView {
            id: grid
            readonly property var group: Ui.groups[Ui.groupIdx] || Ui.groups[0]
            x: groupList.x + groupList.width + 16
            width: parent.width - x - Theme.margin + 14
            height: parent.height
            clip: true
            cellWidth: width / 3
            cellHeight: cellWidth - 14 + 92
            bottomMargin: Theme.margin
            model: group ? group.books : []
            boundsBehavior: Flickable.StopAtBounds

            delegate: Item {
                width: grid.cellWidth
                height: grid.cellHeight

                Column {
                    width: parent.width - 14
                    spacing: 6

                    Item {
                        width: parent.width
                        height: width
                        Shadow {
                            anchors.fill: parent
                            blur: 20
                            dy: 10
                            strength: 0.3
                        }
                        Cover {
                            anchors.fill: parent
                            bookId: modelData.id
                            title: modelData.title
                            author: modelData.author
                            hasCover: modelData.hasCover
                            decodeSize: 200
                        }
                    }
                    Text {
                        width: parent.width
                        text: modelData.title
                        color: "white"
                        font.pixelSize: Theme.textSmall
                        font.weight: Font.Medium
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        lineHeight: 1.05
                    }
                    Text {
                        text: Ui.tab === "series" && modelData.sequence ? qsTr("Book %1").arg(modelData.sequence)
                                                                         : Theme.fmtDur(modelData.duration)
                        color: Theme.dim
                        font.pixelSize: Theme.textSmall
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: Ui.openDetail(modelData.id)
                }
            }
        }
    }

    Connections {
        target: Ui
        function onGroupJump(index) {
            Ui.groupIdx = index
            groupList.positionViewAtIndex(index, ListView.Beginning)
            grid.contentY = grid.originY
        }
    }

    // A–Z: jump by first letter instead of swiping through hundreds of books.
    PillButton {
        visible: Library.books.count > 0
        x: Theme.margin
        y: parent.height - Theme.margin - height
        width: Theme.touch
        padLeft: 0
        padRight: 0
        fontWeight: Font.DemiBold
        fontSize: 20
        text: "A–Z"
        onClicked: Ui.letterJump = true
    }
}
