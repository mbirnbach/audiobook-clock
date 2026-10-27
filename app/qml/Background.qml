import QtQuick
import Nightstand.Core

// Two soft colour glows from the book's palette, cross-faded when the book changes.
Item {
    id: root

    property string bookId
    property real scrim: 0.3

    readonly property string source: {
        Palette.revision
        return Palette.forBook(bookId).bg
    }
    property Image front: a

    onSourceChanged: {
        const next = front === a ? b : a
        next.opacity = 0
        next.source = source
    }

    function reveal(img) {
        if (img === front || img.source.toString() !== root.source)
            return
        const old = front
        img.z = 1
        old.z = 0
        front = img
        fade.target = img
        fade.restart()
    }

    Image {
        id: a
        anchors.fill: parent
        smooth: true
        asynchronous: true
        cache: true
        onStatusChanged: if (status === Image.Ready) root.reveal(a)
    }
    Image {
        id: b
        anchors.fill: parent
        smooth: true
        asynchronous: true
        cache: true
        opacity: 0
        onStatusChanged: if (status === Image.Ready) root.reveal(b)
    }

    NumberAnimation {
        id: fade
        property: "opacity"
        from: 0
        to: 1
        duration: Settings.reducedMotion ? 0 : 800
    }

    Component.onCompleted: {
        a.source = source
        a.opacity = 1
    }

    Rectangle {
        anchors.fill: parent
        z: 2
        color: "black"
        opacity: root.scrim
        Behavior on opacity { NumberAnimation { duration: 600 } }
    }
}
