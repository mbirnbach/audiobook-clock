import QtQuick
import Nightstand.Core

// A–Z grid with large tiles: one tap jumps the cover flow (Books) or the group list
// (Series/Authors) to the first entry with that letter. Letters without entries are dimmed.
Rectangle {
    id: root

    readonly property bool books: Ui.tab === "books"
    readonly property var targets: {
        Library.revision
        if (books)
            return Library.bookLetters()
        const map = {}
        const groups = Ui.groups
        for (let i = 0; i < groups.length; ++i) {
            const l = Library.letterOf(groups[i].name)
            if (map[l] === undefined)
                map[l] = i
        }
        return map
    }
    readonly property var keys: {
        const k = "ABCDEFGHIJKLMNOPQRSTUVWXYZ".split("")
        k.push("#")
        k.push(books ? "now" : "")
        return k
    }

    function jump(key) {
        const target = targets[key]
        if (target === undefined)
            return
        if (books)
            Ui.flowJump(target)
        else
            Ui.groupJump(target)
        Ui.letterJump = false
    }

    color: "#121216"

    MouseArea { anchors.fill: parent } // swallow taps

    Item {
        id: header
        width: parent.width
        height: Theme.topBar
        Text {
            x: Theme.margin + 4
            anchors.verticalCenter: parent.verticalCenter
            text: root.books ? qsTr("Jump to title") : Ui.tab === "series" ? qsTr("Jump to series") : qsTr("Jump to author")
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
            onClicked: Ui.letterJump = false
        }
    }

    Grid {
        id: grid
        x: Theme.margin
        y: Theme.topBar
        columns: 7
        spacing: 8
        readonly property real cellW: (root.width - 2 * Theme.margin - 6 * spacing) / 7
        readonly property real cellH: (root.height - Theme.topBar - Theme.margin - 3 * spacing) / 4

        Repeater {
            model: root.keys
            Rectangle {
                readonly property bool enabled: modelData !== "" && root.targets[modelData] !== undefined
                width: grid.cellW
                height: grid.cellH
                radius: 16
                visible: modelData !== ""
                color: modelData === "now" ? Theme.selected : Qt.rgba(1, 1, 1, enabled ? 0.1 : 0.03)
                opacity: enabled ? 1 : 0.35

                Text {
                    anchors.centerIn: parent
                    text: modelData === "now" ? qsTr("Now") : modelData
                    color: "white"
                    font.pixelSize: modelData === "now" ? Theme.textBody : 32
                    font.weight: Font.DemiBold
                }
                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    color: "white"
                    opacity: tap.pressed ? 0.15 : 0
                }
                MouseArea {
                    id: tap
                    anchors.fill: parent
                    enabled: parent.enabled
                    onClicked: root.jump(modelData)
                }
            }
        }
    }
}
