#include "MeasurementUnits.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QSettings>

namespace {
constexpr double kMillimetersPerInch = 25.4;
constexpr const char* kSettingsKey = "preferences/project/lengthUnit";
constexpr const char* kNumberSeparatorSettingsKey =
    "preferences/project/numberSeparator";
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

NumberDecimalSeparator LoadNumberDecimalSeparator() {
    QSettings settings("Dom3D", "Dom3D_Pro");
    return NumberDecimalSeparatorFromKey(
        settings.value(kNumberSeparatorSettingsKey, "dot").toString());
}

void SaveNumberDecimalSeparator(NumberDecimalSeparator separator) {
    QSettings settings("Dom3D", "Dom3D_Pro");
    settings.setValue(
        kNumberSeparatorSettingsKey, NumberDecimalSeparatorKey(separator));
}

QString NumberDecimalSeparatorKey(NumberDecimalSeparator separator) {
    return separator == NumberDecimalSeparator::Comma ? "comma" : "dot";
}

QString NumberDecimalSeparatorLabel(NumberDecimalSeparator separator) {
    return separator == NumberDecimalSeparator::Comma ? "Comma (,)" : "Dot (.)";
}

NumberDecimalSeparator NumberDecimalSeparatorFromKey(const QString& key) {
    return key.compare("comma", Qt::CaseInsensitive) == 0
        ? NumberDecimalSeparator::Comma
        : NumberDecimalSeparator::Dot;
}

QLocale NumberInputLocale() {
    QLocale locale = LoadNumberDecimalSeparator() == NumberDecimalSeparator::Comma
        ? QLocale(QLocale::German, QLocale::Germany)
        : QLocale::c();
    locale.setNumberOptions(locale.numberOptions() | QLocale::OmitGroupSeparator);
    return locale;
}

void ApplyNumberInputLocale() {
    const QLocale locale = NumberInputLocale();
    QLocale::setDefault(locale);
    if (!qApp) return;
    for (QWidget* widget : QApplication::allWidgets()) {
        if (auto* spin_box = qobject_cast<QAbstractSpinBox*>(widget)) {
            spin_box->setLocale(locale);
        }
    }
}
