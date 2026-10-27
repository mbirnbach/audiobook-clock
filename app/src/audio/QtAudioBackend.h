#pragma once

#include "AudioBackend.h"

#include <QMediaPlayer>

class QAudioOutput;

class QtAudioBackend : public AudioBackend
{
    Q_OBJECT

public:
    explicit QtAudioBackend(QObject *parent = nullptr);

    void load(const QList<AudioTrack> &tracks, double position) override;
    void play() override;
    void pause() override;
    void stop() override;
    void seek(double position) override;
    double position() const override;
    bool isPlaying() const override;
    void setVolume(double level) override;
    void setRate(double rate) override;

private:
    int trackAt(double position) const;
    void openTrack(int index, double offsetInTrack);
    void onMediaStatus(QMediaPlayer::MediaStatus status);
    void applyPendingSeek();

    QMediaPlayer m_player;
    QAudioOutput *m_output;
    QList<AudioTrack> m_tracks;
    int m_current = -1;
    double m_pendingSeek = -1; // seconds into the current track, applied once loaded
    bool m_wantPlay = false;
    double m_rate = 1.0;
    bool m_sourceLoading = false; // current source has reported LoadingMedia
    bool m_sourceReady = false;   // ...and then Loaded/Buffered
};
