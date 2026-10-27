#include "AppSettings.h"

#include <cmath>

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
{
    // Volume used to go through Qt's logarithmic scale at full speaker gain; now it is
    // even dB steps over 38 dB, 14 dB lower at the top. Keep the loudness the user picked.
    if (m_s.value("playback/volumeCurve").toInt() < 2) {
        if (m_s.contains("playback/volume")) {
            const double old = m_s.value("playback/volume").toInt() / 100.0;
            const double amp = old >= 0.99 ? 1.0 : -std::log(1 - old) / std::log(100.0);
            const double db = amp > 0 ? 20 * std::log10(amp) : -100;
            m_s.setValue("playback/volume", qBound(old > 0 ? 1 : 0, int(std::lround(100 * (1 + (db + 14) / 38))), 100));
        }
        m_s.setValue("playback/volumeCurve", 2);
    }
    // The volume keys step in fives; round a value saved before that to the nearest five.
    if (m_s.value("playback/volumeCurve").toInt() < 3) {
        if (m_s.contains("playback/volume")) {
            const int v = m_s.value("playback/volume").toInt();
            m_s.setValue("playback/volume", v > 0 ? qBound(5, (v + 2) / 5 * 5, 100) : 0);
        }
        m_s.setValue("playback/volumeCurve", 3);
    }
}

void AppSettings::set(const QString &key, const QVariant &value)
{
    if (m_s.value(key) == value)
        return;
    m_s.setValue(key, value);
    emit changed();
}
