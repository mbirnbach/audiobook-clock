import QtQuick
import Nightstand.Core

// First run / re-pairing. Steps (Ui.setupStep):
//   wifi   – join a network (on screen, or from a phone via the setup hotspot)
//   pair   – show code + QR; the phone page signs the clock in
//   server, login – manual fallback with the on-screen keyboard
Item {
    id: root

    property string url: Abs.serverUrl || "http://"
    property string user: Abs.username
    property string pass: ""
    property string field: "" // url | user | pass, "" = keyboard closed
    property string error: ""
    property bool showedWifi: false
    Component.onCompleted: showedWifi = Ui.setupStep === "wifi"

    readonly property bool kbOpen: field !== ""
    readonly property string step: Ui.setupStep

    Binding {
        target: Device
        property: "textEntry"
        value: root.kbOpen
    }
    Binding {
        target: Pairing
        property: "active"
        value: root.step === "pair" || Device.accessPointActive
    }

    function type(k) {
        const v = root[field]
        if (k === "Done")
            field = ""
        else if (k === "⌫")
            root[field] = v.slice(0, -1)
        else if (k === "space")
            root[field] = v + " "
        else
            root[field] = v + k
    }

    function submit() {
        field = ""
        error = ""
        if (step === "server")
            Abs.checkServer(url)
        else if (step === "login")
            Abs.login(user, pass)
    }

    function pickNetwork(ssid, secure, saved) {
        if (saved)
            Device.selectWifi(ssid)
        else if (!secure)
            Device.connectWifi(ssid, "")
        else
            Ui.wifiJoinSsid = ssid
    }

    Connections {
        target: Abs
        function onServerCheckFinished(ok, message) {
            if (root.step !== "server")
                return
            if (ok) {
                root.url = Abs.serverUrl
                Ui.setupStep = "login"
            } else {
                root.error = message
            }
        }
        function onLoginFinished(ok, message) {
            if (ok) {
                root.pass = ""
                Player.stop()
                Library.clear()
                Library.refresh()
                Ui.go("library")
            } else if (root.step === "login") {
                root.error = message
            }
        }
    }

    Connections {
        target: Device
        function onWifiStateChanged() {
            if (Device.wifiState === "connected" && root.step === "wifi") {
                Ui.wifiJoinSsid = ""
                Ui.setupStep = "pair"
            }
        }
    }

    // Typing with a real keyboard (desktop development).
    focus: true
    Keys.onPressed: event => {
        if (!kbOpen)
            return
        if (event.key === Qt.Key_Backspace)
            type("⌫")
        else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
            submit()
        else if (event.key === Qt.Key_Tab)
            field = field === "user" ? "pass" : field === "pass" ? "user" : field
        else if (event.text.length === 1 && event.text >= " ")
            root[field] = root[field] + event.text
        else
            return
        event.accepted = true
    }

    component StepHeader: Column {
        property string label
        property string title
        property string subtitle
        spacing: 8
        Text {
            visible: parent.label !== ""
            text: parent.label
            color: Theme.dim
            font.pixelSize: Theme.textSmall
            font.weight: Font.DemiBold
            font.letterSpacing: 2
        }
        Text {
            width: parent.width
            text: parent.title
            color: "white"
            font.pixelSize: Theme.textHeading
            font.weight: Font.DemiBold
            font.letterSpacing: -0.5
            wrapMode: Text.WordWrap
        }
        Text {
            width: parent.width
            text: parent.subtitle
            color: Theme.dim
            font.pixelSize: Theme.textBody
            wrapMode: Text.WordWrap
        }
    }

    // ------------------------------------------------------------------ wifi
    Item {
        anchors.fill: parent
        visible: root.step === "wifi"

        StepHeader {
            x: Theme.margin + 8
            width: 310
            y: Theme.topBar - 24
            label: qsTr("STEP 1 OF 2")
            title: qsTr("Connect to Wi-Fi")
            subtitle: Device.accessPointActive ? "" : qsTr("Choose the network the clock should use.")
        }

        PillButton {
            visible: Device.accessPointAvailable
            x: Theme.margin
            y: parent.height - Theme.margin - height
            icon: Device.accessPointActive ? "wifi" : "phone"
            text: Device.accessPointActive ? qsTr("Pick a network here") : qsTr("Use my phone instead")
            onClicked: Device.accessPointActive ? Device.stopAccessPoint() : Device.startAccessPoint()
        }

        Text {
            visible: !Device.accessPointActive && Device.wifiState === "failed"
            x: Theme.margin + 8
            y: 260
            width: 310
            text: Device.wifiMessage
            color: Theme.error
            font.pixelSize: Theme.textSmall
            wrapMode: Text.WordWrap
        }

        WifiList {
            visible: !Device.accessPointActive
            x: 360
            // Below the Cancel button when re-running setup from settings.
            y: Abs.signedIn ? Theme.topBar : Theme.margin
            width: parent.width - x - Theme.margin
            height: parent.height - y - Theme.margin
            bottomMargin: Theme.cornerInset
            onPicked: (ssid, secure, saved) => root.pickNetwork(ssid, secure, saved)
        }

        // Phone path: join the clock's hotspot, then open the setup page.
        Row {
            visible: Device.accessPointActive
            x: 360
            anchors.verticalCenter: parent.verticalCenter
            spacing: 24
            Repeater {
                model: [
                    { qr: "WIFI:S:" + Device.accessPointSsid + ";T:nopass;;",
                      caption: qsTr("1. Join <b>%1</b>").arg(Device.accessPointSsid) },
                    { qr: Pairing.url,
                      caption: qsTr("2. Open <b>%1</b><br>Code %2").arg(Pairing.displayUrl).arg(Pairing.code) }
                ]
                Column {
                    spacing: 12
                    QrCode {
                        width: 184
                        height: 184
                        text: modelData.qr
                    }
                    Text {
                        width: 184
                        text: modelData.caption
                        textFormat: Text.StyledText
                        color: Theme.dim
                        font.pixelSize: Theme.textSmall
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }

    // ------------------------------------------------------------------ pair
    PairingView {
        anchors.fill: parent
        visible: root.step === "pair"
        stepLabel: root.showedWifi ? qsTr("STEP 2 OF 2") : ""
        onManual: {
            root.error = ""
            Ui.setupStep = "server"
        }
    }

    PillButton {
        visible: Abs.signedIn && (root.step === "pair" || root.step === "wifi")
        anchors.right: parent.right
        anchors.rightMargin: Theme.margin
        y: 12
        text: qsTr("Cancel")
        onClicked: Ui.go("library")
    }

    // ------------------------------------------------------------------ manual
    Item {
        id: content
        visible: root.step === "server" || root.step === "login"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: kb.visible ? kb.top : parent.bottom
        anchors.leftMargin: 48
        anchors.rightMargin: 48

        Column {
            width: parent.width
            anchors.verticalCenter: parent.verticalCenter
            spacing: root.step === "server" ? 22 : 18

            StepHeader {
                visible: !root.kbOpen
                width: parent.width
                title: root.step === "server" ? qsTr("Connect to Audiobookshelf") : qsTr("Sign in")
                subtitle: root.step === "server" ? qsTr("Enter the address of your server, including the port.")
                                                 : Abs.serverHost
            }

            InputField {
                visible: root.step === "server"
                width: parent.width
                label: qsTr("Server address")
                value: root.url
                active: root.field === "url"
                onTapped: { root.error = ""; root.field = "url" }
            }

            // Servers found on the LAN, one tap to use.
            Row {
                visible: root.step === "server" && !root.kbOpen && Pairing.servers.length > 0
                spacing: 8
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Found:")
                    color: Theme.dim
                    font.pixelSize: Theme.textSmall
                }
                Repeater {
                    model: Pairing.servers.slice(0, 3)
                    PillButton {
                        fontSize: Theme.textSmall
                        padLeft: 18
                        padRight: 18
                        text: modelData.host
                        onClicked: root.url = modelData.url
                    }
                }
            }

            Row {
                visible: root.step === "login"
                width: parent.width
                spacing: 14
                InputField {
                    width: (parent.width - 14) / 2
                    label: qsTr("Username")
                    value: root.user
                    active: root.field === "user"
                    onTapped: { root.error = ""; root.field = "user" }
                }
                InputField {
                    width: (parent.width - 14) / 2
                    label: qsTr("Password")
                    value: root.pass
                    masked: true
                    active: root.field === "pass"
                    onTapped: { root.error = ""; root.field = "pass" }
                }
            }

            Row {
                visible: !root.kbOpen
                spacing: 12

                PillButton {
                    text: qsTr("Back")
                    onClicked: {
                        root.error = ""
                        Ui.setupStep = root.step === "login" ? "server" : "pair"
                    }
                }
                PillButton {
                    color: "white"
                    textColor: "#111111"
                    fontWeight: Font.DemiBold
                    padLeft: 32
                    padRight: 32
                    text: Abs.busy ? (root.step === "server" ? qsTr("Connecting…") : qsTr("Signing in…"))
                                   : (root.step === "server" ? qsTr("Continue") : qsTr("Sign in"))
                    onClicked: if (!Abs.busy) root.submit()
                }
                Text {
                    visible: root.error !== ""
                    width: content.width - 330
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.error
                    color: Theme.error
                    font.pixelSize: Theme.textSmall
                    wrapMode: Text.WordWrap
                    leftPadding: 8
                }
            }
        }
    }

    Keyboard {
        id: kb
        visible: root.kbOpen
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        onKey: k => root.type(k)
    }

    WifiJoin {
        anchors.fill: parent
        visible: Ui.wifiJoinSsid !== ""
        ssid: Ui.wifiJoinSsid
        onDone: connected => Ui.wifiJoinSsid = ""
    }
}
