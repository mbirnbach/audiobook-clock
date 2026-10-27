pragma Singleton
import QtQuick
import Nightstand.Core

// Navigation and screen state shared by all screens.
QtObject {
    id: ui

    // setup | library | detail | player | clock | settings
    property string screen: "setup"
    property string backTo: "library"
    property string setupStep: "server"

    property string tab: "books" // books | series | authors
    property real flowFocus: 0
    property int groupIdx: 0
    property string detailId: ""
    property string settingsSec: "display"

    property string overlay: "" // chapters | bookmarks | sleep
    property string wifiJoinSsid: "" // network whose password sheet is open
    property bool letterJump: false  // A–Z grid open
    property int fps: -1             // set by --fps

    // Night mode: dimmer backlight and warmer standby clock.
    readonly property bool night: {
        if (Settings.nightMode === "on")
            return true
        if (Settings.nightMode === "sensor" && Device.lightSensorAvailable)
            return Device.dark
        if (Settings.nightMode !== "auto" && Settings.nightMode !== "sensor")
            return false
        const h = now.getHours(), s = Settings.nightStart, e = Settings.nightEnd
        return s > e ? (h >= s || h < e) : (h >= s && h < e)
    }

    // Anti burn-in: the always-on clocks drift by a few pixels, a new spot every minute.
    property point burnShift: Qt.point(0, 0)
    property int burnMinute: -1
    onNowChanged: {
        if (now.getMinutes() === burnMinute)
            return
        burnMinute = now.getMinutes()
        burnShift = Qt.point(Math.round(Math.random() * 24 - 12), Math.round(Math.random() * 16 - 8))
    }
    property string overlayBook: ""

    property bool screenOff: false
    property bool wakeClock: false
    property double clockSince: Date.now()
    property bool osd: false
    property date now: new Date()

    readonly property string clockText: {
        const d = now
        const h = Settings.clock24 ? Theme.pad(d.getHours()) : String(d.getHours() % 12 || 12)
        return h + ":" + Theme.pad(d.getMinutes())
    }
    readonly property string ampm: Settings.clock24 ? "" : (now.getHours() < 12 ? "AM" : "PM")
    readonly property string dateText: Settings.language === "de"
                                       ? Qt.locale("de_DE").toString(now, "dddd, d. MMMM")
                                       : Qt.locale("en_GB").toString(now, "dddd d MMMM")

    readonly property var groups: {
        Library.revision
        return tab === "books" ? [] : Library.groups(tab)
    }

    function go(s) {
        overlay = ""
        if (s === "clock")
            clockSince = Date.now()
        screen = s
    }

    function openDetail(id) {
        Library.ensureDetail(id)
        detailId = id
        backTo = screen
        go("detail")
    }

    function play(id, t) {
        Player.play(id, t === undefined ? -1 : t)
        go("player")
    }

    function openOverlay(kind, id) {
        Library.ensureDetail(id)
        overlayBook = id
        overlay = kind
    }

    signal flowStep(int d)
    signal flowJump(int row)
    signal groupJump(int index)

    function step(d) {
        flowStep(d)
    }

    function wake() {
        Device.poke()
        clockSince = Date.now()
        screenOff = false
        wakeClock = false
    }

    function toggleScreen() {
        if (Settings.muteAction === "play") {
            Player.togglePlay()
            return
        }
        wakeClock = false
        if (screenOff)
            wake()
        else
            screenOff = true
    }

    // Steps land on multiples of d (5, 10, 15, ...) even from an odd starting value.
    function volume(d) {
        const v = Player.volume
        Player.volume = d > 0 ? Math.floor(v / d) * d + d : Math.ceil(v / -d) * -d + d
        osd = true
        osdTimer.restart()
    }

    // Called once per second.
    function idleCheck() {
        now = new Date()
        const idle = Device.idleMs() / 1000
        if (!screenOff && overlay === "" && ["library", "detail", "player", "settings"].indexOf(screen) >= 0
                && Settings.standbySecs > 0 && idle >= Settings.standbySecs)
            go("clock")
        if (!screenOff && screen === "clock" && Settings.screenOffSecs > 0
                && Date.now() - Math.max(clockSince, Device.lastActivityMs()) >= Settings.screenOffSecs * 1000)
            screenOff = true
    }

    property Timer ticker: Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: ui.idleCheck()
    }
    property Timer osdTimer: Timer {
        interval: 1400
        onTriggered: ui.osd = false
    }
    property Timer wakeTimer: Timer {
        interval: 6000
        onTriggered: ui.wakeClock = false
    }

    property Binding nightBinding: Binding {
        target: Device
        property: "nightMode"
        value: ui.night
    }
    property Binding autoBrightnessBinding: Binding {
        target: Device
        property: "autoBrightness"
        value: Settings.autoBrightness
    }
    // The proximity sensor is only read while the screen is dark.
    property Binding proximityBinding: Binding {
        target: Device
        property: "proximityWanted"
        value: ui.screenOff && Settings.wakeOnApproach
    }
    property Connections approachWatch: Connections {
        target: Device
        function onApproached() {
            // Like the first tap: the dim clock for a few seconds; a tap then wakes.
            if (!ui.screenOff)
                return
            ui.wakeClock = true
            ui.wakeTimer.restart()
        }
    }
    property Binding holdBinding: Binding {
        target: Device
        property: "holdToSkip"
        value: Settings.holdToSkip
    }

    property Binding displayMode: Binding {
        target: Device
        property: "displayMode"
        value: !ui.screenOff ? "on" : (ui.wakeClock ? "dim" : "off")
    }
}
