#include "DraftingWorkspace.h"
#include "DraftingDimensions.h"
#include "DraftingDimensionReferences.h"
#include <QColorDialog>
#include <QCursor>
#include <QSet>
#include <QDataStream>
#include <QFontMetricsF>
#include <QSpinBox>
#include <QGraphicsPathItem>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopExp.hxx>

#include "../CAlfaDoc.h"
#include "../solid/Solid.h"

#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFontComboBox>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsItem>
#include <QGraphicsItemGroup>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QShowEvent>
#include <QTabWidget>
#include <QToolBar>
#include <QToolTip>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QPolygonF>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <HLRAlgo_Projector.hxx>
#include <HLRBRep_Algo.hxx>
#include <HLRBRep_HLRToShape.hxx>
#include <HLRBRep_PolyAlgo.hxx>
#include <HLRBRep_PolyHLRToShape.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
struct Primitive {
    int id = 0;
    QString type;
    QPointF p1;
    QPointF p2;
    QString text;
    int ref1 = -1;
    int anchor1 = -1;
    int ref2 = -1;
    int anchor2 = -1;
    QString font = "Arial";
    double text_height = 5.0;
    double rotation = 0.0;
    drafting::Dimension dimension;
    bool has_dimension = false;
    QPointF view_center;
    bool has_view_center = false;
    bool has_center_lines = false;
    QString projection;
    double view_scale = 1.0;
    bool show_hidden = false;
    QVector<QPolygonF> visible_lines;
    QVector<QPolygonF> hidden_lines;
    QVector<QPolygonF> center_lines;
};

struct SheetData {
    QString name = "Draft";
    QString format = "A4";
    QSizeF size_mm{210.0, 297.0};
    bool landscape = false;
    QString stamp = "1-й лист";
    bool stamp_visible = true;
    double scale = 1.0;
    int next_primitive_id = 1;
    QVector<Primitive> primitives;
};

QSizeF format_size(const QString& format) {
    if (format == "A0") return {841.0, 1189.0};
    if (format == "A1") return {594.0, 841.0};
    if (format == "A2") return {420.0, 594.0};
    if (format == "A3") return {297.0, 420.0};
    return {210.0, 297.0};
}

QJsonObject primitive_json(const Primitive& p) {
    const auto write_lines = [](const QVector<QPolygonF>& lines) {
        QJsonArray result;
        for (const QPolygonF& line : lines) {
            QJsonArray points;
            for (QPointF point : line)
                points.push_back(QJsonArray{point.x(), point.y()});
            result.push_back(points);
        }
        return result;
    };
    return {{"id", p.id}, {"type", p.type},
            {"x1", p.p1.x()}, {"y1", p.p1.y()},
            {"x2", p.p2.x()}, {"y2", p.p2.y()}, {"text", p.text},
            {"ref1", p.ref1}, {"anchor1", p.anchor1},
            {"ref2", p.ref2}, {"anchor2", p.anchor2},
            {"font", p.font}, {"textHeight", p.text_height},
            {"rotation", p.rotation}, {"projection", p.projection},
            {"dimension", p.has_dimension ? QJsonValue(drafting::ToJson(p.dimension)) : QJsonValue()},
            {"viewCenterX", p.view_center.x()}, {"viewCenterY", p.view_center.y()},
            {"viewScale", p.view_scale},
            {"showHidden", p.show_hidden},
            {"visibleLines", write_lines(p.visible_lines)},
            {"hiddenLines", write_lines(p.hidden_lines)},
            {"centerLines", write_lines(p.center_lines)}, {"centerLinesVersion", 2}};
}

Primitive primitive_from_json(const QJsonObject& object) {
    Primitive p;
    p.id = object.value("id").toInt();
    p.has_dimension = object.value("dimension").isObject();
    if (p.has_dimension) p.dimension = drafting::FromJson(object.value("dimension").toObject());
    p.has_view_center = object.contains("viewCenterX") && object.contains("viewCenterY");
    // Saved view geometry may predate recovered/edited solid axes even when
    // its format version is current. Revalidate once when opening the sheet.
    p.has_center_lines = false;
    p.view_center = {object.value("viewCenterX").toDouble(), object.value("viewCenterY").toDouble()};
    p.type = object.value("type").toString();
    p.p1 = {object.value("x1").toDouble(), object.value("y1").toDouble()};
    p.p2 = {object.value("x2").toDouble(), object.value("y2").toDouble()};
    p.text = object.value("text").toString();
    p.ref1 = object.value("ref1").toInt(-1);
    p.anchor1 = object.value("anchor1").toInt(-1);
    p.ref2 = object.value("ref2").toInt(-1);
    p.anchor2 = object.value("anchor2").toInt(-1);
    p.font = object.value("font").toString("Arial");
    p.text_height = object.value("textHeight").toDouble(5.0);
    p.rotation = object.value("rotation").toDouble();
    p.projection = object.value("projection").toString();
    p.view_scale = object.value("viewScale").toDouble(1.0);
    const auto read_lines = [](const QJsonArray& source) {
        QVector<QPolygonF> result;
        for (const QJsonValue& line_value : source) {
            QPolygonF line;
            for (const QJsonValue& point_value : line_value.toArray()) {
                const QJsonArray point = point_value.toArray();
                if (point.size() >= 2)
                    line.push_back({point[0].toDouble(), point[1].toDouble()});
            }
            if (line.size() >= 2) result.push_back(line);
        }
        return result;
    };
    p.visible_lines = read_lines(object.value("visibleLines").toArray());
    p.hidden_lines = read_lines(object.value("hiddenLines").toArray());
    p.center_lines = read_lines(object.value("centerLines").toArray());
    p.show_hidden = object.value("showHidden").toBool(!p.hidden_lines.isEmpty());
    return p;
}

class DrawingParametersDialog final : public QDialog {
public:
    explicit DrawingParametersDialog(QWidget* parent) : QDialog(parent) {
        setWindowTitle(QString::fromUtf8("Параметры чертежа"));
        auto* root = new QVBoxLayout(this);
        auto* form = new QFormLayout;
        name_ = new QLineEdit("Draft", this);
        format_ = new QComboBox(this);
        format_->addItems({"A4 210x297", "A3 297x420", "A2 420x594",
                           "A1 594x841", "A0 841x1189"});
        form->addRow(QString::fromUtf8("Имя"), name_);
        form->addRow(QString::fromUtf8("Формат"), format_);
        root->addLayout(form);

        auto* orientation_box = new QGroupBox(QString::fromUtf8("Ориентация"), this);
        auto* orientation = new QHBoxLayout(orientation_box);
        portrait_ = new QRadioButton(QString::fromUtf8("Книжная"), orientation_box);
        landscape_ = new QRadioButton(QString::fromUtf8("Альбомная"), orientation_box);
        portrait_->setChecked(true);
        orientation->addWidget(portrait_);
        orientation->addStretch();
        orientation->addWidget(landscape_);
        root->addWidget(orientation_box);

        auto* bottom = new QFormLayout;
        stamp_ = new QComboBox(this);
        stamp_->addItems({QString::fromUtf8("1-й лист"),
                          QString::fromUtf8("Последующие листы")});
        stamp_visible_ = new QCheckBox(QString::fromUtf8("Штамп виден"), this);
        stamp_visible_->setChecked(true);
        auto* stamp_row = new QWidget(this);
        auto* stamp_layout = new QHBoxLayout(stamp_row);
        stamp_layout->setContentsMargins(0, 0, 0, 0);
        stamp_layout->addWidget(stamp_);
        stamp_layout->addWidget(stamp_visible_);
        scale_ = new QComboBox(this);
        scale_->setEditable(true);
        scale_->addItems({"1:1", "1:2", "1:5", "1:10", "1:20", "1:50", "1:100"});
        bottom->addRow(QString::fromUtf8("Угловой штамп"), stamp_row);
        bottom->addRow(QString::fromUtf8("Масштаб"), scale_);
        root->addLayout(bottom);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        root->addWidget(buttons);
        setMinimumWidth(390);
    }

