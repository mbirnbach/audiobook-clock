#include "Palette.h"

#include "Library.h"

#include <QPainter>
#include <QRadialGradient>
#include <cmath>

namespace {

double srgbEncode(double x)
{
    x = std::clamp(x, 0.0, 1.0);
    return x <= 0.0031308 ? 12.92 * x : 1.055 * std::pow(x, 1 / 2.4) - 0.055;
}

double srgbDecode(double x)
{
    return x <= 0.04045 ? x / 12.92 : std::pow((x + 0.055) / 1.055, 2.4);
}

struct Lab { double L, a, b; };

Lab toOklab(const QColor &c)
{
    const double r = srgbDecode(c.redF()), g = srgbDecode(c.greenF()), b = srgbDecode(c.blueF());
    const double l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
    const double m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
    const double s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
    return {0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
            1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
            0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s};
}

QString hex(const QColor &c)
{
    return c.name(QColor::HexRgb).mid(1);
}

} // namespace

QColor PaletteService::oklch(double L, double C, double h)
{
    const double hr = h * M_PI / 180.0;
    const double a = C * std::cos(hr), b = C * std::sin(hr);
    const double l_ = L + 0.3963377774 * a + 0.2158037573 * b;
    const double m_ = L - 0.1055613458 * a - 0.0638541728 * b;
    const double s_ = L - 0.0894841775 * a - 1.2914855480 * b;
    const double l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
    return QColor::fromRgbF(float(srgbEncode(4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s)),
                            float(srgbEncode(-1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s)),
                            float(srgbEncode(-0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s)));
}

PaletteService::PaletteService(Library *library, QObject *parent)
    : QObject(parent)
    , m_library(library)
{
}

PaletteService::Params PaletteService::params(const QString &id) const
{
    const auto it = m_params.constFind(id);
    if (it != m_params.cend())
        return *it;
    if (const Book *b = m_library->find(id); b && b->hue >= 0)
        return {0.6, 1.0, b->hue};
    // Until the cover arrives: a stable hue from the id, a little muted.
    return {0.55, 0.6, double(qHash(id) % 360)};
}

QVariantMap PaletteService::forBook(const QString &id) const
{
    QColor base, a, b;
    QVariantMap out;
    if (id.isEmpty()) {
        base = oklch(0.17, 0.01, 260);
        a = oklch(0.3, 0.04, 250);
        b = oklch(0.26, 0.04, 300);
        out.insert("acc", QColor(Qt::white));
        out.insert("ink", QColor(Qt::white));
        out.insert("coverTop", oklch(0.4, 0.02, 260));
        out.insert("coverBottom", oklch(0.2, 0.02, 260));
    } else {
        const Params p = params(id);
        const double c = p.chroma, h = p.hue, L = p.L;
        base = oklch(0.2, 0.04 * c, h);
        a = oklch(L - 0.12, 0.12 * c, h);
        b = oklch(L - 0.25, 0.1 * c, h + 50);
        out.insert("acc", oklch(0.86, 0.1 * c, h));
        out.insert("ink", oklch(0.97, 0.025 * c, h));
        out.insert("coverTop", oklch(L, 0.12 * c, h));
        out.insert("coverBottom", oklch(L - 0.3, 0.09 * c, h + 20));
    }
    out.insert("base", base);
    out.insert("a", a);
    out.insert("b", b);
    out.insert("bg", QStringLiteral("image://bg/%1_%2_%3").arg(hex(base), hex(a), hex(b)));
    return out;
}

void PaletteService::setFromImage(const QString &id, const QImage &image)
{
    if (image.isNull())
        return;
    const QImage small = image.scaled(24, 24, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                             .convertToFormat(QImage::Format_RGB32);
    // Chroma-weighted circular mean of the hue, so the colourful parts of the cover win.
    double sx = 0, sy = 0, wsum = 0, lsum = 0, csum = 0;
    const int n = small.width() * small.height();
    for (int y = 0; y < small.height(); ++y) {
        for (int x = 0; x < small.width(); ++x) {
            const Lab lab = toOklab(QColor(small.pixel(x, y)));
            const double c = std::hypot(lab.a, lab.b);
            const double w = c * c;
            sx += w * lab.a / std::max(c, 1e-6);
            sy += w * lab.b / std::max(c, 1e-6);
            wsum += w;
            lsum += lab.L;
            csum += c;
        }
    }
    Params p;
    p.hue = std::fmod(std::atan2(sy, sx) * 180.0 / M_PI + 360.0, 360.0);
    p.chroma = std::clamp((csum / n) / 0.08, 0.15, 1.0);
    p.L = std::clamp(lsum / n + 0.05, 0.5, 0.7);
    if (wsum <= 0)
        p.chroma = 0.15;
    m_params.insert(id, p);
    ++m_revision;
    emit revisionChanged();
}

BackgroundProvider::BackgroundProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage BackgroundProvider::requestImage(const QString &id, QSize *size, const QSize &)
{
    const QStringList parts = id.split('_');
    const QColor base('#' + parts.value(0, "101014"));
    const QColor a('#' + parts.value(1, "202030"));
    const QColor b('#' + parts.value(2, "201828"));

    // Quarter resolution of the 800x480 screen; geometry matches the mockup's blurred circles.
    constexpr double k = 0.25;
    QImage img(int(800 * k), int(480 * k), QImage::Format_RGB32);
    img.fill(base);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    const auto blob = [&](QPointF center, double radius, double blur, QColor c, double opacity) {
        QRadialGradient g(center * k, (radius + blur) * k);
        const double inner = std::max(0.0, (radius - blur) / (radius + blur));
        QColor c0 = c, c1 = c;
        c0.setAlphaF(float(opacity));
        c1.setAlphaF(0);
        g.setColorAt(0, c0);
        g.setColorAt(inner, c0);
        QColor mid = c;
        mid.setAlphaF(float(opacity * 0.45));
        g.setColorAt(inner + (1 - inner) * 0.5, mid);
        g.setColorAt(1, c1);
        p.setBrush(g);
        p.drawEllipse(center * k, (radius + blur) * k, (radius + blur) * k);
    };
    blob({170, 90}, 330, 90, a, 0.85);
    blob({700, 440}, 320, 100, b, 0.8);
    p.end();
    if (size)
        *size = img.size();
    return img;
}
