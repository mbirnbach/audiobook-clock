#include "DeviceControl.h"

#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QNetworkInterface>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QWindow>
#include <cmath>
#include <memory>

static int readInt(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll().trimmed().toInt() : 0;
}

static void writeInt(const QString &path, int value)
{
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(QByteArray::number(value));
}

static QByteArray readText(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll().trimmed() : QByteArray();
}

static void writeText(const QString &path, const QByteArray &text)
{
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(text);
}

// "0x2e" or "2e"
static int readHex(const QString &path, bool *ok)
{
    QByteArray v = readText(path);
    if (v.startsWith("0x"))
        v = v.mid(2);
    return v.toInt(ok, 16);
}

DeviceControl::DeviceControl(QObject *parent)
    : QObject(parent)
{
    const QDir dir("/sys/class/backlight");
    const QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (!entries.isEmpty())
        m_backlightDir = dir.filePath(entries.first());
    // The stock MediaTek kernel exposes the panel backlight as an LED class device
    // (same brightness/max_brightness files).
    else if (QFile::exists("/sys/class/leds/lcd-backlight/brightness"))
        m_backlightDir = "/sys/class/leds/lcd-backlight";
    if (!m_backlightDir.isEmpty()) {
        m_maxBrightness = readInt(m_backlightDir + "/max_brightness");
        m_savedBrightness = readInt(m_backlightDir + "/brightness");
    }
    m_wpaCli = QStandardPaths::findExecutable("wpa_cli");
    if (!m_wpaCli.isEmpty())
        readWifiStatus();
    // Optional OS helper that turns wlan0 into an open setup hotspot (see README).
    m_apHelper = QStandardPaths::findExecutable("nightstand-ap", {"/usr/libexec/nightstand"});
    if (m_apHelper.isEmpty())
        m_apHelper = QStandardPaths::findExecutable("nightstand-ap");
    // Suffix from the Wi-Fi MAC so two clocks in one house get different hotspot names.
    QString suffix = "0000";
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        const QString mac = iface.hardwareAddress().remove(':');
        if (mac.size() == 12 && mac != "000000000000") {
            suffix = mac.right(4).toUpper();
            if (iface.name().startsWith("wlan"))
                break;
        }
    }
    m_apSsid = "Nightstand-" + suffix;

    m_holdTimer.setSingleShot(false);
    connect(&m_holdTimer, &QTimer::timeout, this, [this] {
        m_holdFired = true;
        m_holdTimer.setInterval(900);
        emit hardwareKey(m_heldKey == "volumeUp" ? "skipForward" : "skipBack");
    });

    initSensors();

    const QString runtimeDir = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (!m_backlightDir.isEmpty() && !runtimeDir.isEmpty()) {
        connect(&m_heartbeat, &QTimer::timeout, this, [this, path = runtimeDir + "/alive"] {
            writeText(path, QByteArray::number(++m_beats));
        });
        m_heartbeat.start(5000);
    }
}

void DeviceControl::initSensors()
{
    // EM3071X light/proximity sensor behind the front glass (Xiaomi X04G, stock MediaTek
    // kernel). The sensor framework switches on the light part; its proximity enable fails,
    // so the proximity bit (0x80) goes into the chip's config register directly.
    const QString dir = "/sys/bus/platform/drivers/als_ps";
    if (!QFile::exists(dir + "/als"))
        return;
    writeText("/sys/class/sensor/m_als_misc/alsbatch", "4,0,200000000,0");
    writeText("/sys/class/sensor/m_als_misc/alsactive", "4,1");
    m_alsPath = dir + "/als";
    bool ok = false;
    const int config = readHex(dir + "/config", &ok);
    if (ok) {
        writeText(dir + "/config", "0x" + QByteArray::number(config | 0x80, 16));
        m_psPath = dir + "/ps";
    }

    connect(&m_lightTimer, &QTimer::timeout, this, &DeviceControl::readLight);
    m_lightTimer.start(1000);
    connect(&m_proximityTimer, &QTimer::timeout, this, &DeviceControl::readProximity);
    connect(this, &DeviceControl::proximityWantedChanged, this, [this] {
        m_near = false;
        if (m_proximityWanted && !m_psPath.isEmpty())
            m_proximityTimer.start(500);
        else
            m_proximityTimer.stop();
    });
}

