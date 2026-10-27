import QtQuick
import QtQuick.Shapes

// 24x24 line icons from the mockup, drawn as paths so no SVG plugin is needed.
Item {
    id: root

    property string name
    property real size: 24
    property color color: "white"
    property real strokeWidth: -1 // -1 = icon default

    readonly property var spec: ({
        moon: { d: "M20 14.5A8 8 0 1 1 9.5 4a6.5 6.5 0 0 0 10.5 10.5z", w: 1.8 },
        sliders: { d: "M4 7h9M17 7h3M4 17h3M11 17h9M17 7a2 2 0 1 1-4 0a2 2 0 1 1 4 0zM11 17a2 2 0 1 1-4 0a2 2 0 1 1 4 0z", w: 1.8 },
        back: { d: "M15 5l-7 7 7 7", w: 2 },
        play: { d: "M8 5.5v13a1 1 0 0 0 1.5.86l10.5-6.5a1 1 0 0 0 0-1.72L9.5 4.64A1 1 0 0 0 8 5.5z", fill: true },
        pause: { d: "M7 5h2a1 1 0 0 1 1 1v12a1 1 0 0 1-1 1h-2a1 1 0 0 1-1-1v-12a1 1 0 0 1 1-1zM15 5h2a1 1 0 0 1 1 1v12a1 1 0 0 1-1 1h-2a1 1 0 0 1-1-1v-12a1 1 0 0 1 1-1z", fill: true },
        list: { d: "M9 6h11M9 12h11M9 18h11M4.5 6h.01M4.5 12h.01M4.5 18h.01", w: 1.8 },
        bookmark: { d: "M6 4h12v17l-6-4-6 4z", w: 1.8 },
        close: { d: "M6 6l12 12M18 6L6 18", w: 2 },
        prev: { d: "M6 5h2v14H6zM19 5.5v13L9.5 12z", fill: true },
        next: { d: "M16 5h2v14h-2zM5 5.5v13l9.5-6.5z", fill: true },
        skipBack: { d: "M4 12a8 8 0 1 0 2.6-5.9M4 3.5v4h4", w: 1.6 },
        skipFwd: { d: "M20 12a8 8 0 1 1-2.6-5.9M20 3.5v4h-4", w: 1.6 },
        check: { d: "M5 12l5 5 9-10", w: 2.2 },
        plus: { d: "M12 5v14M5 12h14", w: 2 },
        backspace: { d: "M21 5H9l-6 7 6 7h12a1 1 0 0 0 1-1V6a1 1 0 0 0-1-1zM12 9l6 6M18 9l-6 6", w: 1.8 },
        shift: { d: "M12 4l8 8h-4v7H8v-7H4z", w: 1.8 },
        shiftOn: { d: "M12 4l8 8h-4v7H8v-7H4z", fill: true },
        wifi: { d: "M2.5 9a14 14 0 0 1 19 0M5.5 12.5a9.5 9.5 0 0 1 13 0M8.6 15.9a5 5 0 0 1 6.8 0M12 19.5h.01", w: 2 },
        lock: { d: "M6 11h12v9H6zM8.5 11V8a3.5 3.5 0 0 1 7 0v3", w: 1.8 },
        phone: { d: "M8 3h8a1 1 0 0 1 1 1v16a1 1 0 0 1-1 1H8a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1zM11 18h2", w: 1.8 }
    })[name] || { d: "", w: 1.8 }

    width: size
    height: size

    Shape {
        width: 24
        height: 24
        scale: root.size / 24
        transformOrigin: Item.TopLeft
        antialiasing: true
        preferredRendererType: Shape.SoftwareRenderer

        ShapePath {
            strokeColor: root.spec.fill ? "transparent" : root.color
            strokeWidth: root.spec.fill ? 0 : (root.strokeWidth > 0 ? root.strokeWidth : root.spec.w)
            fillColor: root.spec.fill ? root.color : "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.spec.d }
        }
    }
}
