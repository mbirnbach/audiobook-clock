import QtQuick
import Nightstand.Core

// Book cover from the server, or a generated typographic cover (as in the mockup) when
// the book has none or it hasn't loaded yet.
Item {
    id: root

    property string bookId
    property string title
    property string author
    property bool hasCover: false
    property int decodeSize: 320
    property bool round: false

    readonly property var pal: {
        Palette.revision
        return Palette.forBook(bookId)
    }

    Rectangle {
        anchors.fill: parent
        visible: img.status !== Image.Ready
        radius: root.round ? width / 2 : 0
        gradient: Gradient {
            GradientStop { position: 0; color: root.pal.coverTop }
            GradientStop { position: 1; color: root.pal.coverBottom }
        }

        Item {
            anchors.fill: parent
            anchors.margins: root.width * 0.09
            visible: root.width >= 90 && !root.round

            Text {
                width: parent.width
                text: root.author.toUpperCase()
                color: root.pal.ink
                opacity: 0.86
                font.pixelSize: Math.max(6, root.width * 0.054)
                font.weight: Font.DemiBold
                font.letterSpacing: Math.max(6, root.width * 0.054) * 0.14
                elide: Text.ElideRight
            }

            Column {
                anchors.bottom: parent.bottom
                width: parent.width
                spacing: root.width * 0.04

                Rectangle {
                    width: root.width * 0.14
                    height: Math.max(1, root.width * 0.009)
                    color: root.pal.ink
                    opacity: 0.7
                }
                Text {
                    width: parent.width
                    text: root.title
                    color: root.pal.ink
                    font.pixelSize: root.width * 0.1
                    font.weight: Font.DemiBold
                    lineHeightMode: Text.FixedHeight
                    lineHeight: font.pixelSize
                    wrapMode: Text.WordWrap
                    maximumLineCount: 4
                    elide: Text.ElideRight
                }
            }
        }
    }

    Image {
        id: img
        anchors.fill: parent
        source: root.hasCover && root.bookId ? "image://cover/" + root.bookId + (root.round ? "/round" : "") : ""
        sourceSize: Qt.size(root.decodeSize, root.decodeSize)
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        smooth: true
        visible: status === Image.Ready
    }

    Rectangle {
        anchors.fill: parent
        radius: root.round ? width / 2 : 0
        color: "transparent"
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.08)
    }
}