void DeviceControl::readLight()
{
    m_lightSamples.append(readInt(m_alsPath));
    if (m_lightSamples.size() > 8)
        m_lightSamples.removeFirst();
    double sum = 0;
    for (int v : std::as_const(m_lightSamples))
        sum += v;
    m_light = sum / m_lightSamples.size();

    // Behind the glass the sensor reads ~1 with the lights off, 2 for a lamp, 6 for a
    // ceiling light. Going dark takes 30 s (not a hand over the sensor), light again 5 s.
    const bool crossing = m_dark ? m_light >= 2.0 : m_light < 1.5;
    m_darkSecs = crossing ? m_darkSecs + 1 : 0;
    if (m_darkSecs >= (m_dark ? 5 : 30)) {
        m_dark = !m_dark;
        m_darkSecs = 0;
        emit darkChanged();
    }

    if (m_autoBrightness && m_displayMode == "on" && std::abs(targetBrightness() - m_lastWritten) >= 6)
        writeBacklight();
}

void DeviceControl::readProximity()
{
    bool ok = false;
    const int ps = readHex(m_psPath, &ok);
    // A hand a few cm away reads 0x3a-0x82; a phone flashlight alone stays below 0x28.
    const bool near = ok && ps >= 0x30;
    if (near && !m_near)
        emit approached();
    m_near = near;
}

void DeviceControl::setAutoBrightness(bool on)
{
    if (on == m_autoBrightness)
        return;
    m_autoBrightness = on;
    writeBacklight();
    emit autoBrightnessChanged();
}

void DeviceControl::setNightMode(bool on)
{
    if (on == m_nightMode)
        return;
    m_nightMode = on;
    writeBacklight();
    emit nightModeChanged();
}

void DeviceControl::attach(QWindow *window)
{
    window->installEventFilter(this);
}

QString DeviceControl::keyName(int key) const
{
    switch (key) {
    case Qt::Key_VolumeUp:
        return "volumeUp";
    case Qt::Key_VolumeDown:
        return "volumeDown";
    case Qt::Key_MicMute:
        return "mute";
    // Desktop stand-ins for the hardware keys, as in the mockup.
    case Qt::Key_Up:
        return m_textEntry ? QString() : "volumeUp";
    case Qt::Key_Down:
        return m_textEntry ? QString() : "volumeDown";
    case Qt::Key_M:
        return m_textEntry ? QString() : "mute";
    case Qt::Key_Left:
        return m_textEntry ? QString() : "left";
    case Qt::Key_Right:
        return m_textEntry ? QString() : "right";
    }
    return {};
}

bool DeviceControl::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::TouchBegin:
        poke();
        emit activity();
        break;
    case QEvent::KeyPress:
    case QEvent::KeyRelease: {
        const auto *ke = static_cast<QKeyEvent *>(event);
        const QString key = keyName(ke->key());
        if (key.isEmpty())
            break;
        const bool press = event->type() == QEvent::KeyPress;
        const bool volume = key.startsWith("volume");
        if (press && !ke->isAutoRepeat())
            poke();
        if (volume && m_holdToSkip) {
            // Short press = volume step on release; holding = skip, repeated while held.
            if (press && !ke->isAutoRepeat()) {
                m_heldKey = key;
                m_holdFired = false;
                m_holdTimer.start(600);
            } else if (!press && !ke->isAutoRepeat() && key == m_heldKey) {
                m_holdTimer.stop();
                if (!m_holdFired)
                    emit hardwareKey(key);
                m_heldKey.clear();
            }
            return true;
        }
        // Volume keys repeat while held; the others fire once.
        if (press && (!ke->isAutoRepeat() || volume))
            emit hardwareKey(key);
        return true;
    }
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

void DeviceControl::setDisplayMode(const QString &mode)
{
    if (mode == m_displayMode)
        return;
    if (m_displayMode == "on" && !m_nightMode && !m_autoBrightness && !m_backlightDir.isEmpty())
        m_savedBrightness = std::max(1, readInt(m_backlightDir + "/brightness"));
    m_displayMode = mode;
    writeBacklight();
    emit displayModeChanged();
}

int DeviceControl::targetBrightness() const
{
    const int normal = m_savedBrightness > 0 ? m_savedBrightness : m_maxBrightness;
    if (m_displayMode == "off")
        return 0;
    if (m_displayMode == "dim")
        return std::max(1, m_maxBrightness / 20);
    if (m_nightMode)
        return std::max(1, normal / 4);
    if (m_autoBrightness && m_light >= 0) {
        // Log scale over the sensor readings: lamp-lit room ~50%, ceiling light ~70%,
        // daylight (24 and up) full.
        const double f = 0.35 + 0.65 * std::log2(std::max(m_light, 1.0)) / std::log2(24.0);
        return std::max(1, int(normal * std::min(f, 1.0)));
    }
    return normal;
}

