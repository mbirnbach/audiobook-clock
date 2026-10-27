import QtQuick
import Nightstand.Core

// Cheap soft drop shadow: a few stacked translucent rounded rects instead of a blur.
// Place it before (under) the item it belongs to, with the same geometry.
Item {
    id: root

    property real blur: 40
    property real dy: 22
    property real strength: 0.4
    readonly property int steps: 5

    visible: !Settings.reducedMotion

    Repeater {
        model: root.steps
        Rectangle {
            readonly property real grow: (index + 1) / root.steps * root.blur * 0.6
            x: -grow
            y: root.dy * 0.6 - grow
            width: root.width + grow * 2
            height: root.height + grow * 2
            radius: 8 + grow
            color: Qt.rgba(0, 0, 0, root.strength / root.steps)
        }
    }
}