    SheetData data() const {
        SheetData result;
        result.name = name_->text().trimmed().isEmpty() ? "Draft" : name_->text().trimmed();
        result.format = format_->currentText().left(2);
        result.size_mm = format_size(result.format);
        result.landscape = landscape_->isChecked();
        result.stamp = stamp_->currentText();
        result.stamp_visible = stamp_visible_->isChecked();
        const QStringList scale_parts = scale_->currentText().split(':');
        bool ok = false;
        const double denominator = scale_parts.size() == 2 ? scale_parts[1].toDouble(&ok) : 1.0;
        result.scale = ok && denominator > 0.0 ? denominator : 1.0;
        return result;
    }

private:
    QLineEdit* name_ = nullptr;
    QComboBox* format_ = nullptr;
    QRadioButton* portrait_ = nullptr;
    QRadioButton* landscape_ = nullptr;
    QComboBox* stamp_ = nullptr;
    QCheckBox* stamp_visible_ = nullptr;
    QComboBox* scale_ = nullptr;
};

class DraftingTextDialog final : public QDialog {
public:
    explicit DraftingTextDialog(QWidget* parent) : QDialog(parent) {
        setWindowTitle(QString::fromUtf8("Создание текста"));
        auto* root = new QVBoxLayout(this);
        auto* form = new QFormLayout;
        text_ = new QPlainTextEdit(this);
        text_->setPlainText("Text");
        text_->setMinimumHeight(105);
        font_ = new QFontComboBox(this);
        height_ = new QDoubleSpinBox(this);
        height_->setRange(0.5, 500.0);
        height_->setDecimals(3);
        height_->setValue(5.0);
        height_->setSuffix(" mm");
        rotation_ = new QDoubleSpinBox(this);
        rotation_->setRange(-360.0, 360.0);
        rotation_->setDecimals(1);
        rotation_->setSuffix(QString::fromUtf8("°"));
        form->addRow(QString::fromUtf8("Текст:"), text_);
        form->addRow(QString::fromUtf8("Шрифт:"), font_);
        form->addRow(QString::fromUtf8("Высота:"), height_);
        form->addRow(QString::fromUtf8("Поворот:"), rotation_);
        root->addLayout(form);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        root->addWidget(buttons);
        resize(430, 300);
    }

    QString text() const { return text_->toPlainText(); }
    QString font() const { return font_->currentFont().family(); }
    double height() const { return height_->value(); }
    double rotation() const { return rotation_->value(); }

private:
    QPlainTextEdit* text_ = nullptr;
    QFontComboBox* font_ = nullptr;
    QDoubleSpinBox* height_ = nullptr;
    QDoubleSpinBox* rotation_ = nullptr;
};

class ModelViewDialog final : public QDialog {
public:
    explicit ModelViewDialog(QWidget* parent) : QDialog(parent) {
        setWindowTitle(QString::fromUtf8("Создание вида модели"));
        auto* root = new QVBoxLayout(this);
        auto* form = new QFormLayout;
        projection_ = new QComboBox(this);
        projection_->addItem(QString::fromUtf8("Спереди"), "front");
        projection_->addItem(QString::fromUtf8("Сверху"), "top");
        projection_->addItem(QString::fromUtf8("Справа"), "right");
        projection_->addItem(QString::fromUtf8("Изометрия"), "isometric");
        scale_ = new QComboBox(this);
        scale_->setEditable(true);
        scale_->addItems({"1:1", "1:2", "1:5", "1:10", "1:20", "1:50", "1:100"});
        hidden_ = new QCheckBox(QString::fromUtf8("Показывать скрытые линии"), this);
        form->addRow(QString::fromUtf8("Проекция:"), projection_);
        form->addRow(QString::fromUtf8("Масштаб:"), scale_);
        form->addRow(QString(), hidden_);
        root->addLayout(form);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        root->addWidget(buttons);
        setMinimumWidth(360);
    }

    QString projection() const { return projection_->currentData().toString(); }
    double scale() const {
        const QStringList parts = scale_->currentText().split(':');
        bool ok = false;
        const double value = parts.size() == 2 ? parts[1].toDouble(&ok) : 1.0;
        return ok && value > 0.0 ? value : 1.0;
    }
    bool hidden() const { return hidden_->isChecked(); }

private:
    QComboBox* projection_ = nullptr;
    QComboBox* scale_ = nullptr;
    QCheckBox* hidden_ = nullptr;
};

QVector<QPolygonF> extract_hlr_polylines(const TopoDS_Shape& shape) {
    QVector<QPolygonF> lines;
    if (shape.IsNull()) return lines;
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
        BRepAdaptor_Curve curve(edge);
        const double first = curve.FirstParameter();
        const double last = curve.LastParameter();
        if (!std::isfinite(first) || !std::isfinite(last)) continue;
        const int samples = curve.GetType() == GeomAbs_Line ? 2 : 32;
        QPolygonF polyline;
        for (int index = 0; index < samples; ++index) {
            const double t = samples == 1 ? first
                : first + (last - first) * index / static_cast<double>(samples - 1);
            const gp_Pnt point = curve.Value(t);
            polyline.push_back({point.X(), point.Y()});
        }
        if (polyline.size() >= 2) lines.push_back(polyline);
    }
    return lines;
}

