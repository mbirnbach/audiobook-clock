#pragma once

#include "Library.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

class AbsClient;
class AppSettings;
class AudioBackend;

// Owns the current listening session: resume position, chapter navigation, seeking, the
// sleep timer, and progress reporting to the server.
class PlaybackController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString bookId READ bookId NOTIFY bookChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY bookChanged)
    Q_PROPERTY(int chapterIndex READ chapterIndex NOTIFY chapterChanged)
    Q_PROPERTY(int chapterCount READ chapterCount NOTIFY bookChanged)
    Q_PROPERTY(QString chapterTitle READ chapterTitle NOTIFY chapterChanged)
    Q_PROPERTY(double chapterStart READ chapterStart NOTIFY chapterChanged)
    Q_PROPERTY(double chapterDuration READ chapterDuration NOTIFY chapterChanged)
    Q_PROPERTY(QString sleepMode READ sleepMode NOTIFY sleepChanged)
    Q_PROPERTY(double sleepRemaining READ sleepRemaining NOTIFY sleepChanged)
    Q_PROPERTY(int sleepMinutes READ sleepMinutes NOTIFY sleepChanged)
    // True during the last 30 s of a sleep timer, while the volume fades out.
    Q_PROPERTY(bool sleepFading READ sleepFading NOTIFY sleepChanged)
    Q_PROPERTY(double rate READ rate WRITE setRate NOTIFY rateChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)

public:
    PlaybackController(AbsClient *client, Library *library, AppSettings *settings, AudioBackend *backend,
                       QObject *parent = nullptr);
    ~PlaybackController() override;

    QString bookId() const { return m_bookId; }
    bool playing() const { return m_playing; }
    bool loading() const { return m_loading; }
    QString error() const { return m_error; }
    double position() const { return m_position; }
    double duration() const { return m_duration; }
    int chapterIndex() const { return m_chapterIndex; }
    int chapterCount() const { return int(m_chapters.size()); }
    QString chapterTitle() const;
    double chapterStart() const;
    double chapterDuration() const;
    QString sleepMode() const { return m_sleepMode; }
    double sleepRemaining() const { return m_sleepRemaining; }
    int sleepMinutes() const { return m_sleepMinutes; }
    int volume() const { return m_volume; }
    void setVolume(int v);
    bool sleepFading() const { return m_sleepFading; }
    double rate() const { return m_rate; }
    void setRate(double r);

    // start < 0 resumes from the saved position.
    Q_INVOKABLE void play(const QString &bookId, double start = -1);
    // Shows a book as current (mini player, standby clock) without starting playback.
    Q_INVOKABLE void prepare(const QString &bookId);
    Q_INVOKABLE void togglePlay();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void seek(double position);
    Q_INVOKABLE void skip(double seconds);
    Q_INVOKABLE void previousChapter();
    Q_INVOKABLE void nextChapter();
    Q_INVOKABLE void setSleepMinutes(int minutes);
    Q_INVOKABLE void setSleepEndOfChapter();
    Q_INVOKABLE void cancelSleep();
    // "Still listening": restart the sleep timer with this many minutes.
    Q_INVOKABLE void extendSleep(int minutes = 5);
    // Pause and close the server session (sign-out, shutdown).
    Q_INVOKABLE void stop();

signals:
    void bookChanged();
    void playingChanged();
    void loadingChanged();
    void errorChanged();
    void positionChanged();
    void chapterChanged();
    void sleepChanged();
    void volumeChanged();
    void rateChanged();
    // The sleep timer ran out: playback paused, the UI should turn the screen off.
    void sleepTimerFired();

private:
    void startSession(double start);
    void startDemo(double start);
    void onBackendPosition(double pos);
    void onBackendPlaying(bool playing);
    void onBackendError(const QString &message);
    void updateChapter();
    void tick();
    void sync(bool close = false);
    void setLoading(bool l);
    void setError(const QString &e);
    void fireSleep();
    void updateFade();
    double sleepSecondsLeft() const;
    double resumePosition(double position);

    AbsClient *m_client;
    Library *m_library;
    AppSettings *m_settings;
    AudioBackend *m_backend;

    QString m_bookId;
    QString m_sessionId;
    QList<Chapter> m_chapters;
    double m_position = 0;
    double m_duration = 0;
    int m_chapterIndex = 0;
    bool m_playing = false;
    bool m_loading = false;
    bool m_loaded = false;      // backend has the book's tracks
    bool m_recovering = false;  // reloading streams after an error (e.g. expired token)
    QString m_error;
    int m_volume = 45;

    QString m_sleepMode; // "", "min", "eoc"
    double m_sleepRemaining = 0;
    int m_sleepMinutes = 0;
    int m_sleepChapter = -1;
    bool m_sleepFading = false;
    bool m_rewindAfterSleep = false;
    double m_rate = 1.0;

    QTimer m_ticker;
    QElapsedTimer m_tickClock;
    double m_listenedSinceSync = 0;
    double m_sinceSync = 0;
    double m_sinceLocalProgress = 0;
};
