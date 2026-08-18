#include "MaterialLibrary.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QDomDocument>
#include <QFile>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QXmlStreamWriter>

namespace {
void write_color(QXmlStreamWriter& xml, const QString& name, const Color& color) {
    xml.writeStartElement(name);
    xml.writeAttribute("r", QString::number(color.r, 'g', 9));
    xml.writeAttribute("g", QString::number(color.g, 'g', 9));
    xml.writeAttribute("b", QString::number(color.b, 'g', 9));
    xml.writeEndElement();
}

bool read_color(const QDomElement& parent, const QString& name, Color& color, QString* error) {
    const QDomElement element = parent.firstChildElement(name);
    if (element.isNull()) {
        return true;
    }

    bool ok = false;
    const float r = element.attribute("r", QString::number(color.r)).toFloat(&ok);
    if (!ok) {
        if (error) {
            *error = QString("Unsupported red value in %1.").arg(name);
        }
        return false;
    }
    const float g = element.attribute("g", QString::number(color.g)).toFloat(&ok);
    if (!ok) {
        if (error) {
            *error = QString("Unsupported green value in %1.").arg(name);
        }
        return false;
    }
    const float b = element.attribute("b", QString::number(color.b)).toFloat(&ok);
    if (!ok) {
        if (error) {
            *error = QString("Unsupported blue value in %1.").arg(name);
        }
        return false;
    }

    color = {r, g, b};
    return true;
}

float read_float_attr(const QDomElement& element, const QString& name, float fallback) {
    bool ok = false;
    const float value = element.attribute(name, QString::number(fallback)).toFloat(&ok);
    return ok ? value : fallback;
}

QString pbr_category(const QString& asset_name) {
    const QRegularExpressionMatch match =
        QRegularExpression(QStringLiteral("^([A-Za-z]+)")).match(asset_name);
    return QStringLiteral("PBR %1").arg(
        match.hasMatch() ? match.captured(1) : QStringLiteral("Materials"));
}

QString display_pbr_name(QString folder_name) {
    folder_name.remove(QRegularExpression(
        QStringLiteral("_[0-9]+K-(JPG|PNG)$"),
        QRegularExpression::CaseInsensitiveOption));
    return folder_name.replace('_', ' ');
}

void append_pbr_materials(const QString& root_path,
                          std::vector<MaterialLibrary::Entry>& entries,
                          QSet<QString>& known_directories) {
    if (!QDir(root_path).exists()) return;

    QSet<QString> directories;
    QDirIterator files(root_path, QDir::Files, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QFileInfo info(files.next());
        if (info.fileName().contains(QStringLiteral("_Color."), Qt::CaseInsensitive)) {
            directories.insert(info.absolutePath());
        }
    }

    for (const QString& directory_path : directories) {
        const QString canonical = QFileInfo(directory_path).canonicalFilePath();
        if (canonical.isEmpty() || known_directories.contains(canonical)) continue;
        known_directories.insert(canonical);

        QDir directory(directory_path);
        const QFileInfoList images = directory.entryInfoList(
            {QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
             QStringLiteral("*.png"), QStringLiteral("*.tif"),
             QStringLiteral("*.tiff")},
            QDir::Files, QDir::Name);
        const auto find_map = [&images](const QStringList& markers) {
            for (const QString& marker : markers) {
                for (const QFileInfo& image : images) {
                    if (image.completeBaseName().endsWith(marker, Qt::CaseInsensitive))
                        return image.absoluteFilePath();
                }
            }
            return QString{};
        };

        const QString color = find_map({QStringLiteral("_Color"), QStringLiteral("_BaseColor"),
                                        QStringLiteral("_Albedo")});
        if (color.isEmpty()) continue;

        Material material = Material::DefaultWhite();
        material.id = 0;
        material.name = display_pbr_name(directory.dirName()).toStdString();
        material.diffuse = {1.0f, 1.0f, 1.0f};
        material.ambient = {0.18f, 0.18f, 0.18f};
        material.specular = 1.0f;
        material.shininess = 32.0f;
        material.roughness = 0.5f;
        material.color_texture_path = color.toStdString();
        material.normal_texture_path = find_map(
            {QStringLiteral("_NormalGL"), QStringLiteral("_Normal")}).toStdString();
        material.roughness_texture_path =
            find_map({QStringLiteral("_Roughness")}).toStdString();
        material.metallic_texture_path =
            find_map({QStringLiteral("_Metalness"), QStringLiteral("_Metallic")}).toStdString();
        material.displacement_texture_path = find_map(
            {QStringLiteral("_Displacement"), QStringLiteral("_Height")}).toStdString();
        material.bump_texture_path = material.displacement_texture_path;
        material.source_file_path = color.toStdString();
        entries.push_back({pbr_category(directory.dirName()), color, std::move(material)});
    }
}
}

