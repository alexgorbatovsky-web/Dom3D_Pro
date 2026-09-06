#pragma once

#include "../Material.h"
#include <QString>
#include <functional>

struct PlasterBakeSettings {
    int resolution = 1024;
    float widthMm = 100.f;
    float heightMm = 100.f;
    Vec3 originMm{};
};

// Creates a new package directory. Existing files are never overwritten.
// Progress returns false to cancel; incomplete packages are removed.
bool BakePlasterMaps(const Material& material, const PlasterBakeSettings& settings,
                     const QString& parentDirectory, QString& packageDirectory,
                     QString& error,
                     const std::function<bool(int, const QString&)>& progress = {});
