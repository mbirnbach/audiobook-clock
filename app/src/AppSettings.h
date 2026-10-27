#pragma once

#include <QLocale>
#include <QObject>
#include <QSettings>

// User preferences, persisted with QSettings. Server/auth state lives in AbsClient.
class AppSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int standbySecs READ standbySecs WRITE setStandbySecs NOTIFY changed)
    Q_PROPERTY(int screenOffSecs READ screenOffSecs WRITE setScreenOffSecs NOTIFY changed)
    Q_PROPERTY(bool tapToWake READ tapToWake WRITE setTapToWake NOTIFY changed)
    Q_PROPERTY(bool wakeOnApproach READ wakeOnApproach WRITE setWakeOnApproach NOTIFY changed)
    Q_PROPERTY(bool autoBrightness READ autoBrightness WRITE setAutoBrightness NOTIFY changed)
    Q_PROPERTY(QString muteAction READ muteAction WRITE setMuteAction NOTIFY changed)
    Q_PROPERTY(int skipSecs READ skipSecs WRITE setSkipSecs NOTIFY changed)
    Q_PROPERTY(int rewindSecs READ rewindSecs WRITE setRewindSecs NOTIFY changed)
    Q_PROPERTY(bool clock24 READ clock24 WRITE setClock24 NOTIFY changed)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY changed)
    Q_PROPERTY(double rate READ rate WRITE setRate NOTIFY changed)
    Q_PROPERTY(int sleepRewindSecs READ sleepRewindSecs WRITE setSleepRewindSecs NOTIFY changed)
    Q_PROPERTY(QString nightMode READ nightMode WRITE setNightMode NOTIFY changed)
    Q_PROPERTY(int nightStart READ nightStart WRITE setNightStart NOTIFY changed)
    Q_PROPERTY(int nightEnd READ nightEnd WRITE setNightEnd NOTIFY changed)
    Q_PROPERTY(bool holdToSkip READ holdToSkip WRITE setHoldToSkip NOTIFY changed)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY changed)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY changed)

public:
    explicit AppSettings(QObject *parent = nullptr);

    // 0 means "never" for the two timeouts.
    int standbySecs() const { return m_s.value("display/standbySecs", 300).toInt(); }
    int screenOffSecs() const { return m_s.value("display/screenOffSecs", 900).toInt(); }
    bool tapToWake() const { return m_s.value("display/tapToWake", true).toBool(); }
    // A hand near the proximity sensor shows the dim clock, like the first tap.
    bool wakeOnApproach() const { return m_s.value("display/wakeOnApproach", true).toBool(); }
    // Backlight follows the light sensor (where there is one) instead of full brightness.
    bool autoBrightness() const { return m_s.value("display/autoBrightness", true).toBool(); }
    // "screen" toggles the screen, "play" toggles playback.
    QString muteAction() const { return m_s.value("display/muteAction", "screen").toString(); }
    int skipSecs() const { return m_s.value("playback/skipSecs", 30).toInt(); }
    int rewindSecs() const { return m_s.value("playback/rewindSecs", 5).toInt(); }
    bool clock24() const { return m_s.value("display/clock24", true).toBool(); }
    int volume() const { return m_s.value("playback/volume", 45).toInt(); }
    double rate() const { return m_s.value("playback/rate", 1.0).toDouble(); }
    // Extra rewind on the first resume after the sleep timer stopped playback.
    int sleepRewindSecs() const { return m_s.value("playback/sleepRewindSecs", 60).toInt(); }
    // "off", "auto" (between nightStart and nightEnd, full hours), "sensor" (dark room,
    // falls back to the schedule without a light sensor) or "on"
    QString nightMode() const { return m_s.value("display/nightMode", "auto").toString(); }
    int nightStart() const { return m_s.value("display/nightStart", 22).toInt(); }
    int nightEnd() const { return m_s.value("display/nightEnd", 7).toInt(); }
    // Holding a volume key skips instead of changing volume.
    bool holdToSkip() const { return m_s.value("playback/holdToSkip", false).toBool(); }
    // No cover tilt, cross-fades or shadows, for slow hardware.
    bool reducedMotion() const { return m_s.value("display/reducedMotion", false).toBool(); }
    // "en" or "de"; defaults to German on a German system.
    QString language() const
    {
        return m_s.value("display/language", QLocale::system().language() == QLocale::German ? "de" : "en").toString();
    }

    void setStandbySecs(int v) { set("display/standbySecs", v); }
    void setScreenOffSecs(int v) { set("display/screenOffSecs", v); }
    void setTapToWake(bool v) { set("display/tapToWake", v); }
    void setWakeOnApproach(bool v) { set("display/wakeOnApproach", v); }
    void setAutoBrightness(bool v) { set("display/autoBrightness", v); }
    void setMuteAction(const QString &v) { set("display/muteAction", v); }
    void setSkipSecs(int v) { set("playback/skipSecs", v); }
    void setRewindSecs(int v) { set("playback/rewindSecs", v); }
    void setClock24(bool v) { set("display/clock24", v); }
    void setVolume(int v) { set("playback/volume", qBound(0, v, 100)); }
    void setRate(double v) { set("playback/rate", v); }
    void setSleepRewindSecs(int v) { set("playback/sleepRewindSecs", v); }
    void setNightMode(const QString &v) { set("display/nightMode", v); }
    void setNightStart(int v) { set("display/nightStart", v); }
    void setNightEnd(int v) { set("display/nightEnd", v); }
    void setHoldToSkip(bool v) { set("playback/holdToSkip", v); }
    void setReducedMotion(bool v) { set("display/reducedMotion", v); }
    void setLanguage(const QString &v) { set("display/language", v); }

signals:
    void changed();

private:
    void set(const QString &key, const QVariant &value);

    QSettings m_s;
};
