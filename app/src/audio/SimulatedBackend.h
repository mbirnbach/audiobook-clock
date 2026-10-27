#pragma once

#include "AudioBackend.h"

#include <QElapsedTimer>
#include <QTimer>

// Silent clock-driven backend for demo mode. `speed` > 1 fast-forwards time.
class SimulatedBackend : public AudioBackend
{
    Q_OBJECT

public:
    explicit SimulatedBackend(double speed = 1.0, QObject *parent = nullptr);

    void load(const QList<AudioTrack> &tracks, double position) override;
    void play() override;
    void pause() override;
    void stop() override;
    void seek(double position) override;
    double position() const override { return m_position; }
    bool isPlaying() const override { return m_playing; }
    void setVolume(double) override {}
    void setRate(double rate) override { m_rate = rate; }

private:
    void tick();

    QTimer m_timer;
    QElapsedTimer m_clock;
    double m_speed;
    double m_rate = 1.0;
    double m_duration = 0;
    double m_position = 0;
    bool m_playing = false;
};
