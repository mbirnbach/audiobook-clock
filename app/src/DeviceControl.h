#pragma once

#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <QVariantList>

class QProcess;
class QWindow;

// Device glue: idle tracking, hardware keys, backlight and Wi-Fi status. Everything that
// touches the OS degrades to a no-op on the desktop.
class DeviceControl : public QObject
{
    Q_OBJECT
    // "on", "dim" (tap-to-wake clock) or "off"
    Q_PROPERTY(QString displayMode READ displayMode WRITE setDisplayMode NOTIFY displayModeChanged)
    // While a text field takes typing, letter/arrow keys are not hardware shortcuts.
    Q_PROPERTY(bool textEntry MEMBER m_textEntry NOTIFY textEntryChanged)
    // Night: the backlight runs at a quarter of the normal level.
    Q_PROPERTY(bool nightMode READ nightMode WRITE setNightMode NOTIFY nightModeChanged)
    Q_PROPERTY(bool backlightAvailable READ backlightAvailable CONSTANT)
    // Ambient light and proximity (the clock's EM3071X); all no-ops without the sensor.
    Q_PROPERTY(bool lightSensorAvailable READ lightSensorAvailable CONSTANT)
    Q_PROPERTY(bool proximityAvailable READ proximityAvailable CONSTANT)
    // Backlight follows the room light (night mode still divides by four).
    Q_PROPERTY(bool autoBrightness READ autoBrightness WRITE setAutoBrightness NOTIFY autoBrightnessChanged)
    // Room is dark (lights off), with hysteresis so a passing hand doesn't flip it.
    Q_PROPERTY(bool dark READ dark NOTIFY darkChanged)
    // Only polled while true: reading the sensor logs a kernel line each time.
    Q_PROPERTY(bool proximityWanted MEMBER m_proximityWanted NOTIFY proximityWantedChanged)
    // Holding a volume key emits skipForward/skipBack instead of repeating volume steps.
    Q_PROPERTY(bool holdToSkip MEMBER m_holdToSkip NOTIFY holdToSkipChanged)
    Q_PROPERTY(bool wifiAvailable READ wifiAvailable NOTIFY wifiChanged)
    Q_PROPERTY(QString wifiSsid READ wifiSsid NOTIFY wifiChanged)
    Q_PROPERTY(QVariantList wifiNetworks READ wifiNetworks NOTIFY wifiChanged)
    // idle | connecting | connected | failed, for the last connectWifi()
    Q_PROPERTY(QString wifiState READ wifiState NOTIFY wifiStateChanged)
    Q_PROPERTY(QString wifiMessage READ wifiMessage NOTIFY wifiStateChanged)
    Q_PROPERTY(bool accessPointAvailable READ accessPointAvailable CONSTANT)
    Q_PROPERTY(bool accessPointActive READ accessPointActive NOTIFY accessPointChanged)
    Q_PROPERTY(QString accessPointSsid READ accessPointSsid CONSTANT)

public:
    explicit DeviceControl(QObject *parent = nullptr);

    void attach(QWindow *window);

    QString displayMode() const { return m_displayMode; }
    void setDisplayMode(const QString &mode);
    bool nightMode() const { return m_nightMode; }
    void setNightMode(bool on);
    bool backlightAvailable() const { return !m_backlightDir.isEmpty(); }
    bool lightSensorAvailable() const { return !m_alsPath.isEmpty(); }
    bool proximityAvailable() const { return !m_psPath.isEmpty(); }
    bool autoBrightness() const { return m_autoBrightness; }
    void setAutoBrightness(bool on);
    bool dark() const { return m_dark; }
    bool wifiAvailable() const { return m_simulated || !m_wpaCli.isEmpty(); }
    QString wifiState() const { return m_wifiState; }
    QString wifiMessage() const { return m_wifiMessage; }
    bool accessPointAvailable() const { return m_simulated || !m_apHelper.isEmpty(); }
    bool accessPointActive() const { return m_apActive; }
    QString accessPointSsid() const { return m_apSsid; }
    void setSimulated(bool on);
    QString wifiSsid() const { return m_ssid; }
    QVariantList wifiNetworks() const { return m_networks; }

    Q_INVOKABLE qint64 idleMs() const { return m_lastActivity.msecsTo(QDateTime::currentDateTime()); }
    Q_INVOKABLE qint64 lastActivityMs() const { return m_lastActivity.toMSecsSinceEpoch(); }
    Q_INVOKABLE void poke() { m_lastActivity = QDateTime::currentDateTime(); }
    Q_INVOKABLE void scanWifi();
    // Switches to a network already saved in wpa_supplicant.conf.
    Q_INVOKABLE void selectWifi(const QString &ssid);
    // Adds (or updates) a network and joins it; empty password = open network.
    Q_INVOKABLE void connectWifi(const QString &ssid, const QString &password);
    // Setup hotspot for phones, via the OS helper `nightstand-ap start|stop`.
    Q_INVOKABLE void startAccessPoint();
    Q_INVOKABLE void stopAccessPoint();

signals:
    void displayModeChanged();
    void textEntryChanged();
    void nightModeChanged();
    void holdToSkipChanged();
    void wifiChanged();
    void wifiStateChanged();
    void accessPointChanged();
    void autoBrightnessChanged();
    void darkChanged();
    void proximityWantedChanged();
    // Something came close to the proximity sensor.
    void approached();
    // "volumeUp", "volumeDown", "mute", "left", "right", "skipForward", "skipBack"
    void hardwareKey(const QString &key);
    void activity();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void writeBacklight();
    int targetBrightness() const;
    void initSensors();
    void readLight();
    void readProximity();
    QString keyName(int key) const;
    QString wpa(const QStringList &args, int timeoutMs = 3000) const;
    void readWifiStatus();
    int savedNetworkId(const QString &ssid) const;
    void waitForConnection(const QString &ssid, int newId);
    void setWifiState(const QString &state, const QString &message);

    QDateTime m_lastActivity = QDateTime::currentDateTime();
    QString m_displayMode = "on";
    bool m_textEntry = false;
    bool m_nightMode = false;
    bool m_holdToSkip = false;
    QTimer m_holdTimer;
    QString m_heldKey;
    bool m_holdFired = false;
    QString m_backlightDir;
    int m_maxBrightness = 0;
    int m_savedBrightness = 0;
    QString m_wpaCli;
    QString m_ssid;
    bool m_wifiConnected = false;
    QVariantList m_networks;
    QString m_wifiState = "idle";
    QString m_wifiMessage;
    QString m_apHelper;
    QString m_apSsid;
    bool m_apActive = false;
    bool m_simulated = false;

    QString m_alsPath;
    QString m_psPath;
    QTimer m_lightTimer;
    QTimer m_proximityTimer;
    QList<int> m_lightSamples; // last few seconds, averaged
    double m_light = -1;
    bool m_autoBrightness = false;
    bool m_dark = false;
    int m_darkSecs = 0;         // how long the room has been on the other side of the threshold
    int m_lastWritten = -1;
    bool m_proximityWanted = false;
    bool m_near = false;

    // Bumped from the event loop; nightstand-watch restarts the app when it stops.
    QTimer m_heartbeat;
    quint64 m_beats = 0;
};
