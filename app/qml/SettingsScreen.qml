import QtQuick
import Nightstand.Core

Item {
    id: root

    // Sections as tabs along the top so the content gets the full width.
    Item {
        width: parent.width
        height: Theme.topBar

        CircleButton {
            id: back
            x: Theme.margin
            anchors.verticalCenter: parent.verticalCenter
            icon: "back"
            onClicked: Ui.go("library")
        }

        Rectangle {
            anchors.left: back.right
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            width: tabs.implicitWidth + 8
            height: Theme.touch + 8
            radius: height / 2
            color: Qt.rgba(0, 0, 0, 0.25)

            Row {
                id: tabs
                x: 4
                anchors.verticalCenter: parent.verticalCenter
                spacing: 0
                Repeater {
                    model: [{ key: "account", label: qsTr("Account") }, { key: "wifi", label: qsTr("Wi-Fi") },
                            { key: "display", label: qsTr("Display") }, { key: "playback", label: qsTr("Playback") },
                            { key: "about", label: qsTr("About") }]
                    PillButton {
                        text: modelData.label
                        padLeft: 17
                        padRight: 17
                        color: Ui.settingsSec === modelData.key ? "white" : "transparent"
                        textColor: Ui.settingsSec === modelData.key ? Theme.dark : "white"
                        onClicked: Ui.settingsSec = modelData.key
                    }
                }
            }
        }

        ClockLabel {
            anchors.right: parent.right
            anchors.rightMargin: Theme.margin
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Flickable {
        id: pane
        x: Theme.margin + 8
        y: Theme.topBar + 8
        width: parent.width - 2 * x
        height: parent.height - y
        contentHeight: section.implicitHeight + Theme.margin + Theme.cornerInset
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Loader {
            id: section
            width: pane.width
            sourceComponent: ({ account: accountSec, wifi: wifiSec, display: displaySec,
                                playback: playbackSec, about: aboutSec })[Ui.settingsSec] || displaySec
        }
    }

    component Field: Column {
        property string key
        property string value
        spacing: 2
        Text { text: parent.key; color: Theme.dim; font.pixelSize: Theme.textSmall }
        Text { text: parent.value; color: "white"; font.pixelSize: Theme.textBody }
    }

    Component {
        id: accountSec
        Column {
            spacing: 24
            Row {
                spacing: 16
                Rectangle {
                    width: 64
                    height: 64
                    radius: 32
                    color: Qt.rgba(1, 1, 1, 0.14)
                    Text {
                        anchors.centerIn: parent
                        text: (Abs.username || "?").charAt(0).toUpperCase()
                        color: "white"
                        font.pixelSize: Theme.textTitle
                        font.weight: Font.DemiBold
                    }
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text {
                        text: Abs.username
                        color: "white"
                        font.pixelSize: Theme.textBody
                        font.weight: Font.DemiBold
                    }
                    Row {
                        spacing: 8
                        Rectangle {
                            width: 10
                            height: 10
                            radius: 5
                            anchors.verticalCenter: parent.verticalCenter
                            color: Abs.online ? "#5fd08a" : "#ff8a7a"
                        }
                        Text {
                            text: (Abs.demo ? qsTr("Demo mode") : Abs.online ? qsTr("Connected to %1") : qsTr("Can't reach %1"))
                                  .arg(Abs.serverHost)
                            color: Theme.dim
                            font.pixelSize: Theme.textSmall
                        }
                    }
                }
            }
            SegmentedRow {
                visible: Library.libraries.length > 1
                width: pane.width
                label: qsTr("Library on this clock")
                options: Library.libraries.map(l => ({ label: l.name, value: l.id }))
                current: Library.libraryId
                onPicked: v => {
                    Ui.flowFocus = 0
                    Library.selectLibrary(v)
                }
            }
            Text {
                visible: Library.libraries.length <= 1
                text: qsTr("Library: %1 · %2 books").arg(Library.libraryName).arg(Library.books.count)
                color: Theme.dim
                font.pixelSize: Theme.textSmall
            }
            Row {
                spacing: 10
                PillButton {
                    icon: "phone"
                    text: qsTr("Pair with phone")
                    onClicked: {
                        Ui.setupStep = "pair"
                        Ui.go("setup")
                    }
                }
                PillButton {
                    text: qsTr("Enter manually")
                    onClicked: {
                        Ui.setupStep = "server"
                        Ui.go("setup")
                    }
                }
                PillButton {
                    text: qsTr("Sign out")
                    onClicked: {
                        Player.stop()
                        Abs.signOut()
                        Library.clear()
                        Ui.setupStep = "pair"
                        Ui.go("setup")
                    }
                }
            }
        }
    }

    Component {
        id: wifiSec
        Column {
            spacing: 10
            Text {
                visible: Device.wifiState === "connecting" || Device.wifiState === "failed"
                width: pane.width
                text: Device.wifiMessage
                color: Device.wifiState === "failed" ? Theme.error : Theme.dim
                font.pixelSize: Theme.textSmall
                wrapMode: Text.WordWrap
            }
            WifiList {
                width: pane.width
                height: Math.max(contentHeight, 80)
                interactive: false
                onPicked: (ssid, secure, saved) => {
                    if (saved)
                        Device.selectWifi(ssid)
                    else if (!secure)
                        Device.connectWifi(ssid, "")
                    else
                        Ui.wifiJoinSsid = ssid
                }
            }
        }
    }

    Component {
        id: displaySec
        Column {
            spacing: 22
            // Each language is labelled in itself, so it can be found in either.
            SegmentedRow {
                width: pane.width
                label: qsTr("Language")
                options: [{ label: "English", value: "en" }, { label: "Deutsch", value: "de" }]
                current: Settings.language
                onPicked: v => Settings.language = v
            }
            SegmentedRow {
                width: pane.width
                label: qsTr("Standby clock after")
                options: [{ label: Theme.secs(30), value: 30 }, { label: Theme.mins(1), value: 60 },
                          { label: Theme.mins(5), value: 300 }, { label: qsTr("Never"), value: 0 }]
                current: Settings.standbySecs
                onPicked: v => Settings.standbySecs = v
            }
            SegmentedRow {
                width: pane.width
                label: qsTr("Screen off after standby")
                options: [{ label: Theme.mins(5), value: 300 }, { label: Theme.mins(15), value: 900 },
                          { label: Theme.mins(30), value: 1800 }, { label: qsTr("Never"), value: 0 }]
                current: Settings.screenOffSecs
                onPicked: v => Settings.screenOffSecs = v
            }
            SegmentedRow {
                width: pane.width
                label: qsTr("Tap the dark screen to see the time")
                options: [{ label: qsTr("On"), value: true }, { label: qsTr("Off"), value: false }]
                current: Settings.tapToWake
                onPicked: v => Settings.tapToWake = v
            }
            SegmentedRow {
                visible: Device.proximityAvailable
                width: pane.width
                label: qsTr("Show the time when a hand comes near")
                options: [{ label: qsTr("On"), value: true }, { label: qsTr("Off"), value: false }]
                current: Settings.wakeOnApproach
                onPicked: v => Settings.wakeOnApproach = v
            }
            SegmentedRow {
                width: pane.width
                label: qsTr("Mute button")
                options: [{ label: qsTr("Screen on/off"), value: "screen" }, { label: qsTr("Play/Pause"), value: "play" }]
                current: Settings.muteAction
                onPicked: v => Settings.muteAction = v
            }
            SegmentedRow {
                visible: Device.lightSensorAvailable
                width: pane.width
                label: qsTr("Screen brightness")
                options: [{ label: qsTr("Automatic"), value: true }, { label: qsTr("Full"), value: false }]
                current: Settings.autoBrightness
                onPicked: v => Settings.autoBrightness = v
            }
            SegmentedRow {
                width: pane.width
                label: qsTr("Night mode: dimmer screen, warmer clock")
                options: [{ label: qsTr("Off"), value: "off" }]
                         .concat(Device.lightSensorAvailable ? [{ label: qsTr("When dark"), value: "sensor" }] : [])
                         .concat([{ label: qsTr("Scheduled"), value: "auto" }, { label: qsTr("Always"), value: "on" }])
                current: Settings.nightMode
                onPicked: v => Settings.nightMode = v
            }
            SegmentedRow {
                visible: Settings.nightMode === "auto" || (Settings.nightMode === "sensor" && !Device.lightSensorAvailable)
                width: pane.width
                label: qsTr("Night starts")
                options: [21, 22, 23, 0].map(h => ({ label: h + ":00", value: h }))
                current: Settings.nightStart
                onPicked: v => Settings.nightStart = v
            }
            SegmentedRow {
                visible: Settings.nightMode === "auto" || (Settings.nightMode === "sensor" && !Device.lightSensorAvailable)
                width: pane.width
                label: qsTr("Night ends")
                options: [6, 7, 8, 9].map(h => ({ label: h + ":00", value: h }))
                current: Settings.nightEnd
                onPicked: v => Settings.nightEnd = v
            }
            SegmentedRow {
                width: pane.width
                label: qsTr("Animations")
                options: [{ label: qsTr("Full"), value: false }, { label: qsTr("Reduced"), value: true }]
                current: Settings.reducedMotion
                onPicked: v => Settings.reducedMotion = v
            }
            SegmentedRow {
                width: pane.width
                label: qsTr("Clock")
                options: [{ label: "24 h", value: true }, { label: "12 h", value: false }]
                current: Settings.clock24
                onPicked: v => Settings.clock24 = v
            }
        }
    }

    Component {
        id: playbackSec
        Column {
            spacing: 22
            SegmentedRow {
                width: pane.width
                label: qsTr("Skip interval")
                options: [10, 15, 30, 60].map(s => ({ label: Theme.secs(s), value: s }))
                current: Settings.skipSecs
                onPicked: v => Settings.skipSecs = v
            }
            SegmentedRow {
                width: pane.width
                label: qsTr("Rewind on resume")
                options: [{ label: qsTr("Off"), value: 0 }, { label: Theme.secs(5), value: 5 },
                          { label: Theme.secs(10), value: 10 }, { label: Theme.secs(30), value: 30 }]
                current: Settings.rewindSecs
                onPicked: v => Settings.rewindSecs = v
            }
            SegmentedRow {
                width: pane.width
                label: qsTr("Rewind after the sleep timer")
                options: [{ label: qsTr("Off"), value: 0 }, { label: Theme.secs(30), value: 30 },
                          { label: Theme.mins(1), value: 60 }, { label: Theme.mins(2), value: 120 }]
                current: Settings.sleepRewindSecs
                onPicked: v => Settings.sleepRewindSecs = v
            }
            SegmentedRow {
                width: pane.width
                label: qsTr("Hold a volume key to skip")
                options: [{ label: qsTr("Off"), value: false }, { label: qsTr("On"), value: true }]
                current: Settings.holdToSkip
                onPicked: v => Settings.holdToSkip = v
            }
        }
    }

    Component {
        id: aboutSec
        Column {
            spacing: 16
            Field { key: qsTr("Device"); value: "Xiaomi Mi Smart Clock (X04G)" }
            Field { key: qsTr("Server"); value: Abs.serverHost + (Abs.serverVersion ? " · v" + Abs.serverVersion : "") }
            Field { key: qsTr("Wi-Fi"); value: Device.wifiSsid || qsTr("Not connected") }
            Field { key: qsTr("App version"); value: Qt.application.version }
        }
    }

    WifiJoin {
        anchors.fill: parent
        z: 10
        visible: Ui.wifiJoinSsid !== ""
        ssid: Ui.wifiJoinSsid
        onDone: connected => Ui.wifiJoinSsid = ""
    }
}
