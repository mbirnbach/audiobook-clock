import QtQuick
import QtQuick.Window
import Nightstand.Core

Window {
    id: win

    property bool fullScreen: false
    // linuxfb in Qt 6 can't rotate; the clock's panel is portrait (480×800)
    property int stageRotation: 0

    width: 800
    height: 480
    minimumWidth: 800
    minimumHeight: 480
    maximumWidth: fullScreen ? 16384 : 800
    maximumHeight: fullScreen ? 16384 : 480
    visibility: fullScreen ? Window.FullScreen : Window.Windowed
    visible: true
    color: "black"
    title: "Nightstand"

    readonly property string backgroundBook: {
        Library.revision
        switch (Ui.screen) {
        case "library":
            if (Ui.tab === "books")
                return Library.books.idAt(Math.max(0, Math.min(Library.books.count - 1, Math.round(Ui.flowFocus))))
            const g = Ui.groups[Ui.groupIdx]
            return g && g.books.length ? g.books[0].id : Player.bookId
        case "detail":
            return Ui.detailId
        case "player":
        case "clock":
            return Player.bookId
        }
        return ""
    }

    Component.onCompleted: {
        if (Abs.signedIn) {
            Ui.go("library")
            Library.refresh()
        } else {
            Ui.setupStep = Device.wifiAvailable && Device.wifiSsid === "" ? "wifi" : "pair"
            Ui.go("setup")
        }
    }

    Item {
        id: stage
        width: 800
        height: 480
        anchors.centerIn: parent
        rotation: win.stageRotation
        clip: true

        Background {
            anchors.fill: parent
            bookId: win.backgroundBook
            scrim: Ui.screen === "player" ? 0.22 : Ui.screen === "clock" ? 0.78 : 0.3
        }

        Loader {
            anchors.fill: parent
            active: Ui.screen === "setup"
            focus: active
            source: "SetupScreen.qml"
        }
        Loader {
            anchors.fill: parent
            active: Ui.screen === "library"
            source: "LibraryScreen.qml"
        }
        Loader {
            anchors.fill: parent
            active: Ui.screen === "detail"
            source: "DetailScreen.qml"
        }
        Loader {
            anchors.fill: parent
            active: Ui.screen === "player"
            source: "PlayerScreen.qml"
        }
        Loader {
            anchors.fill: parent
            active: Ui.screen === "clock"
            source: "ClockScreen.qml"
        }
        Loader {
            anchors.fill: parent
            active: Ui.screen === "settings"
            source: "SettingsScreen.qml"
        }

        Loader {
            anchors.fill: parent
            z: 50
            active: Ui.overlay !== ""
            source: "SidePanel.qml"
        }

        Loader {
            anchors.fill: parent
            z: 60
            active: Ui.letterJump
            source: "LetterJump.qml"
        }

        // Last 30 s of the sleep timer: one big target to keep listening.
        Rectangle {
            anchors.fill: parent
            z: 90
            visible: Player.sleepFading && !Ui.screenOff
            color: Qt.rgba(0, 0, 0, 0.88)
            MouseArea {
                anchors.fill: parent
                onClicked: Player.extendSleep(5)
            }
            Column {
                anchors.centerIn: parent
                spacing: 20
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Still listening?")
                    color: "white"
                    font.pixelSize: Theme.textHeading
                    font.weight: Font.DemiBold
                }
                PillButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    height: Theme.touchLarge
                    padLeft: 40
                    padRight: 40
                    color: "white"
                    textColor: Theme.dark
                    fontWeight: Font.DemiBold
                    icon: "plus"
                    text: qsTr("5 more minutes")
                    onClicked: Player.extendSleep(5)
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Or tap anywhere")
                    color: Theme.dim
                    font.pixelSize: Theme.textSmall
                }
            }
        }

        VolumeOsd {
            z: 80
            anchors.right: parent.right
            anchors.rightMargin: 22
            anchors.verticalCenter: parent.verticalCenter
            visible: Ui.osd && (!Ui.screenOff || Ui.wakeClock)
        }

        ScreenOff {
            anchors.fill: parent
            z: 100
            visible: Ui.screenOff
        }

        // Night: soften everything a little on top of the dimmed backlight (more on the
        // desktop, which has no backlight to dim).
        Rectangle {
            anchors.fill: parent
            z: 150
            color: "black"
            opacity: Ui.night ? (Device.backlightAvailable ? 0.15 : 0.4) : 0
            Behavior on opacity { NumberAnimation { duration: 1500 } }
        }

        CornerMask {
            anchors.fill: parent
            z: 200
        }

        Text {
            visible: Ui.fps >= 0
            z: 300
            anchors.horizontalCenter: parent.horizontalCenter
            y: 4
            text: Ui.fps + " fps"
            color: "#7CFC00"
            font.pixelSize: Theme.textSmall
            font.weight: Font.Bold
        }
    }

    Connections {
        target: Device
        function onHardwareKey(key) {
            // While the sleep timer fades out, any key means "keep listening".
            if (Player.sleepFading && key !== "mute") {
                Player.extendSleep(5)
                return
            }
            switch (key) {
            case "volumeUp": Ui.volume(5); break
            case "volumeDown": Ui.volume(-5); break
            case "skipForward": Player.skip(Settings.skipSecs); break
            case "skipBack": Player.skip(-Settings.skipSecs); break
            case "mute": Ui.toggleScreen(); break
            case "left":
            case "right":
                if (Ui.screen === "library" && Ui.tab === "books" && !Ui.screenOff)
                    Ui.step(key === "left" ? -1 : 1)
                break
            }
        }
    }

    Connections {
        target: Player
        function onSleepTimerFired() {
            Ui.overlay = ""
            Ui.wakeClock = false
            Ui.screenOff = true
        }
    }

    Connections {
        target: Abs
        function onSessionExpired() {
            Player.stop()
            Ui.setupStep = "pair"
            Ui.go("setup")
        }
    }

    // On first load, show the most recently played book as current and focus it.
    Connections {
        target: Library
        function onRevisionChanged() {
            if (Player.bookId !== "" || Library.books.count === 0)
                return
            const id = Library.mostRecentId()
            if (!id)
                return
            Player.prepare(id)
            Library.ensureDetail(id)
            Ui.flowFocus = Math.max(0, Library.books.indexOf(id))
        }
    }
}
