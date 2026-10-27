#pragma once

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QQuickImageProvider>
#include <QTcpServer>
#include <QVariantList>

class AbsClient;
class DeviceControl;
class QNetworkAccessManager;
class QTcpSocket;

// Phone pairing: while setup is open, the clock serves a small web page on the LAN (or on its
// own access point). The page asks for the 6-digit code shown on screen, then takes the
// Audiobookshelf address/credentials and Wi-Fi credentials, so nothing has to be typed on
// the 800x480 touchscreen. The password is only used for the login call and never stored.
class PairingServer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QString code READ code NOTIFY codeChanged)
    Q_PROPERTY(QString url READ url NOTIFY addressChanged)
    Q_PROPERTY(QString displayUrl READ displayUrl NOTIFY addressChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString busyMessage READ busyMessage NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QVariantList servers READ servers NOTIFY serversChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY serversChanged)

public:
    PairingServer(AbsClient *abs, DeviceControl *device, QObject *parent = nullptr);

    bool active() const { return m_server.isListening(); }
    void setActive(bool on);
    QString code() const { return m_code; }
    QString url() const;
    QString displayUrl() const;
    bool busy() const { return m_busy; }
    QString busyMessage() const { return m_busyMessage; }
    QString error() const { return m_error; }
    QVariantList servers() const { return m_servers; }
    bool scanning() const { return m_scanPending > 0; }

    // Looks for Audiobookshelf servers on the local /24 networks (port 13378).
    Q_INVOKABLE void scanForServers();
    Q_INVOKABLE void clearError();

signals:
    void activeChanged();
    void codeChanged();
    void addressChanged();
    void busyChanged();
    void errorChanged();
    void serversChanged();

private:
    struct Request
    {
        QByteArray method;
        QByteArray path;
        QByteArray query;
        QByteArray body;
    };

    void onConnection();
    void onReadyRead(QTcpSocket *socket);
    void handle(QTcpSocket *socket, const Request &req);
    void handlePair(QTcpSocket *socket, const QJsonObject &body);
    void handleWifi(QTcpSocket *socket, const QJsonObject &body);
    QJsonObject info() const;
    bool checkCode(const QString &code);
    void newCode();
    void setBusy(bool busy, const QString &message = {});
    void setError(const QString &e);
    void refreshAddress();
    static void respond(QTcpSocket *socket, int status, const QByteArray &contentType, const QByteArray &body);
    static void respondJson(QTcpSocket *socket, int status, const QJsonObject &obj);

    AbsClient *m_abs;
    DeviceControl *m_device;
    QNetworkAccessManager *m_scanNam;
    QTcpServer m_server;
    QString m_code;
    QString m_address;
    int m_failedAttempts = 0;
    bool m_busy = false;
    QString m_busyMessage;
    QString m_error;
    QVariantList m_servers;
    int m_scanPending = 0;
};

// "image://qr/<text>" renders <text> as a QR code, one pixel per module (scale it up in QML
// with smooth: false).
class QrImageProvider : public QQuickImageProvider
{
public:
    QrImageProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
