#pragma once

#include <QImage>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// Integrate over directions, rather than resizing an equirectangular image.
// Resizing leaves room features visible in matte reflections and diffuse light.
inline QImage PrefilterEnvironment(const QImage& source, bool diffuse)
{
    if (source.isNull()) return {};
    constexpr double pi = 3.14159265358979323846;
    const auto direction = [=](int x, int y, int width, int height) {
        const double latitude = ((y + 0.5) / height - 0.5) * pi;
        const double longitude = ((x + 0.5) / width - 0.5) * 2 * pi;
        return std::array<double, 3>{std::cos(latitude) * std::cos(longitude),
            std::sin(latitude), std::cos(latitude) * std::sin(longitude)};
    };
    struct Sample { std::array<double, 3> direction; double area; QRgb color; };
    const QImage reduced = source.scaled(64, 32, Qt::IgnoreAspectRatio,
        Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB32);
    std::vector<Sample> samples;
    for (int y = 0; y < reduced.height(); ++y) {
        const double area = std::cos(((y + 0.5) / reduced.height() - 0.5) * pi);
        for (int x = 0; x < reduced.width(); ++x)
            samples.push_back({direction(x, y, 64, 32), area, reduced.pixel(x, y)});
    }
    QImage result(128, 64, QImage::Format_RGB32);
    for (int y = 0; y < result.height(); ++y) {
        auto* row = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            const auto n = direction(x, y, result.width(), result.height());
            double red = 0, green = 0, blue = 0, total = 0;
            for (const auto& sample : samples) {
                double weight = std::max(0.0, n[0] * sample.direction[0]
                    + n[1] * sample.direction[1] + n[2] * sample.direction[2]);
                // Cosine hemisphere for diffuse; narrower lobe for the
                // intermediate rough reflection. Sharp reflections stay intact.
                if (!diffuse) { weight *= weight; weight *= weight; weight *= weight; }
                weight *= sample.area;
                red += weight * qRed(sample.color);
                green += weight * qGreen(sample.color);
                blue += weight * qBlue(sample.color);
                total += weight;
            }
            row[x] = qRgb(qRound(red / total), qRound(green / total), qRound(blue / total));
        }
    }
    return result;
}
