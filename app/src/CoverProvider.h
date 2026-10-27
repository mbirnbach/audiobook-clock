#pragma once

#include <QCache>
#include <QImage>
#include <QObject>
#include <QQuickAsyncImageProvider>
#include <QSet>

class AbsClient;
class PaletteService;

// Fetches cover bytes on the main thread (auth header + disk cache via AbsClient), keeps a
// small in-memory cache of the encoded bytes and feeds the palette.
class CoverStore : public QObject
{
    Q_OBJECT

public:
    CoverStore(AbsClient *client, PaletteService *palette, QObject *parent = nullptr);

    Q_INVOKABLE void fetch(const QString &id);

signals:
    // Null image when the cover could not be loaded.
    void coverReady(const QString &id, const QImage &image);

private:
    AbsClient *m_client;
    PaletteService *m_palette;
    QCache<QString, QByteArray> m_bytes;
    QSet<QString> m_pending;
};

class CoverProvider : public QQuickAsyncImageProvider
{
public:
    explicit CoverProvider(CoverStore *store);
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

private:
    CoverStore *m_store;
};
