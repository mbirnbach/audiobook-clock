#pragma once

#include <QList>
#include <QObject>
#include <QUrl>

struct AudioTrack
{
    QUrl url;
    double startOffset = 0; // seconds into the book
    double duration = 0;
};

// Playback engine seam. A book is a list of tracks; all positions are in book time (seconds).
// Implementations: QtAudioBackend (Qt Multimedia, GStreamer on the device) and
// SimulatedBackend (demo mode). A libmpv backend can slot in here later.
class AudioBackend : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~AudioBackend() override = default;

    virtual void load(const QList<AudioTrack> &tracks, double position) = 0;
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual void seek(double position) = 0;
    virtual double position() const = 0;
    virtual bool isPlaying() const = 0;
    virtual void setVolume(double level) = 0; // 0..1, perceptual
    virtual void setRate(double rate) = 0;     // playback speed, pitch-corrected where possible

signals:
    void positionChanged(double position);
    void playingChanged(bool playing);
    void bufferingChanged(bool buffering);
    void ended();
    void errorOccurred(const QString &message);
};
