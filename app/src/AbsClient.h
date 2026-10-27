#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSettings>
#include <QUrl>
#include <functional>

class QNetworkReply;

// Minimal Audiobookshelf REST client: server check, login (with access/refresh tokens on
// ABS >= 2.26, legacy token otherwise) and authenticated JSON requests.
class AbsClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY serverChanged)
    Q_PROPERTY(QString serverHost READ serverHost NOTIFY serverChanged)
    Q_PROPERTY(QString serverVersion READ serverVersion NOTIFY serverChanged)
    Q_PROPERTY(QString username READ username NOTIFY authChanged)
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY authChanged)
    Q_PROPERTY(bool online READ online NOTIFY onlineChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool demo READ demo CONSTANT)

public:
    // json is null on transport errors; status is the HTTP status (0 when unreachable).
    using Callback = std::function<void(const QJsonDocument &json, int status, const QString &error)>;

    explicit AbsClient(bool demo, QObject *parent = nullptr);

    QString serverUrl() const { return m_server; }
    QString serverHost() const;
    QString serverVersion() const { return m_serverVersion; }
    QString username() const { return m_username; }
    bool signedIn() const { return m_demo || !m_accessToken.isEmpty(); }
    bool online() const { return m_online; }
    bool busy() const { return m_busy; }
    bool demo() const { return m_demo; }
    QString defaultLibraryId() const { return m_defaultLibraryId; }
    QString deviceId() const { return m_deviceId; }

    Q_INVOKABLE void checkServer(const QString &url);
    Q_INVOKABLE void login(const QString &username, const QString &password);
    Q_INVOKABLE void signOut();
    Q_INVOKABLE QString normalizeUrl(const QString &url) const;

    void get(const QString &path, Callback cb);
    void post(const QString &path, const QJsonObject &body, Callback cb);
    void del(const QString &path, Callback cb);
    // Unauthenticated-safe raw GET (used for cover bytes); status 0 on network error.
    void getBytes(const QString &path, std::function<void(const QByteArray &, int)> cb);

    // Streams can't carry headers in every backend, so the token goes in the query.
    QUrl streamUrl(const QString &contentUrl) const;
    QNetworkAccessManager *nam() { return &m_nam; }

signals:
    void serverChanged();
    void authChanged();
    void onlineChanged();
    void busyChanged();
    void serverCheckFinished(bool ok, const QString &message);
    void loginFinished(bool ok, const QString &message);
    // Emitted when a refresh failed and the user has to sign in again.
    void sessionExpired();

private:
    enum class Op { Get, Post, Delete };
    void send(Op op, const QString &path, const QByteArray &body, Callback cb, bool retried);
    void refreshAccessToken(std::function<void(bool)> done);
    QNetworkRequest request(const QString &path, bool auth) const;
    void setOnline(bool on);
    void setBusy(bool b);
    void saveAuth();
    static QString describeError(QNetworkReply *reply, int status);

    bool m_demo;
    QNetworkAccessManager m_nam;
    QSettings m_settings;
    QString m_server;
    QString m_serverVersion;
    QString m_username;
    QString m_accessToken;
    QString m_refreshToken;
    QString m_defaultLibraryId;
    QString m_deviceId;
    bool m_online = true;
    bool m_busy = false;
    bool m_refreshing = false;
    QList<std::function<void(bool)>> m_refreshWaiters;
};