void DeviceControl::writeBacklight()
{
    if (m_backlightDir.isEmpty())
        return;
    m_lastWritten = targetBrightness();
    writeInt(m_backlightDir + "/brightness", m_lastWritten);
}

QString DeviceControl::wpa(const QStringList &args, int timeoutMs) const
{
    QProcess p;
    p.start(m_wpaCli, args);
    if (!p.waitForFinished(timeoutMs))
        return {};
    return QString::fromUtf8(p.readAllStandardOutput());
}

void DeviceControl::setSimulated(bool on)
{
    // Desktop/demo stand-in so the Wi-Fi screens can be exercised without wpa_supplicant.
    m_simulated = on;
    if (on) {
        m_ssid = "Nightjar";
        m_networks = {
            QVariantMap{{"ssid", "Nightjar"}, {"detail", tr("Strong · WPA2")}, {"secure", true}, {"saved", true}},
            QVariantMap{{"ssid", "FRITZ!Box 7530 LM"}, {"detail", tr("Good · WPA2")}, {"secure", true}, {"saved", false}},
            QVariantMap{{"ssid", "Vodafone-2G7K"}, {"detail", tr("Weak · WPA2")}, {"secure", true}, {"saved", false}},
            QVariantMap{{"ssid", "Guest"}, {"detail", tr("Good · Open")}, {"secure", false}, {"saved", false}},
        };
    }
    emit wifiChanged();
}

void DeviceControl::setWifiState(const QString &state, const QString &message)
{
    m_wifiState = state;
    m_wifiMessage = message;
    emit wifiStateChanged();
}

void DeviceControl::readWifiStatus()
{
    if (m_simulated) {
        emit wifiChanged();
        return;
    }
    QString ssid, state, ip;
    for (const QString &line : wpa({"status"}).split('\n')) {
        if (line.startsWith("ssid="))
            ssid = line.mid(5);
        else if (line.startsWith("wpa_state="))
            state = line.mid(10);
        else if (line.startsWith("ip_address="))
            ip = line.mid(11);
    }
    m_ssid = state == "COMPLETED" ? ssid : QString();
    m_wifiConnected = state == "COMPLETED" && !ip.isEmpty();
    emit wifiChanged();
}

void DeviceControl::scanWifi()
{
    if (m_simulated || m_wpaCli.isEmpty())
        return;
    wpa({"scan"});
    // Results arrive within a couple of seconds; read them once without blocking the UI.
    auto *proc = new QProcess(this);
    connect(proc, &QProcess::finished, this, [this, proc] {
        proc->deleteLater();
        QSet<QString> saved;
        for (const QString &line : wpa({"list_networks"}).split('\n').mid(1)) {
            const QStringList f = line.split('\t');
            if (f.size() > 1)
                saved.insert(f.at(1));
        }
        QMap<QString, QVariantMap> best;
        for (const QString &line : QString::fromUtf8(proc->readAllStandardOutput()).split('\n').mid(1)) {
            // bssid / frequency / signal level / flags / ssid
            const QStringList f = line.split('\t');
            if (f.size() < 5 || f.at(4).isEmpty() || f.at(4).startsWith("\\x00"))
                continue;
            const int level = f.at(2).toInt();
            if (best.contains(f.at(4)) && best[f.at(4)].value("level").toInt() >= level)
                continue;
            const QString strength = level > -60 ? tr("Strong") : level > -72 ? tr("Good") : tr("Weak");
            const bool secure = f.at(3).contains("WPA") || f.at(3).contains("WEP") || f.at(3).contains("SAE");
            const QString security = f.at(3).contains("SAE") || f.at(3).contains("WPA3") ? "WPA3"
                                     : f.at(3).contains("WPA2") ? "WPA2"
                                     : secure ? "WPA"
                                              : tr("Open");
            best.insert(f.at(4), {{"ssid", f.at(4)}, {"level", level}, {"detail", strength + " · " + security},
                                  {"secure", secure}, {"saved", saved.contains(f.at(4))}});
        }
        QList<QVariantMap> list = best.values();
        std::sort(list.begin(), list.end(), [](const QVariantMap &a, const QVariantMap &b) {
            return a.value("level").toInt() > b.value("level").toInt();
        });
        m_networks.clear();
        for (const auto &n : list)
            m_networks.append(n);
        readWifiStatus();
    });
    QTimer::singleShot(2500, proc, [this, proc] { proc->start(m_wpaCli, {"scan_results"}); });
}

