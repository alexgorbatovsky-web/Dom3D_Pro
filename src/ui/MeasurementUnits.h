#pragma once

#include <QString>

enum class DisplayLengthUnit {
    Millimeters,
    Inches
};

DisplayLengthUnit LoadDisplayLengthUnit();
void SaveDisplayLengthUnit(DisplayLengthUnit unit);
QString DisplayLengthUnitKey(DisplayLengthUnit unit);
QString DisplayLengthUnitLabel(DisplayLengthUnit unit);
QString DisplayLengthUnitSuffix(DisplayLengthUnit unit);
DisplayLengthUnit DisplayLengthUnitFromKey(const QString& key);
double MillimetersToDisplay(double value_mm, DisplayLengthUnit unit);
double DisplayToMillimeters(double value, DisplayLengthUnit unit);
