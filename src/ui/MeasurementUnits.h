#pragma once

#include <QLocale>
#include <QString>

enum class DisplayLengthUnit {
    Millimeters,
    Inches
};

enum class NumberDecimalSeparator {
    Dot,
    Comma
};

DisplayLengthUnit LoadDisplayLengthUnit();
void SaveDisplayLengthUnit(DisplayLengthUnit unit);
QString DisplayLengthUnitKey(DisplayLengthUnit unit);
QString DisplayLengthUnitLabel(DisplayLengthUnit unit);
QString DisplayLengthUnitSuffix(DisplayLengthUnit unit);
DisplayLengthUnit DisplayLengthUnitFromKey(const QString& key);
double MillimetersToDisplay(double value_mm, DisplayLengthUnit unit);
double DisplayToMillimeters(double value, DisplayLengthUnit unit);

NumberDecimalSeparator LoadNumberDecimalSeparator();
void SaveNumberDecimalSeparator(NumberDecimalSeparator separator);
QString NumberDecimalSeparatorKey(NumberDecimalSeparator separator);
QString NumberDecimalSeparatorLabel(NumberDecimalSeparator separator);
NumberDecimalSeparator NumberDecimalSeparatorFromKey(const QString& key);
QLocale NumberInputLocale();
void ApplyNumberInputLocale();
