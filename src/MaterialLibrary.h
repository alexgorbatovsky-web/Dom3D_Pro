#pragma once

#include "Material.h"

#include <QString>
#include <QStringList>

#include <vector>

class MaterialLibrary {
public:
    struct Entry {
        QString category;
        QString file_path;
        Material material;
    };

    static QString DefaultLibraryPath();
    static QStringList DefaultCategories();

    bool Load(const QString& root_path);
    bool SaveMaterial(const QString& file_path, const Material& material, QString* error = nullptr) const;
    bool LoadMaterial(const QString& file_path, Material& material, QString* error = nullptr) const;

    const std::vector<Entry>& Entries() const;

private:
    std::vector<Entry> entries_;
};
