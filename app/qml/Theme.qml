pragma Singleton
import QtQuick
import Nightstand.Core

QtObject {
    readonly property string sans: "Instrument Sans"

    // The panel is ~85 x 50 mm at 800 x 480, so 1 mm ≈ 9.4 px. Sizes below are chosen for
    // reading and tapping half asleep at arm's length.
    readonly property int margin: 24        // edge inset, 2.5 mm
    readonly property int screenRadius: 56  // rounded panel corners, ~6 mm (measured from a product photo); masked in previews
    readonly property int cornerInset: 40   // extra bottom padding so list ends scroll clear of the corners
    readonly property int touch: 72         // minimum touch target, 7.7 mm
    readonly property int touchLarge: 96    // primary controls, 10 mm
    readonly property int topBar: 96

    readonly property int textSmall: 19     // secondary/meta text, ~2 mm
    readonly property int textBody: 22      // labels, list rows, buttons
    readonly property int textTitle: 26     // item titles
    readonly property int textHeading: 32   // screen titles

    readonly property color text: "#ffffff"
    readonly property color dark: "#141414"
    readonly property color dim: Qt.rgba(1, 1, 1, 0.72)
    readonly property color faint: Qt.rgba(1, 1, 1, 0.2)
    readonly property color chip: Qt.rgba(1, 1, 1, 0.1)
    readonly property color fill: Qt.rgba(1, 1, 1, 0.12)
    readonly property color selected: Qt.rgba(1, 1, 1, 0.14)
    readonly property color panel: Qt.rgba(18 / 255, 18 / 255, 22 / 255, 0.96)
    readonly property color error: "#ff9b8a"

    function pad(n) {
        return n < 10 ? "0" + n : String(n)
    }

    // 1:02:03 or 2:03
    function fmt(sec) {
        sec = Math.max(0, Math.floor(sec || 0))
        const h = Math.floor(sec / 3600), m = Math.floor(sec % 3600 / 60), s = sec % 60
        return h ? h + ":" + pad(m) + ":" + pad(s) : m + ":" + pad(s)
    }

    // 6h 22m or 45m
    function fmtDur(sec) {
        sec = Math.max(0, sec || 0)
        const h = Math.floor(sec / 3600), m = Math.round(sec % 3600 / 60)
        return h ? qsTr("%1h %2m", "duration").arg(h).arg(m) : qsTr("%1m", "duration").arg(m)
    }

    // p: Library.progress(id)
    function progress(p) {
        const dur = p.duration || 0, t = p.time || 0
        if (p.finished || (dur > 0 && t / dur >= 0.999))
            return { pct: 100, label: qsTr("Finished") }
        if (t <= 0)
            return { pct: 0, label: qsTr("Not started · %1").arg(fmtDur(dur)) }
        const pct = dur > 0 ? Math.min(100, t / dur * 100) : 0
        return { pct: pct, label: qsTr("%1% · %2 left").arg(Math.round(pct)).arg(fmtDur(dur - t)) }
    }

    // Option labels: "30 s", "5 min"
    function secs(n) {
        return qsTr("%1 s", "seconds").arg(n)
    }
    function mins(n) {
        return qsTr("%1 min", "minutes").arg(n)
    }

    // 1.0×, 1.25×
    function fmtRate(r) {
        return (Math.round(r * 100) % 100 === 0 ? r.toFixed(1) : String(Math.round(r * 100) / 100)) + "×"
    }

    function relativeTime(ms) {
        const d = new Date(ms), now = new Date()
        if (now - d < 60000)
            return qsTr("Just now")
        const day = 86400000
        const start = new Date(now.getFullYear(), now.getMonth(), now.getDate()).getTime()
        const hm = pad(d.getHours()) + ":" + pad(d.getMinutes())
        if (ms >= start)
            return qsTr("Today, %1").arg(hm)
        if (ms >= start - day)
            return qsTr("Yesterday, %1").arg(hm)
        const loc = Qt.locale(Settings.language === "de" ? "de_DE" : "en_GB")
        if (ms >= start - 6 * day)
            return loc.toString(d, "ddd") + ", " + hm
        return loc.toString(d, "d MMM")
    }
}
