import QtQuick
import Nightstand.Core

Item {
    id: root

    readonly property string bookId: Ui.detailId
    readonly property var info: { Library.revision; return Library.book(bookId) }
    readonly property var progress: { Library.revision; return Library.progress(bookId) }
    readonly property var prog: Theme.progress(progress)
    readonly property var pal: { Palette.revision; return Palette.forBook(bookId) }

    Item {
        width: parent.width
        height: Theme.topBar
        BackPill {
            x: Theme.margin
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Back")
            onClicked: Ui.go(Ui.backTo === "detail" ? "library" : Ui.backTo)
        }
        ClockLabel {
            anchors.right: parent.right
            anchors.rightMargin: Theme.margin
            anchors.verticalCenter: parent.verticalCenter
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
        width: 248
        height: 248
        bookId: root.bookId
        title: root.info.title || ""
        author: root.info.author || ""
        hasCover: !!root.info.hasCover
    }

    Column {
        id: infoCol
        x: cover.x + cover.width + 28
        y: Theme.topBar + 4
        width: parent.width - x - Theme.margin
        spacing: 4

        Text {
            visible: !!root.info.seriesLabel
            width: parent.width
            text: (root.info.seriesLabel || "").toUpperCase()
            color: root.pal.acc
            font.pixelSize: Theme.textSmall
            font.weight: Font.DemiBold
            font.letterSpacing: 1.5
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            text: root.info.title || ""
            color: "white"
            font.pixelSize: 28
            font.weight: Font.DemiBold
            font.letterSpacing: -0.4
            lineHeightMode: Text.FixedHeight
            lineHeight: 32
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            text: root.info.author || ""
            color: "white"
            font.pixelSize: Theme.textBody
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            text: {
                // Narrator last: long cast lists get elided, not the length.
                const parts = [Theme.fmtDur(root.info.duration)]
                if (root.info.numChapters)
                    parts.push(qsTr("%1 chapters").arg(root.info.numChapters))
                if (root.info.narrator)
                    parts.push(qsTr("Read by %1").arg(root.info.narrator))
                return parts.join(" · ")
            }
            color: Theme.dim
            font.pixelSize: Theme.textSmall
            elide: Text.ElideRight
        }
        Item {
            width: parent.width
            height: 34
            Rectangle {
                width: parent.width - progLabel.width - 14
                height: 6
                radius: 3
                anchors.verticalCenter: progLabel.verticalCenter
                color: Theme.faint
                Rectangle {
                    width: parent.width * root.prog.pct / 100
                    height: parent.height
                    radius: 3
                    color: root.pal.acc
                }
            }
            Text {
                id: progLabel
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                text: root.prog.label
                color: Qt.rgba(1, 1, 1, 0.85)
                font.pixelSize: Theme.textSmall
            }
        }
    }

    // Description takes whatever height is left above the buttons.
    Text {
        x: infoCol.x
        y: infoCol.y + infoCol.height + 10
        width: infoCol.width
        height: buttons.y - y - 14
        text: root.info.description || ""
        color: Qt.rgba(1, 1, 1, 0.8)
        font.pixelSize: Theme.textSmall
        lineHeightMode: Text.FixedHeight
        lineHeight: 25
        wrapMode: Text.WordWrap
        elide: Text.ElideRight
        clip: true
    }

    Row {
        id: buttons
        x: infoCol.x
        y: parent.height - Theme.margin - Theme.touch
        spacing: 10

        PillButton {
            padLeft: 22
            padRight: 28
            color: root.pal.acc
            textColor: Theme.dark
            fontWeight: Font.DemiBold
            icon: "play"
            iconSize: 28
            text: root.prog.pct >= 100 ? qsTr("Play again") : (root.progress.time > 0 ? qsTr("Resume") : qsTr("Play"))
            onClicked: Ui.play(root.bookId, root.prog.pct >= 100 ? 0 : -1)
        }
        PillButton {
            icon: "list"
            text: qsTr("Chapters")
            onClicked: Ui.openOverlay("chapters", root.bookId)
        }
        CircleButton {
            color: Theme.fill
            icon: "bookmark"
            iconSize: 28
            onClicked: Ui.openOverlay("bookmarks", root.bookId)
        }
    }
}
