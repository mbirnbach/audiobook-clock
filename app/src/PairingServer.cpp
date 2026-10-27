#include "PairingServer.h"

#include "AbsClient.h"
#include "DeviceControl.h"
#include "qrcodegen.hpp"

#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <memory>

static constexpr int kMaxRequestBytes = 64 * 1024;
static constexpr int kMaxFailedAttempts = 5;
static constexpr int kAbsPort = 13378;

PairingServer::PairingServer(AbsClient *abs, DeviceControl *device, QObject *parent)
    : QObject(parent)
    , m_abs(abs)
    , m_device(device)
    , m_scanNam(new QNetworkAccessManager(this))
{
    connect(&m_server, &QTcpServer::newConnection, this, &PairingServer::onConnection);
    // The address can change while setup is open (Wi-Fi joined, DHCP renewed).
    auto *addrTimer = new QTimer(this);
    addrTimer->setInterval(5000);
    connect(addrTimer, &QTimer::timeout, this, [this] {
        if (active())
            refreshAddress();
    });
    addrTimer->start();
    connect(m_device, &DeviceControl::wifiChanged, this, &PairingServer::refreshAddress);
    newCode();
}

void PairingServer::setActive(bool on)
{
    if (on == active())
        return;
    if (on) {
        // Port 80 keeps the address short on screen; fall back when it isn't available.
        if (!m_server.listen(QHostAddress::AnyIPv4, 80) && !m_server.listen(QHostAddress::AnyIPv4, 8080)) {
            qWarning("pairing: cannot listen: %s", qPrintable(m_server.errorString()));
            return;
        }
        refreshAddress();
        scanForServers();
        qInfo("pairing: open http://%s/ code %s", qPrintable(displayUrl()), qPrintable(m_code));
    } else {
        m_server.close();
        setError({});
    }
    emit activeChanged();
    emit addressChanged();
}

QString PairingServer::url() const
{
    if (m_address.isEmpty() || !active())
        return {};
    return QStringLiteral("http://%1/?code=%2").arg(displayUrl(), m_code);
}

QString PairingServer::displayUrl() const
{
    if (m_address.isEmpty())
        return {};
    const int port = m_server.serverPort();
    return port == 80 || port == 0 ? m_address : QStringLiteral("%1:%2").arg(m_address).arg(port);
}

void PairingServer::refreshAddress()
{
    // Prefer Wi-Fi, then any other running non-loopback IPv4 interface.
    QString best;
    int bestScore = -1;
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning)
            || (flags & QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry &e : iface.addressEntries()) {
            const QHostAddress ip = e.ip();
            if (ip.protocol() != QAbstractSocket::IPv4Protocol || ip.isLinkLocal())
                continue;
            int score = 1;
            if (iface.type() == QNetworkInterface::Wifi || iface.name().startsWith("wlan"))
                score = 3;
            else if (iface.name().startsWith("en") || iface.name().startsWith("eth"))
                score = 2;
            if (score > bestScore) {
                bestScore = score;
                best = ip.toString();
            }
        }
    }
    if (best != m_address) {
        m_address = best;
        emit addressChanged();
    }
}

void PairingServer::newCode()
{
    m_code = QString::number(QRandomGenerator::system()->bounded(100000, 1000000));
    m_failedAttempts = 0;
    emit codeChanged();
    emit addressChanged();
}

bool PairingServer::checkCode(const QString &code)
{
    QString digits = code;
    digits.remove(QRegularExpression("\\D"));
    if (digits == m_code)
        return true;
    if (++m_failedAttempts >= kMaxFailedAttempts)
        newCode();
    return false;
}

void PairingServer::setBusy(bool busy, const QString &message)
{
    m_busy = busy;
    m_busyMessage = message;
    emit busyChanged();
}

void PairingServer::setError(const QString &e)
{
    if (e == m_error)
        return;
    m_error = e;
    emit errorChanged();
}

void PairingServer::clearError()
{
    setError({});
}

// ---------------------------------------------------------------- HTTP

void PairingServer::onConnection()
{
    while (QTcpSocket *socket = m_server.nextPendingConnection()) {
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] { onReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        // Drop idle or slow clients.
        QTimer::singleShot(30000, socket, [socket] { socket->abort(); });
    }
}

