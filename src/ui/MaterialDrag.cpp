#include "MaterialDrag.h"

#include <QBuffer>
#include <QColor>
#include <QDataStream>
#include <QDir>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QFileInfo>

#include <algorithm>

namespace {
QColor to_qcolor(Color color)
{
    return QColor::fromRgbF(std::clamp(color.r, 0.0f, 1.0f),
                            std::clamp(color.g, 0.0f, 1.0f),
                            std::clamp(color.b, 0.0f, 1.0f));
}

QString resolved_texture_path(const Material& material, const QString& material_file_path)
{
    const QString texture = QString::fromStdString(material.color_texture_path).trimmed();
    if (texture.isEmpty()) {
        return {};
    }
    QFileInfo texture_info(texture);
    if (texture_info.isAbsolute() && texture_info.exists()) {
        return texture_info.absoluteFilePath();
    }

    const QString source_path = material_file_path.isEmpty()
        ? QString::fromStdString(material.source_file_path)
        : material_file_path;
    if (!source_path.isEmpty()) {
        QFileInfo material_info(source_path);
        const QString sibling_path = material_info.absoluteDir().filePath(texture);
        if (QFileInfo::exists(sibling_path)) {
            return sibling_path;
        }
    }

    if (QFileInfo::exists(texture)) {
        return QFileInfo(texture).absoluteFilePath();
    }
    return {};
}

void write_color(QDataStream& stream, Color color)
{
    stream << color.r << color.g << color.b;
}

void read_color(QDataStream& stream, Color& color)
{
    stream >> color.r >> color.g >> color.b;
}
}

namespace MaterialDrag {
const char* MimeType()
{
    return "application/x-dom3d-material";
}

QByteArray Encode(const Material& material)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << static_cast<quint32>(5);
    stream << static_cast<qulonglong>(material.id);
    stream << QString::fromStdString(material.name);
    write_color(stream, material.ambient);
    write_color(stream, material.diffuse);
    write_color(stream, material.emission);
    stream << material.alpha << material.specular << material.shininess << material.reflectivity;
    stream << material.texture_offset_u << material.texture_offset_v;
    stream << material.texture_scale_u << material.texture_scale_v;
    stream << material.texture_rotation_degrees;
    stream << material.texture_fit_to_surface;
    stream << QString::fromStdString(material.color_texture_path);
    stream << QString::fromStdString(material.light_texture_path);
    stream << QString::fromStdString(material.bump_texture_path);
    stream << QString::fromStdString(material.source_file_path);
    stream << material.roughness << material.metallic
           << material.normal_strength << material.displacement_scale;
    stream << QString::fromStdString(material.normal_texture_path)
           << QString::fromStdString(material.roughness_texture_path)
           << QString::fromStdString(material.metallic_texture_path)
           << QString::fromStdString(material.displacement_texture_path);
    stream << material.coat_weight << material.coat_roughness;
    return payload;
}

bool Decode(const QMimeData* mime_data, Material& material)
{
    if (!mime_data || !mime_data->hasFormat(MimeType())) {
        return false;
    }

    QByteArray payload = mime_data->data(MimeType());
    QDataStream stream(&payload, QIODevice::ReadOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    quint32 version = 0;
    qulonglong id = 0;
    QString name;
    stream >> version;
    if (version < 1 || version > 5) {
        return false;
    }

    stream >> id >> name;
    material.id = static_cast<unsigned long>(id);
    material.name = name.toStdString();
    read_color(stream, material.ambient);
    read_color(stream, material.diffuse);
    read_color(stream, material.emission);
    stream >> material.alpha >> material.specular >> material.shininess >> material.reflectivity;
    if (version >= 2) {
        stream >> material.texture_offset_u >> material.texture_offset_v;
        stream >> material.texture_scale_u >> material.texture_scale_v;
        stream >> material.texture_rotation_degrees;
        if (version >= 3) {
            stream >> material.texture_fit_to_surface;
        }
    }
    QString color_texture;
    QString light_texture;
    QString bump_texture;
    stream >> color_texture >> light_texture >> bump_texture;
    material.color_texture_path = color_texture.toStdString();
    material.light_texture_path = light_texture.toStdString();
    material.bump_texture_path = bump_texture.toStdString();
    if (!stream.atEnd()) {
        QString source_file_path;
        stream >> source_file_path;
        material.source_file_path = source_file_path.toStdString();
    }
    if (version >= 4) {
        stream >> material.roughness >> material.metallic
               >> material.normal_strength >> material.displacement_scale;
        QString normal_texture;
        QString roughness_texture;
        QString metallic_texture;
        QString displacement_texture;
        stream >> normal_texture >> roughness_texture
               >> metallic_texture >> displacement_texture;
        material.normal_texture_path = normal_texture.toStdString();
        material.roughness_texture_path = roughness_texture.toStdString();
        material.metallic_texture_path = metallic_texture.toStdString();
        material.displacement_texture_path = displacement_texture.toStdString();
    }
    if (version >= 5) {
        stream >> material.coat_weight >> material.coat_roughness;
    }
    return stream.status() == QDataStream::Ok;
}

QPixmap SpherePixmap(const Material& material, int size, bool selected)
{
    return SpherePixmap(material, size, selected, {});
}

QPixmap SpherePixmap(const Material& material, int size, bool selected, const QString& material_file_path)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const qreal inset = selected ? 2.0 : 6.0;
    const QRectF sphere(inset, inset, size - inset * 2.0, size - inset * 2.0);
    const QColor diffuse = to_qcolor(material.diffuse);
    const QColor ambient = to_qcolor(material.ambient);

    painter.setPen(QPen(selected ? QColor(255, 255, 255) : QColor(205, 205, 198), selected ? 3 : 2));
    const QString texture_path = resolved_texture_path(material, material_file_path);
    const QImage texture(texture_path);
    if (!texture.isNull()) {
        QPainterPath clip;
        clip.addEllipse(sphere);
        painter.save();
        painter.setClipPath(clip);
        painter.drawImage(sphere, texture);
        painter.setCompositionMode(QPainter::CompositionMode_Multiply);
        painter.fillRect(sphere, QColor::fromRgbF(diffuse.redF(), diffuse.greenF(), diffuse.blueF(), std::clamp(material.alpha, 0.25f, 1.0f)));
        painter.restore();
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(sphere);
    } else {
        QRadialGradient gradient(sphere.center() - QPointF(size * 0.17, size * 0.22), size * 0.62);
        gradient.setColorAt(0.0, diffuse.lighter(172));
        gradient.setColorAt(0.45, diffuse);
        gradient.setColorAt(1.0, ambient.darker(155));
        painter.setBrush(gradient);
        painter.drawEllipse(sphere);
    }

    QRadialGradient volume(sphere.center() - QPointF(size * 0.16, size * 0.22), size * 0.66);
    volume.setColorAt(0.0, QColor(255, 255, 255, 72));
    volume.setColorAt(0.42, QColor(255, 255, 255, 0));
    volume.setColorAt(0.78, QColor(0, 0, 0, 30));
    volume.setColorAt(1.0, QColor(0, 0, 0, 105));
    painter.setPen(Qt::NoPen);
    painter.setBrush(volume);
    painter.drawEllipse(sphere);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 62));
    painter.drawEllipse(QRectF(sphere.left() + size * 0.19, sphere.top() + size * 0.16, size * 0.15, size * 0.11));
    painter.end();
    return pixmap;
}
}
