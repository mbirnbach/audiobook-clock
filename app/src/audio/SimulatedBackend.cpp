#include "SimulatedBackend.h"

SimulatedBackend::SimulatedBackend(double speed, QObject *parent)
    : AudioBackend(parent)
    , m_speed(speed)
{
    m_timer.setInterval(250);
    connect(&m_timer, &QTimer::timeout, this, &SimulatedBackend::tick);
}

void SimulatedBackend::load(const QList<AudioTrack> &tracks, double position)
{
    m_duration = 0;
    for (const AudioTrack &t : tracks)
        m_duration = std::max(m_duration, t.startOffset + t.duration);
    m_position = position;
}

void SimulatedBackend::play()
{
    if (m_playing)
        return;
    m_playing = true;
    m_clock.start();
    m_timer.start();
    emit playingChanged(true);
}

void SimulatedBackend::pause()
{
    if (!m_playing)
        return;
    tick();
    m_playing = false;
    m_timer.stop();
    emit playingChanged(false);
}

void SimulatedBackend::stop()
{
    pause();
    m_position = 0;
}

void SimulatedBackend::seek(double position)
{
    m_position = std::clamp(position, 0.0, m_duration);
    if (m_playing)
        m_clock.restart();
    emit positionChanged(m_position);
}

void SimulatedBackend::tick()
{
    m_position = std::min(m_duration, m_position + m_clock.restart() / 1000.0 * m_speed * m_rate);
    emit positionChanged(m_position);
    if (m_position >= m_duration) {
        m_playing = false;
        m_timer.stop();
        emit playingChanged(false);
        emit ended();
    }
}