bool generate_model_view(const CAlfaDoc* document,
                         const QString& projection,
                         double scale,
                         bool include_hidden,
                         QVector<QPolygonF>& visible,
                         QVector<QPolygonF>& hidden,
                         QString& error, QPointF* output_center = nullptr,
                         QVector<QPolygonF>* centerlines = nullptr) {
    if (centerlines) centerlines->clear();
    visible.clear();
    hidden.clear();
    error.clear();
    if (!std::isfinite(scale) || scale <= 0.0) {
        error = QString::fromUtf8("Масштаб вида должен быть положительным.");
        return false;
    }
    if (!document) {
        error = QString::fromUtf8("Документ модели недоступен.");
        return false;
    }
    Handle(HLRBRep_Algo) algorithm = new HLRBRep_Algo();
    std::vector<TopoDS_Shape> shapes;
    bool spline_outlines = false;
    int shape_count = 0;
    for (const auto& object : document->GetObjects()) {
        const auto* solid = object ? dynamic_cast<const CSolid*>(object.get()) : nullptr;
        if (!solid || solid->m_Shape.IsNull() || !solid->IsVisible()) continue;
        algorithm->Add(solid->m_Shape);
        shapes.push_back(solid->m_Shape);
        for (TopExp_Explorer face(solid->m_Shape, TopAbs_FACE); face.More(); face.Next()) {
            const auto type = BRepAdaptor_Surface(TopoDS::Face(face.Current())).GetType();
            spline_outlines = spline_outlines || type == GeomAbs_BSplineSurface
                || type == GeomAbs_BezierSurface;
        }
        ++shape_count;
    }
    if (shape_count == 0) {
        error = QString::fromUtf8("В модели нет видимых Solid-тел.");
        return false;
    }

    gp_Dir direction(0.0, -1.0, 0.0);
    gp_Dir x_direction(1.0, 0.0, 0.0);
    if (projection == "top") {
        direction = gp_Dir(0.0, 0.0, 1.0);
        x_direction = gp_Dir(1.0, 0.0, 0.0);
    } else if (projection == "right") {
        direction = gp_Dir(1.0, 0.0, 0.0);
        x_direction = gp_Dir(0.0, 1.0, 0.0);
    } else if (projection == "isometric") {
        direction = gp_Dir(1.0, -1.0, 1.0);
        x_direction = gp_Dir(1.0, 1.0, 0.0);
    }

    try {
        const HLRAlgo_Projector projector(gp_Ax2(gp_Pnt(0, 0, 0), direction, x_direction));
        algorithm->Projector(projector);
        algorithm->Update();
        algorithm->Hide();
        HLRBRep_HLRToShape result(algorithm);
        visible = extract_hlr_polylines(result.VCompound());
        // HLR separates sharp CAD edges from view-dependent outlines. A
        // cylinder or swept face has no topological edges at its silhouette.
        if (include_hidden) {
            hidden = extract_hlr_polylines(result.HCompound());
        }
        if (spline_outlines) {
            // Exact HLR can extrapolate swept B-spline outlines outside the
            // face (Wire's isometric view). Use tessellation-based HLR only
            // for silhouettes; retain the exact CAD edge curves above.
            // Triangulate private copies, independently of viewport/Low Poly
            // meshes. Deflection is 0.02 mm on paper, below the line width.
            Handle(HLRBRep_PolyAlgo) poly = new HLRBRep_PolyAlgo();
            poly->Projector(projector);
            for (const auto& shape : shapes) {
                TopoDS_Shape copy = BRepBuilderAPI_Copy(shape).Shape();
                BRepMesh_IncrementalMesh mesher(copy, 0.02 * scale, false, 0.08, true);
                if (!mesher.IsDone()) {
                    error = QString::fromUtf8("Не удалось построить очерк поверхности.");
                    return false;
                }
                poly->Load(copy);
            }
            poly->Update();
            HLRBRep_PolyHLRToShape outline;
            outline.Update(poly);
            visible += extract_hlr_polylines(outline.OutLineVCompound());
            if (include_hidden) hidden += extract_hlr_polylines(outline.OutLineHCompound());
        } else {
            visible += extract_hlr_polylines(result.OutLineVCompound());
            if (include_hidden) hidden += extract_hlr_polylines(result.OutLineHCompound());
        }
    } catch (const Standard_Failure& failure) {
        error = QString::fromUtf8("Open Cascade не смог построить вид: %1")
                    .arg(QString::fromLocal8Bit(failure.GetMessageString()));
        return false;
    }
    if (visible.isEmpty()) {
        error = QString::fromUtf8("Open Cascade не вернул видимых рёбер.");
        return false;
    }

    double min_x = std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::lowest();
    double max_y = std::numeric_limits<double>::lowest();
    const auto include_bounds = [&min_x, &min_y, &max_x, &max_y](const QVector<QPolygonF>& lines) {
        for (const QPolygonF& line : lines) {
            for (QPointF point : line) {
                min_x = std::min(min_x, point.x());
                min_y = std::min(min_y, point.y());
                max_x = std::max(max_x, point.x());
                max_y = std::max(max_y, point.y());
            }
        }
    };
    include_bounds(visible);
    include_bounds(hidden);
    const QPointF center((min_x + max_x) * 0.5, (min_y + max_y) * 0.5);
    if (output_center) *output_center = center;
    const auto normalize = [center, scale](QVector<QPolygonF>& lines) {
        for (QPolygonF& line : lines)
            for (QPointF& point : line)
                point = {(point.x() - center.x()) / scale,
                         -(point.y() - center.y()) / scale};
    };
    normalize(visible);
    normalize(hidden);
    if (centerlines) {
        const gp_Ax2 frame(gp_Pnt(0, 0, 0), direction, x_direction);
        const auto project = [&](const gp_Pnt& p) {
            return QPointF((p.XYZ().Dot(frame.XDirection().XYZ()) - center.x()) / scale,
                          -(p.XYZ().Dot(frame.YDirection().XYZ()) - center.y()) / scale);
        };
        const auto append = [&](const QPolygonF& line) {
            for (const auto& other : *centerlines) {
                if (other.size() != line.size()) continue;
                bool same = true, reversed = true;
                for (int i = 0; i < line.size(); ++i) {
                    same &= QLineF(other[i], line[i]).length() < 1e-5;
                    reversed &= QLineF(other[other.size() - 1 - i], line[i]).length() < 1e-5;
                }
                if (same || reversed) return;
            }
            centerlines->push_back(line);
        };
        for (const auto& object : document->GetObjects()) {
            const auto* solid = dynamic_cast<const CSolid*>(object.get());
            if (!solid || !solid->IsVisible() || solid->m_Shape.IsNull()) continue;
            for (const auto& source : solid->GetCenterlines()) {
                QPolygonF line;
                for (const auto& p : source.points) {
                    const auto q = project(p);
                    if (line.isEmpty() || QLineF(line.back(), q).length() > 1e-6) line.push_back(q);
                }
                if (line.isEmpty()) continue;
                if (line.size() == 1 && source.kind != SolidCenterlineKind::Path) {
                    // Looking down an axis: cross the associated circular surface,
                    // extending each end by 5% of its diameter.
                    const auto p = line.front();
                    double radius = 0;
                    for (TopExp_Explorer faces(solid->m_Shape, TopAbs_FACE); faces.More(); faces.Next()) {
                        BRepAdaptor_Surface surface(TopoDS::Face(faces.Current()));
                        if (surface.GetType() != GeomAbs_Cylinder) continue;
                        const auto cylinder = surface.Cylinder();
                        if (!cylinder.Axis().Direction().IsParallel(direction, 1e-6)) continue;
                        if (QLineF(project(cylinder.Location()), p).length() > 1e-5) continue;
                        radius = std::max(radius, cylinder.Radius() / scale);
                    }
                    const double extent = radius > 1e-6 ? radius * 1.1 : 3.0;
                    append(QPolygonF(QVector<QPointF>{p - QPointF(extent, 0), p + QPointF(extent, 0)}));
                    append(QPolygonF(QVector<QPointF>{p - QPointF(0, extent), p + QPointF(0, extent)}));
                } else if (line.size() >= 2) {
                    if (source.closed) {
                        if (QLineF(line.front(), line.back()).length() > 1e-6) line.push_back(line.front());
                    } else {
                        const auto first = line[1] - line.front();
                        const auto last = line.back() - line[line.size() - 2];
                        double length = 0;
                        for (int i = 1; i < line.size(); ++i) length += QLineF(line[i-1], line[i]).length();
                        line.front() -= first * (0.05 * length / std::hypot(first.x(), first.y()));
                        line.back() += last * (0.05 * length / std::hypot(last.x(), last.y()));
                    }
                    append(line);
                }
            }
        }
    }
    return true;
}
}

class DraftingScene final : public QGraphicsScene {
public:
    explicit DraftingScene(SheetData* data, const CAlfaDoc* document, QObject* parent = nullptr)
        : QGraphicsScene(parent), data_(data), document_(document) {
        UpdatePage();
        Rebuild();
    }

    std::function<void()> changed;
    std::function<void(const QString&)> dimensionPromptChanged;

    void SetTool(DraftingWorkspace::Tool tool) {
        CancelDimension();
        tool_ = tool;
        clearSelection();
        if (IsDimensionTool()) ShowDimensionPrompt();
        else {
            for(auto* view:views()) {view->unsetCursor();view->setToolTip({});view->setStatusTip({});}
            if(dimensionPromptChanged)dimensionPromptChanged({});
            QToolTip::hideText();
        }
    }

    bool DeleteSelectedPrimitives() {
        if (selectedItems().isEmpty()) return false;
        QVector<int> indices;
        QVector<int> deleted_ids;
        for (QGraphicsItem* item : selectedItems()) indices.push_back(item->data(0).toInt());
        std::sort(indices.begin(), indices.end(), std::greater<int>());
        indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
        for (int index : indices) {
            if (index >= 0 && index < data_->primitives.size())
                deleted_ids.push_back(data_->primitives[index].id);
        }
        for (int index : indices) {
            if (index >= 0 && index < data_->primitives.size()) data_->primitives.removeAt(index);
        }
        data_->primitives.erase(
            std::remove_if(data_->primitives.begin(), data_->primitives.end(),
                [&deleted_ids](const Primitive& primitive) {
                    if (primitive.type != "dimension") return false;
                    if (deleted_ids.contains(primitive.ref1) || deleted_ids.contains(primitive.ref2)) return true;
                    for (const auto& ref : primitive.dimension.refs) if (deleted_ids.contains(ref.primitive)) return true;
                    return false;
                }), data_->primitives.end());
        Rebuild();
        if (changed) changed();
        return true;
    }

