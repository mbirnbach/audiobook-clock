#pragma once

#include <QColor>
#include <QHash>
#include <QObject>
#include <QQuickImageProvider>
#include <QVariantMap>

class Library;

// Per-book colours (background glow, accent, generated-cover gradient), derived from the
// cover's dominant hue, using the same OKLCH recipe as the mockup.
class PaletteService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    explicit PaletteService(Library *library, QObject *parent = nullptr);

    int revision() const { return m_revision; }

    // Keys: base, a, b, acc, ink, coverTop, coverBottom (colors) and bg (image:// URL).
    Q_INVOKABLE QVariantMap forBook(const QString &id) const;

    void setFromImage(const QString &id, const QImage &image);
    bool hasImagePalette(const QString &id) const { return m_params.contains(id); }

    static QColor oklch(double L, double C, double h);

signals:
    void revisionChanged();

private:
    struct Params { double L; double chroma; double hue; };
    Params params(const QString &id) const;

    Library *m_library;
    QHash<QString, Params> m_params;
    int m_revision = 0;
};

// Renders the soft two-blob background for "image://bg/<base>_<a>_<b>" (hex colours without #).
// Painted small and upscaled by the Image, which gives the blur for free.
class BackgroundProvider : public QQuickImageProvider
{
public:
    BackgroundProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
