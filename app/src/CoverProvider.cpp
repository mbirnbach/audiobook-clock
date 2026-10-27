#include "CoverProvider.h"

#include "AbsClient.h"
#include "Palette.h"

#include <QBuffer>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>

// Covers are shown at most ~308 px wide; ask the server for a resized JPEG (no WebP plugin
// on the target).
static constexpr int kCoverWidth = 320;

CoverStore::CoverStore(AbsClient *client, PaletteService *palette, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_palette(palette)
    , m_bytes(6 * 1024) // cost in KiB
{
}

void CoverStore::fetch(const QString &id)
{
    const auto decode = [](const QByteArray &bytes) {
        QBuffer buf;
        buf.setData(bytes);
        QImageReader reader(&buf);
        reader.setAutoTransform(true);
        return reader.read();
    };
    if (const QByteArray *cached = m_bytes.object(id)) {
        emit coverReady(id, decode(*cached));
        return;
    }
    if (m_pending.contains(id))
        return;
    m_pending.insert(id);
    m_client->getBytes(QStringLiteral("/api/items/%1/cover?width=%2&format=jpeg").arg(id).arg(kCoverWidth),
                       [this, id, decode](const QByteArray &bytes, int) {
        m_pending.remove(id);
        const QImage img = bytes.isEmpty() ? QImage() : decode(bytes);
        if (!img.isNull()) {
            m_bytes.insert(id, new QByteArray(bytes), int(std::max<qsizetype>(1, bytes.size() / 1024)));
            if (!m_palette->hasImagePalette(id))
                m_palette->setFromImage(id, img);
        }
        emit coverReady(id, img);
    });
}

namespace {

class CoverResponse : public QQuickImageResponse
{
public:
    CoverResponse(CoverStore *store, const QString &id, const QSize &requestedSize, bool round)
        : m_id(id)
        , m_size(requestedSize)
        , m_round(round)
    {
        // Connected from the loader thread, so delivery is queued to this object's thread
        // and the connection dies with the response if QML cancels it.
        connect(store, &CoverStore::coverReady, this, [this](const QString &id, const QImage &img) {
            if (id != m_id || m_done)
                return;
            m_done = true;
            if (img.isNull()) {
                m_error = QStringLiteral("cover unavailable");
            } else if (m_size.isValid() && (img.width() > m_size.width() || img.height() > m_size.height())) {
                m_image = img.scaled(m_size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            } else {
                m_image = img;
            }
            if (m_round && !m_image.isNull())
                m_image = circular(m_image);
            emit finished();
        });
        QMetaObject::invokeMethod(store, "fetch", Qt::QueuedConnection, Q_ARG(QString, id));
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    QString errorString() const override { return m_error; }

private:
    // Centre square of the cover, cut to a circle (the software renderer can't mask in QML).
    static QImage circular(const QImage &src)
    {
        const int d = std::min(src.width(), src.height());
        QImage out(d, d, QImage::Format_ARGB32_Premultiplied);
        out.fill(Qt::transparent);
        QPainter p(&out);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        QPainterPath clip;
        clip.addEllipse(QRectF(0, 0, d, d));
        p.setClipPath(clip);
        p.drawImage(QPoint(-(src.width() - d) / 2, -(src.height() - d) / 2), src);
        return out;
    }

    QString m_id;
    QSize m_size;
    QImage m_image;
    QString m_error;
    bool m_round;
    bool m_done = false;
};

} // namespace

CoverProvider::CoverProvider(CoverStore *store)
    : m_store(store)
{
}

QQuickImageResponse *CoverProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    // "<bookId>" or "<bookId>/round"
    const QString bookId = id.section('/', 0, 0);
    return new CoverResponse(m_store, bookId, requestedSize, id.section('/', 1) == QLatin1String("round"));
}