QString MaterialLibrary::DefaultLibraryPath() {
    return QDir(QCoreApplication::applicationDirPath()).filePath("materials");
}

QStringList MaterialLibrary::DefaultCategories() {
    return {"Default", "Plastic", "Paint", "Powder Coating", "Oracal Film",
            "Leather", "Fabric", "Metall", "Glass"};
}

bool MaterialLibrary::Load(const QString& root_path) {
    entries_.clear();

    QDir root(root_path);
    if (!root.exists()) {
        return false;
    }

    const QStringList categories = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& category : categories) {
        QDir category_dir(root.filePath(category));
        const QStringList material_files = category_dir.entryList({"*.d3mat"}, QDir::Files, QDir::Name);
        for (const QString& file_name : material_files) {
            const QString file_path = category_dir.filePath(file_name);
            Material material;
            if (LoadMaterial(file_path, material)) {
                material.source_file_path = file_path.toStdString();
                entries_.push_back({category, file_path, material});
            }
        }
    }

    QSet<QString> known_pbr_directories;
    const QString documents =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QStringList pbr_roots{
        QDir(documents).filePath(QStringLiteral("Dom3D Pro/PBR Materials")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("PBR Materials")),
        QDir(root_path).filePath(QStringLiteral("PBR Materials"))};
    for (const QString& pbr_root : pbr_roots)
        append_pbr_materials(pbr_root, entries_, known_pbr_directories);

    return true;
}

bool MaterialLibrary::SaveMaterial(const QString& file_path, const Material& material, QString* error) const {
    QSaveFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement("material");
    xml.writeAttribute("version", "1");
    xml.writeAttribute("id", QString::number(material.id));
    xml.writeAttribute("name", QString::fromStdString(material.name));
    xml.writeAttribute("alpha", QString::number(material.alpha, 'g', 9));
    xml.writeAttribute("specular", QString::number(material.specular, 'g', 9));
    xml.writeAttribute("shininess", QString::number(material.shininess, 'g', 9));
    xml.writeAttribute("reflectivity", QString::number(material.reflectivity, 'g', 9));
    xml.writeAttribute("roughness", QString::number(material.roughness, 'g', 9));
    xml.writeAttribute("metallic", QString::number(material.metallic, 'g', 9));
    xml.writeAttribute("coatWeight", QString::number(material.coat_weight, 'g', 9));
    xml.writeAttribute("coatRoughness", QString::number(material.coat_roughness, 'g', 9));
    xml.writeAttribute("normalStrength", QString::number(material.normal_strength, 'g', 9));
    xml.writeAttribute("displacementScale", QString::number(material.displacement_scale, 'g', 9));
    xml.writeAttribute("textureOffsetU", QString::number(material.texture_offset_u, 'g', 9));
    xml.writeAttribute("textureOffsetV", QString::number(material.texture_offset_v, 'g', 9));
    xml.writeAttribute("textureScaleU", QString::number(material.texture_scale_u, 'g', 9));
    xml.writeAttribute("textureScaleV", QString::number(material.texture_scale_v, 'g', 9));
    xml.writeAttribute("textureRotation", QString::number(material.texture_rotation_degrees, 'g', 9));
    xml.writeAttribute("textureFitToSurface", material.texture_fit_to_surface ? "1" : "0");
    write_color(xml, "ambient", material.ambient);
    write_color(xml, "diffuse", material.diffuse);
    write_color(xml, "emission", material.emission);
    xml.writeStartElement("textures");
    xml.writeAttribute("color", QString::fromStdString(material.color_texture_path));
    xml.writeAttribute("light", QString::fromStdString(material.light_texture_path));
    xml.writeAttribute("bump", QString::fromStdString(material.bump_texture_path));
    xml.writeAttribute("normal", QString::fromStdString(material.normal_texture_path));
    xml.writeAttribute("roughness", QString::fromStdString(material.roughness_texture_path));
    xml.writeAttribute("metallic", QString::fromStdString(material.metallic_texture_path));
    xml.writeAttribute("displacement", QString::fromStdString(material.displacement_texture_path));
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();

    if (!file.commit()) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }

    return true;
}

