#include "MaterialPreviewGL.h"
#include "../materials/ProceduralMaterialIO.h"
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
    stream << static_cast<quint32>(8);
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
    stream << EncodePlaster(material);
    stream << EncodeFabric(material) << material.texture_wrap_object;
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
    if (version < 1 || version > 8) {
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
    material.plaster={};
    if(version>=6){QString text;stream>>text;if(!DecodePlaster(text,material))return false;}
    if(version>=7){QString text;stream>>text;if(!DecodeFabric(text,material))return false;}
    if(version>=8)stream>>material.texture_wrap_object;
    return stream.status() == QDataStream::Ok;
}

QPixmap SpherePixmap(const Material& material, int size, bool selected)
{
    return SpherePixmap(material, size, selected, {});
}

QPixmap SpherePixmap(const Material& material, int size, bool selected, const QString& material_file_path)
{
    Material preview=material;if(!material_file_path.isEmpty())preview.source_file_path=material_file_path.toStdString();
    QImage image=RenderMaterialSphereGL(preview,size);
    QPixmap pixmap=size>0?QPixmap(size,size):QPixmap();pixmap.fill(QColor(28,30,33));
    QPainter painter(&pixmap);
    if(!image.isNull())painter.drawImage(0,0,image);
    else {painter.setPen(Qt::gray);painter.drawText(pixmap.rect(),Qt::AlignCenter,"GL unavailable");}
    if(selected){painter.setPen(QPen(Qt::white,2));painter.drawRect(pixmap.rect().adjusted(1,1,-2,-2));}
    return pixmap;
}
}