int DeviceControl::savedNetworkId(const QString &ssid) const
{
    for (const QString &line : wpa({"list_networks"}).split('\n').mid(1)) {
        const QStringList f = line.split('\t');
        if (f.size() > 1 && f.at(1) == ssid)
            return f.at(0).toInt();
    }
    return -1;
}

void DeviceControl::selectWifi(const QString &ssid)
{
    if (m_simulated) {
        connectWifi(ssid, QString());
        return;
    }
    const int id = m_wpaCli.isEmpty() ? -1 : savedNetworkId(ssid);
    if (id < 0)
        return;
    wpa({"select_network", QString::number(id)});
    waitForConnection(ssid, -1);
}

void DeviceControl::connectWifi(const QString &ssid, const QString &password)
{
    if (ssid.isEmpty())
        return;
    if (!password.isEmpty() && (password.size() < 8 || password.size() > 63)) {
        setWifiState("failed", tr("Wi-Fi passwords are 8 to 63 characters."));
        return;
    }
    if (m_apActive)
        stopAccessPoint();
    setWifiState("connecting", tr("Connecting to %1…").arg(ssid));

    if (m_simulated) {
        QTimer::singleShot(2000, this, [this, ssid, password] {
            if (password == "wrong") {
                setWifiState("failed", tr("Couldn't join %1. Check the password.").arg(ssid));
                return;
            }
            m_ssid = ssid;
            emit wifiChanged();
            setWifiState("connected", tr("Connected to %1").arg(ssid));
        });
        return;
    }
    if (m_wpaCli.isEmpty())
        return;

    // Replace any saved entry, the password may have changed.
    if (const int old = savedNetworkId(ssid); old >= 0)
        wpa({"remove_network", QString::number(old)});
    const QString id = wpa({"add_network"}).trimmed().split('\n').last();
    // Hex SSID avoids quoting issues with unusual names.
    wpa({"set_network", id, "ssid", QString::fromLatin1(ssid.toUtf8().toHex())});
    if (password.isEmpty())
        wpa({"set_network", id, "key_mgmt", "NONE"});
    else
        wpa({"set_network", id, "psk", '"' + password + '"'});
    wpa({"select_network", id});
    waitForConnection(ssid, id.toInt());
}

void DeviceControl::waitForConnection(const QString &ssid, int newId)
{
    auto *poll = new QTimer(this);
    auto started = std::make_shared<QElapsedTimer>();
    started->start();
    poll->setInterval(1000);
    connect(poll, &QTimer::timeout, this, [this, poll, started, ssid, newId] {
        readWifiStatus();
        if (m_wifiConnected && m_ssid == ssid) {
            poll->deleteLater();
            // select_network disabled every other network; keep them as fallbacks.
            wpa({"enable_network", "all"});
            wpa({"save_config"});
            setWifiState("connected", tr("Connected to %1").arg(ssid));
            return;
        }
        if (started->elapsed() > 25000) {
            poll->deleteLater();
            if (newId >= 0)
                wpa({"remove_network", QString::number(newId)});
            wpa({"enable_network", "all"});
            wpa({"reassociate"});
            setWifiState("failed", tr("Couldn't join %1. Check the password.").arg(ssid));
        }
    });
    poll->start();
}

void DeviceControl::startAccessPoint()
{
    if (m_apActive || (!m_simulated && m_apHelper.isEmpty()))
        return;
    if (!m_simulated) {
        QProcess p;
        p.start(m_apHelper, {"start", m_apSsid});
        if (!p.waitForFinished(15000) || p.exitCode() != 0) {
            setWifiState("failed", tr("Couldn't start the setup hotspot."));
            return;
        }
    }
    m_apActive = true;
    emit accessPointChanged();
}

void DeviceControl::stopAccessPoint()
{
    if (!m_apActive)
        return;
    if (!m_simulated) {
        QProcess p;
        p.start(m_apHelper, {"stop"});
        p.waitForFinished(15000);
    }
    m_apActive = false;
    emit accessPointChanged();
}