bool MaterialLibrary::LoadMaterial(const QString& file_path, Material& material, QString* error) const {
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }

    QDomDocument document;
    QString parse_error;
    int line = 0;
    int column = 0;
    if (!document.setContent(&file, &parse_error, &line, &column)) {
        if (error) {
            *error = QString("%1 at %2:%3").arg(parse_error).arg(line).arg(column);
        }
        return false;
    }

    const QDomElement root = document.documentElement();
    if (root.tagName() != "material") {
        if (error) {
            *error = "Material file must contain <material> root element.";
        }
        return false;
    }

    bool ok = false;
    const unsigned long id = root.attribute("id", QString::number(material.id)).toULong(&ok);
    if (ok) {
        material.id = id;
    }
    material.name = root.attribute("name", QString::fromStdString(material.name)).toStdString();
    material.alpha = read_float_attr(root, "alpha", material.alpha);
    material.specular = read_float_attr(root, "specular", material.specular);
    material.shininess = read_float_attr(root, "shininess", material.shininess);
    material.reflectivity = read_float_attr(root, "reflectivity", material.reflectivity);
    material.roughness = read_float_attr(root, "roughness", material.roughness);
    material.metallic = read_float_attr(root, "metallic", material.metallic);
    material.coat_weight = read_float_attr(root, "coatWeight", material.coat_weight);
    material.coat_roughness = read_float_attr(root, "coatRoughness", material.coat_roughness);
    material.normal_strength = read_float_attr(root, "normalStrength", material.normal_strength);
    material.displacement_scale = read_float_attr(root, "displacementScale", material.displacement_scale);
    material.texture_offset_u = read_float_attr(root, "textureOffsetU", material.texture_offset_u);
    material.texture_offset_v = read_float_attr(root, "textureOffsetV", material.texture_offset_v);
    material.texture_scale_u = read_float_attr(root, "textureScaleU", material.texture_scale_u);
    material.texture_scale_v = read_float_attr(root, "textureScaleV", material.texture_scale_v);
    material.texture_rotation_degrees = read_float_attr(root, "textureRotation", material.texture_rotation_degrees);
    material.texture_fit_to_surface = root.attribute("textureFitToSurface", "0") == "1";
    if (!read_color(root, "ambient", material.ambient, error)
        || !read_color(root, "diffuse", material.diffuse, error)
        || !read_color(root, "emission", material.emission, error)) {
        return false;
    }

    const QDomElement textures = root.firstChildElement("textures");
    if (!textures.isNull()) {
        material.color_texture_path = textures.attribute("color").toStdString();
        material.light_texture_path = textures.attribute("light").toStdString();
        material.bump_texture_path = textures.attribute("bump").toStdString();
        material.normal_texture_path = textures.attribute("normal").toStdString();
        material.roughness_texture_path = textures.attribute("roughness").toStdString();
        material.metallic_texture_path = textures.attribute("metallic").toStdString();
        material.displacement_texture_path = textures.attribute("displacement").toStdString();
    }

    material.source_file_path = file_path.toStdString();
    return true;
}

const std::vector<MaterialLibrary::Entry>& MaterialLibrary::Entries() const {
    return entries_;
}
