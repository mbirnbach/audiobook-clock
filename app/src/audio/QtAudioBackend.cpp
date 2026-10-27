#include "QtAudioBackend.h"

#include <cmath>

#include <QAudioOutput>
#include <QtMultimedia/qtaudio.h>

// Volume 1 is this far below 100. On the clock the speaker gain is also lowered in
// hardware (x04g-audio-init), so 100 is a sensible maximum for a bedroom.
static constexpr double kVolumeRangeDb = 38;

QtAudioBackend::QtAudioBackend(QObject *parent)
    : AudioBackend(parent)
    , m_output(new QAudioOutput(this))
{
    m_player.setAudioOutput(m_output);
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    // Keep voices natural at 1.25x/1.5x where the media backend supports it.
    if (m_player.pitchCompensationAvailability() != QMediaPlayer::PitchCompensationAvailability::Unavailable)
        m_player.setPitchCompensation(true);
#endif
    connect(&m_player, &QMediaPlayer::positionChanged, this, [this](qint64) { emit positionChanged(position()); });
    connect(&m_player, &QMediaPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState) { emit playingChanged(isPlaying()); });
    connect(&m_player, &QMediaPlayer::mediaStatusChanged, this, &QtAudioBackend::onMediaStatus);
    // A freshly opened HTTP stream ignores setPosition() until it reports being seekable.
    connect(&m_player, &QMediaPlayer::seekableChanged, this, &QtAudioBackend::applyPendingSeek);
    connect(&m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &msg) { emit errorOccurred(msg); });
}

void QtAudioBackend::load(const QList<AudioTrack> &tracks, double position)
{
    m_tracks = tracks;
    m_wantPlay = false;
    m_current = -1;
    const int idx = trackAt(position);
    if (idx >= 0)
        openTrack(idx, position - m_tracks.at(idx).startOffset);
}

int QtAudioBackend::trackAt(double position) const
{
    int k = m_tracks.isEmpty() ? -1 : 0;
    for (int i = 0; i < m_tracks.size(); ++i)
        if (m_tracks.at(i).startOffset <= position)
            k = i;
    return k;
}

void QtAudioBackend::openTrack(int index, double offsetInTrack)
{
    m_current = index;
    m_pendingSeek = offsetInTrack > 0.5 ? offsetInTrack : -1;
    m_sourceLoading = false;
    m_sourceReady = false;
    m_player.setSource(m_tracks.at(index).url);
    m_player.setPlaybackRate(m_rate);
    if (m_wantPlay)
        m_player.play();
}

void QtAudioBackend::onMediaStatus(QMediaPlayer::MediaStatus status)
{
    switch (status) {
    case QMediaPlayer::LoadedMedia:
    case QMediaPlayer::BufferedMedia:
        // Right after setSource() the player can still report the previous file as loaded;
        // only trust this once the new source went through LoadingMedia.
        if (m_sourceLoading)
            m_sourceReady = true;
        applyPendingSeek();
        emit bufferingChanged(false);
        break;
    case QMediaPlayer::LoadingMedia:
        m_sourceLoading = true;
        emit bufferingChanged(true);
        break;
    case QMediaPlayer::StalledMedia:
    case QMediaPlayer::BufferingMedia:
        emit bufferingChanged(true);
        break;
    case QMediaPlayer::EndOfMedia:
        if (m_current + 1 < m_tracks.size()) {
            openTrack(m_current + 1, 0);
        } else {
            m_wantPlay = false;
            emit ended();
        }
        break;
    default:
        break;
    }
}

void QtAudioBackend::applyPendingSeek()
{
    if (m_pendingSeek < 0 || !m_sourceReady || !m_player.isSeekable())
        return;
    m_player.setPosition(qint64(m_pendingSeek * 1000));
    m_pendingSeek = -1;
}

void QtAudioBackend::play()
{
    m_wantPlay = true;
    m_player.play();
}

void QtAudioBackend::pause()
{
    m_wantPlay = false;
    m_player.pause();
}

void QtAudioBackend::stop()
{
    m_wantPlay = false;
    m_player.stop();
    m_player.setSource(QUrl());
    m_tracks.clear();
    m_current = -1;
}

void QtAudioBackend::seek(double position)
{
    const int idx = trackAt(position);
    if (idx < 0)
        return;
    const double offset = position - m_tracks.at(idx).startOffset;
    if (idx == m_current && m_pendingSeek < 0) {
        m_player.setPosition(qint64(offset * 1000));
    } else if (idx == m_current) {
        m_pendingSeek = offset;
    } else {
        openTrack(idx, offset);
    }
    emit positionChanged(position);
}

double QtAudioBackend::position() const
{
    if (m_current < 0)
        return 0;
    const double inTrack = m_pendingSeek >= 0 ? m_pendingSeek : m_player.position() / 1000.0;
    return m_tracks.at(m_current).startOffset + inTrack;
}

bool QtAudioBackend::isPlaying() const
{
    return m_player.playbackState() == QMediaPlayer::PlayingState;
}

void QtAudioBackend::setVolume(double level)
{
    // Even steps in dB over kVolumeRangeDb, so the quiet end (a bedside clock at night)
    // gets as much of the scale as the loud one; 0 is silence.
    m_output->setVolume(level <= 0 ? 0.0f : float(std::pow(10.0, kVolumeRangeDb * (std::min(level, 1.0) - 1) / 20)));
}

void QtAudioBackend::setRate(double rate)
{
    m_rate = rate;
    m_player.setPlaybackRate(rate);
}
