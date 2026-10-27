#include "AbsClient.h"

#include <QJsonArray>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrlQuery>
#include <QUuid>

static constexpr int kTimeoutMs = 15000;

AbsClient::AbsClient(bool demo, QObject *parent)
    : QObject(parent)
    , m_demo(demo)
{
    auto *cache = new QNetworkDiskCache(this);
    cache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/http");
    cache->setMaximumCacheSize(32 * 1024 * 1024);
    m_nam.setCache(cache);

    m_server = m_settings.value("server/url").toString();
    m_username = m_settings.value("server/username").toString();
    m_accessToken = m_settings.value("server/accessToken").toString();
    m_refreshToken = m_settings.value("server/refreshToken").toString();
    m_defaultLibraryId = m_settings.value("server/defaultLibraryId").toString();
    m_deviceId = m_settings.value("device/id").toString();
    if (m_deviceId.isEmpty()) {
        m_deviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_settings.setValue("device/id", m_deviceId);
    }
    if (m_demo) {
        m_server = "http://demo.local:13378";
        m_username = "night";
    }
}

QString AbsClient::serverHost() const
{
    QString host = m_server;
    host.remove(QRegularExpression("^https?://"));
    return host;
}

QString AbsClient::normalizeUrl(const QString &url) const
{
    QString u = url.trimmed();
    if (u.isEmpty())
        return u;
    if (!u.contains("://"))
        u.prepend("http://");
    while (u.endsWith('/'))
        u.chop(1);
    return u;
}

void AbsClient::checkServer(const QString &url)
{
    const QString base = normalizeUrl(url);
    if (m_demo) {
        m_server = base;
        emit serverChanged();
        emit serverCheckFinished(true, {});
        return;
    }
    const QUrl u(base + "/status");
    if (!u.isValid() || u.host().isEmpty()) {
        emit serverCheckFinished(false, tr("That doesn't look like a server address."));
        return;
    }
    setBusy(true);
    QNetworkRequest req(u);
    req.setTransferTimeout(kTimeoutMs);
    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, base] {
        reply->deleteLater();
        setBusy(false);
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        if (reply->error() != QNetworkReply::NoError || !o.contains("isInit")) {
            emit serverCheckFinished(false, reply->error() != QNetworkReply::NoError
                                                ? describeError(reply, status)
                                                : tr("No Audiobookshelf server found at this address."));
            return;
        }
        if (!o.value("isInit").toBool()) {
            emit serverCheckFinished(false, tr("This server isn't set up yet. Finish setup in a browser first."));
            return;
        }
        m_server = base;
        m_serverVersion = o.value("serverVersion").toString();
        m_settings.setValue("server/url", m_server);
        setOnline(true);
        emit serverChanged();
        emit serverCheckFinished(true, {});
    });
}

void AbsClient::login(const QString &username, const QString &password)
{
    if (m_demo) {
        m_username = username;
        m_accessToken = "demo";
        emit authChanged();
        emit loginFinished(true, {});
        return;
    }
    setBusy(true);
    QNetworkRequest req = request("/login", false);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // Ask ABS >= 2.26 for access + refresh tokens; older servers ignore it.
    req.setRawHeader("x-return-tokens", "true");
    const QJsonObject body{{"username", username}, {"password", password}};
    QNetworkReply *reply = m_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        setBusy(false);
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        if (status == 401) {
            emit loginFinished(false, tr("Wrong username or password."));
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            emit loginFinished(false, describeError(reply, status));
            return;
        }
        const QJsonObject user = o.value("user").toObject();
        QString access = user.value("accessToken").toString();
        if (access.isEmpty())
            access = user.value("token").toString();
        if (access.isEmpty()) {
            emit loginFinished(false, tr("The server did not return a token."));
            return;
        }
        m_accessToken = access;
        m_refreshToken = user.value("refreshToken").toString();
        m_username = user.value("username").toString();
        m_defaultLibraryId = o.value("userDefaultLibraryId").toString();
        if (m_serverVersion.isEmpty())
            m_serverVersion = o.value("serverSettings").toObject().value("version").toString();
        saveAuth();
        setOnline(true);
        emit authChanged();
        emit loginFinished(true, {});
    });
}

void AbsClient::signOut()
{
    if (!m_demo && !m_accessToken.isEmpty()) {
        // Best effort: invalidates the refresh token on ABS >= 2.26.
        QNetworkRequest req = request("/logout", true);
        if (!m_refreshToken.isEmpty())
            req.setRawHeader("x-refresh-token", m_refreshToken.toUtf8());
        QNetworkReply *reply = m_nam.post(req, QByteArray());
        connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    }
    m_accessToken.clear();
    m_refreshToken.clear();
    saveAuth();
    emit authChanged();
}

void AbsClient::saveAuth()
{
    m_settings.setValue("server/username", m_username);
    m_settings.setValue("server/accessToken", m_accessToken);
    m_settings.setValue("server/refreshToken", m_refreshToken);
    m_settings.setValue("server/defaultLibraryId", m_defaultLibraryId);
}

