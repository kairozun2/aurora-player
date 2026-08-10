// Aurora Player - cover art provider for QML (image://covers/<path>).
//
// Reads embedded artwork through the core TagReader (ID3 APIC, FLAC PICTURE,
// MP4 covr, sidecar cover.jpg). When a track has no artwork at all we generate
// a deterministic abstract cover from the file path, so the library never looks
// broken or empty.
#pragma once

#include "aurora/TagReader.hpp"

#include <QColor>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QQuickImageProvider>
#include <QString>
#include <QUrl>

#include <string>
#include <vector>

namespace aurora {

class CoverImageProvider : public QQuickImageProvider {
public:
    CoverImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override {
        QString path = QUrl::fromPercentEncoding(id.toUtf8());
        const int cacheBuster = path.indexOf(QLatin1Char('#'));
        if (cacheBuster > 0) path = path.left(cacheBuster);

        QImage image;
        if (!path.isEmpty()) {
            std::vector<unsigned char> bytes;
            std::string mime;
            if (TagReader::readCover(path.toStdString(), &bytes, &mime) && !bytes.empty()) {
                image.loadFromData(reinterpret_cast<const uchar*>(bytes.data()),
                                   static_cast<int>(bytes.size()));
            }
        }
        if (image.isNull()) image = placeholder(path);

        if (requestedSize.isValid() && requestedSize.width() > 0 && requestedSize.height() > 0) {
            image = image.scaled(requestedSize, Qt::KeepAspectRatioByExpanding,
                                 Qt::SmoothTransformation);
        }
        if (size) *size = image.size();
        return image;
    }

    /// Abstract, deterministic artwork: same track always gets the same cover.
    static QImage placeholder(const QString& seed) {
        quint32 hash = 2166136261u;
        for (const QChar c : seed) {
            hash ^= static_cast<quint32>(c.unicode());
            hash *= 16777619u;
        }
        const int hue = static_cast<int>(hash % 360u);
        const QColor top = QColor::fromHsl(hue, 150, 120);
        const QColor bottom = QColor::fromHsl((hue + 42) % 360, 130, 60);

        QImage image(600, 600, QImage::Format_ARGB32_Premultiplied);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QLinearGradient gradient(0, 0, 600, 600);
        gradient.setColorAt(0.0, top);
        gradient.setColorAt(1.0, bottom);
        painter.fillRect(image.rect(), gradient);

        // Concentric rings, like light falling across a record.
        painter.setPen(Qt::NoPen);
        for (int ring = 5; ring >= 1; --ring) {
            QColor tint = ring % 2 == 0 ? QColor(255, 255, 255, 16) : QColor(0, 0, 0, 22);
            painter.setBrush(tint);
            const int r = 120 + ring * 78;
            painter.drawEllipse(QPointF(300 + (hash % 90) - 45, 300 + (hash % 70) - 35), r, r);
        }
        painter.end();
        return image;
    }

    /// Average colour plus the most vivid colour, used for the dynamic theme.
    static void palette(const QImage& source, QColor* dominant, QColor* accent) {
        if (source.isNull()) return;
        const QImage small = source.scaled(48, 48, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                   .convertToFormat(QImage::Format_ARGB32);

        qint64 r = 0, g = 0, b = 0, samples = 0;
        int bestScore = -1;
        QColor best = QColor(245, 166, 91);

        for (int y = 0; y < small.height(); ++y) {
            for (int x = 0; x < small.width(); ++x) {
                const QColor c = small.pixelColor(x, y);
                r += c.red();
                g += c.green();
                b += c.blue();
                ++samples;

                // Prefer saturated, mid-bright pixels for the accent.
                const int score = c.saturation() * 2 + (128 - std::abs(c.lightness() - 150));
                if (score > bestScore) {
                    bestScore = score;
                    best = c;
                }
            }
        }
        if (samples == 0) return;

        if (dominant) {
            QColor mean(static_cast<int>(r / samples), static_cast<int>(g / samples),
                        static_cast<int>(b / samples));
            // Darken so white text always passes contrast on the backdrop.
            *dominant = QColor::fromHsl(mean.hslHue() < 0 ? 30 : mean.hslHue(),
                                        qBound(30, mean.hslSaturation(), 190), 42);
        }
        if (accent) {
            *accent = QColor::fromHsl(best.hslHue() < 0 ? 32 : best.hslHue(),
                                      qBound(110, best.hslSaturation(), 235), 168);
        }
    }
};

} // namespace aurora
