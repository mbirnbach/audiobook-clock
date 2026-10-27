import QtQuick
import Nightstand.Core

// Cover Flow carousel. Only a window of 11 delegates around the focus exists, so library size
// doesn't matter. Geometry follows the mockup's CSS (perspective 1000px, 55° side covers).
Item {
    id: flow

    // Flat cards in reduced-motion mode: the projective tilt is the most expensive part.
    property real angle: Settings.reducedMotion ? 0 : 55
    property real coverSize: 256
    // Mockup geometry is for 256 px covers; scale offsets with the cover.
    readonly property real k: coverSize / 256
    readonly property int count: Library.books.count
    readonly property int window: 5
    property bool dragging: false

    signal openBook(string id)

    clip: true

    function snapTo(target) {
        snap.to = Math.max(0, Math.min(count - 1, target))
        snap.restart()
    }

    NumberAnimation {
        id: snap
        target: Ui
        property: "flowFocus"
        duration: Settings.reducedMotion ? 150 : 450
        easing.type: Easing.OutQuint
    }

    // Hardware/keyboard steps animate like taps.
    Connections {
        target: Ui
        function onFlowStep(d) { flow.snapTo(Math.round(Ui.flowFocus) + d) }
        function onFlowJump(row) {
            // Long jumps start a few covers away so the animation stays short and readable.
            if (Math.abs(row - Ui.flowFocus) > 4)
                Ui.flowFocus = row + (row > Ui.flowFocus ? -3 : 3)
            flow.snapTo(row)
        }
    }

    Repeater {
        id: cards
        model: flow.window * 2 + 1

        delegate: Item {
            id: card

            readonly property int idx: Math.round(Ui.flowFocus) - flow.window + index
            readonly property bool valid: idx >= 0 && idx < flow.count
            readonly property string bookId: {
                Library.revision
                return valid ? Library.books.idAt(idx) : ""
            }
            readonly property var info: {
                Library.revision
                return bookId ? Library.book(bookId) : ({})
            }
            readonly property real d: idx - Ui.flowFocus
            readonly property real a: Math.abs(d)
            readonly property real sg: d < 0 ? -1 : (d > 0 ? 1 : 0)
            readonly property real tx: (a < 1 ? d * 176 : sg * (176 + (a - 1) * 62)) * flow.k
            readonly property real tz: (a < 1 ? -a * 170 : -170 - (a - 1) * 24) * flow.k
            readonly property real persp: 1000 / (1000 - tz)

            visible: valid && a <= 4.6
            width: flow.coverSize
            height: flow.coverSize
            x: flow.width / 2 - flow.coverSize / 2 + tx * persp
            y: (flow.height - flow.coverSize) / 2
            z: 100 - Math.round(a * 10)
            scale: persp

            transform: Rotation {
                origin.x: flow.coverSize / 2
                origin.y: flow.coverSize / 2
                axis { x: 0; y: 1; z: 0 }
                angle: card.a < 1 ? -card.d * flow.angle : -card.sg * flow.angle
            }

            Cover {
                anchors.fill: parent
                bookId: card.bookId
                title: card.info.title || ""
                author: card.info.author || ""
                hasCover: !!card.info.hasCover
                decodeSize: flow.coverSize
            }
            Rectangle {
                anchors.fill: parent
                color: "black"
                opacity: Math.min(0.6, card.a * 0.32)
            }
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent

        property real startX: 0
        property real startFocus: 0
        property bool moved: false

        onPressed: mouse => {
            snap.stop()
            startX = mouse.x
            startFocus = Ui.flowFocus
            moved = false
        }
        onPositionChanged: mouse => {
            const dx = mouse.x - startX
            if (Math.abs(dx) > 6 && !moved) {
                moved = true
                flow.dragging = true
            }
            if (moved)
                Ui.flowFocus = Math.max(-0.4, Math.min(flow.count - 0.6, startFocus - dx / 150))
        }
        onReleased: mouse => {
            flow.dragging = false
            if (moved) {
                flow.snapTo(Math.round(Ui.flowFocus))
                return
            }
            // Tap: the frontmost card under the finger.
            let hit = null
            for (let i = 0; i < cards.count; ++i) {
                const c = cards.itemAt(i)
                if (!c || !c.visible)
                    continue
                const p = c.mapFromItem(area, mouse.x, mouse.y)
                if (c.contains(p) && (!hit || c.z > hit.z))
                    hit = c
            }
            if (!hit)
                return
            if (hit.idx === Math.round(Ui.flowFocus))
                flow.openBook(hit.bookId)
            else
                flow.snapTo(hit.idx)
        }
        onCanceled: {
            flow.dragging = false
            flow.snapTo(Math.round(Ui.flowFocus))
        }

        property real wheelAcc: 0
        onWheel: wheel => {
            const delta = Math.abs(wheel.angleDelta.x) > Math.abs(wheel.angleDelta.y) ? -wheel.angleDelta.x
                                                                                        : -wheel.angleDelta.y
            wheelAcc += delta
            if (Math.abs(wheelAcc) >= 60) {
                const s = wheelAcc > 0 ? 1 : -1
                wheelAcc = 0
                flow.snapTo(Math.round(Ui.flowFocus) + s)
            }
        }
    }
}