    void UpdatePage() {
        QSizeF size = data_->size_mm;
        if (data_->landscape) size.transpose();
        paper_rect_ = QRectF(0.0, 0.0, size.width(), size.height());
        // Keep a generous pasteboard around the sheet so both scrollbars
        // remain useful even when the paper itself fits inside the viewport.
        setSceneRect(paper_rect_.adjusted(-size.width(), -size.height(),
                                          size.width(), size.height()));
        update();
    }

    QRectF PaperRect() const { return paper_rect_; }

    void Rebuild() {
        clear();
        dimension_preview_ = nullptr;
        dimension_snap_ = nullptr;
        preview_item_ = nullptr;
        // QGraphicsScene::clear() invalidates the automatically calculated
        // scene rectangle. Restore the physical paper rectangle explicitly.
        UpdatePage();
        for (Primitive& primitive : data_->primitives) {
            if (primitive.type == "model_view") {
                if((!primitive.has_view_center || !primitive.has_center_lines) && document_) {
                    QString error;
                    primitive.has_view_center=generate_model_view(document_,primitive.projection,primitive.view_scale,
                        primitive.show_hidden,primitive.visible_lines,primitive.hidden_lines,error,&primitive.view_center,&primitive.center_lines);
                    primitive.has_center_lines = primitive.has_view_center;
                }
                ClampModelViewToPaper(primitive);
            }
        }
        BuildDimensionFeatures();
        AddPaperItems();
        const QPen pen(Qt::black, 0.35);
        for (int i = 0; i < data_->primitives.size(); ++i) {
            const Primitive& p = data_->primitives[i];
            QGraphicsItem* item = nullptr;
            if (p.type == "line") {
                item = addLine(QLineF(p.p1, p.p2), pen);
            } else if (p.type == "rectangle") {
                item = addRect(QRectF(p.p1, p.p2).normalized(), pen);
            } else if (p.type == "ellipse") {
                item = addEllipse(QRectF(p.p1, p.p2).normalized(), pen);
            } else if (p.type == "text") {
                QFont text_font(p.font);
                text_font.setPixelSize(std::max(1, qRound(p.text_height)));
                auto* text = addText(p.text, text_font);
                text->setDefaultTextColor(Qt::black);
                text->setPos(p.p1);
                text->setTransformOriginPoint(0, 0);
                text->setRotation(p.rotation);
                item = text;
            } else if (p.type == "model_view") {
                auto* group = createItemGroup({});
                const QPen visible_pen(Qt::black, 0.3);
                const QPen hidden_pen(QColor(100, 100, 100), 0.25, Qt::DashLine);
                // QPainterPath has to be built point-by-point; keep each OCCT
                // edge separate so gaps and hidden-line styles remain exact.
                for (const QPolygonF& line : p.visible_lines) {
                    if (line.size() < 2) continue;
                    QPainterPath path(p.p1 + line.front());
                    for (int point = 1; point < line.size(); ++point) path.lineTo(p.p1 + line[point]);
                    group->addToGroup(addPath(path, visible_pen));
                }
                for (const QPolygonF& line : p.hidden_lines) {
                    if (line.size() < 2) continue;
                    QPainterPath path(p.p1 + line.front());
                    for (int point = 1; point < line.size(); ++point) path.lineTo(p.p1 + line[point]);
                    group->addToGroup(addPath(path, hidden_pen));
                }
                QPen center_pen(Qt::black, 0.18, Qt::DashDotLine);
                center_pen.setDashPattern({12.0 / 0.18, 2.0 / 0.18, 1.0 / 0.18, 2.0 / 0.18});
                for (const auto& line : p.center_lines) {
                    if (line.size() < 2) continue;
                    QPainterPath path(p.p1 + line.front());
                    for (int point = 1; point < line.size(); ++point) path.lineTo(p.p1 + line[point]);
                    auto* axis = addPath(path, center_pen);
                    axis->setData(1, "centerline");
                    group->addToGroup(axis);
                }
                item = group;
            } else if (p.type == "dimension") {
                item = DrawDimension(p);
            }
            if (item) {
                item->setFlag(QGraphicsItem::ItemIsSelectable);
                item->setFlag(QGraphicsItem::ItemIsMovable);
                item->setData(0, i);
            }
        }
        update();
    }

protected:
    void drawBackground(QPainter* painter, const QRectF&) override {
        // The pasteboard is supplied by QGraphicsView.  Paper, frame and
        // stamp are real scene items so they remain visible on screen and
        // are rendered identically by print preview, PDF and a plotter.
        Q_UNUSED(painter);
    }

    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override {
        if (tool_ == DraftingWorkspace::Tool::Select && EditDimensionAt(event->scenePos())) {event->accept();return;}
        QGraphicsScene::mouseDoubleClickEvent(event);
    }
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        if (IsDimensionTool()) { DimensionClick(event); return; }
        if (tool_ == DraftingWorkspace::Tool::Select) {
            QGraphicsScene::mousePressEvent(event);
            return;
        }
        start_ = event->scenePos();
        dimension_start_ref_ = -1;
        dimension_start_anchor_ = -1;
        if (tool_ == DraftingWorkspace::Tool::Dimension) {
            QPointF snapped;
            if (!FindSnap(start_, dimension_start_ref_, dimension_start_anchor_, snapped)) {
                QToolTip::showText(event->screenPos(),
                    QString::fromUtf8("Укажите опорную точку элемента чертежа"));
                return;
            }
            start_ = snapped;
        }
        if (tool_ == DraftingWorkspace::Tool::Text) {
            DraftingTextDialog dialog(QApplication::activeWindow());
            if (dialog.exec() == QDialog::Accepted && !dialog.text().isEmpty()) {
                Primitive primitive;
                primitive.id = data_->next_primitive_id++;
                primitive.type = "text";
                primitive.p1 = start_;
                primitive.p2 = start_;
                primitive.text = dialog.text();
                primitive.font = dialog.font();
                primitive.text_height = dialog.height();
                primitive.rotation = dialog.rotation();
                data_->primitives.push_back(primitive);
                Rebuild();
                if (changed) changed();
            }
            return;
        }
        if (tool_ == DraftingWorkspace::Tool::ModelView) {
            ModelViewDialog dialog(QApplication::activeWindow());
            if (dialog.exec() != QDialog::Accepted) return;
            Primitive primitive;
            primitive.id = data_->next_primitive_id++;
            primitive.type = "model_view";
            primitive.p1 = start_;
            primitive.p2 = start_;
            primitive.projection = dialog.projection();
            primitive.view_scale = dialog.scale();
            primitive.show_hidden = dialog.hidden();
            QString error;
            QApplication::setOverrideCursor(Qt::WaitCursor);
            const bool generated = generate_model_view(
                document_, primitive.projection, primitive.view_scale,
                primitive.show_hidden, primitive.visible_lines, primitive.hidden_lines, error, &primitive.view_center, &primitive.center_lines);
            QApplication::restoreOverrideCursor();
            if (!generated) {
                QMessageBox::warning(QApplication::activeWindow(),
                                     QString::fromUtf8("Создание вида"), error);
                return;
            }
            ClampModelViewToPaper(primitive);
            data_->primitives.push_back(std::move(primitive));
            Rebuild();
            if (changed) changed();
            return;
        }
        drawing_ = true;
        CreatePreview(start_);
        event->accept();
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override {
        if (IsDimensionTool()) { DimensionHover(event); return; }
        if (!drawing_) {
            QGraphicsScene::mouseMoveEvent(event);
            return;
        }
        QPointF end = event->scenePos();
        if (tool_ == DraftingWorkspace::Tool::Dimension) {
            int ref = -1;
            int anchor = -1;
            QPointF snapped;
            if (FindSnap(end, ref, anchor, snapped)) end = snapped;
        }
        UpdatePreview(end);
        event->accept();
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override {
        if (IsDimensionTool()) {event->accept();return;}
        if (!drawing_) {
            QGraphicsScene::mouseReleaseEvent(event);
            if (tool_ == DraftingWorkspace::Tool::Select)
                CommitMovedSelection();
            return;
        }
        drawing_ = false;
        QPointF end = event->scenePos();
        int end_ref = -1;
        int end_anchor = -1;
        if (tool_ == DraftingWorkspace::Tool::Dimension) {
            QPointF snapped;
            if (!FindSnap(end, end_ref, end_anchor, snapped)) {
                RemovePreview();
                QToolTip::showText(event->screenPos(),
                    QString::fromUtf8("Размер должен заканчиваться на элементе чертежа"));
                return;
            }
            end = snapped;
        }
        RemovePreview();
        if (QLineF(start_, end).length() < 0.5) return;
        QString type;
        if (tool_ == DraftingWorkspace::Tool::Line) type = "line";
        if (tool_ == DraftingWorkspace::Tool::Rectangle) type = "rectangle";
        if (tool_ == DraftingWorkspace::Tool::Ellipse) type = "ellipse";
        if (tool_ == DraftingWorkspace::Tool::Dimension) type = "dimension";
        if (!type.isEmpty()) {
            Primitive primitive;
            primitive.id = data_->next_primitive_id++;
            primitive.type = type;
            primitive.p1 = start_;
            primitive.p2 = end;
            if (type == "dimension") {
                primitive.ref1 = dimension_start_ref_;
                primitive.anchor1 = dimension_start_anchor_;
                primitive.ref2 = end_ref;
                primitive.anchor2 = end_anchor;
            }
            data_->primitives.push_back(primitive);
            Rebuild();
            if (changed) changed();
        }
        event->accept();
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Escape && IsDimensionTool()) {CancelDimension();if(IsDimensionTool())ShowDimensionPrompt();else SetTool(DraftingWorkspace::Tool::Select);event->accept();return;}
        if (event->key() == Qt::Key_Delete && DeleteSelectedPrimitives()) {
            return;
        }
        QGraphicsScene::keyPressEvent(event);
    }

private:
#include "DraftingDimensionScene.inc"
    void AddPaperItems() {
        auto prepare_background_item = [](QGraphicsItem* item, qreal z) {
            item->setZValue(z);
            item->setAcceptedMouseButtons(Qt::NoButton);
            item->setFlag(QGraphicsItem::ItemIsSelectable, false);
            item->setFlag(QGraphicsItem::ItemIsMovable, false);
        };

        auto* paper = addRect(paper_rect_, QPen(QColor(70, 70, 70), 0.7),
                              QBrush(Qt::white));
        prepare_background_item(paper, -10000.0);

        const QPen frame_pen(Qt::black, 0.35);
        const QRectF border = paper_rect_.adjusted(10.0, 5.0, -5.0, -5.0);
        auto* frame = addRect(border, frame_pen);
        prepare_background_item(frame, -9999.0);
        if (!data_->stamp_visible) return;

        const double width = data_->stamp.contains(QString::fromUtf8("1-й")) ? 180.0 : 120.0;
        const double height = data_->stamp.contains(QString::fromUtf8("1-й")) ? 55.0 : 35.0;
        const QRectF stamp(border.right() - std::min(width, border.width() * 0.7),
                           border.bottom() - height,
                           std::min(width, border.width() * 0.7), height);
        auto* stamp_rect = addRect(stamp, frame_pen);
        prepare_background_item(stamp_rect, -9999.0);
        auto* stamp_row = addLine(stamp.left(), stamp.top() + 15.0,
                                  stamp.right(), stamp.top() + 15.0, frame_pen);
        prepare_background_item(stamp_row, -9999.0);
        auto* stamp_column = addLine(stamp.left() + stamp.width() * 0.58, stamp.top(),
                                     stamp.left() + stamp.width() * 0.58, stamp.bottom(),
                                     frame_pen);
        prepare_background_item(stamp_column, -9999.0);

        QFont stamp_font("Arial");
        stamp_font.setPixelSize(5);
        QPainterPath stamp_text;
        stamp_text.addText(stamp.left() + 3.0, stamp.top() + 8.0, stamp_font,
                           data_->name + QString("   1:%1").arg(data_->scale, 0, 'g', 6));
        auto* stamp_label = addPath(stamp_text, Qt::NoPen, QBrush(Qt::black));
        prepare_background_item(stamp_label, -9998.0);
    }

