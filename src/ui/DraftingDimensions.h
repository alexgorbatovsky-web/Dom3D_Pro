#pragma once
#include <QColor>
#include <QJsonObject>
#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>
#include <QVector>
#include <array>

namespace drafting {
enum class DimensionKind { Horizontal, Vertical, Parallel, Perpendicular, Radius, Diameter, Angular };
QString DimensionName(DimensionKind kind);
struct Reference {
    int primitive = -1;
    unsigned long body = 0;
    int edge = -1, edge_count = 0, curve_type = -1, anchor = 0;
    double parameter = 0.0;
    bool whole_edge = false;
    QVector<std::array<double,3>> edge_points;
};
struct Dimension {
    DimensionKind kind = DimensionKind::Parallel;
    QVector<Reference> refs;
    QPointF offset{0, -10};
    QString prefix, suffix, override_text, below, upper, lower, symbol;
    QString font = "Arial";
    double height = 3.5, arrow_size = 3, line_width = 0.25;
    int precision = 2, arrows = 0, text_position = 0;
    bool outside = false, center_mark = false, center_line = true;
    QColor color = Qt::black;
};
struct Geometry {
    bool valid = false, line = false, circle = false, arc = false;
    QPointF point, a, b, center;
    double radius = 0, scale = 1;
};
struct DimensionLayout {
    bool valid = false;
    QString error, text;
    double value = 0, angle = 0;
    QPointF label;
    QPainterPath lines, arrows;
};
QJsonObject ToJson(const Dimension& dimension);
Dimension FromJson(const QJsonObject& json);
DimensionLayout Layout(const Dimension& dimension, const QVector<Geometry>& geometry);
bool FitCircle(const QPolygonF& points, QPointF& center, double& radius);
}
