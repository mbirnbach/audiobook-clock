#include "PlaybackController.h"

#include "AbsClient.h"
#include "AppSettings.h"
#include "audio/AudioBackend.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSysInfo>

static constexpr double kSyncInterval = 15.0;
static constexpr double kSleepFade = 30.0; // seconds of fade-out before the sleep timer stops

PlaybackController::PlaybackController(AbsClient *client, Library *library, AppSettings *settings,
                                       AudioBackend *backend, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_library(library)
    , m_settings(settings)
    , m_backend(backend)
{
    m_volume = settings->volume();
    m_backend->setVolume(m_volume / 100.0);
    m_rate = std::clamp(settings->rate(), 0.5, 3.0);
    m_backend->setRate(m_rate);
    connect(m_backend, &AudioBackend::positionChanged, this, &PlaybackController::onBackendPosition);
    connect(m_backend, &AudioBackend::playingChanged, this, &PlaybackController::onBackendPlaying);
    connect(m_backend, &AudioBackend::errorOccurred, this, &PlaybackController::onBackendError);
    connect(m_backend, &AudioBackend::bufferingChanged, this, [this](bool b) { setLoading(b && m_playing); });
    connect(m_backend, &AudioBackend::ended, this, [this] {
        m_position = m_duration;
        emit positionChanged();
        sync();
    });
    // Chapters often arrive after the book was picked (detail fetched lazily).
    connect(m_library, &Library::detailLoaded, this, [this](const QString &id) {
        const Book *b = m_library->find(id);
        if (id != m_bookId || !b || b->chapters.isEmpty() || !m_chapters.isEmpty())
            return;
        m_chapters = b->chapters;
        updateChapter();
        emit bookChanged();
        emit chapterChanged();
    });
    // 4 Hz: smooth enough for the sleep fade, cheap otherwise.
    m_ticker.setInterval(250);
    connect(&m_ticker, &QTimer::timeout, this, &PlaybackController::tick);
}

PlaybackController::~PlaybackController()
{
    // Best effort on shutdown; the request may not complete.
    if (!m_sessionId.isEmpty())
        sync(true);
}

QString PlaybackController::chapterTitle() const
{
    return m_chapters.isEmpty() ? QString() : m_chapters.at(m_chapterIndex).title;
}

double PlaybackController::chapterStart() const
{
    return m_chapters.isEmpty() ? 0 : m_chapters.at(m_chapterIndex).start;
}

double PlaybackController::chapterDuration() const
{
    if (m_chapters.isEmpty())
        return m_duration;
    const Chapter &c = m_chapters.at(m_chapterIndex);
    return std::max(1.0, c.end - c.start);
}

void PlaybackController::setVolume(int v)
{
    v = qBound(0, v, 100);
    if (v == m_volume)
        return;
    m_volume = v;
    m_settings->setVolume(v);
    updateFade();
    if (!m_sleepFading)
        m_backend->setVolume(v / 100.0);
    emit volumeChanged();
}

void PlaybackController::setRate(double r)
{
    r = std::clamp(r, 0.5, 3.0);
    if (qFuzzyCompare(r, m_rate))
        return;
    m_rate = r;
    m_backend->setRate(r);
    m_settings->setRate(r);
    emit rateChanged();
}

// Where to continue after a pause: a few seconds back (more after the sleep timer, since
// the listener probably dozed off before it ended), or the start once a book is finished.
double PlaybackController::resumePosition(double position)
{
    if (position >= m_duration - 1)
        return 0;
    int back = m_settings->rewindSecs();
    if (m_rewindAfterSleep)
        back = std::max(back, m_settings->sleepRewindSecs());
    m_rewindAfterSleep = false;
    return std::max(0.0, position - back);
}

void PlaybackController::play(const QString &bookId, double start)
{
    if (bookId.isEmpty())
        return;
    if (bookId == m_bookId && m_loaded) {
        if (start >= 0)
            seek(start);
        if (!m_playing)
            togglePlay();
        return;
    }
    // Switching books: close the old session, and the sleep timer belongs to it.
    if (!m_sessionId.isEmpty()) {
        m_backend->pause();
        sync(true);
    }
    m_backend->stop();
    m_loaded = false;
    m_sessionId.clear();
    if (bookId != m_bookId)
        cancelSleep();
    setError({});

    m_bookId = bookId;
    const Book *b = m_library->find(bookId);
    m_duration = b ? b->duration : 0;
    m_chapters = b ? b->chapters : QList<Chapter>();
    if (start < 0) {
        const QVariantMap p = m_library->progress(bookId);
        start = p.value("finished").toBool() ? 0 : resumePosition(p.value("time").toDouble());
    }
    m_position = start;
    updateChapter();
    emit bookChanged();
    emit positionChanged();

    if (m_client->demo())
        startDemo(start);
    else
        startSession(start);
}

void PlaybackController::prepare(const QString &bookId)
{
    const Book *b = m_library->find(bookId);
    if (!m_bookId.isEmpty() || !b)
        return;
    m_bookId = bookId;
    m_duration = b->duration;
    m_chapters = b->chapters;
    const QVariantMap p = m_library->progress(bookId);
    m_position = p.value("finished").toBool() ? 0 : p.value("time").toDouble();
    updateChapter();
    emit bookChanged();
    emit positionChanged();
}

void PlaybackController::startDemo(double start)
{
    m_backend->load({AudioTrack{QUrl(), 0, m_duration}}, start);
    m_loaded = true;
    m_backend->seek(start);
    m_backend->play();
}

void PlaybackController::startSession(double start)
{
    setLoading(true);
    const QString bookId = m_bookId;
    const QJsonObject body{
        {"deviceInfo", QJsonObject{{"clientName", "Nightstand"},
                                   {"clientVersion", NIGHTSTAND_VERSION},
                                   {"deviceId", m_client->deviceId()},
                                   {"manufacturer", "Xiaomi"},
                                   {"model", "Mi Smart Clock"}}},
        {"forceDirectPlay", true},
        {"forceTranscode", false},
        {"mediaPlayer", "nightstand"},
        {"supportedMimeTypes", QJsonArray{"audio/mpeg", "audio/mp4", "audio/x-m4a", "audio/x-m4b", "audio/aac",
                                          "audio/flac", "audio/ogg", "audio/opus", "audio/wav"}},
    };
    m_client->post("/api/items/" + bookId + "/play", body,
                   [this, bookId, start](const QJsonDocument &json, int, const QString &err) {
        if (bookId != m_bookId)
            return;
        setLoading(false);
        const QJsonObject s = json.object();
        if (json.isNull() || s.value("audioTracks").toArray().isEmpty()) {
            setError(err.isEmpty() ? tr("This book has no playable audio.") : err);
            return;
        }
        m_sessionId = s.value("id").toString();
        QList<AudioTrack> tracks;
        for (const auto &v : s.value("audioTracks").toArray()) {
            const QJsonObject t = v.toObject();
            tracks.append({m_client->streamUrl(t.value("contentUrl").toString()), t.value("startOffset").toDouble(),
                           t.value("duration").toDouble()});
        }
        QList<Chapter> chapters;
        for (const auto &v : s.value("chapters").toArray()) {
            const QJsonObject c = v.toObject();
            chapters.append({c.value("start").toDouble(), c.value("end").toDouble(), c.value("title").toString()});
        }
        if (!chapters.isEmpty()) {
            m_chapters = chapters;
            m_library->setChapters(bookId, chapters);
        }
        if (s.value("duration").toDouble() > 0)
            m_duration = s.value("duration").toDouble();
        double pos = start;
        if (pos >= m_duration - 1)
            pos = 0;
        m_position = pos;
        m_backend->load(tracks, pos);
        m_loaded = true;
        m_backend->play();
        updateChapter();
        emit bookChanged();
        emit positionChanged();
    });
}

void PlaybackController::togglePlay()
{
    if (m_playing) {
        pause();
        return;
    }
    if (m_bookId.isEmpty())
        return;
    const double pos = resumePosition(m_position);
    if (!m_loaded) {
        play(m_bookId, pos);
        return;
    }
    m_backend->seek(pos);
    m_backend->play();
}

void PlaybackController::pause()
{
    if (!m_playing)
        return;
    m_backend->pause();
}

void PlaybackController::seek(double position)
{
    position = std::clamp(position, 0.0, std::max(0.0, m_duration - 1));
    m_position = position;
    if (m_loaded)
        m_backend->seek(position);
    updateChapter();
    emit positionChanged();
    m_library->setLocalProgress(m_bookId, m_position, m_duration);
}

void PlaybackController::skip(double seconds)
{
    seek(m_position + seconds);
}

void PlaybackController::previousChapter()
{
    if (m_chapters.isEmpty()) {
        seek(0);
        return;
    }
    const double into = m_position - chapterStart();
    if (into > 3 || m_chapterIndex == 0)
        seek(chapterStart());
    else
        seek(m_chapters.at(m_chapterIndex - 1).start);
}

void PlaybackController::nextChapter()
{
    if (m_chapterIndex + 1 < m_chapters.size())
        seek(m_chapters.at(m_chapterIndex + 1).start);
}

void PlaybackController::setSleepMinutes(int minutes)
{
    m_sleepFading = false;
    m_backend->setVolume(m_volume / 100.0);
    m_sleepMode = "min";
    m_sleepMinutes = minutes;
    m_sleepRemaining = minutes * 60.0;
    m_sleepChapter = -1;
    emit sleepChanged();
}

void PlaybackController::setSleepEndOfChapter()
{
    m_sleepFading = false;
    m_backend->setVolume(m_volume / 100.0);
    m_sleepMode = "eoc";
    m_sleepMinutes = 0;
    m_sleepRemaining = 0;
    m_sleepChapter = m_chapterIndex;
    emit sleepChanged();
}

void PlaybackController::cancelSleep()
{
    if (m_sleepMode.isEmpty())
        return;
    m_sleepMode.clear();
    m_sleepMinutes = 0;
    m_sleepRemaining = 0;
    m_sleepChapter = -1;
    if (m_sleepFading) {
        m_sleepFading = false;
        m_backend->setVolume(m_volume / 100.0);
    }
    emit sleepChanged();
}

void PlaybackController::extendSleep(int minutes)
{
    setSleepMinutes(minutes);
}

double PlaybackController::sleepSecondsLeft() const
{
    if (m_sleepMode == "min")
        return m_sleepRemaining;
    if (m_sleepMode == "eoc" && !m_chapters.isEmpty())
        return (m_chapters.at(m_chapterIndex).end - m_position) / m_rate;
    return -1;
}

void PlaybackController::updateFade()
{
    const double left = sleepSecondsLeft();
    const bool fading = m_playing && left >= 0 && left <= kSleepFade;
    if (fading)
        m_backend->setVolume(m_volume / 100.0 * std::clamp(left / kSleepFade, 0.0, 1.0));
    else if (m_sleepFading)
        m_backend->setVolume(m_volume / 100.0);
    if (fading != m_sleepFading) {
        m_sleepFading = fading;
        emit sleepChanged();
    }
}

void PlaybackController::stop()
{
    if (m_playing)
        m_backend->pause();
    if (!m_sessionId.isEmpty())
        sync(true);
    m_backend->stop();
    m_sessionId.clear();
    m_loaded = false;
    cancelSleep();
}

void PlaybackController::fireSleep()
{
    m_backend->pause();
    cancelSleep(); // also restores the faded volume
    m_rewindAfterSleep = true;
    emit sleepTimerFired();
}

void PlaybackController::onBackendPosition(double pos)
{
    if (!m_loaded)
        return;
    m_position = pos;
    updateChapter();
    emit positionChanged();
}

void PlaybackController::onBackendPlaying(bool playing)
{
    if (playing == m_playing)
        return;
    m_playing = playing;
    if (playing) {
        m_tickClock.start();
        m_ticker.start();
    } else {
        tick();
        m_ticker.stop();
        updateFade(); // a pause ends the fade (restores volume); resuming re-evaluates it
        sync();
        m_library->setLocalProgress(m_bookId, m_position, m_duration);
    }
    emit playingChanged();
}

void PlaybackController::onBackendError(const QString &message)
{
    // A stream that dies mid-book is most often an expired access token in the URL: start a
    // fresh session once (the API call refreshes the token) and carry on from here.
    if (!m_recovering && !m_client->demo() && !m_bookId.isEmpty()) {
        m_recovering = true;
        const double pos = m_position;
        m_loaded = false;
        m_sessionId.clear();
        startSession(pos);
        return;
    }
    setError(message);
}

void PlaybackController::updateChapter()
{
    int k = 0;
    for (int i = 0; i < m_chapters.size(); ++i)
        if (m_chapters.at(i).start <= m_position)
            k = i;
    if (k == m_chapterIndex)
        return;
    const int previous = m_chapterIndex;
    m_chapterIndex = k;
    emit chapterChanged();

    if (m_sleepMode == "eoc" && m_playing && k > m_sleepChapter && previous == m_sleepChapter) {
        // Stop at the start of the next chapter so tomorrow starts cleanly.
        seek(m_chapters.at(k).start);
        fireSleep();
    }
}

void PlaybackController::tick()
{
    if (!m_tickClock.isValid())
        return;
    const double dt = m_tickClock.restart() / 1000.0;
    if (!m_playing)
        return;
    m_recovering = false;
    m_listenedSinceSync += dt;
    m_sinceSync += dt;
    m_sinceLocalProgress += dt;

    if (m_sleepMode == "min") {
        m_sleepRemaining -= dt;
        if (m_sleepRemaining <= 0) {
            fireSleep();
        } else {
            updateFade();
            emit sleepChanged();
        }
    } else if (m_sleepMode == "eoc") {
        updateFade();
    }
    if (m_sinceLocalProgress >= 5) {
        m_sinceLocalProgress = 0;
        m_library->setLocalProgress(m_bookId, m_position, m_duration);
    }
    if (m_sinceSync >= kSyncInterval)
        sync();
}

void PlaybackController::sync(bool close)
{
    m_sinceSync = 0;
    if (m_client->demo() || m_sessionId.isEmpty())
        return;
    const QJsonObject body{{"currentTime", m_position},
                           {"timeListened", std::round(m_listenedSinceSync)},
                           {"duration", m_duration}};
    m_listenedSinceSync = 0;
    const QString path = "/api/session/" + m_sessionId + (close ? "/close" : "/sync");
    m_client->post(path, body, nullptr);
    if (close)
        m_sessionId.clear();
}

void PlaybackController::setLoading(bool l)
{
    if (m_loading == l)
        return;
    m_loading = l;
    emit loadingChanged();
}

void PlaybackController::setError(const QString &e)
{
    if (m_error == e)
        return;
    m_error = e;
    emit errorChanged();
}