    QRectF ModelViewBounds(const Primitive& primitive) const {
        double min_x = std::numeric_limits<double>::max();
        double min_y = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::lowest();
        double max_y = std::numeric_limits<double>::lowest();
        bool has_point = false;
        const auto include_lines = [&min_x, &min_y, &max_x, &max_y, &has_point](
                                       const QVector<QPolygonF>& lines) {
            for (const QPolygonF& line : lines) {
                for (const QPointF& point : line) {
                    min_x = std::min(min_x, point.x());
                    min_y = std::min(min_y, point.y());
                    max_x = std::max(max_x, point.x());
                    max_y = std::max(max_y, point.y());
                    has_point = true;
                }
            }
        };
        include_lines(primitive.visible_lines);
        include_lines(primitive.hidden_lines);
        include_lines(primitive.center_lines);
        return has_point ? QRectF(QPointF(min_x, min_y), QPointF(max_x, max_y))
                         : QRectF();
    }

    void ClampModelViewToPaper(Primitive& primitive) const {
        const QRectF local_bounds = ModelViewBounds(primitive);
        if (local_bounds.isNull() && local_bounds.isEmpty()) return;
        const QRectF printable = paper_rect_.adjusted(12.0, 7.0, -7.0, -7.0);
        QRectF placed = local_bounds.translated(primitive.p1);
        QPointF correction;
        if (placed.width() <= printable.width()) {
            if (placed.left() < printable.left())
                correction.rx() += printable.left() - placed.left();
            if (placed.right() > printable.right())
                correction.rx() += printable.right() - placed.right();
        }
        if (placed.height() <= printable.height()) {
            if (placed.top() < printable.top())
                correction.ry() += printable.top() - placed.top();
            if (placed.bottom() > printable.bottom())
                correction.ry() += printable.bottom() - placed.bottom();
        }
        primitive.p1 += correction;
        primitive.p2 += correction;
    }

    void CommitMovedSelection() {
        bool moved = false;
        QSet<int> moving_sources;
        for(auto* item:selectedItems()) {
            const int index=item->data(0).toInt();
            if(index>=0 && index<data_->primitives.size() && data_->primitives[index].type!="dimension" && !item->pos().isNull())
                moving_sources.insert(data_->primitives[index].id);
        }
        for (QGraphicsItem* item : selectedItems()) {
            const QPointF delta = item->pos();
            if (qFuzzyIsNull(delta.x()) && qFuzzyIsNull(delta.y())) continue;
            const int index = item->data(0).toInt();
            if (index < 0 || index >= data_->primitives.size()) continue;
            Primitive& primitive = data_->primitives[index];
            if (primitive.type == "dimension") {
                UpgradeDimension(primitive);
                if(primitive.dimension.refs.isEmpty() || !moving_sources.contains(primitive.dimension.refs.front().primitive))
                    primitive.dimension.offset += delta;
            } else {
                primitive.p1 += delta;
                primitive.p2 += delta;
            }
            if (primitive.type == "model_view") ClampModelViewToPaper(primitive);
            item->setPos(0, 0);
            moved = true;
        }
        if (!moved) return;
        Rebuild();
        if (changed) changed();
    }

    const Primitive* FindPrimitive(int id) const {
        for (const Primitive& primitive : data_->primitives)
            if (primitive.id == id) return &primitive;
        return nullptr;
    }