QNetworkRequest AbsClient::request(const QString &path, bool auth) const
{
    QNetworkRequest req(QUrl(m_server + path));
    req.setTransferTimeout(kTimeoutMs);
    if (auth && !m_accessToken.isEmpty())
        req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
    return req;
}

void AbsClient::get(const QString &path, Callback cb)
{
    send(Op::Get, path, {}, std::move(cb), false);
}

void AbsClient::post(const QString &path, const QJsonObject &body, Callback cb)
{
    send(Op::Post, path, QJsonDocument(body).toJson(QJsonDocument::Compact), std::move(cb), false);
}

void AbsClient::del(const QString &path, Callback cb)
{
    send(Op::Delete, path, {}, std::move(cb), false);
}

void AbsClient::send(Op op, const QString &path, const QByteArray &body, Callback cb, bool retried)
{
    if (m_demo) {
        if (cb)
            cb(QJsonDocument(), 0, tr("Demo mode"));
        return;
    }
    QNetworkRequest req = request(path, true);
    QNetworkReply *reply = nullptr;
    switch (op) {
    case Op::Get:
        reply = m_nam.get(req);
        break;
    case Op::Post:
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        reply = m_nam.post(req, body);
        break;
    case Op::Delete:
        reply = m_nam.deleteResource(req);
        break;
    }
    connect(reply, &QNetworkReply::finished, this, [this, reply, op, path, body, cb, retried] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 401 && !retried && !m_refreshToken.isEmpty()) {
            refreshAccessToken([this, op, path, body, cb](bool ok) {
                if (ok)
                    send(op, path, body, cb, true);
                else if (cb)
                    cb(QJsonDocument(), 401, tr("Signed out"));
            });
            return;
        }
        const QByteArray data = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            const bool unreachable = status == 0;
            setOnline(!unreachable);
            if (cb)
                cb(QJsonDocument(), status, describeError(reply, status));
            return;
        }
        setOnline(true);
        if (cb)
            cb(QJsonDocument::fromJson(data), status, {});
    });
}

void AbsClient::getBytes(const QString &path, std::function<void(const QByteArray &, int)> cb)
{
    if (m_demo) {
        cb({}, 0);
        return;
    }
    QNetworkRequest req = request(path, true);
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [reply, cb] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        cb(reply->error() == QNetworkReply::NoError ? reply->readAll() : QByteArray(), status);
    });
}

void AbsClient::refreshAccessToken(std::function<void(bool)> done)
{
    m_refreshWaiters.append(std::move(done));
    if (m_refreshing)
        return;
    m_refreshing = true;
    QNetworkRequest req = request("/auth/refresh", false);
    req.setRawHeader("x-refresh-token", m_refreshToken.toUtf8());
    QNetworkReply *reply = m_nam.post(req, QByteArray());
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_refreshing = false;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject user = QJsonDocument::fromJson(reply->readAll()).object().value("user").toObject();
        const QString access = user.value("accessToken").toString();
        bool ok = reply->error() == QNetworkReply::NoError && !access.isEmpty();
        if (ok) {
            m_accessToken = access;
            const QString refresh = user.value("refreshToken").toString();
            if (!refresh.isEmpty())
                m_refreshToken = refresh;
            saveAuth();
        } else if (status == 401 || status == 403) {
            m_accessToken.clear();
            m_refreshToken.clear();
            saveAuth();
            emit authChanged();
            emit sessionExpired();
        }
        const auto waiters = std::exchange(m_refreshWaiters, {});
        for (const auto &w : waiters)
            w(ok);
    });
}

QUrl AbsClient::streamUrl(const QString &contentUrl) const
{
    QUrl u(m_server + contentUrl);
    QUrlQuery q(u);
    q.removeQueryItem("token");
    q.addQueryItem("token", m_accessToken);
    u.setQuery(q);
    return u;
}

void AbsClient::setOnline(bool on)
{
    if (m_online == on)
        return;
    m_online = on;
    emit onlineChanged();
}

void AbsClient::setBusy(bool b)
{
    if (m_busy == b)
        return;
    m_busy = b;
    emit busyChanged();
}

QString AbsClient::describeError(QNetworkReply *reply, int status)
{
    switch (reply->error()) {
    case QNetworkReply::ConnectionRefusedError:
        return tr("Connection refused. Check the address and port.");
    case QNetworkReply::HostNotFoundError:
        return tr("Server not found. Check the address.");
    case QNetworkReply::TimeoutError:
    case QNetworkReply::OperationCanceledError:
        return tr("The server took too long to answer.");
    case QNetworkReply::SslHandshakeFailedError:
        return tr("Secure connection failed.");
    default:
        break;
    }
    if (status)
        return tr("Server error (%1).").arg(status);
    return reply->errorString();
}
