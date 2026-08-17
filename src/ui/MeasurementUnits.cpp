#include "MeasurementUnits.h"

#include <QSettings>

namespace {
constexpr double kMillimetersPerInch = 25.4;
constexpr const char* kSettingsKey = "preferences/project/lengthUnit";
}

DisplayLengthUnit LoadDisplayLengthUnit() {
    QSettings settings("Dom3D", "Dom3D_Pro");
    return DisplayLengthUnitFromKey(settings.value(kSettingsKey, "mm").toString());
}

void SaveDisplayLengthUnit(DisplayLengthUnit unit) {
    QSettings settings("Dom3D", "Dom3D_Pro");
    settings.setValue(kSettingsKey, DisplayLengthUnitKey(unit));
}

QString DisplayLengthUnitKey(DisplayLengthUnit unit) {
    return unit == DisplayLengthUnit::Inches ? "in" : "mm";
}

QString DisplayLengthUnitLabel(DisplayLengthUnit unit) {
    return unit == DisplayLengthUnit::Inches ? "Inches (in)" : "Millimeters (mm)";
}

QString DisplayLengthUnitSuffix(DisplayLengthUnit unit) {
    return unit == DisplayLengthUnit::Inches ? " in" : " mm";
}

DisplayLengthUnit DisplayLengthUnitFromKey(const QString& key) {
    return key.compare("in", Qt::CaseInsensitive) == 0 || key.compare("inch", Qt::CaseInsensitive) == 0
        ? DisplayLengthUnit::Inches
        : DisplayLengthUnit::Millimeters;
}

double MillimetersToDisplay(double value_mm, DisplayLengthUnit unit) {
    return unit == DisplayLengthUnit::Inches ? value_mm / kMillimetersPerInch : value_mm;
}

double DisplayToMillimeters(double value, DisplayLengthUnit unit) {
    return unit == DisplayLengthUnit::Inches ? value * kMillimetersPerInch : value;
}