    QPointF AnchorPoint(const Primitive& primitive, int anchor) const {
        if (primitive.type == "line") {
            if (anchor == 0) return primitive.p1;
            if (anchor == 1) return primitive.p2;
            return (primitive.p1 + primitive.p2) / 2.0;
        }
        const QRectF rect(primitive.p1, primitive.p2);
        const QRectF normalized = rect.normalized();
        if (primitive.type == "rectangle") {
            if (anchor == 0) return normalized.topLeft();
            if (anchor == 1) return normalized.topRight();
            if (anchor == 2) return normalized.bottomRight();
            if (anchor == 3) return normalized.bottomLeft();
            return normalized.center();
        }
        if (primitive.type == "ellipse") {
            if (anchor == 0) return normalized.center();
            if (anchor == 1) return {normalized.left(), normalized.center().y()};
            if (anchor == 2) return {normalized.right(), normalized.center().y()};
            if (anchor == 3) return {normalized.center().x(), normalized.top()};
            return {normalized.center().x(), normalized.bottom()};
        }
        if (primitive.type == "model_view" && anchor >= 0) {
            const int line_index = anchor / 10000;
            const int point_index = anchor % 10000;
            if (line_index >= 0 && line_index < primitive.visible_lines.size()
                && point_index >= 0
                && point_index < primitive.visible_lines[line_index].size()) {
                return primitive.p1 + primitive.visible_lines[line_index][point_index];
            }
        }
        return primitive.p1;
    }

    QPointF ResolveReferencePoint(int id, int anchor, QPointF fallback) const {
        const Primitive* primitive = FindPrimitive(id);
        return primitive ? AnchorPoint(*primitive, anchor) : fallback;
    }

    bool FindSnap(QPointF position, int& primitive_id, int& anchor, QPointF& point) const {
        constexpr double kSnapDistanceMm = 6.0;
        double best = kSnapDistanceMm;
        bool found = false;
        for (const Primitive& primitive : data_->primitives) {
            if (primitive.type == "model_view") {
                for (int line = 0; line < primitive.visible_lines.size(); ++line) {
                    const QPolygonF& polyline = primitive.visible_lines[line];
                    for (int vertex = 0; vertex < polyline.size(); ++vertex) {
                        const QPointF candidate_point = primitive.p1 + polyline[vertex];
                        const double distance = QLineF(position, candidate_point).length();
                        if (distance < best) {
                            best = distance;
                            primitive_id = primitive.id;
                            anchor = line * 10000 + vertex;
                            point = candidate_point;
                            found = true;
                        }
                    }
                }
                continue;
            }
            int count = 0;
            if (primitive.type == "line") count = 3;
            else if (primitive.type == "rectangle" || primitive.type == "ellipse") count = 5;
            for (int candidate = 0; candidate < count; ++candidate) {
                const QPointF candidate_point = AnchorPoint(primitive, candidate);
                const double distance = QLineF(position, candidate_point).length();
                if (distance < best) {
                    best = distance;
                    primitive_id = primitive.id;
                    anchor = candidate;
                    point = candidate_point;
                    found = true;
                }
            }
        }
        return found;
    }

    void CreatePreview(QPointF point) {
        const QPen preview_pen(QColor(0, 120, 215), 0.45, Qt::DashLine);
        if (tool_ == DraftingWorkspace::Tool::Line || tool_ == DraftingWorkspace::Tool::Dimension)
            preview_item_ = addLine(QLineF(start_, point), preview_pen);
        else if (tool_ == DraftingWorkspace::Tool::Rectangle)
            preview_item_ = addRect(QRectF(start_, point).normalized(), preview_pen);
        else if (tool_ == DraftingWorkspace::Tool::Ellipse)
            preview_item_ = addEllipse(QRectF(start_, point).normalized(), preview_pen);
        if (preview_item_) preview_item_->setZValue(1000.0);
    }

    void UpdatePreview(QPointF point) {
        if (auto* line = dynamic_cast<QGraphicsLineItem*>(preview_item_))
            line->setLine(QLineF(start_, point));
        else if (auto* rect = dynamic_cast<QGraphicsRectItem*>(preview_item_))
            rect->setRect(QRectF(start_, point).normalized());
        else if (auto* ellipse = dynamic_cast<QGraphicsEllipseItem*>(preview_item_))
            ellipse->setRect(QRectF(start_, point).normalized());
    }

    void RemovePreview() {
        if (!preview_item_) return;
        removeItem(preview_item_);
        delete preview_item_;
        preview_item_ = nullptr;
    }

    SheetData* data_ = nullptr;
    const CAlfaDoc* document_ = nullptr;
    QRectF paper_rect_;
    DraftingWorkspace::Tool tool_ = DraftingWorkspace::Tool::Select;
    QPointF start_;
    bool drawing_ = false;
    QGraphicsItem* preview_item_ = nullptr;
    int dimension_start_ref_ = -1;
    int dimension_start_anchor_ = -1;
};

class DraftingSheetView final : public QWidget {
public:
    explicit DraftingSheetView(SheetData data, const CAlfaDoc* document, QWidget* parent = nullptr)
        : QWidget(parent), data_(std::move(data)) {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        scene_ = new DraftingScene(&data_, document, this);
        class ZoomView final : public QGraphicsView {
        public:
            using QGraphicsView::QGraphicsView;
        protected:
            void wheelEvent(QWheelEvent* event) override {
                const qreal factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
                scale(factor, factor);
                event->accept();
            }

            void mousePressEvent(QMouseEvent* event) override {
                if (event->button() == Qt::MiddleButton) {
                    panning_ = true;
                    last_pan_position_ = event->pos();
                    setCursor(Qt::ClosedHandCursor);
                    event->accept();
                    return;
                }
                QGraphicsView::mousePressEvent(event);
            }

            void mouseMoveEvent(QMouseEvent* event) override {
                if (panning_) {
                    const QPoint delta = event->pos() - last_pan_position_;
                    last_pan_position_ = event->pos();
                    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
                    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
                    event->accept();
                    return;
                }
                QGraphicsView::mouseMoveEvent(event);
            }

            void mouseReleaseEvent(QMouseEvent* event) override {
                if (event->button() == Qt::MiddleButton && panning_) {
                    panning_ = false;
                    unsetCursor();
                    event->accept();
                    return;
                }
                QGraphicsView::mouseReleaseEvent(event);
            }

            void mouseDoubleClickEvent(QMouseEvent* event) override {
                if (event->button() == Qt::MiddleButton) {
                    const auto* drafting_scene = static_cast<const DraftingScene*>(scene());
                    fitInView(drafting_scene->PaperRect().adjusted(-8, -8, 8, 8), Qt::KeepAspectRatio);
                    event->accept();
                    return;
                }
                QGraphicsView::mouseDoubleClickEvent(event);
            }

            void keyPressEvent(QKeyEvent* event) override {
                if (event->key() == Qt::Key_Delete
                    && static_cast<DraftingScene*>(scene())->DeleteSelectedPrimitives()) {
                    event->accept();
                    return;
                }
                QGraphicsView::keyPressEvent(event);
            }

        private:
            bool panning_ = false;
            QPoint last_pan_position_;
        };
        view_ = new ZoomView(scene_, this);
        view_->setMouseTracking(true);
        view_->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
        view_->setBackgroundBrush(QColor(220, 220, 220));
        view_->setDragMode(QGraphicsView::RubberBandDrag);
        layout->addWidget(view_);
        auto* hint=new QLabel(this);hint->setObjectName("draftingDimensionHint");hint->setWordWrap(true);
        hint->setStyleSheet("QLabel { background: #e8f2ff; color: #17395d; padding: 6px; }");
        hint->hide();layout->addWidget(hint);
        scene_->dimensionPromptChanged=[hint](const QString& text) {hint->setText(text);hint->setVisible(!text.isEmpty());};
        Fit();
    }

