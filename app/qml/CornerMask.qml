import QtQuick
import QtQuick.Shapes

// Blacks out the panel's rounded corners so desktop previews and screenshots show what the
// device will actually cut off. Harmless on the device itself.
Item {
    id: root

    property real radius: Theme.screenRadius

    Repeater {
        model: 4
        Shape {
            width: root.radius
            height: root.radius
            x: index % 2 === 0 ? 0 : root.width - width
            y: index < 2 ? 0 : root.height - height
            // One top-left corner shape, rotated into the other three corners.
            rotation: [0, 90, 270, 180][index]
            preferredRendererType: Shape.SoftwareRenderer
            antialiasing: true

            ShapePath {
                fillColor: "black"
                strokeWidth: -1
                PathSvg {
                    path: "M0 0L%1 0A%1 %1 0 0 0 0 %1Z".arg(root.radius)
                }
            }
        }
    }
}