void PairingServer::onReadyRead(QTcpSocket *socket)
{
    QByteArray buf = socket->property("buf").toByteArray() + socket->readAll();
    if (buf.size() > kMaxRequestBytes) {
        respond(socket, 413, "text/plain", "Too large");
        return;
    }
    socket->setProperty("buf", buf);
    const int headerEnd = buf.indexOf("\r\n\r\n");
    if (headerEnd < 0)
        return;
    const QList<QByteArray> lines = buf.left(headerEnd).split('\n');
    const QList<QByteArray> requestLine = lines.value(0).trimmed().split(' ');
    if (requestLine.size() < 2) {
        respond(socket, 400, "text/plain", "Bad request");
        return;
    }
    int contentLength = 0;
    for (const QByteArray &line : lines.mid(1)) {
        const int colon = line.indexOf(':');
        if (colon > 0 && line.left(colon).trimmed().toLower() == "content-length")
            contentLength = line.mid(colon + 1).trimmed().toInt();
    }
    if (buf.size() < headerEnd + 4 + contentLength)
        return; // wait for the rest of the body
    socket->setProperty("buf", QVariant());
    disconnect(socket, &QTcpSocket::readyRead, this, nullptr);

    Request req;
    req.method = requestLine.at(0);
    const QByteArray target = requestLine.at(1);
    const int q = target.indexOf('?');
    req.path = q < 0 ? target : target.left(q);
    req.query = q < 0 ? QByteArray() : target.mid(q + 1);
    req.body = buf.mid(headerEnd + 4, contentLength);
    handle(socket, req);
}

void PairingServer::handle(QTcpSocket *socket, const Request &req)
{
    if (req.method == "GET" && (req.path == "/" || req.path == "/index.html")) {
        QFile f(":/qt/qml/Nightstand/web/index.html");
        respond(socket, 200, "text/html; charset=utf-8", f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray());
    } else if (req.method == "GET" && req.path == "/font.ttf") {
        QFile f(":/qt/qml/Nightstand/fonts/InstrumentSans.ttf");
        respond(socket, 200, "font/ttf", f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray());
    } else if (req.method == "GET" && req.path == "/api/info") {
        respondJson(socket, 200, info());
    } else if (req.method == "POST" && (req.path == "/api/pair" || req.path == "/api/wifi")) {
        const QJsonObject body = QJsonDocument::fromJson(req.body).object();
        if (!checkCode(body.value("code").toString())) {
            respondJson(socket, 403, {{"ok", false},
                                      {"field", "code"},
                                      {"message", tr("That code doesn't match the one on the clock.")}});
            return;
        }
        if (req.path == "/api/pair")
            handlePair(socket, body);
        else
            handleWifi(socket, body);
    } else if (req.method == "POST" && req.path == "/api/check") {
        // Lets the page validate the code before showing the forms.
        const QJsonObject body = QJsonDocument::fromJson(req.body).object();
        const bool ok = checkCode(body.value("code").toString());
        respondJson(socket, ok ? 200 : 403, {{"ok", ok}});
    } else {
        respond(socket, 404, "text/plain", "Not found");
    }
}

QJsonObject PairingServer::info() const
{
    QJsonArray servers;
    for (const QVariant &s : m_servers)
        servers.append(QJsonObject::fromVariantMap(s.toMap()));
    QJsonArray networks;
    for (const QVariant &n : m_device->wifiNetworks()) {
        const QVariantMap m = n.toMap();
        networks.append(QJsonObject{{"ssid", m.value("ssid").toString()},
                                    {"detail", m.value("detail").toString()},
                                    {"secure", m.value("secure").toBool()}});
    }
    return {{"device", "Nightstand"},
            {"server", m_abs->serverUrl()},
            {"username", m_abs->username()},
            {"signedIn", m_abs->signedIn()},
            {"servers", servers},
            {"wifi", QJsonObject{{"available", m_device->wifiAvailable()},
                                 {"ssid", m_device->wifiSsid()},
                                 {"networks", networks}}}};
}

void PairingServer::handlePair(QTcpSocket *socket, const QJsonObject &body)
{
    if (m_busy) {
        respondJson(socket, 409, {{"ok", false}, {"message", tr("The clock is busy, try again in a moment.")}});
        return;
    }
    const QString server = body.value("server").toString().trimmed();
    const QString username = body.value("username").toString().trimmed();
    const QString password = body.value("password").toString();
    if (server.isEmpty() || username.isEmpty()) {
        respondJson(socket, 400, {{"ok", false}, {"message", tr("Server and username are required.")}});
        return;
    }
    setError({});
    setBusy(true, tr("Signing in as %1…").arg(username));
    QPointer<QTcpSocket> sock(socket);
    const auto finish = [this, sock](bool ok, const QString &message) {
        setBusy(false);
        if (!ok)
            setError(message);
        else
            newCode(); // the code was seen by whoever paired; don't reuse it
        if (sock)
            respondJson(sock, ok ? 200 : 401, {{"ok", ok}, {"message", message}});
    };

    auto checkConn = std::make_shared<QMetaObject::Connection>();
    *checkConn = connect(m_abs, &AbsClient::serverCheckFinished, this,
                         [this, checkConn, finish, username, password](bool ok, const QString &message) {
        disconnect(*checkConn);
        if (!ok) {
            finish(false, message);
            return;
        }
        auto loginConn = std::make_shared<QMetaObject::Connection>();
        *loginConn = connect(m_abs, &AbsClient::loginFinished, this,
                             [loginConn, finish](bool ok, const QString &message) {
            disconnect(*loginConn);
            finish(ok, message);
        });
        m_abs->login(username, password);
    });
    m_abs->checkServer(server);
}