    std::function<void()> changed;
    SheetData& data() { return data_; }
    const SheetData& data() const { return data_; }
    DraftingScene* scene() const { return scene_; }
    void SetTool(DraftingWorkspace::Tool tool) { scene_->SetTool(tool); }
    void Fit() {
        fit_pending_ = true;
        QTimer::singleShot(0, view_, [this] {
            // Project loading also creates sheets in the hidden workspace
            // and inactive tabs. Their viewport sizes are not usable yet.
            if (!fit_pending_ || !isVisible() || !view_->isVisible()) return;
            view_->fitInView(scene_->PaperRect().adjusted(-8, -8, 8, 8), Qt::KeepAspectRatio);
            fit_pending_ = false;
        });
    }

    QJsonObject ToJson() const {
        QJsonArray primitives;
        for (const Primitive& p : data_.primitives) primitives.push_back(primitive_json(p));
        return {{"name", data_.name}, {"format", data_.format},
                {"width", data_.size_mm.width()}, {"height", data_.size_mm.height()},
                {"landscape", data_.landscape}, {"stamp", data_.stamp},
                {"stampVisible", data_.stamp_visible}, {"scale", data_.scale},
                {"primitives", primitives}};
    }

    static SheetData FromJson(const QJsonObject& object) {
        SheetData data;
        data.name = object.value("name").toString("Draft");
        data.format = object.value("format").toString("A4");
        data.size_mm = {object.value("width").toDouble(format_size(data.format).width()),
                        object.value("height").toDouble(format_size(data.format).height())};
        data.landscape = object.value("landscape").toBool(false);
        data.stamp = object.value("stamp").toString(QString::fromUtf8("1-й лист"));
        data.stamp_visible = object.value("stampVisible").toBool(true);
        data.scale = object.value("scale").toDouble(1.0);
        for (const QJsonValue& value : object.value("primitives").toArray()) {
            Primitive primitive = primitive_from_json(value.toObject());
            if (primitive.id <= 0) primitive.id = data.next_primitive_id;
            data.next_primitive_id = std::max(data.next_primitive_id, primitive.id + 1);
            data.primitives.push_back(primitive);
        }
        return data;
    }

protected:
    void showEvent(QShowEvent* event) override {
        QWidget::showEvent(event);
        // Queue after layout has assigned the visible viewport its size.
        // Only the initial/pending fit runs here; preserve later zoom/pan.
        if (fit_pending_) Fit();
    }

private:
    bool fit_pending_ = true;
    SheetData data_;
    DraftingScene* scene_ = nullptr;
    QGraphicsView* view_ = nullptr;
};

namespace {
void configure_printer(QPrinter& printer, const SheetData& data) {
    QSizeF size = data.size_mm;
    const QPageSize page_size(size, QPageSize::Millimeter, data.format);
    printer.setPageSize(page_size);
    printer.setPageOrientation(data.landscape ? QPageLayout::Landscape : QPageLayout::Portrait);
    printer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
}

void render_sheet(QPrinter* printer, DraftingSheetView* sheet) {
    if (!printer || !sheet) return;
    QPainter painter(printer);
    if (!painter.isActive()) return;
    const QRectF target = printer->pageLayout().paintRectPixels(printer->resolution());
    sheet->scene()->render(&painter, target, sheet->scene()->PaperRect(), Qt::KeepAspectRatio);
}
}

DraftingWorkspace::DraftingWorkspace(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    toolbar_ = new QToolBar(QString::fromUtf8("Черчение"), this);
    toolbar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar_->addAction(QString::fromUtf8("Добавить чертёж"), this, &DraftingWorkspace::AddDrawing);
    toolbar_->addAction(QString::fromUtf8("Удалить лист"), this, &DraftingWorkspace::RemoveCurrentDrawing);
    toolbar_->addSeparator();

    toolbar_->addAction(QString::fromUtf8("Вписать лист"), this, [this] { if (auto* s = CurrentSheet()) s->Fit(); });
    toolbar_->addAction(QString::fromUtf8("Обновить виды"), this, &DraftingWorkspace::RefreshViews);
    toolbar_->addAction(QString::fromUtf8("Просмотр печати"), this, &DraftingWorkspace::ShowPrintPreview);
    toolbar_->addAction(QString::fromUtf8("Печать / плоттер"), this, &DraftingWorkspace::PrintCurrentDrawing);
    toolbar_->addAction("PDF", this, &DraftingWorkspace::ExportPdf);
    root->addWidget(toolbar_);

    tools_toolbar_ = new QToolBar(QString::fromUtf8("Инструменты Drafting"), this);
    tools_toolbar_->setOrientation(Qt::Vertical);
    tools_toolbar_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    tools_toolbar_->setMovable(false);
    tools_toolbar_->setMinimumWidth(118);
    tools_toolbar_->setStyleSheet(
        "QToolBar { background: #eeeeee; border-left: 1px solid #999; spacing: 3px; padding: 5px; }"
        "QToolButton { min-width: 100px; min-height: 28px; text-align: left; padding-left: 8px; }"
        "QToolButton:checked { background: #287bd1; color: white; }");

    auto* tools = new QActionGroup(this);
    tools->setExclusive(true);
    const auto add_tool = [this, tools](const QString& title, Tool tool, bool checked = false) {
        QAction* action = tools_toolbar_->addAction(title);
        action->setCheckable(true);
        action->setChecked(checked);
        tools->addAction(action);
        connect(action, &QAction::triggered, this, [this, tool] { SetTool(tool); });
    };
    add_tool(QString::fromUtf8("Выбор"), Tool::Select, true);
    add_tool(QString::fromUtf8("Линия"), Tool::Line);
    add_tool(QString::fromUtf8("Прямоугольник"), Tool::Rectangle);
    add_tool(QString::fromUtf8("Эллипс"), Tool::Ellipse);
    add_tool(QString::fromUtf8("Текст"), Tool::Text);
    add_tool(QString::fromUtf8("Горизонтальный"), Tool::HorizontalDimension);
    add_tool(QString::fromUtf8("Вертикальный"), Tool::VerticalDimension);
    add_tool(QString::fromUtf8("Параллельный"), Tool::ParallelDimension);
    add_tool(QString::fromUtf8("Перпендикулярный"), Tool::PerpendicularDimension);
    add_tool(QString::fromUtf8("Радиусный"), Tool::RadiusDimension);
    add_tool(QString::fromUtf8("Диаметральный"), Tool::DiameterDimension);
    add_tool(QString::fromUtf8("Угловой"), Tool::AngularDimension);
    tools_toolbar_->addSeparator();
    add_tool(QString::fromUtf8("Вид модели"), Tool::ModelView);
    tools_toolbar_->addSeparator();
    tools_toolbar_->addAction(QString::fromUtf8("Удалить выбранное"),
                              this, &DraftingWorkspace::DeleteSelected);

    sheets_ = new QTabWidget(this);
    sheets_->setTabPosition(QTabWidget::South);
    sheets_->setDocumentMode(true);
    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    body->addWidget(sheets_, 1);
    body->addWidget(tools_toolbar_);
    root->addLayout(body, 1);
    connect(sheets_, &QTabWidget::currentChanged, this, [this] { UpdateActionState(); });
    UpdateActionState();
}

bool DraftingWorkspace::BuildModelView(const CAlfaDoc* document, const QString& projection,
                                      double scale, bool includeHidden,
                                      QVector<QPolygonF>& visible, QVector<QPolygonF>& hidden,
                                      QString& error, QVector<QPolygonF>* centerlines) {
    return generate_model_view(document, projection, scale, includeHidden, visible, hidden, error, nullptr, centerlines);
}

