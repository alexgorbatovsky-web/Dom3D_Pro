#pragma once

#include "../Material.h"

#include <QByteArray>
#include <QPixmap>
#include <QString>

class QMimeData;

namespace MaterialDrag {
const char* MimeType();
QByteArray Encode(const Material& material);
bool Decode(const QMimeData* mime_data, Material& material);
QPixmap SpherePixmap(const Material& material, int size, bool selected = false);
QPixmap SpherePixmap(const Material& material, int size, bool selected, const QString& material_file_path);
}