void PairingServer::handleWifi(QTcpSocket *socket, const QJsonObject &body)
{
    const QString ssid = body.value("ssid").toString();
    if (ssid.isEmpty() || !m_device->wifiAvailable()) {
        respondJson(socket, 400, {{"ok", false}, {"message", tr("Wi-Fi can't be configured on this device.")}});
        return;
    }
    // Answer first: joining another network (or leaving the setup hotspot) drops this connection.
    respondJson(socket, 200, {{"ok", true}, {"message", tr("Connecting the clock to %1…").arg(ssid)}});
    const QString password = body.value("password").toString();
    QTimer::singleShot(300, m_device, [this, ssid, password] { m_device->connectWifi(ssid, password); });
}

void PairingServer::respond(QTcpSocket *socket, int status, const QByteArray &contentType, const QByteArray &body)
{
    static const QHash<int, QByteArray> reasons = {{200, "OK"},         {400, "Bad Request"}, {401, "Unauthorized"},
                                                   {403, "Forbidden"},  {404, "Not Found"},   {409, "Conflict"},
                                                   {413, "Too Large"}};
    QByteArray head = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reasons.value(status, "Error") + "\r\n";
    head += "Content-Type: " + contentType + "\r\n";
    head += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    head += "Cache-Control: no-store\r\nConnection: close\r\n\r\n";
    socket->write(head + body);
    socket->disconnectFromHost();
}

void PairingServer::respondJson(QTcpSocket *socket, int status, const QJsonObject &obj)
{
    respond(socket, status, "application/json", QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

// ---------------------------------------------------------------- server discovery

void PairingServer::scanForServers()
{
    if (m_scanPending > 0)
        return;
    QSet<QString> prefixes;
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsRunning) || (iface.flags() & QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry &e : iface.addressEntries()) {
            const QHostAddress ip = e.ip();
            if (ip.protocol() == QAbstractSocket::IPv4Protocol && !ip.isLinkLocal() && e.prefixLength() >= 16)
                prefixes.insert(ip.toString().section('.', 0, 2));
        }
    }
    m_servers.clear();
    const QString saved = m_abs->serverUrl();
    for (const QString &prefix : std::as_const(prefixes)) {
        for (int host = 1; host < 255; ++host) {
            const QString base = QStringLiteral("http://%1.%2:%3").arg(prefix).arg(host).arg(kAbsPort);
            QNetworkRequest req(QUrl(base + "/status"));
            req.setTransferTimeout(1500);
            QNetworkReply *reply = m_scanNam->get(req);
            ++m_scanPending;
            connect(reply, &QNetworkReply::finished, this, [this, reply, base, saved] {
                reply->deleteLater();
                const QJsonObject o = reply->error() == QNetworkReply::NoError
                                          ? QJsonDocument::fromJson(reply->readAll()).object()
                                          : QJsonObject();
                if (o.value("app").toString() == "audiobookshelf") {
                    const QVariantMap entry{{"url", base},
                                            {"host", QUrl(base).host() + ":" + QString::number(kAbsPort)},
                                            {"version", o.value("serverVersion").toString()}};
                    if (base == saved)
                        m_servers.prepend(entry);
                    else
                        m_servers.append(entry);
                }
                if (--m_scanPending == 0 || !m_servers.isEmpty())
                    emit serversChanged();
            });
        }
    }
    emit serversChanged();
}

// ---------------------------------------------------------------- QR

QrImageProvider::QrImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage QrImageProvider::requestImage(const QString &id, QSize *size, const QSize &)
{
    const QByteArray text = QUrl::fromPercentEncoding(id.toUtf8()).toUtf8();
    const auto qr = qrcodegen::QrCode::encodeText(text.constData(), qrcodegen::QrCode::Ecc::LOW);
    constexpr int border = 2;
    const int n = qr.getSize();
    QImage img(n + 2 * border, n + 2 * border, QImage::Format_RGB32);
    img.fill(Qt::white);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            if (qr.getModule(x, y))
                img.setPixel(x + border, y + border, qRgb(0, 0, 0));
    if (size)
        *size = img.size();
    return img;
}