bool DraftingWorkspace::RefreshModelViews(CAlfaDoc& document, QString& error) {
    error.clear();
    if (document.GetDraftingData().empty()) return true;
    QJsonParseError parse_error;
    const auto json = QJsonDocument::fromJson(
        QByteArray::fromStdString(document.GetDraftingData()), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !json.isObject()) {
        error = QString::fromUtf8("Не удалось прочитать данные чертежа.");
        return false;
    }
    auto root = json.object();
    auto sheets = root.value("sheets").toArray();
    for (qsizetype s = 0; s < sheets.size(); ++s) {
        auto sheet = sheets[s].toObject();
        auto primitives = sheet.value("primitives").toArray();
        for (qsizetype i = 0; i < primitives.size(); ++i) {
            auto original = primitives[i].toObject();
            Primitive view = primitive_from_json(original);
            if (view.type != "model_view") continue;
            if (!generate_model_view(&document, view.projection, view.view_scale,
                                     view.show_hidden, view.visible_lines, view.hidden_lines, error, &view.view_center, &view.center_lines))
                return false;
            const auto rebuilt = primitive_json(view);
            original.insert("visibleLines", rebuilt.value("visibleLines"));
            original.insert("hiddenLines", rebuilt.value("hiddenLines"));
            original.insert("centerLines", rebuilt.value("centerLines"));
            original.insert("centerLinesVersion", rebuilt.value("centerLinesVersion"));
            original.insert("showHidden", view.show_hidden);
            original.insert("viewCenterX", view.view_center.x());
            original.insert("viewCenterY", view.view_center.y());
            primitives[i] = original;
        }
        sheet.insert("primitives", primitives);
        sheets[s] = sheet;
    }
    root.insert("sheets", sheets);
    // Commit only after every projection succeeds; failed HLR leaves all
    // previously saved views and their annotation references intact.
    document.SetDraftingData(QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString());
    return true;
}

void DraftingWorkspace::RefreshViews() {
    if (!document_) return;
    SaveToDocument();
    const int selected_sheet = sheets_->currentIndex();
    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool refreshed = RefreshModelViews(*document_, error);
    QApplication::restoreOverrideCursor();
    if (!refreshed) {
        QMessageBox::warning(this, QString::fromUtf8("Обновление видов"), error);
        return;
    }
    ReloadFromDocument();
    sheets_->setCurrentIndex(selected_sheet);
}

QByteArray DraftingWorkspace::ModelSignature() const {
    QByteArray bytes;QDataStream stream(&bytes,QIODevice::WriteOnly);
    if(document_)for(const auto& object:document_->GetObjects()) {
        const auto* solid=object?dynamic_cast<const CSolid*>(object.get()):nullptr;
        if(!solid)continue;
        stream<<quint64(solid->m_id)<<solid->IsVisible();
        if(solid->m_Shape.IsNull()){stream<<quint64(0);continue;}
        stream<<quint64(reinterpret_cast<quintptr>(solid->m_Shape.TShape().get()));
        const auto trsf=solid->m_Shape.Location().Transformation();
        for(int row=1;row<=3;++row)for(int col=1;col<=4;++col)stream<<trsf.Value(row,col);
        stream<<int(solid->m_Shape.Orientation());
        for (const auto& line : solid->GetCenterlines()) {
            stream << QString::fromStdString(line.id) << int(line.kind) << line.closed << quint64(line.points.size());
            for (const auto& p : line.points) stream << p.X() << p.Y() << p.Z();
        }
    }
    return bytes;
}

void DraftingWorkspace::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    QTimer::singleShot(0,this,[this] {
        if(!isVisible() || !document_ || !sheets_->count())return;
        const QByteArray current=ModelSignature();
        if(current==model_signature_)return;
        // Geometry changed while the model workspace was active. Update the
        // projection and its associative annotations together on returning.
        model_signature_=current;
        RefreshViews();
    });
}

void DraftingWorkspace::SetDocument(CAlfaDoc* document) {
    document_ = document;
    ReloadFromDocument();
}

void DraftingWorkspace::ReloadFromDocument() {
    model_signature_=ModelSignature();
    while (sheets_->count()) delete sheets_->widget(0);
    if (!document_ || document_->GetDraftingData().empty()) {
        UpdateActionState();
        return;
    }
    QJsonParseError parse_error;
    const QJsonDocument json = QJsonDocument::fromJson(
        QByteArray::fromStdString(document_->GetDraftingData()), &parse_error);
    if (parse_error.error != QJsonParseError::NoError) return;
    for (const QJsonValue& value : json.object().value("sheets").toArray()) {
        auto* sheet = new DraftingSheetView(DraftingSheetView::FromJson(value.toObject()), document_, sheets_);
        sheet->SetTool(current_tool_);
        sheet->scene()->changed = [this] { SaveToDocument(); };
        sheets_->addTab(sheet, sheet->data().name);
    }
    UpdateActionState();
}

void DraftingWorkspace::AddDrawing() {
    DrawingParametersDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) return;
    auto* sheet = new DraftingSheetView(dialog.data(), document_, sheets_);
    sheet->SetTool(current_tool_);
    sheet->scene()->changed = [this] { SaveToDocument(); };
    sheets_->addTab(sheet, sheet->data().name);
    sheets_->setCurrentWidget(sheet);
    SaveToDocument();
    UpdateActionState();
}

DraftingSheetView* DraftingWorkspace::CurrentSheet() const {
    return static_cast<DraftingSheetView*>(sheets_->currentWidget());
}

void DraftingWorkspace::SetTool(Tool tool) {
    current_tool_ = tool;
    for (int i = 0; i < sheets_->count(); ++i)
        static_cast<DraftingSheetView*>(sheets_->widget(i))->SetTool(tool);
}

void DraftingWorkspace::SaveToDocument() {
    if (!document_) return;
    QJsonArray sheets;
    for (int i = 0; i < sheets_->count(); ++i)
        sheets.push_back(static_cast<DraftingSheetView*>(sheets_->widget(i))->ToJson());
    document_->SetDraftingData(QJsonDocument(QJsonObject{{"version", 1}, {"sheets", sheets}})
                                   .toJson(QJsonDocument::Compact).toStdString());
}

void DraftingWorkspace::RemoveCurrentDrawing() {
    const int index = sheets_->currentIndex();
    if (index < 0) return;
    if (QMessageBox::question(this, QString::fromUtf8("Удаление листа"),
                              QString::fromUtf8("Удалить текущий чертёж?")) != QMessageBox::Yes) return;
    delete sheets_->widget(index);
    SaveToDocument();
    UpdateActionState();
}

void DraftingWorkspace::DeleteSelected() {
    if (DraftingSheetView* sheet = CurrentSheet())
        sheet->scene()->DeleteSelectedPrimitives();
}

void DraftingWorkspace::ShowPrintPreview() {
    DraftingSheetView* sheet = CurrentSheet();
    if (!sheet) return;
    QPrinter printer(QPrinter::HighResolution);
    configure_printer(printer, sheet->data());
    QPrintPreviewDialog preview(&printer, this);
    preview.setWindowTitle(QString::fromUtf8("Просмотр перед печатью — ") + sheet->data().name);
    connect(&preview, &QPrintPreviewDialog::paintRequested, this,
            [sheet](QPrinter* requested) { render_sheet(requested, sheet); });
    preview.exec();
}

void DraftingWorkspace::PrintCurrentDrawing() {
    DraftingSheetView* sheet = CurrentSheet();
    if (!sheet) return;
    QPrinter printer(QPrinter::HighResolution);
    configure_printer(printer, sheet->data());
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(QString::fromUtf8("Печать / плоттер"));
    if (dialog.exec() == QDialog::Accepted) render_sheet(&printer, sheet);
}

void DraftingWorkspace::ExportPdf() {
    DraftingSheetView* sheet = CurrentSheet();
    if (!sheet) return;
    QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("Экспорт чертежа в PDF"),
                                                sheet->data().name + ".pdf", "PDF (*.pdf)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".pdf", Qt::CaseInsensitive)) path += ".pdf";
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    configure_printer(printer, sheet->data());
    render_sheet(&printer, sheet);
}

void DraftingWorkspace::UpdateActionState() {
    const bool has_sheet = CurrentSheet() != nullptr;
    const QList<QAction*> actions = toolbar_->actions();
    for (QAction* action : actions) {
        if (!action->isSeparator() && action != actions.first()) action->setEnabled(has_sheet);
    }
    for (QAction* action : tools_toolbar_->actions()) action->setEnabled(has_sheet);
}
