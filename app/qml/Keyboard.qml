import QtQuick

// On-screen keyboard: letters (with one-shot shift), numbers/punctuation and symbols,
// enough for server addresses and Wi-Fi passwords.
Rectangle {
    id: root

    property string mode: "abc" // abc | 123 | sym
    property bool shift: false
    signal key(string k)

    readonly property var rows: mode === "abc"
        ? [["q","w","e","r","t","y","u","i","o","p"], ["a","s","d","f","g","h","j","k","l"],
           ["shift","z","x","c","v","b","n","m","bksp"], ["123",".","/",":","space","Done"]]
        : mode === "123"
        ? [["1","2","3","4","5","6","7","8","9","0"], ["-","/",":",";","(",")","$","&","@","\""],
           ["sym",".",",","?","!","'","bksp"], ["abc","space","Done"]]
        : [["[","]","{","}","#","%","^","*","+","="], ["_","\\","|","~","<",">","€","£","¥","•"],
           ["123",".",",","?","!","'","bksp"], ["abc","space","Done"]]
    readonly property var fnKeys: ["bksp", "shift", "123", "abc", "sym", "space", "Done"]

    function flexOf(k) {
        return k === "space" ? 4 : k === "Done" ? 1.8 : (fnKeys.indexOf(k) >= 0) ? 1.5 : 1
    }
    function labelOf(k) {
        if (k === "space" || k === "bksp" || k === "shift")
            return ""
        if (k === "sym")
            return "#+="
        if (k === "Done")
            return qsTr("Done")
        return shift && k.length === 1 ? k.toUpperCase() : k
    }
    function press(k) {
        if (k === "123" || k === "abc" || k === "sym") {
            mode = k
            shift = false
        } else if (k === "shift") {
            shift = !shift
        } else if (k === "bksp") {
            root.key("⌫")
        } else {
            root.key(shift && k.length === 1 ? k.toUpperCase() : k)
            shift = false
        }
    }

    // Extra side/bottom inset keeps the corner keys clear of the rounded panel corners.
    implicitHeight: col.implicitHeight + 10 + 16
    color: Qt.rgba(16 / 255, 16 / 255, 20 / 255, 0.96)

    Rectangle {
        width: parent.width
        height: 1
        color: Qt.rgba(1, 1, 1, 0.08)
    }

    Column {
        id: col
        x: 14
        y: 10
        width: parent.width - 28
        spacing: 6

        Repeater {
            model: root.rows
            Row {
                id: row
                readonly property var keys: modelData
                readonly property real totalFlex: keys.reduce((s, k) => s + root.flexOf(k), 0)
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 6

                Repeater {
                    model: row.keys
                    Rectangle {
                        readonly property string k: modelData
                        readonly property bool isFn: root.fnKeys.indexOf(k) >= 0
                        // Keys fill the row by flex weight, like the mockup's CSS flex.
                        width: (col.width - row.spacing * (row.keys.length - 1)) * root.flexOf(k) / row.totalFlex
                        height: 60
                        radius: 10
                        color: k === "Done" ? "white" : (k === "shift" && root.shift) ? "#4a4a52"
                               : isFn ? "#1d1d21" : "#2e2e34"

                        Text {
                            anchors.centerIn: parent
                            text: root.labelOf(parent.k)
                            color: parent.k === "Done" ? "#141414" : "white"
                            font.pixelSize: text.length > 1 ? 20 : 25
                        }
                        Icon {
                            anchors.centerIn: parent
                            visible: parent.k === "bksp" || parent.k === "shift"
                            name: parent.k === "bksp" ? "backspace" : root.shift ? "shiftOn" : "shift"
                            size: 28
                        }
                        Rectangle {
                            anchors.fill: parent
                            radius: parent.radius
                            color: "white"
                            opacity: keyMouse.pressed ? 0.18 : 0
                        }
                        MouseArea {
                            id: keyMouse
                            anchors.fill: parent
                            onClicked: root.press(parent.k)
                        }
                    }
                }
            }
        }
    }
}
