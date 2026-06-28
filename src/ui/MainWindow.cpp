#include "MainWindow.h"

#include "../CadCurve3D.h"
#include "../CBSpline.h"
#include "../CMesh3D.h"
#include "../CPolyline.h"
#include "../MaterialLibrary.h"
#include "../solid/Solid.h"
#include "../solid/SurfaceSet.h"
#include "MaterialEditorDialog.h"
#include "MaterialDrag.h"
#include "PreferencesDialog.h"
#include "BooleanDialog.h"
#include "ExtrudeFaceDialog.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QColor>
#include <QComboBox>
#include <QCursor>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QDrag>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QLinearGradient>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QRadialGradient>
#include <QSettings>
#include <QSize>
#include <QSignalBlocker>
#include <QSlider>
#include <QStatusBar>
#include <QTabBar>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>

#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <functional>
#include <vector>

namespace {
constexpr int kMaxRecentProjectFiles = 18;
constexpr int kSceneTreeObjectIndexRole = Qt::UserRole + 1;
constexpr int kSceneTreeGroupRole = Qt::UserRole + 2;

std::unique_ptr<CMesh3D> CreateLowPolyMeshFromSolid(const CSolid& solid)
{
    std::vector<Vec3> vertices;
    std::vector<CMesh3D::Face> faces;
    for (int surface_index = 0; surface_index < solid.GetNumSurfaces(); ++surface_index) {
        const CSurfaceFace* surface = solid.GetSurfaceFace(surface_index);
        if (!surface || !surface->pMesh3D) {
            continue;
        }

        const std::vector<Vec3>& source_vertices = surface->pMesh3D->GetVertices();
        const std::vector<CMesh3D::Face>& source_faces = surface->pMesh3D->GetFaces();
        const size_t vertex_offset = vertices.size();
        vertices.insert(vertices.end(), source_vertices.begin(), source_vertices.end());

        for (CMesh3D::Face face : source_faces) {
            if (face.deleted || face.corners.empty()) {
                continue;
            }
            for (MeshCorner& corner : face.corners) {
                corner.v += vertex_offset;
                corner.uv = corner.v;
                corner.n = corner.v;
            }
            face.sourceFaceId = surface_index;
            faces.push_back(std::move(face));
        }
    }

    if (vertices.empty() || faces.empty()) {
        return {};
    }

    auto mesh = std::make_unique<CMesh3D>(solid.GetName() + " Low Poly");
    if (!mesh->SetGeometry(std::move(vertices), std::move(faces))) {
        return {};
    }
    mesh->SetMaterial(solid.GetMaterial());
    mesh->SetMaterialId(solid.GetMaterialId());
    return mesh;
}

class SolidLowPolyDialog : public QDialog {
public:
    SolidLowPolyDialog(CAlfaDoc& document,
                       std::function<void()> refresh_scene,
                       std::function<void(const QString&)> set_status,
                       QWidget* parent)
        : QDialog(parent),
          document_(document),
          refresh_scene_(std::move(refresh_scene)),
          set_status_(std::move(set_status)) {
        setWindowTitle("Tool Options");
        setAttribute(Qt::WA_DeleteOnClose, true);

        auto* layout = new QVBoxLayout(this);
        auto* list_label = new QLabel("Bodies", this);
        bodies_list_ = new QListWidget(this);
        bodies_list_->setMinimumHeight(68);
        layout->addWidget(list_label);
        layout->addWidget(bodies_list_);

        auto* form = new QFormLayout();
        density_ = new QDoubleSpinBox(this);
        density_->setRange(0.05, 100.0);
        density_->setSingleStep(0.05);
        density_->setDecimals(2);
        density_->setValue(1.0);
        form->addRow("Density", density_);

        mesh_quadro_ = new QCheckBox("Mesh Quadro", this);
        form->addRow(mesh_quadro_);
        layout->addLayout(form);

        auto* buttons = new QHBoxLayout();
        auto* create = new QPushButton("Create Low Poly", this);
        auto* close = new QPushButton("Close", this);
        buttons->addWidget(create);
        buttons->addWidget(close);
        layout->addLayout(buttons);

        RememberSelectedSolids();
        RebuildBodiesList();
        LoadInitialSolidSettings();

        connect(density_, &QDoubleSpinBox::valueChanged, this, [this](double) {
            RebuildSolids();
        });
        connect(mesh_quadro_, &QCheckBox::toggled, this, [this](bool) {
            RebuildSolids();
        });
        connect(create, &QPushButton::clicked, this, [this]() {
            CreateLowPoly();
        });
        connect(close, &QPushButton::clicked, this, &QDialog::close);
    }

    bool HasBodies() const
    {
        return !solid_ids_.empty();
    }

private:
    void RememberSelectedSolids()
    {
        const CAlfaDoc::ObjectList& objects = document_.GetObjects();
        for (size_t index : document_.GetSelectedObjectIndices()) {
            if (index >= objects.size()) {
                continue;
            }
            const auto* solid = dynamic_cast<const CSolid*>(objects[index].get());
            if (solid) {
                solid_ids_.push_back(solid->m_id);
            }
        }
    }

    CSolid* FindSolid(unsigned long id)
    {
        for (const CAlfaDoc::ObjectPtr& object : document_.GetObjects()) {
            auto* solid = dynamic_cast<CSolid*>(object.get());
            if (solid && solid->m_id == id) {
                return solid;
            }
        }
        return nullptr;
    }

    std::vector<CSolid*> Solids()
    {
        std::vector<CSolid*> solids;
        for (unsigned long id : solid_ids_) {
            if (CSolid* solid = FindSolid(id)) {
                solids.push_back(solid);
            }
        }
        return solids;
    }

    void RebuildBodiesList()
    {
        bodies_list_->clear();
        for (CSolid* solid : Solids()) {
            bodies_list_->addItem(QString::fromStdString(solid->GetName()));
        }
    }

    void LoadInitialSolidSettings()
    {
        const std::vector<CSolid*> solids = Solids();
        if (solids.empty()) {
            return;
        }
        density_->setValue(solids.front()->ptchDensity);
        mesh_quadro_->setChecked(solids.front()->MeshQuadro);
    }

    void RebuildSolids()
    {
        const double density = density_->value();
        const bool mesh_quadro = mesh_quadro_->isChecked();
        int rebuilt = 0;
        for (CSolid* solid : Solids()) {
            solid->ptchDensity = static_cast<float>(density);
            solid->MeshQuadro = mesh_quadro;
            if (solid->ReBuldMesh()) {
                ++rebuilt;
            }
        }
        if (refresh_scene_) {
            refresh_scene_();
        }
        if (set_status_) {
            set_status_(QString("Low Poly: rebuilt %1 bodies").arg(rebuilt));
        }
    }

    void CreateLowPoly()
    {
        int created = 0;
        for (CSolid* solid : Solids()) {
            std::unique_ptr<CMesh3D> mesh = CreateLowPolyMeshFromSolid(*solid);
            if (!mesh) {
                continue;
            }
            document_.AddMesh(std::move(mesh));
            ++created;
        }
        if (refresh_scene_) {
            refresh_scene_();
        }
        if (set_status_) {
            set_status_(created > 0
                ? QString("Low Poly: created %1 meshes").arg(created)
                : QString("Low Poly: no mesh created"));
        }
    }

    CAlfaDoc& document_;
    std::function<void()> refresh_scene_;
    std::function<void(const QString&)> set_status_;
    std::vector<unsigned long> solid_ids_;
    QListWidget* bodies_list_ = nullptr;
    QDoubleSpinBox* density_ = nullptr;
    QCheckBox* mesh_quadro_ = nullptr;
};
constexpr int kMaterialLibraryEntryRole = Qt::UserRole + 20;
constexpr int kMaterialDocumentIndexRole = Qt::UserRole + 21;
constexpr int kMaterialSourceRole = Qt::UserRole + 22;

enum class MaterialListSource {
    Library,
    Document
};

class MaterialListWidget : public QListWidget {
public:
    using MaterialResolver = std::function<bool(QListWidgetItem*, Material*)>;

    explicit MaterialListWidget(QWidget* parent = nullptr)
        : QListWidget(parent) {
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
    }

    void SetMaterialResolver(MaterialResolver resolver) {
        material_resolver_ = std::move(resolver);
    }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            drag_start_pos_ = event->pos();
            drag_start_item_ = itemAt(event->pos());
        }
        QListWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (!(event->buttons() & Qt::LeftButton) || !drag_start_item_) {
            QListWidget::mouseMoveEvent(event);
            return;
        }
        if ((event->pos() - drag_start_pos_).manhattanLength() < QApplication::startDragDistance()) {
            QListWidget::mouseMoveEvent(event);
            return;
        }

        StartMaterialDrag(drag_start_item_);
        drag_start_item_ = nullptr;
    }

    void startDrag(Qt::DropActions supported_actions) override {
        (void)supported_actions;
        StartMaterialDrag(currentItem());
    }

private:
    void StartMaterialDrag(QListWidgetItem* item) {
        Material material;
        if (!material_resolver_ || !material_resolver_(item, &material)) {
            return;
        }

        auto* mime_data = new QMimeData;
        mime_data->setData(MaterialDrag::MimeType(), MaterialDrag::Encode(material));

        auto* drag = new QDrag(this);
        drag->setMimeData(mime_data);
        const QPixmap sphere = MaterialDrag::SpherePixmap(material, 58, true);
        drag->setPixmap(sphere);
        drag->setHotSpot(QPoint(sphere.width() / 2, sphere.height() / 2));
        drag->exec(Qt::CopyAction);
    }

    MaterialResolver material_resolver_;
    QPoint drag_start_pos_;
    QListWidgetItem* drag_start_item_ = nullptr;
};

QColor ToQColor(Color color) {
    return QColor::fromRgbF(std::clamp(color.r, 0.0f, 1.0f),
                            std::clamp(color.g, 0.0f, 1.0f),
                            std::clamp(color.b, 0.0f, 1.0f));
}

QIcon SearchIcon() {
    QPixmap pixmap(22, 22);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(232, 232, 232), 2));
    painter.drawEllipse(QRectF(4.5, 4.5, 9.5, 9.5));
    painter.drawLine(QPointF(12.5, 12.5), QPointF(18.0, 18.0));
    painter.setPen(QPen(QColor(120, 120, 120, 120), 1));
    painter.drawEllipse(QRectF(6.5, 6.5, 4.0, 4.0));
    painter.end();
    return QIcon(pixmap);
}

QIcon DuplicateObjectIcon() {
    QPixmap pixmap(22, 22);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(42, 46, 54), 1));
    painter.setBrush(QColor(122, 132, 146));
    painter.drawRect(QRectF(5.0, 4.0, 10.0, 10.0));
    painter.setBrush(QColor(220, 224, 230));
    painter.drawRect(QRectF(9.0, 8.0, 10.0, 10.0));
    painter.setPen(QPen(QColor(42, 46, 54), 1.4));
    painter.drawLine(QPointF(12.0, 13.0), QPointF(16.0, 13.0));
    painter.drawLine(QPointF(14.0, 11.0), QPointF(14.0, 15.0));
    painter.end();
    return QIcon(pixmap);
}

QIcon SceneVisibilityIcon(bool visible) {
    QPixmap pixmap(20, 20);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(66, 66, 66), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    QPainterPath eye;
    if (visible) {
        eye.moveTo(2.0, 10.0);
        eye.cubicTo(5.0, 5.2, 8.0, 3.8, 10.0, 3.8);
        eye.cubicTo(12.0, 3.8, 15.0, 5.2, 18.0, 10.0);
        eye.cubicTo(15.0, 14.8, 12.0, 16.2, 10.0, 16.2);
        eye.cubicTo(8.0, 16.2, 5.0, 14.8, 2.0, 10.0);
        eye.closeSubpath();
        painter.drawPath(eye);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(66, 66, 66));
        painter.drawEllipse(QPointF(10.0, 10.0), 3.2, 3.2);
    } else {
        eye.moveTo(2.0, 9.0);
        eye.cubicTo(5.0, 13.8, 8.0, 15.2, 10.0, 15.2);
        eye.cubicTo(12.0, 15.2, 15.0, 13.8, 18.0, 9.0);
        painter.drawPath(eye);
        const QPointF lash_starts[] = {
            {4.3, 11.6}, {7.2, 13.5}, {10.0, 14.3}, {12.8, 13.5}, {15.7, 11.6}
        };
        const QPointF lash_ends[] = {
            {3.2, 15.0}, {6.5, 17.0}, {10.0, 18.0}, {13.5, 17.0}, {16.8, 15.0}
        };
        for (int i = 0; i < 5; ++i) {
            painter.drawLine(lash_starts[i], lash_ends[i]);
        }
    }

    painter.end();
    return QIcon(pixmap);
}

QIcon MirrorObjectIcon() {
    QPixmap pixmap(22, 22);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(80, 170, 245), 1.5, Qt::DashLine));
    painter.drawLine(QPointF(11.0, 2.5), QPointF(11.0, 19.5));
    painter.setPen(QPen(QColor(42, 46, 54), 1));
    painter.setBrush(QColor(220, 224, 230));
    painter.drawRect(QRectF(3.0, 6.0, 6.0, 10.0));
    painter.setBrush(QColor(122, 132, 146));
    painter.drawRect(QRectF(13.0, 6.0, 6.0, 10.0));
    painter.end();
    return QIcon(pixmap);
}

QIcon NewSketchIcon() {
    QPixmap pixmap(22, 22);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(40, 46, 54), 1));
    painter.setBrush(QColor(230, 235, 240));
    painter.drawRect(QRectF(4.0, 5.0, 14.0, 13.0));
    painter.setPen(QPen(QColor(235, 20, 22), 1.6));
    painter.drawLine(QPointF(7.0, 8.0), QPointF(15.5, 8.0));
    painter.drawLine(QPointF(7.0, 8.0), QPointF(7.0, 15.0));
    painter.setPen(QPen(QColor(15, 120, 210), 1.5));
    painter.drawLine(QPointF(14.0, 13.0), QPointF(19.0, 13.0));
    painter.drawLine(QPointF(16.5, 10.5), QPointF(16.5, 15.5));
    painter.end();
    return QIcon(pixmap);
}

QIcon SketchRectangleIcon() {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(20, 20, 20), 3.0));
    painter.drawRect(QRectF(10.0, 10.0, 24.0, 22.0));
    painter.setBrush(QColor(255, 62, 78));
    painter.setPen(QPen(QColor(155, 0, 16), 1.0));
    painter.drawEllipse(QPointF(10.0, 32.0), 4.0, 4.0);
    painter.drawEllipse(QPointF(34.0, 10.0), 4.0, 4.0);
    painter.end();
    return QIcon(pixmap);
}

QIcon LoftSurfaceIcon() {
    QPixmap pixmap(28, 28);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    QLinearGradient gradient(6.0, 8.0, 22.0, 22.0);
    gradient.setColorAt(0.0, QColor(210, 212, 208));
    gradient.setColorAt(1.0, QColor(116, 120, 116));
    QPainterPath surface;
    surface.moveTo(4.0, 19.0);
    surface.cubicTo(8.0, 10.0, 15.0, 9.0, 24.0, 6.0);
    surface.lineTo(24.0, 16.5);
    surface.cubicTo(16.0, 19.0, 10.0, 22.5, 4.0, 23.0);
    surface.closeSubpath();
    painter.setBrush(gradient);
    painter.drawPath(surface);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(235, 20, 22), 1.4));
    const QPointF rows[][4] = {
        {QPointF(4, 19), QPointF(8, 10), QPointF(15, 9), QPointF(24, 6)},
        {QPointF(4, 23), QPointF(10, 22.5), QPointF(16, 19), QPointF(24, 16.5)}
    };
    for (const auto& row : rows) {
        QPainterPath path;
        path.moveTo(row[0]);
        path.cubicTo(row[1], row[2], row[3]);
        painter.drawPath(path);
    }
    painter.setPen(QPen(QColor(235, 20, 22), 1.0));
    for (int i = 0; i < 3; ++i) {
        const qreal x = 8.0 + i * 5.5;
        painter.drawLine(QPointF(x, 15.0 - i * 2.0), QPointF(x + 1.2, 21.5 - i * 1.7));
    }
    painter.end();
    return QIcon(pixmap);
}

QIcon ReverseNormalsIcon() {
    QPixmap pixmap(28, 28);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(QColor(155, 160, 158));
    painter.setPen(QPen(QColor(44, 48, 54), 1));
    QPainterPath surface;
    surface.moveTo(5.0, 20.0);
    surface.cubicTo(10.0, 12.0, 18.0, 12.0, 24.0, 8.0);
    surface.lineTo(24.0, 15.0);
    surface.cubicTo(17.0, 19.0, 10.0, 19.0, 5.0, 24.0);
    surface.closeSubpath();
    painter.drawPath(surface);

    painter.setPen(QPen(QColor(0, 220, 60), 2.0));
    painter.drawLine(QPointF(14.0, 18.0), QPointF(14.0, 6.0));
    painter.drawLine(QPointF(14.0, 6.0), QPointF(10.5, 10.0));
    painter.drawLine(QPointF(14.0, 6.0), QPointF(17.5, 10.0));
    painter.setPen(QPen(QColor(235, 20, 22), 1.2));
    painter.drawArc(QRectF(7.0, 8.0, 14.0, 14.0), 35 * 16, 235 * 16);
    painter.end();
    return QIcon(pixmap);
}

QPixmap GreetingIcon(const QString& key) {
    QPixmap pixmap(132, 100);
    pixmap.fill(QColor(32, 34, 38));
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (key == "new") {
        painter.setPen(QPen(QColor(76, 82, 92), 1));
        const QPointF center(66.0, 52.0);
        for (int i = -5; i <= 5; ++i) {
            painter.drawLine(QPointF(center.x() - 48 + i * 7, center.y() + 24 + i * 3),
                             QPointF(center.x() + 38 + i * 7, center.y() - 20 + i * 3));
            painter.drawLine(QPointF(center.x() - 48 + i * 7, center.y() - 20 - i * 3),
                             QPointF(center.x() + 38 + i * 7, center.y() + 24 - i * 3));
        }
    } else {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(120, 124, 130));
        painter.drawRoundedRect(QRectF(23, 31, 86, 48), 3, 3);
        painter.setBrush(QColor(155, 158, 164));
        painter.drawRoundedRect(QRectF(28, 25, 38, 13), 3, 3);
        painter.setBrush(QColor(174, 176, 181));
        painter.drawRoundedRect(QRectF(20, 39, 92, 45), 3, 3);
        painter.setPen(QPen(QColor(105, 108, 114), 3));
        painter.drawArc(QRectF(52, 47, 30, 30), 20 * 16, 220 * 16);
        painter.drawLine(QPointF(56, 72), QPointF(49, 82));
        painter.drawLine(QPointF(79, 72), QPointF(87, 82));
        if (key == "recent") {
            painter.setPen(QPen(QColor(72, 76, 84), 3));
            painter.drawLine(QPointF(86, 28), QPointF(102, 28));
            painter.drawLine(QPointF(102, 28), QPointF(102, 44));
            painter.drawLine(QPointF(102, 44), QPointF(110, 35));
        }
    }

    painter.end();
    return pixmap;
}

class ClickableLabel final : public QLabel {
public:
    explicit ClickableLabel(const QString& text, QWidget* parent = nullptr)
        : QLabel(text, parent) {
        setCursor(Qt::PointingHandCursor);
    }

    std::function<void()> on_click;

protected:
    void mousePressEvent(QMouseEvent* event) override {
        QLabel::mousePressEvent(event);
        if (on_click) {
            on_click();
        }
    }
};

enum class SolidOperationsDialogAction {
    None,
    Accept,
    Edit,
    Delete
};

struct SolidOperationsDialogResult {
    SolidOperationsDialogAction action = SolidOperationsDialogAction::None;
    int operation_index = -1;
    std::string object_name;
};

SolidOperationsDialogResult ShowSolidOperationsDialog(QWidget* parent,
                                                       CSolid& solid,
                                                       const ToolRegistry& registry,
                                                       const std::function<void()>& name_changed) {
    const std::string original_name = solid.GetName();
    QDialog dialog(parent);
    dialog.setWindowTitle("Solid edition box");
    dialog.setModal(true);
    dialog.resize(210, 300);

    auto* root_layout = new QVBoxLayout(&dialog);
    root_layout->setContentsMargins(8, 8, 8, 8);
    root_layout->setSpacing(8);

    auto* undo_label = new QLabel("Undo unable", &dialog);
    undo_label->setAlignment(Qt::AlignCenter);
    root_layout->addWidget(undo_label);

    auto* name_layout = new QHBoxLayout();
    auto* name_label = new QLabel("Name", &dialog);
    auto* name_edit = new QLineEdit(QString::fromStdString(solid.GetName()), &dialog);
    name_layout->addWidget(name_label);
    name_layout->addWidget(name_edit, 1);
    root_layout->addLayout(name_layout);

    auto* list = new QListWidget(&dialog);
    list->setMinimumHeight(150);
    for (int i = 0; i < solid.GetNumOperations(); ++i) {
        const ParametricFunction* operation = solid.GetOperation(i);
        if (!operation) {
            continue;
        }
        QString label = QString::fromStdString(operation->Name);
        if (label.isEmpty()) {
            label = QString::fromStdString(registry.LabelFor(operation->ToolId));
        }
        if (label.isEmpty()) {
            label = QString::fromStdString(operation->ToolId);
        }
        auto* item = new QListWidgetItem(label, list);
        item->setData(Qt::UserRole, i);
    }
    if (list->count() > 0) {
        list->setCurrentRow(0);
    }
    root_layout->addWidget(list, 1);

    auto* delete_button = new QPushButton("Delete", &dialog);
    root_layout->addWidget(delete_button);

    auto* buttons_layout = new QHBoxLayout();
    auto* ok_button = new QPushButton("OK", &dialog);
    auto* cancel_button = new QPushButton("Cancel", &dialog);
    buttons_layout->addWidget(ok_button);
    buttons_layout->addWidget(cancel_button);
    root_layout->addLayout(buttons_layout);

    SolidOperationsDialogResult result;
    QObject::connect(name_edit, &QLineEdit::textChanged, &dialog, [&solid, name_changed](const QString& text) {
        solid.SetName(text.toStdString());
        if (name_changed) {
            name_changed();
        }
    });
    const auto update_delete_state = [list, delete_button]() {
        delete_button->setEnabled(list->currentItem() && list->currentItem()->data(Qt::UserRole).toInt() > 0);
    };
    update_delete_state();

    const auto update_operation_highlight = [&solid, list, name_changed]() {
        const QListWidgetItem* item = list->currentItem();
        const ParametricFunction* operation =
            item ? solid.GetOperation(item->data(Qt::UserRole).toInt()) : nullptr;
        solid.SetOperationHighlightedSurfaces(
            operation ? operation->CreatedSurfaceIndices : std::vector<int>{});
        if (name_changed) {
            name_changed();
        }
    };
    QObject::connect(list, &QListWidget::currentItemChanged, &dialog, [update_delete_state, update_operation_highlight](QListWidgetItem*, QListWidgetItem*) {
        update_delete_state();
        update_operation_highlight();
    });
    update_operation_highlight();
    QObject::connect(ok_button, &QPushButton::clicked, &dialog, [&dialog, list, name_edit, &result]() {
        result.action = SolidOperationsDialogAction::Accept;
        result.object_name = name_edit->text().trimmed().toStdString();
        dialog.accept();
    });
    QObject::connect(cancel_button, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(delete_button, &QPushButton::clicked, &dialog, [&dialog, list, name_edit, &result]() {
        if (!list->currentItem()) {
            return;
        }
        const int operation_index = list->currentItem()->data(Qt::UserRole).toInt();
        if (operation_index <= 0) {
            return;
        }
        result.action = SolidOperationsDialogAction::Delete;
        result.operation_index = operation_index;
        result.object_name = name_edit->text().trimmed().toStdString();
        dialog.accept();
    });
    QObject::connect(list, &QListWidget::itemDoubleClicked, &dialog, [&dialog, name_edit, &result](QListWidgetItem* item) {
        result.action = SolidOperationsDialogAction::Edit;
        result.operation_index = item->data(Qt::UserRole).toInt();
        result.object_name = name_edit->text().trimmed().toStdString();
        dialog.accept();
    });

    const int dialog_result = dialog.exec();
    solid.ClearOperationHighlightedSurfaces();
    if (name_changed) {
        name_changed();
    }
    if (dialog_result != QDialog::Accepted) {
        solid.SetName(original_name);
        if (name_changed) {
            name_changed();
        }
        return {};
    }
    return result;
}

std::vector<ParametricParameterValue> ToSavedParameters(const std::vector<ToolParameter>& parameters) {
    std::vector<ParametricParameterValue> saved;
    saved.reserve(parameters.size());
    for (const ToolParameter& parameter : parameters) {
        saved.push_back({parameter.id, parameter.value});
    }
    return saved;
}

std::vector<ParametricParameterValue> ToFilletEdgeSavedParameters(const std::vector<ToolParameter>& parameters,
                                                                  const std::vector<std::pair<int, int>>& edge_refs) {
    std::vector<ParametricParameterValue> saved = ToSavedParameters(parameters);
    saved.push_back({"edge.count", static_cast<double>(edge_refs.size())});
    for (size_t i = 0; i < edge_refs.size(); ++i) {
        saved.push_back({"edge." + std::to_string(i) + ".surface", static_cast<double>(edge_refs[i].first)});
        saved.push_back({"edge." + std::to_string(i) + ".edge", static_cast<double>(edge_refs[i].second)});
    }
    return saved;
}
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      viewport_(new OpenGLViewport(this)),
      property_panel_(new PropertyPanel(this)) {
    UpdateWindowTitle();
    resize(1280, 760);

    viewport_->SetDocument(&document_);
    setCentralWidget(viewport_);

    LoadUserSettings();
    CreateActions();
    CreateDocks();
    statusBar()->showMessage("Ready");

    connect(viewport_, &OpenGLViewport::DocumentChanged, this, [this]() {
        tool_registry_.ReplayAllProfileDependents(document_);
        RefreshSceneTree();
    });
    connect(viewport_, &OpenGLViewport::SelectionChanged, this, [this]() {
        if (object_color_pick_pending_ && document_.HasSelection()) {
            object_color_pick_pending_ = false;
            QTimer::singleShot(0, this, [this]() {
                EditSelectedObjectColor();
            });
        }
        if (low_poly_pick_pending_ && document_.GetSelectedSolid()) {
            low_poly_pick_pending_ = false;
            QTimer::singleShot(0, this, [this]() {
                ShowLowPolyTool();
            });
        }
        const bool active_edge_tool = active_parametric_object_.tool_id == "fillet_edge"
            || active_parametric_object_.tool_id == "ChamferSolid";
        if (active_edge_tool) {
            TryStartLiveEdgeToolFromSelection();
        } else if (active_parametric_object_.tool_id == "SolidExtrudeTool") {
            TryStartLivePolylineExtrudeFromSelection();
        } else if (active_parametric_object_.tool_id == "SurfaceOfRevolution") {
            TryStartLivePolylineRevolveFromSelection();
        } else if (active_parametric_object_.tool_id != "ThickSolidTool") {
            ClearActiveProperties();
        }
        RefreshSceneTree();
        UpdateToolAvailability();
    });
    connect(viewport_, &OpenGLViewport::ObjectDoubleClicked, this, [this]() {
        EditSelectedParametricObject();
    });
    connect(viewport_, &OpenGLViewport::StatusTextChanged, this, [this](const QString& text) {
        statusBar()->showMessage(text);
    });
    connect(viewport_, &OpenGLViewport::ToolModeChanged, this, [this](ToolMode tool) {
        if (tool == ToolMode::Orbit) {
            UpdateActiveToolUi("orbit");
        } else if (tool == ToolMode::Select) {
            UpdateActiveToolUi("select");
        } else if (tool == ToolMode::DrawCurve) {
            UpdateActiveToolUi("PolylineCurve");
        } else if (tool == ToolMode::DrawBSpline) {
            UpdateActiveToolUi("BSplineCurve");
        } else if (tool == ToolMode::EditPoint) {
            UpdateActiveToolUi("EditPoint");
        } else if (tool == ToolMode::SketchRectangle) {
            UpdateActiveToolUi("NewSketch");
        }
    });
    connect(viewport_, &OpenGLViewport::BooleanFinished, this, [this]() {
        UpdateActiveToolUi("select");
    });
    connect(property_panel_, &PropertyPanel::ParametersChanged, this, [this]() {
        active_parametric_object_ = property_panel_->ActiveObject();
        if (active_parametric_edit_existing_
            && (active_parametric_object_.tool_id == "fillet_edge"
                || active_parametric_object_.tool_id == "fillet_all_edges"
                || active_parametric_object_.tool_id == "ChamferSolid")) {
            tool_registry_.Rebuild(active_parametric_object_, document_);
            RefreshSceneTree();
            viewport_->update();
            const double value = active_parametric_object_.parameters.empty() ? 0.20 : active_parametric_object_.parameters[0].value;
            const QString label = active_parametric_object_.tool_id == "ChamferSolid"
                ? "Chamfer"
                : (active_parametric_object_.tool_id == "fillet_edge" ? "Fillet Edge" : "Fillet All");
            statusBar()->showMessage(QString("%1: %2").arg(label).arg(value, 0, 'f', 2));
            return;
        }
        if (active_parametric_object_.tool_id == "fillet_edge" || active_parametric_object_.tool_id == "fillet_all_edges") {
            const double radius = active_parametric_object_.parameters.empty() ? 0.20 : active_parametric_object_.parameters[0].value;
            if (!document_.HasLiveFillet()) {
                statusBar()->showMessage("Fillet: выбери кромку тела");
                return;
            }
            const bool rebuilt = document_.UpdateLiveFillet(radius);
            RefreshSceneTree();
            viewport_->update();
            statusBar()->showMessage(rebuilt
                ? QString("Fillet: radius %1").arg(radius, 0, 'f', 2)
                : "Fillet: радиус не подходит");
            return;
        }
        if (active_parametric_object_.tool_id == "ChamferSolid") {
            const double distance = active_parametric_object_.parameters.empty() ? 0.20 : active_parametric_object_.parameters[0].value;
            if (!document_.HasLiveChamfer()) {
                statusBar()->showMessage("Chamfer: выбери кромку тела");
                return;
            }
            const bool rebuilt = document_.UpdateLiveChamfer(distance);
            RefreshSceneTree();
            viewport_->update();
            statusBar()->showMessage(rebuilt
                ? QString("Chamfer: Distance %1").arg(distance, 0, 'f', 2)
                : "Chamfer: размер не подходит");
            return;
        }
        if (active_parametric_object_.tool_id == "ThickSolidTool") {
            const double thickness = active_parametric_object_.parameters.empty() ? 0.0 : active_parametric_object_.parameters[0].value;
            if (active_parametric_edit_existing_) {
                tool_registry_.Rebuild(active_parametric_object_, document_);
                RefreshSceneTree();
                viewport_->update();
                statusBar()->showMessage(QString("ThickSolid: Thick %1").arg(thickness, 0, 'f', 2));
                return;
            }
            viewport_->SetThickSolidThickness(thickness);
            viewport_->update();
            statusBar()->showMessage(QString("ThickSolid: Thick %1, выбери Face и нажми OK").arg(thickness, 0, 'f', 2));
            return;
        }
        if (active_parametric_object_.tool_id == "SolidExtrudeTool") {
            const auto& parameters = active_parametric_object_.parameters;
            const double distance = parameters.size() > 0 ? parameters[0].value : 1.0;
            const bool reverse = parameters.size() > 1 && parameters[1].value >= 0.5;
            const double taper_angle = parameters.size() > 2 ? parameters[2].value : 0.0;
            if (!document_.HasLivePolylineExtrude()) {
                statusBar()->showMessage("Extrude: выбери замкнутую плоскую Polyline");
                return;
            }
            const bool rebuilt = document_.UpdateLiveExtrudeSelectedPolyline(distance, reverse, taper_angle);
            RefreshSceneTree();
            viewport_->update();
            statusBar()->showMessage(rebuilt
                ? QString("Extrude: Distance %1, Angle %2").arg(distance, 0, 'f', 2).arg(taper_angle, 0, 'f', 2)
                : "Extrude: профиль или параметры не подходят");
            return;
        }
        if (active_parametric_object_.tool_id == "SurfaceOfRevolution") {
            const auto& parameters = active_parametric_object_.parameters;
            const double angle = parameters.size() > 0 ? parameters[0].value : 360.0;
            const int axis_index = parameters.size() > 1 ? static_cast<int>(parameters[1].value) : 2;
            if (active_parametric_edit_existing_) {
                tool_registry_.Rebuild(active_parametric_object_, document_);
                RefreshSceneTree();
                viewport_->update();
                statusBar()->showMessage(QString("Revolve: Angle %1").arg(angle, 0, 'f', 1));
                return;
            }
            if (!document_.HasLivePolylineRevolve()) {
                statusBar()->showMessage("Revolve: выбери плоскую Polyline");
                return;
            }
            const bool rebuilt = document_.UpdateLiveRevolveSelectedPolyline(angle, axis_index);
            RefreshSceneTree();
            viewport_->update();
            statusBar()->showMessage(rebuilt
                ? QString("Revolve: Angle %1").arg(angle, 0, 'f', 1)
                : "Revolve: профиль или параметры не подходят");
            return;
        }
        tool_registry_.Rebuild(active_parametric_object_, document_);
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage("Object rebuilt", 1200);
    });
    connect(property_panel_, &PropertyPanel::Accepted, this, [this]() {
        const bool reopen_solid_editor = reopen_solid_editor_after_properties_;
        AcceptActiveProperties();
        if (reopen_solid_editor && active_parametric_object_.tool_id.empty()) {
            reopen_solid_editor_after_properties_ = false;
            QTimer::singleShot(0, this, [this]() {
                EditSelectedParametricObject();
            });
        }
    });
    connect(property_panel_, &PropertyPanel::Canceled, this, [this]() {
        const bool reopen_solid_editor = reopen_solid_editor_after_properties_;
        CancelActiveProperties();
        if (reopen_solid_editor && active_parametric_object_.tool_id.empty()) {
            reopen_solid_editor_after_properties_ = false;
            QTimer::singleShot(0, this, [this]() {
                EditSelectedParametricObject();
            });
        }
    });

    RefreshSceneTree();

    QTimer::singleShot(0, this, [this]() {
        ShowGreetingDialog();
    });
}

void MainWindow::CreateActions() {
    auto* file_menu = menuBar()->addMenu("&File");
    auto* view_menu = menuBar()->addMenu("&View");
    auto* tools_menu = menuBar()->addMenu("&Tools");
    auto* edit_menu = menuBar()->addMenu("&Edit");
    auto* help_menu = menuBar()->addMenu("&Help");

    tab_toolbar_ = addToolBar("Tool Tabs");
    tab_toolbar_->setObjectName("ToolTabs");
    tab_toolbar_->setMovable(false);
    tab_toolbar_->setFloatable(false);
    tab_toolbar_->setIconSize(QSize(0, 0));

    tool_tabs_ = new QTabBar(tab_toolbar_);
    tool_tabs_->setObjectName("Dom3DToolTabs");
    tool_tabs_->setDrawBase(false);
    tool_tabs_->setExpanding(false);
    tool_tabs_->setDocumentMode(true);
    tool_tabs_->setElideMode(Qt::ElideNone);
    tool_tabs_->addTab("Architecture");
    tool_tabs_->addTab("Furniture");
    tool_tabs_->addTab("Surfaces");
    tool_tabs_->addTab("Solid");
    tool_tabs_->addTab("Curves");
    tool_tabs_->addTab("Sketch");
    tool_tabs_->addTab("Assemblies");
    tool_tabs_->setCurrentIndex(0);
    tab_toolbar_->addWidget(tool_tabs_);
    connect(tool_tabs_, &QTabBar::currentChanged, this, [this](int index) {
        if (!tool_tabs_ || index < 0) {
            return;
        }
        PopulateToolsPanelForTab(index);
        statusBar()->showMessage(QString("%1 tab").arg(tool_tabs_->tabText(index)), 1200);
    });

    tab_toolbar_->setStyleSheet(
        "QToolBar#ToolTabs {"
        "  background: #111111;"
        "  border: 0;"
        "  border-bottom: 1px solid #2f2f2f;"
        "  spacing: 0;"
        "  padding: 0 0 0 0;"
        "}"
        "QTabBar#Dom3DToolTabs {"
        "  background: #111111;"
        "}"
        "QTabBar#Dom3DToolTabs::tab {"
        "  background: #1b1b1b;"
        "  color: #cfcfcf;"
        "  border: 1px solid #2b2b2b;"
        "  border-bottom: 0;"
        "  min-width: 96px;"
        "  height: 26px;"
        "  padding: 0 12px;"
        "  margin-right: 1px;"
        "}"
        "QTabBar#Dom3DToolTabs::tab:selected {"
        "  background: #2d2d2d;"
        "  color: #ffffff;"
        "  border-color: #4c4c4c;"
        "}"
        "QTabBar#Dom3DToolTabs::tab:hover:!selected {"
        "  background: #242424;"
        "  color: #ffffff;"
        "}"
    );

    addToolBarBreak(Qt::TopToolBarArea);

    main_toolbar_ = addToolBar("Toolbar");
    main_toolbar_->setMovable(false);
    main_toolbar_->setIconSize(QSize(22, 22));

    const auto add_action = [this](const QString& text, const QKeySequence& shortcut, auto slot) {
        auto* action = new QAction(text, this);
        if (!shortcut.isEmpty()) {
            action->setShortcut(shortcut);
        }
        connect(action, &QAction::triggered, this, slot);
        return action;
    };

    file_menu->addAction(add_action("&New", QKeySequence::New, [this]() { NewProject(); }));
    file_menu->addAction(add_action("&Open...", QKeySequence::Open, [this]() { OpenProject(); }));
    recent_files_menu_ = file_menu->addMenu("Open &Recent");
    file_menu->addAction(add_action("&Save...", QKeySequence::Save, [this]() { SaveProject(); }));
    file_menu->addSeparator();
    file_menu->addAction(add_action("&Preferences...", {}, [this]() { ShowPreferences(); }));
    file_menu->addSeparator();
    file_menu->addAction(add_action("&Import...", QKeySequence(Qt::CTRL | Qt::Key_I), [this]() { ImportFile(); }));
    file_menu->addAction(add_action("&Export...", QKeySequence(Qt::CTRL | Qt::Key_E), [this]() { ExportFile(); }));
    file_menu->addSeparator();
    file_menu->addAction(add_action("E&xit", QKeySequence::Quit, [this]() { close(); }));

    auto* solid_display_menu = view_menu->addMenu("Solid Display");
    auto* solid_display_group = new QActionGroup(this);
    solid_display_group->setExclusive(true);
    const auto add_solid_display_action = [this, solid_display_menu, solid_display_group](const QString& text, SolidDisplayMode mode) {
        auto* action = solid_display_menu->addAction(text, this, [this, mode]() {
            SetSolidDisplayMode(mode);
        });
        action->setCheckable(true);
        action->setData(static_cast<int>(mode));
        solid_display_group->addAction(action);
        action->setChecked(CSolid::GetDisplayMode() == mode);
        return action;
    };
    surfaces_edges_action_ = add_solid_display_action("Surfaces and Edges", SolidDisplayMode::SurfacesAndEdges);
    mesh_only_action_ = add_solid_display_action("Mesh Only", SolidDisplayMode::MeshOnly);
    surfaces_wire_action_ = add_solid_display_action("Surfaces and Raised Mesh", SolidDisplayMode::SurfacesAndRaisedMesh);
    solid_wireframe_action_ = add_solid_display_action("Wireframe", SolidDisplayMode::Wireframe);
    auto* toggle_wire_shaded_action = view_menu->addAction("Wired / Shaded", this, [this]() {
        ToggleWireShadedDisplay();
    });
    toggle_wire_shaded_action->setShortcut(Qt::Key_W);
    auto* orbit_mode_menu = view_menu->addMenu("Orbit Mode");
    auto* orbit_mode_group = new QActionGroup(this);
    orbit_mode_group->setExclusive(true);
    cad_orbit_action_ = orbit_mode_menu->addAction("CAD", this, [this]() {
        SetOrbitMode(OrbitMode::CAD);
    });
    cad_orbit_action_->setCheckable(true);
    cad_orbit_action_->setChecked(viewport_->GetOrbitMode() == OrbitMode::CAD);
    orbit_mode_group->addAction(cad_orbit_action_);
    architectural_orbit_action_ = orbit_mode_menu->addAction("Architectural", this, [this]() {
        SetOrbitMode(OrbitMode::Architectural);
    });
    architectural_orbit_action_->setCheckable(true);
    architectural_orbit_action_->setChecked(viewport_->GetOrbitMode() == OrbitMode::Architectural);
    orbit_mode_group->addAction(architectural_orbit_action_);
    view_menu->addSeparator();
    orthographic_projection_action_ = add_action("Orthographic Projection", QKeySequence(Qt::Key_5 | Qt::KeypadModifier), [this](bool checked) {
        SetOrthographicProjection(checked);
    });
    orthographic_projection_action_->setCheckable(true);
    orthographic_projection_action_->setChecked(viewport_->IsOrthographicProjection());
    view_menu->addAction(orthographic_projection_action_);
    projection_status_label_ = new QLabel(this);
    projection_status_label_->setMinimumWidth(108);
    projection_status_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    projection_status_label_->setStyleSheet("QLabel { color: #777777; padding-right: 4px; }");
    statusBar()->addPermanentWidget(projection_status_label_);
    UpdateProjectionStatus();

    auto* transparent_solid_action = view_menu->addAction("Transparent Solid Surfaces", this, [this](bool checked) {
        CSolid::SetSurfaceTransparencyEnabled(checked);
        QSettings settings;
        settings.setValue("view/solidSurfaceTransparency", checked);
        viewport_->update();
        statusBar()->showMessage(checked ? "Solid transparency enabled" : "Solid transparency disabled", 1400);
    });
    transparent_solid_action->setCheckable(true);
    transparent_solid_action->setChecked(CSolid::IsSurfaceTransparencyEnabled());

    auto* orbit_action = add_action("Orbit", {}, [this]() { SetTool(ToolMode::Orbit, "Orbit camera"); });
    auto* select_action = add_action("Select", {}, [this]() { SetTool(ToolMode::Select, "Select objects"); });
    auto* curve_action = add_action("Curve", {}, [this]() {
        ActivateParametricTool("PolylineCurve");
    });
    auto* transform_action = add_action("Transform", {}, [this]() { BeginTransformTool(TransformOperation::Move); });
    auto* move_action = add_action("Move", {}, [this]() { BeginTransformTool(TransformOperation::Move); });
    auto* rotate_action = add_action("Rotate", {}, [this]() { BeginTransformTool(TransformOperation::Rotate); });
    auto* scale_action = add_action("Scale", {}, [this]() { BeginTransformTool(TransformOperation::Scale); });
    auto* new_sketch_action = add_action("New Sketch", {}, [this]() { BeginNewSketch(); });
    auto* duplicate_action = add_action("Make Object/Group Copy", {}, [this]() { DuplicateSelectedObject(); });
    duplicate_action->setToolTip("Make a copy of the selected object or whole group and move it");
    duplicate_action->setIcon(DuplicateObjectIcon());
    auto* mirror_action = add_action("Mirror Object by Plane...", {}, [this]() { MirrorSelectedObject(); });
    mirror_action->setToolTip("Create a mirrored copy of the selected object or group");
    mirror_action->setIcon(MirrorObjectIcon());
    auto* all_scene_action = add_action("All Scene", {}, [this]() {
        viewport_->FitToDocument();
        statusBar()->showMessage("All scene fitted", 1400);
    });
    auto* material_editor_action = add_action("Material Editor...", {}, [this]() {
        ShowMaterialEditor();
    });
    RegisterToolAction(orbit_action, "orbit");
    RegisterToolAction(select_action, "select");
    RegisterToolAction(curve_action, "PolylineCurve");
    RegisterToolAction(transform_action, "transform");
    RegisterToolAction(move_action, "move");
    RegisterToolAction(rotate_action, "rotate");
    RegisterToolAction(scale_action, "scale");
    RegisterToolAction(new_sketch_action, "NewSketch");

    tools_menu->addAction(material_editor_action);
    tools_menu->addSeparator();
    tools_menu->addAction(orbit_action);
    tools_menu->addAction(select_action);
    tools_menu->addAction(curve_action);
    tools_menu->addAction(transform_action);
    tools_menu->addSeparator();
    tools_menu->addAction(move_action);
    tools_menu->addAction(rotate_action);
    tools_menu->addAction(scale_action);
    tools_menu->addAction(mirror_action);
    tools_menu->addAction(new_sketch_action);
    tools_menu->addSeparator();
    for (const ToolDefinition& tool : tool_registry_.Tools()) {
        auto* action = tools_menu->addAction(QString::fromStdString(tool.label), this, [this, id = tool.id]() {
            ActivateParametricTool(id);
        });
        RegisterToolAction(action, tool.id);
    }

    edit_menu->addAction(add_action("Close Polyline", {}, [this]() {
        if (document_.CloseSelectedOrActivePolyline()) {
            RefreshSceneTree();
            viewport_->update();
            statusBar()->showMessage("Polyline closed", 1200);
        } else {
            statusBar()->showMessage("Polyline: need at least 3 points", 1600);
        }
    }));
    edit_menu->addAction(add_action("&Delete Selected", QKeySequence::Delete, [this]() { DeleteSelected(); }));

    help_menu->addAction(add_action("Help &Topics", QKeySequence::HelpContents, [this]() {
        QMessageBox::information(this, "Help Topics", "Help system will be added here.");
    }));
    help_menu->addSeparator();
    help_menu->addAction(add_action("&About Dom-3D...", {}, [this]() {
        QMessageBox::about(this, "About Dom-3D",
                           "Dom3D Pro\nApplication for 3D modeling.");
    }));
    help_menu->addAction(add_action("&Support service", {}, [this]() {
        QMessageBox::information(this, "Support service", "Support service page will be connected here.");
    }));
    help_menu->addAction(add_action("&Website House-3D", {}, []() {
        QDesktopServices::openUrl(QUrl("https://dom3d.com.ua/"));
    }));
    help_menu->addAction(add_action("&YouTube Channel", {}, []() {
        QDesktopServices::openUrl(QUrl("https://www.youtube.com/@Dom3d"));
    }));
    help_menu->addAction(add_action("&Greeting Box...", {}, [this]() {
        ShowGreetingDialog(true);
    }));

    main_toolbar_->addAction(orbit_action);
    main_toolbar_->addAction(select_action);
    main_toolbar_->addAction(curve_action);
    main_toolbar_->addAction(transform_action);
    main_toolbar_->addAction(duplicate_action);
    main_toolbar_->addAction(mirror_action);
    main_toolbar_->addSeparator();
    main_toolbar_->addAction(move_action);
    main_toolbar_->addAction(rotate_action);
    main_toolbar_->addAction(scale_action);
    main_toolbar_->addAction(new_sketch_action);
    main_toolbar_->addSeparator();

    auto* selection_mode_group = new QActionGroup(main_toolbar_);
    selection_mode_group->setExclusive(true);
    const auto add_selection_mode_action = [this, selection_mode_group](const QString& text, const QString& tooltip, SelectionMode mode) {
        auto* action = new QAction(text, selection_mode_group);
        action->setToolTip(tooltip);
        action->setCheckable(true);
        action->setData(static_cast<int>(mode));
        action->setChecked(viewport_->GetSelectionMode() == mode);
        connect(action, &QAction::triggered, this, [this, mode, tooltip]() {
            viewport_->SetTool(ToolMode::Select);
            viewport_->SetSelectionMode(mode);
            UpdateActiveToolUi("select");
            statusBar()->showMessage(tooltip, 1200);
        });
        selection_mode_group->addAction(action);
        main_toolbar_->addAction(action);
        return action;
    };
    add_selection_mode_action("Obj", "Select objects", SelectionMode::Object);
    add_selection_mode_action("Face", "Select planar faces", SelectionMode::Face);
    add_selection_mode_action("Edge", "Select solid edges", SelectionMode::Edge);
    add_selection_mode_action("Pt", "Select curve points", SelectionMode::Point);

    coordinate_axes_check_box_ = new QCheckBox("Axis", main_toolbar_);
    coordinate_axes_check_box_->setToolTip("Show coordinate axes");
    coordinate_axes_check_box_->setChecked(viewport_->IsCoordinateAxesVisible());
    connect(coordinate_axes_check_box_, &QCheckBox::toggled, this, [this](bool checked) {
        SetCoordinateAxesVisible(checked);
    });
    main_toolbar_->addWidget(coordinate_axes_check_box_);

    floor_grid_check_box_ = new QCheckBox("Grid", main_toolbar_);
    floor_grid_check_box_->setToolTip("Show floor grid");
    floor_grid_check_box_->setChecked(viewport_->IsFloorGridVisible());
    connect(floor_grid_check_box_, &QCheckBox::toggled, this, [this](bool checked) {
        SetFloorGridVisible(checked);
    });
    main_toolbar_->addWidget(floor_grid_check_box_);

    xy_plane_view_check_box_ = new QCheckBox("Plane XY", main_toolbar_);
    xy_plane_view_check_box_->setToolTip("Top view on XY plane, lock scene rotation");
    xy_plane_view_check_box_->setChecked(viewport_->IsXYPlaneViewEnabled());
    connect(xy_plane_view_check_box_, &QCheckBox::toggled, this, [this](bool checked) {
        SetXYPlaneViewEnabled(checked);
    });
    main_toolbar_->addWidget(xy_plane_view_check_box_);

    mesh_display_button_ = new QPushButton(main_toolbar_);
    mesh_display_button_->setToolTip("Mesh display mode");
    mesh_display_button_->setMinimumWidth(136);
    mesh_display_button_->setFlat(true);
    mesh_display_menu_ = new QMenu(mesh_display_button_);
    auto* toolbar_mesh_group = new QActionGroup(mesh_display_button_);
    toolbar_mesh_group->setExclusive(true);
    const auto add_toolbar_mesh_action = [this, toolbar_mesh_group](const QString& text, MeshDisplayMode mode) {
        auto* action = mesh_display_menu_->addAction(text, this, [this, mode]() {
            SetMeshDisplayMode(mode);
        });
        action->setCheckable(true);
        action->setData(static_cast<int>(mode));
        action->setChecked(CMesh3D::GetDisplayMode() == mode);
        toolbar_mesh_group->addAction(action);
        return action;
    };
    add_toolbar_mesh_action("Surface Gray", MeshDisplayMode::SurfaceGray);
    add_toolbar_mesh_action("Surface Colored", MeshDisplayMode::SurfaceColored);
    add_toolbar_mesh_action("Surface Material", MeshDisplayMode::SurfaceMaterial);
    add_toolbar_mesh_action("Wire", MeshDisplayMode::Wire);
    connect(mesh_display_button_, &QPushButton::clicked, this, [this]() {
        if (!mesh_display_button_ || !mesh_display_menu_) {
            return;
        }
        mesh_display_menu_->exec(mesh_display_button_->mapToGlobal(QPoint(0, mesh_display_button_->height())));
    });
    main_toolbar_->addWidget(mesh_display_button_);
    SetMeshDisplayMode(CMesh3D::GetDisplayMode());
    auto* opacity_label = new QLabel("Opacity", main_toolbar_);
    opacity_label->setStyleSheet("QLabel { padding-left: 6px; padding-right: 2px; }");
    main_toolbar_->addWidget(opacity_label);
    mesh_opacity_slider_ = new QSlider(Qt::Horizontal, main_toolbar_);
    mesh_opacity_slider_->setToolTip("Mesh line opacity");
    mesh_opacity_slider_->setRange(5, 100);
    mesh_opacity_slider_->setSingleStep(1);
    mesh_opacity_slider_->setPageStep(5);
    mesh_opacity_slider_->setFixedWidth(96);
    mesh_opacity_slider_->setValue(static_cast<int>(std::round(CMesh3D::GetWireOpacity() * 100.0f)));
    connect(mesh_opacity_slider_, &QSlider::valueChanged, this, [this](int value) {
        SetMeshWireOpacity(static_cast<float>(value) / 100.0f);
    });
    main_toolbar_->addWidget(mesh_opacity_slider_);
    mesh_opacity_value_label_ = new QLabel(QString("%1%").arg(mesh_opacity_slider_->value()), main_toolbar_);
    mesh_opacity_value_label_->setMinimumWidth(38);
    mesh_opacity_value_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    main_toolbar_->addWidget(mesh_opacity_value_label_);
    auto* color_button = new QPushButton("Color", main_toolbar_);
    color_button->setToolTip("Edit selected object color");
    color_button->setMinimumWidth(62);
    connect(color_button, &QPushButton::clicked, this, &MainWindow::RequestObjectColor);
    main_toolbar_->addWidget(color_button);
    auto* material_editor_button = new QPushButton("Material", main_toolbar_);
    material_editor_button->setToolTip("Material Editor");
    material_editor_button->setMinimumWidth(72);
    connect(material_editor_button, &QPushButton::clicked, this, [this]() {
        ShowMaterialEditor(has_selected_library_material_ ? &selected_library_material_ : nullptr);
    });
    main_toolbar_->addWidget(material_editor_button);
    edit_texture_button_ = new QPushButton("Edit Texture", main_toolbar_);
    edit_texture_button_->setToolTip("Edit texture coordinates for selected surfaces");
    edit_texture_button_->setMinimumWidth(92);
    connect(edit_texture_button_, &QPushButton::clicked, this, &MainWindow::ShowSurfaceTextureEditor);
    main_toolbar_->addWidget(edit_texture_button_);
    main_toolbar_->addAction(all_scene_action);
    UpdateActiveToolUi(active_tool_key_);
    UpdateRecentFilesMenu();
}

void MainWindow::CreateDocks() {
    tools_dock_ = new QDockWidget("Architecture", this);
    tools_dock_->setMinimumWidth(104);
    CreateToolsPanel(tools_dock_);
    addDockWidget(Qt::LeftDockWidgetArea, tools_dock_);

    scene_tree_dock_ = new QDockWidget("Scene Tree", this);
    scene_tree_dock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    scene_tree_dock_->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    scene_tree_dock_->setMinimumWidth(240);
    scene_tree_dock_->setMinimumHeight(180);
    scene_tree_ = new QTreeWidget(scene_tree_dock_);
    scene_tree_->setHeaderLabels({"Visible", "Object", "Type"});
    scene_tree_->setIconSize(QSize(20, 20));
    scene_tree_->setColumnWidth(0, 92);
    scene_tree_->setStyleSheet(
        "QTreeWidget::item:selected {"
        "  background: #2f78c4;"
        "  color: #ffffff;"
        "}"
        "QTreeWidget::item:selected:!active {"
        "  background: #3d82c9;"
        "  color: #ffffff;"
        "}"
    );
    scene_tree_dock_->setWidget(scene_tree_);
    addDockWidget(Qt::RightDockWidgetArea, scene_tree_dock_);
    connect(scene_tree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int column) {
        OnSceneTreeItemClicked(item, column);
    });

    properties_dock_ = new QDockWidget("Property Panel", this);
    properties_dock_->setWidget(property_panel_);
    properties_dock_->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, properties_dock_);
    properties_dock_->hide();

    CreateMaterialLibraryDock();
}

void MainWindow::CreateMaterialLibraryDock() {
    material_library_dock_ = new QDockWidget("Materials Library", this);
    material_library_dock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    material_library_dock_->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    material_library_dock_->setMinimumWidth(280);
    material_library_dock_->setMinimumHeight(260);
    material_library_dock_->resize(330, 360);

    auto* panel = new QWidget(material_library_dock_);
    auto* root_layout = new QVBoxLayout(panel);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(0);

    auto* tabs = new QTabBar(panel);
    tabs->addTab("Shaders");
    tabs->addTab("Poly Models");
    tabs->setCurrentIndex(0);
    root_layout->addWidget(tabs);

    auto* controls = new QWidget(panel);
    auto* controls_layout = new QHBoxLayout(controls);
    controls_layout->setContentsMargins(8, 8, 8, 6);
    controls_layout->setSpacing(6);

    auto* category_combo = new QComboBox(controls);
    auto* add_button = new QPushButton("+", controls);
    auto* search_button = new QPushButton(controls);
    search_button->setIcon(SearchIcon());
    search_button->setIconSize(QSize(18, 18));
    search_button->setToolTip("Search materials");
    search_button->setCheckable(true);
    add_button->setToolTip("Add material to current library category");
    add_button->setFixedSize(26, 24);
    search_button->setFixedSize(26, 24);
    controls_layout->addWidget(category_combo, 1);
    controls_layout->addWidget(add_button);
    controls_layout->addWidget(search_button);
    root_layout->addWidget(controls);

    auto* search_edit = new QLineEdit(panel);
    search_edit->setPlaceholderText("Search materials");
    search_edit->setClearButtonEnabled(true);
    search_edit->hide();
    root_layout->addWidget(search_edit);

    auto* material_list = new MaterialListWidget(panel);
    material_list->setViewMode(QListView::IconMode);
    material_list->setMovement(QListView::Static);
    material_list->setResizeMode(QListView::Adjust);
    material_list->setWrapping(true);
    material_list->setSpacing(8);
    material_list->setIconSize(QSize(46, 46));
    material_list->setGridSize(QSize(58, 58));
    material_list->setUniformItemSizes(true);
    material_list->setTextElideMode(Qt::ElideRight);
    root_layout->addWidget(material_list, 1);

    panel->setStyleSheet(
        "QWidget { background: #1f1f1f; color: #e5e5e5; }"
        "QTabBar::tab {"
        "  background: #1f1f1f;"
        "  color: #9b9b9b;"
        "  min-width: 78px;"
        "  height: 30px;"
        "  padding: 0 8px;"
        "  border-right: 1px solid #333333;"
        "}"
        "QTabBar::tab:selected { background: #2b2b2b; color: #ffffff; }"
        "QComboBox, QLineEdit { background: #555555; color: #eeeeee; border: 1px solid #777777; padding: 2px 6px; }"
        "QPushButton { background: #3f3f3f; color: #eeeeee; border: 1px solid #565656; }"
        "QPushButton:hover { background: #505050; }"
        "QListWidget { background: #1f1f1f; border: 0; outline: 0; }"
        "QListWidget::item { color: transparent; }"
        "QListWidget::item:selected { background: #3b5f8f; }"
    );

    auto library = std::make_shared<MaterialLibrary>();
    library->Load(MaterialLibrary::DefaultLibraryPath());

    std::set<QString> categories;
    for (const auto& entry : library->Entries()) {
        categories.insert(entry.category);
    }
    category_combo->addItem("Default");
    category_combo->addItem("Document");
    for (const QString& default_category : MaterialLibrary::DefaultCategories()) {
        if (default_category != "Default" && categories.find(default_category) != categories.end()) {
            category_combo->addItem(default_category);
            categories.erase(default_category);
        }
    }
    categories.erase("Default");
    for (const QString& category : categories) {
        category_combo->addItem(category);
    }

    const auto material_from_item = [this, library](QListWidgetItem* item, Material* material) {
        if (!item || !material) {
            return false;
        }

        const auto source = static_cast<MaterialListSource>(item->data(kMaterialSourceRole).toInt());
        if (source == MaterialListSource::Document) {
            const int index = item->data(kMaterialDocumentIndexRole).toInt();
            const auto& materials = document_.GetMaterials();
            if (index < 0 || index >= static_cast<int>(materials.size())) {
                return false;
            }
            *material = materials[index];
            return true;
        }

        const int index = item->data(kMaterialLibraryEntryRole).toInt();
        const auto& entries = library->Entries();
        if (index < 0 || index >= static_cast<int>(entries.size())) {
            return false;
        }
        *material = entries[index].material;
        material->source_file_path = entries[index].file_path.toStdString();
        return true;
    };
    material_list->SetMaterialResolver(material_from_item);

    const auto populate_materials = [this, library, material_list](const QString& category, const QString& query) {
        material_list->clear();
        const QString normalized_query = query.trimmed();
        const auto matches_query = [&normalized_query](const QString& material_name, const QString& category_name) {
            return normalized_query.isEmpty()
                || material_name.contains(normalized_query, Qt::CaseInsensitive)
                || category_name.contains(normalized_query, Qt::CaseInsensitive);
        };

        if (category == "Document") {
            const auto& materials = document_.GetMaterials();
            for (int i = 0; i < static_cast<int>(materials.size()); ++i) {
                const Material& material = materials[i];
                const QString material_name = QString::fromStdString(material.name);
                if (!matches_query(material_name, "Document")) {
                    continue;
                }
                auto* item = new QListWidgetItem(QIcon(MaterialDrag::SpherePixmap(material, 48, false)), material_name, material_list);
                item->setData(kMaterialSourceRole, static_cast<int>(MaterialListSource::Document));
                item->setData(kMaterialDocumentIndexRole, i);
                item->setToolTip(QString("Document / %1").arg(material_name));
            }
            return;
        }

        const auto& entries = library->Entries();
        for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
            const auto& entry = entries[i];
            if (category != "Default" && entry.category != category) {
                continue;
            }
            const QString material_name = QString::fromStdString(entry.material.name);
            if (!matches_query(material_name, entry.category)) {
                continue;
            }
            auto* item = new QListWidgetItem(QIcon(MaterialDrag::SpherePixmap(entry.material, 48, false, entry.file_path)), QString::fromStdString(entry.material.name), material_list);
            item->setData(kMaterialSourceRole, static_cast<int>(MaterialListSource::Library));
            item->setData(kMaterialLibraryEntryRole, i);
            item->setToolTip(QString("%1 / %2").arg(entry.category, QString::fromStdString(entry.material.name)));
        }
    };
    refresh_material_library_ = [library, populate_materials, category_combo, search_edit]() {
        library->Load(MaterialLibrary::DefaultLibraryPath());
        populate_materials(category_combo->currentText(), search_edit->text());
    };

    populate_materials(category_combo->currentText(), search_edit->text());
    connect(category_combo, &QComboBox::currentTextChanged, this, [populate_materials, search_edit](const QString& category) {
        populate_materials(category, search_edit->text());
    });
    connect(search_button, &QPushButton::toggled, this, [search_edit](bool checked) {
        search_edit->setVisible(checked);
        if (checked) {
            search_edit->setFocus();
        } else {
            search_edit->clear();
        }
    });
    connect(search_edit, &QLineEdit::textChanged, this, [populate_materials, category_combo](const QString& query) {
        populate_materials(category_combo->currentText(), query);
    });
    connect(material_list, &QListWidget::itemClicked, this, [this, material_from_item](QListWidgetItem* item) {
        Material material;
        if (!material_from_item(item, &material)) {
            has_selected_library_material_ = false;
            return;
        }
        selected_library_material_ = material;
        has_selected_library_material_ = true;
        statusBar()->showMessage(QString("Material selected: %1").arg(QString::fromStdString(selected_library_material_.name)), 1400);
    });
    connect(material_list, &QListWidget::itemDoubleClicked, this, [this, material_from_item, populate_materials, category_combo, search_edit](QListWidgetItem* item) {
        Material material;
        if (!material_from_item(item, &material)) {
            return;
        }
        const auto source = static_cast<MaterialListSource>(item->data(kMaterialSourceRole).toInt());
        if (source == MaterialListSource::Library) {
            ShowMaterialEditor(&material, QString::fromStdString(material.source_file_path));
            return;
        }
        if (document_.HasSelection()) {
            ApplyMaterialToSelection(material);
        } else {
            SaveMaterialToDocument(material);
            populate_materials(category_combo->currentText(), search_edit->text());
        }
    });
    material_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(material_list, &QListWidget::customContextMenuRequested, this, [this, material_list, material_from_item, populate_materials, category_combo, search_edit](const QPoint& position) {
        QListWidgetItem* item = material_list->itemAt(position);
        Material material;
        if (!material_from_item(item, &material)) {
            return;
        }
        const auto source = static_cast<MaterialListSource>(item->data(kMaterialSourceRole).toInt());
        selected_library_material_ = material;
        has_selected_library_material_ = true;

        QMenu menu(material_list);
        QAction* apply_action = menu.addAction("Apply to Selection");
        apply_action->setEnabled(document_.HasSelection());
        QAction* edit_action = menu.addAction("Edit...");
        QAction* duplicate_action = menu.addAction("Duplicate to Document");
        QAction* delete_action = menu.addAction("Delete from Document");
        delete_action->setEnabled(document_.FindMaterial(material.id) != nullptr);

        QAction* chosen = menu.exec(material_list->viewport()->mapToGlobal(position));
        if (!chosen) {
            return;
        }
        if (chosen == apply_action) {
            ApplyMaterialToSelection(material);
        } else if (chosen == edit_action) {
            const QString source_path = source == MaterialListSource::Library ? QString::fromStdString(material.source_file_path) : QString();
            ShowMaterialEditor(&material, source_path);
        } else if (chosen == duplicate_action) {
            Material duplicate = material;
            duplicate.id = 0;
            duplicate.name += " Copy";
            SaveMaterialToDocument(duplicate);
            category_combo->setCurrentText("Document");
            populate_materials(category_combo->currentText(), search_edit->text());
        } else if (chosen == delete_action) {
            if (document_.DeleteMaterial(material.id)) {
                RefreshSceneTree();
                viewport_->update();
                populate_materials(category_combo->currentText(), search_edit->text());
                statusBar()->showMessage(QString("Material deleted from document: %1").arg(QString::fromStdString(material.name)), 1400);
            }
        }
    });
    connect(add_button, &QPushButton::clicked, this, [this, library, material_list, material_from_item, populate_materials, category_combo, search_edit]() {
        QString category = category_combo->currentText();
        if (category == "Document" || category.isEmpty()) {
            QMessageBox::information(this,
                                     "Add Material",
                                     "Select a library category before adding a material.");
            return;
        }

        Material selected_material;
        const bool has_library_selection = material_list->currentItem()
            && static_cast<MaterialListSource>(material_list->currentItem()->data(kMaterialSourceRole).toInt()) == MaterialListSource::Library
            && material_from_item(material_list->currentItem(), &selected_material);

        QMessageBox choice(this);
        choice.setWindowTitle("Add Material");
        choice.setText(QString("Add material to \"%1\".").arg(category));
        choice.setInformativeText("Create a default material or copy the selected library material?");
        QPushButton* default_button = choice.addButton("Default Material", QMessageBox::AcceptRole);
        QPushButton* copy_button = choice.addButton("Copy Selected", QMessageBox::AcceptRole);
        QPushButton* cancel_button = choice.addButton(QMessageBox::Cancel);
        copy_button->setEnabled(has_library_selection);
        if (!has_library_selection) {
            copy_button->setToolTip("Select a material in the library first");
        }
        choice.exec();
        if (choice.clickedButton() == cancel_button || !choice.clickedButton()) {
            return;
        }

        Material material = Material::DefaultWhite();
        if (choice.clickedButton() == copy_button) {
            material = selected_material;
            material.name += " Copy";
        } else if (choice.clickedButton() == default_button) {
            material.name = "New Material";
        }

        unsigned long next_id = 1;
        for (const Material& existing : document_.GetMaterials()) {
            next_id = std::max(next_id, existing.id + 1);
        }
        for (const auto& entry : library->Entries()) {
            next_id = std::max(next_id, entry.material.id + 1);
        }
        material.id = next_id;

        QDir category_dir(QDir(MaterialLibrary::DefaultLibraryPath()).filePath(category));
        if (!category_dir.exists() && !QDir().mkpath(category_dir.absolutePath())) {
            QMessageBox::critical(this, "Add Material", QString("Cannot create category folder:\n%1").arg(category_dir.absolutePath()));
            return;
        }

        QString base_name = QString::fromStdString(material.name).trimmed();
        if (base_name.isEmpty()) {
            base_name = "Material";
        }
        for (QChar& ch : base_name) {
            if (QString("\\/:*?\"<>|").contains(ch)) {
                ch = '_';
            }
        }

        QString file_path = category_dir.filePath(base_name + ".d3mat");
        int suffix = 2;
        while (QFileInfo::exists(file_path)) {
            file_path = category_dir.filePath(QString("%1_%2.d3mat").arg(base_name).arg(suffix++));
        }
        material.source_file_path = file_path.toStdString();

        QString error;
        if (!library->SaveMaterial(file_path, material, &error)) {
            QMessageBox::critical(this, "Add Material", error);
            return;
        }
        library->Load(MaterialLibrary::DefaultLibraryPath());
        if (category_combo->findText(category) < 0) {
            category_combo->addItem(category);
        }
        category_combo->setCurrentText(category);
        populate_materials(category_combo->currentText(), search_edit->text());
        ShowMaterialEditor(&material, file_path);
    });

    material_library_dock_->setWidget(panel);
    addDockWidget(Qt::RightDockWidgetArea, material_library_dock_);
    if (scene_tree_dock_) {
        splitDockWidget(scene_tree_dock_, material_library_dock_, Qt::Vertical);
        resizeDocks({scene_tree_dock_, material_library_dock_}, {320, 520}, Qt::Vertical);
    }
    material_library_dock_->setFloating(false);
    material_library_dock_->show();
    scene_tree_dock_->raise();
}

void MainWindow::CreateToolsPanel(QDockWidget* dock) {
    tools_panel_ = new QWidget(dock);
    tools_layout_ = new QGridLayout(tools_panel_);
    tools_layout_->setContentsMargins(8, 8, 8, 8);
    tools_layout_->setHorizontalSpacing(4);
    tools_layout_->setVerticalSpacing(4);

    tools_panel_->setStyleSheet(
        "QPushButton {"
        "  background-color: #f6f6f6;"
        "  border: 1px solid #b9b9b9;"
        "  border-radius: 2px;"
        "}"
        "QPushButton:hover:!disabled {"
        "  background-color: #eaf2ff;"
        "  border-color: #6ea0e8;"
        "}"
        "QPushButton[placeholder=\"true\"] {"
        "  background-color: #eeeeee;"
        "  border-color: #b9b9b9;"
        "}"
        "QPushButton:checked {"
        "  background-color: #2f80ed;"
        "  color: white;"
        "  border: 1px solid #1f5fbf;"
        "  font-weight: 600;"
        "}"
    );
    dock->setWidget(tools_panel_);
    PopulateToolsPanelForTab(tool_tabs_ ? tool_tabs_->currentIndex() : 0);
}

void MainWindow::AddToolButton(QGridLayout* layout, QWidget* parent, const std::string& key, int row, int column) {
    const ToolDefinition* tool = tool_registry_.Find(key);
    if (!tool) {
        return;
    }

    auto* button = new QPushButton(parent);
    button->setToolTip(QString::fromStdString(tool->label));
    button->setFixedSize(42, 36);
    RegisterToolButton(button, key);
    connect(button, &QPushButton::clicked, this, [this, key]() { ActivateParametricTool(key); });
    layout->addWidget(button, row, column);
}

void MainWindow::AddPlaceholderButton(QGridLayout* layout, QWidget* parent, const QString& icon_key, const QString& title, int row, int column) {
    auto* button = new QPushButton(parent);
    button->setToolTip(title);
    button->setIcon(ToolIcon(icon_key.toStdString()));
    button->setIconSize(QSize(28, 28));
    button->setFixedSize(42, 36);
    button->setProperty("placeholder", true);
    connect(button, &QPushButton::clicked, this, [this, title]() {
        statusBar()->showMessage(QString("%1: tool is not connected yet").arg(title), 1200);
    });
    layout->addWidget(button, row, column);
}

void MainWindow::RefreshSceneTree() {
    QSignalBlocker blocker(scene_tree_);
    std::set<QString> expanded_groups;
    const bool had_tree_items = scene_tree_->topLevelItemCount() > 0;
    for (int i = 0; i < scene_tree_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* top_item = scene_tree_->topLevelItem(i);
        if (top_item && top_item->isExpanded()) {
            const QVariant group_name = top_item->data(0, kSceneTreeGroupRole);
            if (group_name.isValid()) {
                expanded_groups.insert(group_name.toString());
            }
        }
    }

    scene_tree_->clear();
    const auto& objects = document_.GetObjects();
    std::map<QString, QTreeWidgetItem*> group_items;

    for (size_t i = 0; i < objects.size(); ++i) {
        const CAlfaObject* object = objects[i].get();
        if (!object) {
            continue;
        }

        QString type = "Object";
        if (const auto* polyline = dynamic_cast<const CPolyline*>(object)) {
            if (polyline->IsEmpty()) {
                continue;
            }
            type = "Curve";
        } else if (const auto* spline = dynamic_cast<const CBSpline*>(object)) {
            if (spline->IsEmpty()) {
                continue;
            }
            type = "B-Spline";
        } else if (dynamic_cast<const CCadCurve3D*>(object)) {
            type = "CAD Curve";
        } else if (dynamic_cast<const CMesh3D*>(object)) {
            type = "Mesh";
        } else if (dynamic_cast<const CSurfaceSet*>(object)) {
            type = "Surface Set";
        } else if (dynamic_cast<const CSolid*>(object)) {
            type = "Solid";
        }

        QTreeWidgetItem* parent = nullptr;
        const QString group_name = QString::fromStdString(object->GetGroupName());
        if (!group_name.isEmpty()) {
            auto existing_group = group_items.find(group_name);
            if (existing_group == group_items.end()) {
                auto* group_item = new QTreeWidgetItem(scene_tree_);
                group_item->setText(1, group_name);
                group_item->setText(2, "Group");
                group_item->setData(0, kSceneTreeGroupRole, group_name);
                group_item->setExpanded(!had_tree_items || expanded_groups.count(group_name) > 0);
                existing_group = group_items.emplace(group_name, group_item).first;
            }
            parent = existing_group->second;
        }

        auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(scene_tree_);
        item->setIcon(0, SceneVisibilityIcon(object->IsVisible()));
        item->setToolTip(0, object->IsVisible() ? "Hide object" : "Show object");
        item->setText(1, QString::fromStdString(object->GetName()));
        item->setText(2, type);
        item->setData(0, kSceneTreeObjectIndexRole, static_cast<qulonglong>(i));
        if (document_.IsObjectSelected(i)) {
            item->setSelected(true);
        }
    }

    for (auto& group_entry : group_items) {
        QTreeWidgetItem* group_item = group_entry.second;
        bool any_visible = false;
        for (int i = 0; i < group_item->childCount(); ++i) {
            const QVariant object_index = group_item->child(i)->data(0, kSceneTreeObjectIndexRole);
            const size_t index = static_cast<size_t>(object_index.toULongLong());
            const bool child_visible = index < objects.size() && objects[index] && objects[index]->IsVisible();
            any_visible = any_visible || child_visible;
        }
        group_item->setIcon(0, SceneVisibilityIcon(any_visible));
        group_item->setToolTip(0, any_visible ? "Hide group" : "Show group");
    }

    UpdateToolAvailability();
}

void MainWindow::OnSceneTreeItemClicked(QTreeWidgetItem* item, int column) {
    if (!item || column != 0) {
        return;
    }

    auto& objects = document_.GetObjects();
    const QVariant object_index = item->data(0, kSceneTreeObjectIndexRole);
    if (object_index.isValid()) {
        const size_t index = static_cast<size_t>(object_index.toULongLong());
        if (index < objects.size() && objects[index]) {
            objects[index]->SetVisible(!objects[index]->IsVisible());
        }
        document_.ClearSelection();
        RefreshSceneTree();
        viewport_->update();
        return;
    }

    const QVariant group_name = item->data(0, kSceneTreeGroupRole);
    if (group_name.isValid()) {
        const std::string group = group_name.toString().toStdString();
        bool any_visible = false;
        for (const auto& object : objects) {
            if (object && object->GetGroupName() == group && object->IsVisible()) {
                any_visible = true;
                break;
            }
        }
        const bool group_visible = !any_visible;
        for (auto& object : objects) {
            if (object && object->GetGroupName() == group) {
                object->SetVisible(group_visible);
            }
        }
        document_.ClearSelection();
        RefreshSceneTree();
        viewport_->update();
    }
}

void MainWindow::SetTool(ToolMode tool, const QString& status_text) {
    low_poly_pick_pending_ = false;
    ClearActiveProperties();
    viewport_->SetTool(tool);
    if (tool == ToolMode::Orbit) {
        UpdateActiveToolUi("orbit");
    } else if (tool == ToolMode::Select) {
        UpdateActiveToolUi("select");
    } else if (tool == ToolMode::DrawCurve) {
        UpdateActiveToolUi("PolylineCurve");
    } else if (tool == ToolMode::DrawBSpline) {
        UpdateActiveToolUi("BSplineCurve");
    } else if (tool == ToolMode::EditPoint) {
        UpdateActiveToolUi("EditPoint");
    } else if (tool == ToolMode::SketchRectangle) {
        UpdateActiveToolUi("NewSketch");
    }
    statusBar()->showMessage(status_text);
}

void MainWindow::SetSolidDisplayMode(SolidDisplayMode mode) {
    CSolid::SetDisplayMode(mode);
    if (surfaces_edges_action_) {
        surfaces_edges_action_->setChecked(mode == SolidDisplayMode::SurfacesAndEdges);
    }
    if (mesh_only_action_) {
        mesh_only_action_->setChecked(mode == SolidDisplayMode::MeshOnly);
    }
    if (surfaces_wire_action_) {
        surfaces_wire_action_->setChecked(mode == SolidDisplayMode::SurfacesAndRaisedMesh);
    }
    if (solid_wireframe_action_) {
        solid_wireframe_action_->setChecked(mode == SolidDisplayMode::Wireframe);
    }
    QSettings settings;
    settings.setValue("view/solidDisplayMode", static_cast<int>(mode));

    QString message = "Solid display: surfaces and edges";
    if (mode == SolidDisplayMode::MeshOnly) {
        message = "Solid display: mesh only";
    } else if (mode == SolidDisplayMode::SurfacesAndRaisedMesh) {
        message = "Solid display: surfaces and raised mesh";
    } else if (mode == SolidDisplayMode::Wireframe) {
        message = "Solid display: wireframe";
    }
    viewport_->update();
    statusBar()->showMessage(message, 1400);
}

void MainWindow::ToggleWireShadedDisplay() {
    const bool wire_enabled = CSolid::GetDisplayMode() == SolidDisplayMode::Wireframe
        || CMesh3D::GetDisplayMode() == MeshDisplayMode::Wire;
    if (wire_enabled) {
        SetSolidDisplayMode(SolidDisplayMode::SurfacesAndEdges);
        SetMeshDisplayMode(MeshDisplayMode::SurfaceMaterial);
        statusBar()->showMessage("Display: shaded", 1200);
    } else {
        SetSolidDisplayMode(SolidDisplayMode::Wireframe);
        SetMeshDisplayMode(MeshDisplayMode::Wire);
        statusBar()->showMessage("Display: wired", 1200);
    }
}

void MainWindow::SetMeshDisplayMode(MeshDisplayMode mode) {
    CMesh3D::SetDisplayMode(mode);
    if (mesh_display_button_) {
        QString button_text = "Surface Gray v";
        if (mode == MeshDisplayMode::SurfaceColored) {
            button_text = "Surface Colored v";
        } else if (mode == MeshDisplayMode::SurfaceMaterial) {
            button_text = "Surface Material v";
        } else if (mode == MeshDisplayMode::Wire) {
            button_text = "Wire v";
        }
        mesh_display_button_->setText(button_text);
    }
    if (mesh_display_menu_) {
        for (QAction* action : mesh_display_menu_->actions()) {
            action->setChecked(action->data().toInt() == static_cast<int>(mode));
        }
    }

    QSettings settings;
    settings.setValue("view/meshDisplayMode", static_cast<int>(mode));
    viewport_->update();

    QString message = "Mesh display: surface gray";
    if (mode == MeshDisplayMode::SurfaceColored) {
        message = "Mesh display: surface colored";
    } else if (mode == MeshDisplayMode::SurfaceMaterial) {
        message = "Mesh display: material without edges";
    } else if (mode == MeshDisplayMode::Wire) {
        message = "Mesh display: wire";
    }
    statusBar()->showMessage(message, 1200);
}

void MainWindow::SetMeshWireOpacity(float opacity) {
    CMesh3D::SetWireOpacity(opacity);
    const int percent = static_cast<int>(std::round(CMesh3D::GetWireOpacity() * 100.0f));
    if (mesh_opacity_slider_ && mesh_opacity_slider_->value() != percent) {
        const QSignalBlocker blocker(mesh_opacity_slider_);
        mesh_opacity_slider_->setValue(percent);
    }
    if (mesh_opacity_value_label_) {
        mesh_opacity_value_label_->setText(QString("%1%").arg(percent));
    }

    QSettings settings;
    settings.setValue("view/meshWireOpacity", CMesh3D::GetWireOpacity());
    viewport_->update();
    statusBar()->showMessage(QString("Mesh opacity: %1%").arg(percent), 900);
}

void MainWindow::SetOrthographicProjection(bool enabled) {
    viewport_->SetOrthographicProjection(enabled);
    const bool actual_enabled = viewport_->IsOrthographicProjection();
    if (orthographic_projection_action_ && orthographic_projection_action_->isChecked() != actual_enabled) {
        orthographic_projection_action_->setChecked(actual_enabled);
    }

    QSettings settings;
    settings.setValue("view/orthographicProjection", actual_enabled);
    UpdateProjectionStatus();
    statusBar()->showMessage(actual_enabled ? "Orthographic projection" : "Perspective projection", 1400);
}

void MainWindow::SetOrbitMode(OrbitMode mode) {
    viewport_->SetOrbitMode(mode);
    if (cad_orbit_action_) {
        cad_orbit_action_->setChecked(mode == OrbitMode::CAD);
    }
    if (architectural_orbit_action_) {
        architectural_orbit_action_->setChecked(mode == OrbitMode::Architectural);
    }

    QSettings settings;
    settings.setValue("view/orbitMode", mode == OrbitMode::Architectural ? "architectural" : "cad");
    statusBar()->showMessage(mode == OrbitMode::Architectural ? "Orbit mode: Architectural" : "Orbit mode: CAD", 1400);
}

void MainWindow::SetXYPlaneViewEnabled(bool enabled) {
    viewport_->SetXYPlaneViewEnabled(enabled);
    if (xy_plane_view_check_box_ && xy_plane_view_check_box_->isChecked() != enabled) {
        xy_plane_view_check_box_->setChecked(enabled);
    }
    if (orthographic_projection_action_ && orthographic_projection_action_->isChecked() != viewport_->IsOrthographicProjection()) {
        orthographic_projection_action_->setChecked(viewport_->IsOrthographicProjection());
    }

    QSettings settings;
    settings.setValue("view/xyPlaneView", enabled);
    UpdateProjectionStatus();
    statusBar()->showMessage(enabled ? "XY plane view: rotation locked" : "XY plane view disabled", 1400);
}

void MainWindow::SetCoordinateAxesVisible(bool visible) {
    viewport_->SetCoordinateAxesVisible(visible);
    if (coordinate_axes_check_box_ && coordinate_axes_check_box_->isChecked() != visible) {
        coordinate_axes_check_box_->setChecked(visible);
    }

    QSettings settings;
    settings.setValue("view/showCoordinateAxes", visible);
    statusBar()->showMessage(visible ? "Coordinate axes shown" : "Coordinate axes hidden", 1400);
}

void MainWindow::RequestObjectColor() {
    if (document_.HasSelection()) {
        EditSelectedObjectColor();
        return;
    }

    object_color_pick_pending_ = true;
    viewport_->SetTool(ToolMode::Select);
    viewport_->SetSelectionMode(SelectionMode::Object);
    UpdateActiveToolUi("select");
    statusBar()->showMessage("Color: выберите CAlfaObject для редактирования цвета");
}

void MainWindow::EditSelectedObjectColor() {
    if (!document_.HasSelection()) {
        object_color_pick_pending_ = true;
        statusBar()->showMessage("Color: выберите CAlfaObject для редактирования цвета");
        return;
    }

    const CAlfaObject* selected = document_.GetSelectedObject();
    if (!selected) {
        return;
    }
    const Color current = selected->GetColor();
    const QColor chosen = QColorDialog::getColor(
        QColor::fromRgbF(current.r, current.g, current.b),
        this,
        "Object Color");
    if (!chosen.isValid()) {
        statusBar()->showMessage("Color canceled", 900);
        return;
    }

    const Color color{
        static_cast<float>(chosen.redF()),
        static_cast<float>(chosen.greenF()),
        static_cast<float>(chosen.blueF())
    };
    auto& objects = document_.GetObjects();
    int changed = 0;
    for (size_t index : document_.GetSelectedObjectIndices()) {
        if (index < objects.size() && objects[index]) {
            objects[index]->SetColor(color);
            ++changed;
        }
    }
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage(QString("Color applied to %1 object(s)").arg(changed), 1200);
}

void MainWindow::SetFloorGridVisible(bool visible) {
    viewport_->SetFloorGridVisible(visible);
    if (floor_grid_check_box_ && floor_grid_check_box_->isChecked() != visible) {
        floor_grid_check_box_->setChecked(visible);
    }

    QSettings settings;
    settings.setValue("view/showFloorGrid", visible);
    statusBar()->showMessage(visible ? "Grid shown" : "Grid hidden", 1400);
}

void MainWindow::UpdateProjectionStatus() {
    if (!projection_status_label_) {
        return;
    }
    projection_status_label_->setText(viewport_->IsOrthographicProjection() ? "[ORTHO]" : "[PERSPECTIVE]");
}

void MainWindow::ShowMaterialEditor(const Material* initial_material, const QString& material_file_path) {
    auto* dialog = new MaterialEditorDialog(MaterialLibrary::DefaultLibraryPath(), document_.GetMaterials(), document_.HasSelection(), initial_material, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &MaterialEditorDialog::SaveMaterialToDocument, this, [this](const Material& material) {
        SaveMaterialToDocument(material);
    });
    connect(dialog, &MaterialEditorDialog::ApplyMaterialToSelected, this, [this](const Material& material) {
        ApplyMaterialToSelection(material);
    });
    connect(dialog, &MaterialEditorDialog::LibraryMaterialSaved, this, [this](const QString&) {
        if (refresh_material_library_) {
            refresh_material_library_();
        }
        if (material_library_dock_) {
            statusBar()->showMessage("Library material saved", 1400);
        }
    });
    connect(dialog, &MaterialEditorDialog::RequestSelectedObjectMaterial, this, [this, dialog]() {
        const CAlfaObject* selected_object = document_.GetSelectedObject();
        if (!selected_object) {
            statusBar()->showMessage("Material: no selected object", 1400);
            return;
        }
        dialog->SetCurrentMaterial(selected_object->GetMaterial());
        statusBar()->showMessage(QString("Material copied from: %1").arg(QString::fromStdString(selected_object->GetName())), 1400);
    });
    connect(dialog, &MaterialEditorDialog::RequestPaintMaterial, this, [this](const Material& material) {
        viewport_->BeginMaterialPaint(material);
    });
    connect(dialog, &MaterialEditorDialog::RequestPickObjectMaterial, this, [this]() {
        viewport_->BeginMaterialPick();
    });
    connect(viewport_, &OpenGLViewport::MaterialPicked, dialog, [dialog](const Material& material) {
        dialog->SetCurrentMaterial(material);
    });
    if (initial_material && !material_file_path.isEmpty()) {
        dialog->SetCurrentMaterial(*initial_material, material_file_path);
    }
    connect(dialog, &QDialog::finished, this, [this]() {
        viewport_->CancelMaterialInteraction();
    });
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void MainWindow::ShowSurfaceTextureEditor() {
    std::vector<CMesh3D*> selected_meshes;
    for (size_t index : document_.GetSelectedObjectIndices()) {
        auto& objects = document_.GetObjects();
        if (index < objects.size()) {
            if (auto* mesh = dynamic_cast<CMesh3D*>(objects[index].get())) {
                selected_meshes.push_back(mesh);
            }
        }
    }

    if (!selected_meshes.empty()) {
        std::vector<Material> original_materials;
        original_materials.reserve(selected_meshes.size());
        for (const CMesh3D* mesh : selected_meshes) {
            original_materials.push_back(mesh->GetMaterial());
        }

        QDialog dialog(this);
        dialog.setWindowTitle("Edit Mesh Texture Coordinates");
        auto* layout = new QVBoxLayout(&dialog);
        auto* form = new QFormLayout();
        layout->addLayout(form);

        const auto add_spin = [&dialog](double minimum, double maximum, double step, int decimals, double value) {
            auto* spin = new QDoubleSpinBox(&dialog);
            spin->setRange(minimum, maximum);
            spin->setSingleStep(step);
            spin->setDecimals(decimals);
            spin->setKeyboardTracking(true);
            spin->setValue(value);
            return spin;
        };

        const Material current = selected_meshes.front()->GetMaterial();
        QDoubleSpinBox* offset_u = add_spin(-10000.0, 10000.0, 0.05, 4, current.texture_offset_u);
        QDoubleSpinBox* offset_v = add_spin(-10000.0, 10000.0, 0.05, 4, current.texture_offset_v);
        QDoubleSpinBox* scale_u = add_spin(-10000.0, 10000.0, 0.05, 4, current.texture_scale_u);
        QDoubleSpinBox* scale_v = add_spin(-10000.0, 10000.0, 0.05, 4, current.texture_scale_v);
        QDoubleSpinBox* rotation = add_spin(-3600.0, 3600.0, 1.0, 2, current.texture_rotation_degrees);
        rotation->setSuffix(QString::fromUtf8("°"));

        form->addRow("Offset U", offset_u);
        form->addRow("Offset V", offset_v);
        form->addRow("Scale U", scale_u);
        form->addRow("Scale V", scale_v);
        form->addRow("Rotate", rotation);

        const auto apply_preview = [this, selected_meshes, offset_u, offset_v, scale_u, scale_v, rotation]() {
            for (CMesh3D* mesh : selected_meshes) {
                Material material = mesh->GetMaterial();
                material.texture_offset_u = static_cast<float>(offset_u->value());
                material.texture_offset_v = static_cast<float>(offset_v->value());
                material.texture_scale_u = static_cast<float>(scale_u->value());
                material.texture_scale_v = static_cast<float>(scale_v->value());
                material.texture_rotation_degrees = static_cast<float>(rotation->value());
                mesh->SetMaterial(std::move(material));
            }
            viewport_->update();
        };
        connect(offset_u, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dialog, [apply_preview](double) { apply_preview(); });
        connect(offset_v, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dialog, [apply_preview](double) { apply_preview(); });
        connect(scale_u, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dialog, [apply_preview](double) { apply_preview(); });
        connect(scale_v, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dialog, [apply_preview](double) { apply_preview(); });
        connect(rotation, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dialog, [apply_preview](double) { apply_preview(); });

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);

        if (dialog.exec() != QDialog::Accepted) {
            for (size_t i = 0; i < selected_meshes.size(); ++i) {
                selected_meshes[i]->SetMaterial(original_materials[i]);
            }
            viewport_->update();
            statusBar()->showMessage("Mesh texture coordinate changes canceled", 1200);
            return;
        }

        apply_preview();
        viewport_->update();
        statusBar()->showMessage(QString("Texture coordinates updated for %1 mesh(es)")
            .arg(selected_meshes.size()), 1600);
        return;
    }

    CSolid* solid = document_.GetSelectedFaceSolid();
    if (!solid || solid->GetSelectedFaceIndices().empty()) {
        statusBar()->showMessage("Edit Texture: select one or more surfaces", 1600);
        return;
    }

    const CSurfaceFace* first_surface = solid->GetSurfaceFace(solid->GetSelectedFaceIndices().front());
    if (!first_surface) {
        return;
    }

    const std::vector<int> selected_face_indices = solid->GetSelectedFaceIndices();
    std::vector<std::pair<int, SurfaceTextureTransform>> original_transforms;
    original_transforms.reserve(selected_face_indices.size());
    for (int face_index : selected_face_indices) {
        const CSurfaceFace* surface = solid->GetSurfaceFace(face_index);
        if (surface) {
            original_transforms.emplace_back(face_index, surface->TextureTransform);
        }
    }
    solid->ClearSelectedFace();
    viewport_->update();

    const auto restore_selection = [solid, selected_face_indices]() {
        if (selected_face_indices.empty()) {
            return;
        }
        solid->SetSelectedFace(selected_face_indices.front());
        for (size_t i = 1; i < selected_face_indices.size(); ++i) {
            solid->AddSelectedFace(selected_face_indices[i]);
        }
    };

    QDialog dialog(this);
    dialog.setWindowTitle("Edit Texture Coordinates");
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();
    layout->addLayout(form);

    const auto add_spin = [&dialog](double minimum, double maximum, double step, int decimals, double value) {
        auto* spin = new QDoubleSpinBox(&dialog);
        spin->setRange(minimum, maximum);
        spin->setSingleStep(step);
        spin->setDecimals(decimals);
        spin->setKeyboardTracking(true);
        spin->setValue(value);
        return spin;
    };

    const SurfaceTextureTransform& current = first_surface->TextureTransform;
    QDoubleSpinBox* offset_u = add_spin(-10000.0, 10000.0, 0.05, 4, current.offset_u);
    QDoubleSpinBox* offset_v = add_spin(-10000.0, 10000.0, 0.05, 4, current.offset_v);
    QDoubleSpinBox* scale_u = add_spin(-10000.0, 10000.0, 0.05, 4, current.scale_u);
    QDoubleSpinBox* scale_v = add_spin(-10000.0, 10000.0, 0.05, 4, current.scale_v);
    QDoubleSpinBox* rotation = add_spin(-3600.0, 3600.0, 1.0, 2, current.rotation_degrees);
    rotation->setSuffix(QString::fromUtf8("°"));

    form->addRow("Offset U", offset_u);
    form->addRow("Offset V", offset_v);
    form->addRow("Scale U", scale_u);
    form->addRow("Scale V", scale_v);
    form->addRow("Rotate", rotation);

    const auto apply_preview = [this, solid, selected_face_indices, offset_u, offset_v, scale_u, scale_v, rotation]() {
        SurfaceTextureTransform transform;
        transform.offset_u = static_cast<float>(offset_u->value());
        transform.offset_v = static_cast<float>(offset_v->value());
        transform.scale_u = static_cast<float>(scale_u->value());
        transform.scale_v = static_cast<float>(scale_v->value());
        transform.rotation_degrees = static_cast<float>(rotation->value());
        bool changed = false;
        for (int face_index : selected_face_indices) {
            changed = solid->SetSurfaceTextureTransform(face_index, transform) || changed;
        }
        if (changed) {
            viewport_->update();
        }
    };
    connect(offset_u, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dialog, [apply_preview](double) { apply_preview(); });
    connect(offset_v, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dialog, [apply_preview](double) { apply_preview(); });
    connect(scale_u, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dialog, [apply_preview](double) { apply_preview(); });
    connect(scale_v, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dialog, [apply_preview](double) { apply_preview(); });
    connect(rotation, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dialog, [apply_preview](double) { apply_preview(); });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        for (const auto& original : original_transforms) {
            solid->SetSurfaceTextureTransform(original.first, original.second);
        }
        restore_selection();
        viewport_->update();
        statusBar()->showMessage("Texture coordinate changes canceled", 1200);
        return;
    }

    apply_preview();
    restore_selection();
    viewport_->update();
    statusBar()->showMessage(QString("Texture coordinates updated for %1 surface(s)")
        .arg(selected_face_indices.size()), 1600);
}

void MainWindow::SaveMaterialToDocument(const Material& material) {
    const Material& saved = document_.UpsertMaterial(material);
    for (auto& object : document_.GetObjects()) {
        if (object && object->GetMaterialId() == saved.id) {
            object->SetMaterial(saved);
        }
    }
    viewport_->update();
    statusBar()->showMessage(QString("Material saved: %1").arg(QString::fromStdString(saved.name)), 1400);
}

void MainWindow::ApplyMaterialToSelection(const Material& material) {
    const std::vector<size_t> selected_indices = document_.GetSelectedObjectIndices();
    if (selected_indices.empty()) {
        statusBar()->showMessage("Material: no selected object", 1400);
        return;
    }

    const Material& document_material = document_.UpsertMaterial(material);
    auto& objects = document_.GetObjects();
    int applied = 0;
    for (size_t index : selected_indices) {
        if (index < objects.size() && objects[index]) {
            objects[index]->SetMaterial(document_material);
            objects[index]->SetMaterialId(document_material.id);
            ++applied;
        }
    }

    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage(QString("Material applied to %1 object(s)").arg(applied), 1400);
}

void MainWindow::BeginTransformTool(TransformOperation operation) {
    ClearActiveProperties();
    viewport_->SetTransformOperation(operation);

    QString operation_name = "Move";
    std::string tool_key = "move";
    if (operation == TransformOperation::Rotate) {
        operation_name = "Rotate";
        tool_key = "rotate";
    } else if (operation == TransformOperation::Scale) {
        operation_name = "Scale";
        tool_key = "scale";
    }
    UpdateActiveToolUi(tool_key);

    if (document_.HasSelection()) {
        statusBar()->showMessage(QString("Transform: %1. Drag X/Y/Z gizmo axis.").arg(operation_name));
    } else {
        statusBar()->showMessage(QString("Transform: %1. Select an object, then drag X/Y/Z gizmo axis.").arg(operation_name));
    }
}

void MainWindow::BeginNewSketch() {
    ClearActiveProperties();

    QDialog dlg(this);
    dlg.setWindowTitle("New Sketch");
    dlg.setModal(true);

    auto* layout = new QGridLayout(&dlg);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setHorizontalSpacing(12);
    layout->setVerticalSpacing(10);

    auto* name_label = new QLabel("Name of", &dlg);
    auto* name_edit = new QLineEdit(QString("Sketch-%1").arg(sketch_counter_), &dlg);
    auto* coordinate_label = new QLabel("Coordinate system", &dlg);
    auto* plane_combo = new QComboBox(&dlg);
    plane_combo->addItem(QString::fromUtf8("Плоскость XY"), static_cast<int>(OpenGLViewport::SketchPlane::XY));
    plane_combo->addItem(QString::fromUtf8("Плоскость XZ"), static_cast<int>(OpenGLViewport::SketchPlane::XZ));
    plane_combo->addItem(QString::fromUtf8("Плоскость YZ"), static_cast<int>(OpenGLViewport::SketchPlane::YZ));
    plane_combo->addItem(QString::fromUtf8("Грань тела"), static_cast<int>(OpenGLViewport::SketchPlane::XY));
    plane_combo->addItem(QString::fromUtf8("Задать 3-мя точками"), static_cast<int>(OpenGLViewport::SketchPlane::XY));

    layout->addWidget(name_label, 0, 0);
    layout->addWidget(name_edit, 0, 1);
    layout->addWidget(coordinate_label, 1, 0, 1, 2);
    layout->addWidget(plane_combo, 2, 0, 1, 2);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(buttons, 3, 0, 1, 2);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) {
        UpdateActiveToolUi("select");
        statusBar()->showMessage("New Sketch canceled", 800);
        return;
    }

    const QString sketch_name = name_edit->text().trimmed().isEmpty()
        ? QString("Sketch-%1").arg(sketch_counter_)
        : name_edit->text().trimmed();
    const OpenGLViewport::SketchPlane plane = static_cast<OpenGLViewport::SketchPlane>(plane_combo->currentData().toInt());
    ++sketch_counter_;

    viewport_->BeginSketch(sketch_name, plane);
    UpdateActiveToolUi("NewSketch");
    ShowSketchPanel();
    statusBar()->showMessage(QString("%1: Rectangle tool").arg(sketch_name));
}

void MainWindow::BeginSketchFillet() {
    QDialog dlg(this);
    dlg.setWindowTitle("Fillets Box");
    dlg.setModal(true);

    auto* layout = new QGridLayout(&dlg);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setHorizontalSpacing(12);
    layout->setVerticalSpacing(10);

    auto* radius_label = new QLabel("Fillet", &dlg);
    auto* radius_edit = new QLineEdit("10", &dlg);
    radius_edit->selectAll();
    layout->addWidget(radius_label, 0, 0);
    layout->addWidget(radius_edit, 0, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(buttons, 1, 0, 1, 2);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) {
        statusBar()->showMessage("Sketch Fillet canceled", 800);
        return;
    }

    bool ok = false;
    const double radius = radius_edit->text().toDouble(&ok);
    if (!ok || radius <= 0.0) {
        QMessageBox::warning(this, "Fillets Box", "Radius must be positive.");
        statusBar()->showMessage("Sketch Fillet: bad radius", 1200);
        return;
    }

    viewport_->BeginSketchFillet(radius);
    viewport_->setFocus();
    UpdateActiveToolUi("NewSketch");
    statusBar()->showMessage(QString("Sketch Fillet R=%1: укажите вершину полилинии").arg(radius, 0, 'f', 2));
}

void MainWindow::ShowSketchPanel() {
    if (!sketch_dock_) {
        sketch_dock_ = new QDockWidget("Sketch", this);
        sketch_dock_->setObjectName("SketchFloatingPanel");
        sketch_dock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        sketch_dock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
        sketch_dock_->setMinimumWidth(148);

        auto* panel = new QWidget(sketch_dock_);
        auto* root = new QVBoxLayout(panel);
        root->setContentsMargins(8, 8, 8, 8);
        root->setSpacing(8);

        auto* grid = new QGridLayout();
        grid->setSpacing(6);
        root->addLayout(grid);

        auto* rectangle_button = new QPushButton(panel);
        rectangle_button->setCheckable(true);
        rectangle_button->setChecked(true);
        rectangle_button->setIcon(SketchRectangleIcon());
        rectangle_button->setIconSize(QSize(44, 44));
        rectangle_button->setToolTip("Rectangle");
        rectangle_button->setFixedSize(58, 58);
        connect(rectangle_button, &QPushButton::clicked, this, [this, rectangle_button]() {
            rectangle_button->setChecked(true);
            viewport_->SetSketchRectangleTool();
        });
        grid->addWidget(rectangle_button, 0, 0);

        const struct {
            const char* label;
            const char* tooltip;
            int row;
            int column;
        } sketch_placeholders[] = {
            {"P", "Polyline", 0, 1},
            {"A", "Arc", 1, 0},
            {"F", "Fillet", 1, 1},
            {"B", "Bezier", 2, 0}
        };
        for (const auto& placeholder : sketch_placeholders) {
            auto* button = new QPushButton(panel);
            button->setToolTip(placeholder.tooltip);
            button->setFixedSize(58, 58);
            button->setText(placeholder.label);
            if (QString::fromLatin1(placeholder.tooltip) == "Fillet") {
                connect(button, &QPushButton::clicked, this, [this]() {
                    BeginSketchFillet();
                });
            } else {
                button->setEnabled(false);
            }
            grid->addWidget(button, placeholder.row, placeholder.column);
        }

        root->addStretch(1);

        auto* button_row = new QHBoxLayout();
        auto* ok_button = new QPushButton("OK", panel);
        auto* cancel_button = new QPushButton("Cancel", panel);
        ok_button->setMinimumHeight(34);
        cancel_button->setMinimumHeight(34);
        button_row->addWidget(ok_button);
        button_row->addWidget(cancel_button);
        root->addLayout(button_row);

        connect(ok_button, &QPushButton::clicked, this, [this]() {
            viewport_->EndSketch();
            if (sketch_dock_) {
                sketch_dock_->hide();
            }
            UpdateActiveToolUi("select");
            statusBar()->showMessage("Sketch accepted", 900);
        });
        connect(cancel_button, &QPushButton::clicked, this, [this]() {
            viewport_->EndSketch();
            if (sketch_dock_) {
                sketch_dock_->hide();
            }
            UpdateActiveToolUi("select");
            statusBar()->showMessage("Sketch closed", 900);
        });

        panel->setStyleSheet(
            "QPushButton { background: #f5f5f5; border: 1px solid #9b9b9b; color: #111; }"
            "QPushButton:checked { border: 2px solid #d71920; }"
            "QPushButton:disabled { color: #777; background: #eeeeee; }"
        );
        sketch_dock_->setWidget(panel);
        addDockWidget(Qt::RightDockWidgetArea, sketch_dock_);
        if (material_library_dock_) {
            splitDockWidget(material_library_dock_, sketch_dock_, Qt::Horizontal);
        } else if (scene_tree_dock_) {
            splitDockWidget(scene_tree_dock_, sketch_dock_, Qt::Horizontal);
        }
    }

    sketch_dock_->setFloating(false);
    resizeDocks({sketch_dock_}, {170}, Qt::Horizontal);
    sketch_dock_->show();
    sketch_dock_->raise();
}

void MainWindow::ActivateParametricTool(const std::string& tool_id) {
    active_parametric_edit_existing_ = false;
    if (tool_id != "SolidLowPoly") {
        low_poly_pick_pending_ = false;
    }
    if (tool_id == "SolidLowPoly") {
        ShowLowPolyTool();
        return;
    }

    if (tool_id == "PolylineCurve") {
        ClearActiveProperties();
        document_.CreatePolyline();
        viewport_->SetTool(ToolMode::DrawCurve);
        UpdateActiveToolUi(tool_id);
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage("Polyline: click in viewport to add 3D points");
        return;
    }

    if (tool_id == "BSplineCurve") {
        ClearActiveProperties();
        document_.CreateBSpline();
        viewport_->SetTool(ToolMode::DrawBSpline);
        UpdateActiveToolUi(tool_id);
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage("B-Spline: click in viewport to add 3D control points");
        return;
    }

    if (tool_id == "EditPoint") {
        ClearActiveProperties();
        viewport_->SetTool(ToolMode::EditPoint);
        UpdateActiveToolUi(tool_id);
        viewport_->update();
        statusBar()->showMessage("Edit Point: click curve points, drag selected handles");
        return;
    }

    if (tool_id == "boolean") {
        ClearActiveProperties();

        BooleanDialog dlg(this);
        if (dlg.exec() != QDialog::Accepted) {
            // Отмена
            UpdateActiveToolUi("select");
            viewport_->SetTool(ToolMode::Select);
            statusBar()->showMessage("Булева операция отменена", 800);
            return;
        }

        BooleanOperation operation = BooleanOperation::Union;
        const BooleanDialog::Operation selected = dlg.SelectedOperation();
        if (selected == BooleanDialog::Operation::Union) {
            operation = BooleanOperation::Union;
        } else if (selected == BooleanDialog::Operation::Cut) {
            operation = BooleanOperation::Cut;
        } else if (selected == BooleanDialog::Operation::Common) {
            operation = BooleanOperation::Common;
        }

        viewport_->BeginBooleanTool(operation);
        UpdateActiveToolUi("boolean");
        return;
    }

    if (tool_id == "SolidExtrudeFace") {
        ClearActiveProperties();
        ExtrudeFaceDialog dlg(this);
        if (dlg.exec() != QDialog::Accepted) {
            UpdateActiveToolUi("select");
            viewport_->SetTool(ToolMode::Select);
            statusBar()->showMessage("Extrude Face canceled", 800);
            return;
        }

        const double taper_angle = dlg.TaperAngle();
        viewport_->BeginFaceExtrudeTool(taper_angle);
        UpdateActiveToolUi(tool_id);
        if (document_.HasSelectedSolidFace()) {
            statusBar()->showMessage(QString("Extrude Face: тяни оранжевую ручку, Taper Angle %1").arg(taper_angle, 0, 'f', 2));
        } else {
            statusBar()->showMessage(QString("Extrude Face: выбери плоскую грань, Taper Angle %1").arg(taper_angle, 0, 'f', 2));
        }
        return;
    }

    if (tool_id == "SolidDraft") {
        ClearActiveProperties();
        viewport_->BeginDraftFaceTool();
        UpdateActiveToolUi(tool_id);
        if (document_.HasSelectedSolidFace()) {
            statusBar()->showMessage("Draft Face: выбери прямую кромку-ось");
        } else {
            statusBar()->showMessage("Draft Face: выбери плоскую грань");
        }
        return;
    }

    if (tool_id == "ThickSolidTool") {
        ClearActiveProperties();
        active_parametric_object_ = {
            tool_id,
            document_.GetSelectedObjectIndex(),
            0,
            {{"thick", "Thick", 1.0, -100.0, 100.0, 0.1}}
        };
        property_panel_->SetActiveObject(active_parametric_object_);
        ShowPropertyPanelAtCursor("ThickSolid");
        viewport_->BeginThickSolidTool(active_parametric_object_.parameters[0].value);
        UpdateActiveToolUi(tool_id);
        RefreshSceneTree();
        viewport_->update();
        if (document_.HasLiveThickSolid() && document_.GetLiveThickSolidFaceCount() > 0) {
            statusBar()->showMessage("ThickSolid: меняй Thick, OK оставит результат");
        } else {
            statusBar()->showMessage(document_.GetSelectedSolid() ? "ThickSolid: выбери Face" : "ThickSolid: выбери CSolid");
        }
        return;
    }

    if (tool_id == "SolidExtrudeTool") {
        ClearActiveProperties();
        active_parametric_object_ = {
            tool_id,
            document_.GetSelectedObjectIndex(),
            0,
            {
                {"distance", "Distance", 1.0, 0.0, 1000.0, 0.1},
                {"reverse", "Reverse", 0.0, 0.0, 1.0, 1.0, ToolParameterType::Checkbox},
                {"taper", "Angle taper", 0.0, -89.0, 89.0, 1.0}
            }
        };
        property_panel_->SetActiveObject(active_parametric_object_);
        ShowPropertyPanelAtCursor("Extrude");
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionMode(SelectionMode::Object);
        UpdateActiveToolUi(tool_id);

        if (document_.GetSelectedPolyline() && !TryStartLivePolylineExtrudeFromSelection()) {
            document_.CancelLiveExtrudeSelectedPolyline();
            active_parametric_object_ = {};
            UpdateActiveToolUi("select");
            viewport_->SetTool(ToolMode::Select);
            statusBar()->showMessage("Extrude: операция не выполнена");
            return;
        }
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage(document_.HasLivePolylineExtrude()
            ? "Extrude: меняй параметры, OK оставит результат"
            : "Extrude: выбери замкнутую плоскую Polyline");
        return;
    }

    if (tool_id == "SurfaceOfRevolution") {
        ClearActiveProperties();
        active_parametric_object_ = {
            tool_id,
            document_.GetSelectedObjectIndex(),
            0,
            {
                {"angle", "Angle", 360.0, 0.0, 360.0, 1.0},
                {"axis", "Axis", 2.0, 0.0, 2.0, 1.0, ToolParameterType::Combo, {"Axis X", "Axis Y", "Axis Z"}}
            }
        };
        property_panel_->SetActiveObject(active_parametric_object_);
        ShowPropertyPanelAtCursor("Revolve");
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionMode(SelectionMode::Object);
        UpdateActiveToolUi(tool_id);

        if (document_.GetSelectedPolyline() && !TryStartLivePolylineRevolveFromSelection()) {
            document_.CancelLiveRevolveSelectedPolyline();
            active_parametric_object_ = {};
            UpdateActiveToolUi("select");
            viewport_->SetTool(ToolMode::Select);
            statusBar()->showMessage("Revolve: операция не выполнена");
            return;
        }
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage(document_.HasLivePolylineRevolve()
            ? "Revolve: меняй параметры, OK оставит результат"
            : "Revolve: выбери плоскую Polyline");
        return;
    }

    if (tool_id == "SurfaceLoft") {
        ClearActiveProperties();
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionMode(SelectionMode::Object);
        UpdateActiveToolUi(tool_id);
        if (!document_.CreateLoftSurfaceFromSelectedBSplines()) {
            UpdateActiveToolUi("select");
            statusBar()->showMessage("Loft Surface: выбери минимум 2 B-Spline", 1800);
            return;
        }
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage("Loft Surface created", 1600);
        return;
    }

    if (tool_id == "SurfaceReverseNormals") {
        ClearActiveProperties();
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionMode(SelectionMode::Object);
        UpdateActiveToolUi(tool_id);
        if (!document_.ReverseSelectedSurfaceNormals()) {
            UpdateActiveToolUi("select");
            statusBar()->showMessage("Reverse Normals: выбери поверхность", 1800);
            return;
        }
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage("Normals reversed", 1400);
        return;
    }

    if (tool_id == "ChamferSolid") {
        ClearActiveProperties();
        active_parametric_object_ = {
            tool_id,
            document_.GetSelectedObjectIndex(),
            0,
            {{"distance", "Distance", 0.20, 0.01, 10.0, 0.01}}
        };
        property_panel_->SetActiveObject(active_parametric_object_);
        ShowPropertyPanelAtCursor("Chamfer");
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionMode(SelectionMode::Edge);
        UpdateActiveToolUi(tool_id);

        if (document_.HasSelectedSolidEdge() && !TryStartLiveEdgeToolFromSelection()) {
            document_.CancelLiveChamfer();
            active_parametric_object_ = {};
            UpdateActiveToolUi("select");
            viewport_->SetTool(ToolMode::Select);
            statusBar()->showMessage("Chamfer: операция не выполнена");
            return;
        }
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage(document_.HasLiveChamfer()
            ? "Chamfer: меняй Distance, OK оставит результат"
            : "Chamfer: выбери кромку тела");
        return;
    }

    if (tool_id == "fillet_edge" || tool_id == "fillet_all_edges") {
        ClearActiveProperties();
        if (tool_id == "fillet_all_edges" && !document_.GetSelectedSolid()) {
            UpdateActiveToolUi("select");
            viewport_->SetTool(ToolMode::Select);
            statusBar()->showMessage("Fillet All: выбери тело");
            return;
        }

        active_parametric_object_ = {
            tool_id,
            document_.GetSelectedObjectIndex(),
            0,
            {{"radius", "Radius", 0.20, 0.01, 10.0, 0.01}}
        };
        const bool all_edges = tool_id == "fillet_all_edges";
        property_panel_->SetActiveObject(active_parametric_object_);
        ShowPropertyPanelAtCursor(tool_id == "fillet_edge" ? "Fillet Edge" : "Fillet All Edges");
        viewport_->SetTool(ToolMode::Select);
        if (tool_id == "fillet_edge") {
            viewport_->SetSelectionMode(SelectionMode::Edge);
        }
        UpdateActiveToolUi(tool_id);

        const bool should_start_now = all_edges || document_.HasSelectedSolidEdge();
        const bool started = all_edges
            ? document_.BeginLiveFilletSelectedEdges(all_edges)
            : TryStartLiveEdgeToolFromSelection();
        if (should_start_now && (!started || !document_.HasLiveFillet())) {
            document_.CancelLiveFillet();
            active_parametric_object_ = {};
            UpdateActiveToolUi("select");
            viewport_->SetTool(ToolMode::Select);
            statusBar()->showMessage("Fillet: операция не выполнена");
            return;
        }
        if (all_edges && !document_.UpdateLiveFillet(active_parametric_object_.parameters[0].value)) {
            document_.CancelLiveFillet();
            active_parametric_object_ = {};
            UpdateActiveToolUi("select");
            viewport_->SetTool(ToolMode::Select);
            statusBar()->showMessage("Fillet: операция не выполнена");
            return;
        }
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage(document_.HasLiveFillet()
            ? (tool_id == "fillet_edge" ? "Fillet: меняй Radius, OK оставит результат" : "Fillet All: меняй Radius, OK оставит результат")
            : "Fillet: выбери кромку тела");
        return;
    }

    active_parametric_object_ = tool_registry_.Activate(tool_id, document_);
    if (!active_parametric_object_.tool_id.empty()) {
        property_panel_->SetActiveObject(active_parametric_object_);
        ShowPropertyPanelAtCursor(QString::fromStdString(tool_registry_.LabelFor(tool_id)));
        if (tool_id == "SolidBox" || tool_id == "SolidCylinder" || tool_id == "SolidPrismTool") {
            document_.ClearSelection();
        }
    } else {
        ClearActiveProperties();
    }
    viewport_->SetTool(ToolMode::Select);
    UpdateActiveToolUi(tool_id);
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage(QString("%1 applied").arg(QString::fromStdString(tool_id)));
}

void MainWindow::ShowLowPolyTool() {
    auto* dialog = new SolidLowPolyDialog(
        document_,
        [this]() {
            RefreshSceneTree();
            viewport_->update();
        },
        [this](const QString& text) {
            statusBar()->showMessage(text, 1600);
        },
        this);

    if (!dialog->HasBodies()) {
        dialog->deleteLater();
        low_poly_pick_pending_ = true;
        ClearActiveProperties();
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionMode(SelectionMode::Object);
        UpdateActiveToolUi("SolidLowPoly");
        statusBar()->showMessage("Low Poly: выбери тело");
        return;
    }

    low_poly_pick_pending_ = false;
    ClearActiveProperties();
    viewport_->SetTool(ToolMode::Select);
    viewport_->SetSelectionMode(SelectionMode::Object);
    UpdateActiveToolUi("SolidLowPoly");
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    statusBar()->showMessage("Low Poly: меняй параметры или Create Low Poly", 1600);
}

void MainWindow::EditSelectedParametricObject() {
    CAlfaObject* object = document_.GetSelectedObject();
    if (!object || !object->IsParametric()) {
        statusBar()->showMessage("Object has no saved parametric functions", 1400);
        return;
    }

    size_t operation_index = 0;
    if (auto* solid = dynamic_cast<CSolid*>(object)) {
        if (solid->GetNumOperations() > 0) {
            bool replay_supported = true;
            for (int i = 0; i < solid->GetNumOperations(); ++i) {
                const ParametricFunction* operation = solid->GetOperation(i);
                if (!operation) {
                    continue;
                }
                if (i > 0
                    && operation->ToolId != "fillet_all_edges"
                    && operation->ToolId != "fillet_edge"
                    && operation->ToolId != "ChamferSolid"
                    && operation->ToolId != "SolidExtrudeFace"
                    && operation->ToolId != "SolidDraft"
                    && operation->ToolId != "ThickSolidTool"
                    && operation->ToolId != "boolean"
                    && operation->ToolId != "SolidTransform") {
                    replay_supported = false;
                }
            }
            if (replay_supported) {
                const size_t object_index = document_.GetSelectedObjectIndex();
                tool_registry_.ReplayOperations(object_index, document_);
                object = document_.GetSelectedObject();
                solid = dynamic_cast<CSolid*>(object);
                if (!solid) {
                    return;
                }
            }
            const size_t edited_object_index = document_.GetSelectedObjectIndex();
            document_.SetObjectSelectionHighlightHidden(edited_object_index, true);
            viewport_->update();
            const SolidOperationsDialogResult operation_action = ShowSolidOperationsDialog(
                this,
                *solid,
                tool_registry_,
                [this]() {
                    RefreshSceneTree();
                    viewport_->update();
                });
            document_.SetObjectSelectionHighlightHidden(edited_object_index, false);
            viewport_->update();
            if (operation_action.action == SolidOperationsDialogAction::None) {
                return;
            }
            if (!operation_action.object_name.empty()) {
                solid->SetName(operation_action.object_name);
            }
            if (operation_action.action == SolidOperationsDialogAction::Accept) {
                RefreshSceneTree();
                viewport_->update();
                statusBar()->showMessage("Solid changes accepted", 1200);
                return;
            }
            if (operation_action.action == SolidOperationsDialogAction::Delete) {
                const size_t object_index = document_.GetSelectedObjectIndex();
                if (!solid->RemoveParametricOperation(static_cast<size_t>(operation_action.operation_index))
                    || !tool_registry_.ReplayOperations(object_index, document_)) {
                    statusBar()->showMessage("Operation delete failed", 1400);
                    return;
                }
                RefreshSceneTree();
                viewport_->update();
                statusBar()->showMessage("Operation deleted", 1200);
                return;
            }
            operation_index = static_cast<size_t>(operation_action.operation_index);
            reopen_solid_editor_after_properties_ = true;
        }
    }

    active_parametric_object_ = tool_registry_.ActiveObjectFromDocument(document_.GetSelectedObjectIndex(), *object, operation_index);
    if (active_parametric_object_.tool_id.empty()) {
        reopen_solid_editor_after_properties_ = false;
        statusBar()->showMessage("Saved parametric tool is not available", 1400);
        return;
    }

    active_parametric_edit_existing_ = true;
    property_panel_->SetActiveObject(active_parametric_object_);
    const QString tool_label = QString::fromStdString(tool_registry_.LabelFor(active_parametric_object_.tool_id));
    ShowPropertyPanelAtCursor(tool_label);
    if (viewport_->CurrentTool() != ToolMode::Orbit) {
        viewport_->SetTool(ToolMode::Select);
    }
    UpdateActiveToolUi(active_parametric_object_.tool_id);
    statusBar()->showMessage(QString("%1 parameters").arg(tool_label), 1200);
}

bool MainWindow::TryStartLiveEdgeToolFromSelection() {
    if (active_parametric_object_.tool_id == "fillet_edge") {
        if (document_.HasLiveFillet()) {
            return true;
        }
        if (!document_.HasSelectedSolidEdge()) {
            statusBar()->showMessage("Fillet: выбери кромку тела");
            return false;
        }
        const double radius = active_parametric_object_.parameters.empty() ? 0.20 : active_parametric_object_.parameters[0].value;
        if (!document_.BeginLiveFilletSelectedEdges(false) || !document_.UpdateLiveFillet(radius)) {
            document_.CancelLiveFillet();
            statusBar()->showMessage("Fillet: операция не выполнена");
            return false;
        }
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage("Fillet: меняй Radius, OK оставит результат");
        return true;
    }

    if (active_parametric_object_.tool_id == "ChamferSolid") {
        if (document_.HasLiveChamfer()) {
            return true;
        }
        if (!document_.HasSelectedSolidEdge()) {
            statusBar()->showMessage("Chamfer: выбери кромку тела");
            return false;
        }
        const double distance = active_parametric_object_.parameters.empty() ? 0.20 : active_parametric_object_.parameters[0].value;
        if (!document_.BeginLiveChamferSelectedEdges() || !document_.UpdateLiveChamfer(distance)) {
            document_.CancelLiveChamfer();
            statusBar()->showMessage("Chamfer: операция не выполнена");
            return false;
        }
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage("Chamfer: меняй Distance, OK оставит результат");
        return true;
    }

    return false;
}

bool MainWindow::TryStartLivePolylineExtrudeFromSelection() {
    if (active_parametric_object_.tool_id != "SolidExtrudeTool") {
        return false;
    }
    if (document_.HasLivePolylineExtrude()) {
        return true;
    }
    if (!document_.GetSelectedPolyline()) {
        statusBar()->showMessage("Extrude: выбери замкнутую плоскую Polyline");
        return false;
    }

    const auto& parameters = active_parametric_object_.parameters;
    const double distance = parameters.size() > 0 ? parameters[0].value : 1.0;
    const bool reverse = parameters.size() > 1 && parameters[1].value >= 0.5;
    const double taper_angle = parameters.size() > 2 ? parameters[2].value : 0.0;
    if (!document_.BeginLiveExtrudeSelectedPolyline(distance, reverse, taper_angle)) {
        document_.CancelLiveExtrudeSelectedPolyline();
        statusBar()->showMessage("Extrude: Polyline должна быть плоской и замкнутой");
        return false;
    }

    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage("Extrude: меняй параметры, OK оставит результат");
    return true;
}

bool MainWindow::TryStartLivePolylineRevolveFromSelection() {
    if (active_parametric_object_.tool_id != "SurfaceOfRevolution") {
        return false;
    }
    if (document_.HasLivePolylineRevolve()) {
        return true;
    }
    if (!document_.GetSelectedPolyline()) {
        statusBar()->showMessage("Revolve: выбери плоскую Polyline");
        return false;
    }

    const auto& parameters = active_parametric_object_.parameters;
    const double angle = parameters.size() > 0 ? parameters[0].value : 360.0;
    const int axis_index = parameters.size() > 1 ? static_cast<int>(parameters[1].value) : 2;
    if (!document_.BeginLiveRevolveSelectedPolyline(angle, axis_index)) {
        document_.CancelLiveRevolveSelectedPolyline();
        statusBar()->showMessage("Revolve: Polyline должна лежать в Plane XY");
        return false;
    }

    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage("Revolve: меняй параметры, OK оставит результат");
    return true;
}

void MainWindow::ClearActiveProperties() {
    if (!active_parametric_edit_existing_
        && (active_parametric_object_.tool_id == "fillet_edge" || active_parametric_object_.tool_id == "fillet_all_edges")) {
        document_.CancelLiveFillet();
    } else if (active_parametric_object_.tool_id == "ChamferSolid") {
        document_.CancelLiveChamfer();
    } else if (active_parametric_object_.tool_id == "ThickSolidTool") {
        document_.CancelLiveThickSolid();
    } else if (active_parametric_object_.tool_id == "SolidExtrudeTool") {
        document_.CancelLiveExtrudeSelectedPolyline();
    } else if (active_parametric_object_.tool_id == "SurfaceOfRevolution") {
        document_.CancelLiveRevolveSelectedPolyline();
    }
    active_parametric_object_ = {};
    active_parametric_edit_existing_ = false;
    property_panel_->Clear();
    if (properties_dock_) {
        properties_dock_->hide();
    }
}

void MainWindow::AcceptActiveProperties() {
    if (active_parametric_edit_existing_
        && (active_parametric_object_.tool_id == "fillet_edge"
            || active_parametric_object_.tool_id == "fillet_all_edges"
            || active_parametric_object_.tool_id == "ChamferSolid"
            || active_parametric_object_.tool_id == "SolidExtrudeFace"
            || active_parametric_object_.tool_id == "SolidDraft"
            || active_parametric_object_.tool_id == "ThickSolidTool"
            || active_parametric_object_.tool_id == "SurfaceOfRevolution")) {
        const double value = active_parametric_object_.parameters.empty() ? 0.0 : active_parametric_object_.parameters[0].value;
        const QString label = QString::fromStdString(tool_registry_.LabelFor(active_parametric_object_.tool_id));
        ClearActiveProperties();
        RefreshSceneTree();
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi("select");
        viewport_->update();
        statusBar()->showMessage(QString("%1: %2").arg(label).arg(value, 0, 'f', 2));
        return;
    }

    if (active_parametric_object_.tool_id == "fillet_edge" || active_parametric_object_.tool_id == "fillet_all_edges") {
        const double radius = active_parametric_object_.parameters.empty() ? 0.20 : active_parametric_object_.parameters[0].value;
        const bool all_edges = active_parametric_object_.tool_id == "fillet_all_edges";
        const std::vector<std::pair<int, int>> edge_refs = document_.GetLiveFilletEdgeRefs();
        if (!document_.HasLiveFillet()) {
            statusBar()->showMessage(all_edges ? "Fillet All: выбери тело" : "Fillet: выбери кромку тела", 1600);
            return;
        }
        const std::vector<int> created_surface_indices = document_.GetLiveFilletCreatedSurfaceIndices();
        document_.FinishLiveFillet();
        if (all_edges) {
            if (auto* solid = document_.GetSelectedSolid()) {
                solid->SetParametricOperation(solid->GetOperationTree().size(),
                                              active_parametric_object_.tool_id,
                                              "Fillet All Edges",
                                              ToSavedParameters(active_parametric_object_.parameters),
                                              created_surface_indices);
            }
        } else {
            if (auto* solid = document_.GetSelectedSolid()) {
                solid->SetParametricOperation(solid->GetOperationTree().size(),
                                              active_parametric_object_.tool_id,
                                              "Fillet Edge",
                                              ToFilletEdgeSavedParameters(active_parametric_object_.parameters, edge_refs),
                                              created_surface_indices);
            }
        }
        ClearActiveProperties();
        RefreshSceneTree();
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi("select");
        viewport_->update();
        statusBar()->showMessage(QString("%1: radius %2").arg(all_edges ? "Fillet All" : "Fillet").arg(radius, 0, 'f', 2));
        return;
    }

    if (active_parametric_object_.tool_id == "ChamferSolid") {
        const double distance = active_parametric_object_.parameters.empty() ? 0.20 : active_parametric_object_.parameters[0].value;
        const std::vector<std::pair<int, int>> edge_refs = document_.GetLiveChamferEdgeRefs();
        if (!document_.HasLiveChamfer()) {
            statusBar()->showMessage("Chamfer: выбери кромку тела", 1600);
            return;
        }
        const std::vector<int> created_surface_indices = document_.GetLiveChamferCreatedSurfaceIndices();
        document_.FinishLiveChamfer();
        if (auto* solid = document_.GetSelectedSolid()) {
            solid->SetParametricOperation(solid->GetOperationTree().size(),
                                          active_parametric_object_.tool_id,
                                          "Chamfer",
                                          ToFilletEdgeSavedParameters(active_parametric_object_.parameters, edge_refs),
                                          created_surface_indices);
        }
        ClearActiveProperties();
        RefreshSceneTree();
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi("select");
        viewport_->update();
        statusBar()->showMessage(QString("Chamfer: Distance %1").arg(distance, 0, 'f', 2));
        return;
    }

    if (active_parametric_object_.tool_id == "ThickSolidTool") {
        const double thickness = active_parametric_object_.parameters.empty() ? 0.0 : active_parametric_object_.parameters[0].value;
        viewport_->SetThickSolidThickness(thickness);
        if (!document_.FinishLiveThickSolid()) {
            statusBar()->showMessage("ThickSolid: выбери Face и задай Thick != 0", 1600);
            viewport_->update();
            return;
        }
        ClearActiveProperties();
        document_.ClearSelection();
        RefreshSceneTree();
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi("select");
        viewport_->update();
        statusBar()->showMessage("ThickSolid: done", 1200);
        return;
    }

    if (active_parametric_object_.tool_id == "SolidExtrudeTool") {
        const double distance = active_parametric_object_.parameters.empty() ? 0.0 : active_parametric_object_.parameters[0].value;
        if (!document_.FinishLiveExtrudeSelectedPolyline()) {
            statusBar()->showMessage("Extrude: выбери Polyline и задай Distance != 0", 1600);
            viewport_->update();
            return;
        }
        ClearActiveProperties();
        RefreshSceneTree();
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi("select");
        viewport_->update();
        statusBar()->showMessage(QString("Extrude: Distance %1").arg(distance, 0, 'f', 2), 1200);
        return;
    }

    if (active_parametric_object_.tool_id == "SurfaceOfRevolution") {
        const double angle = active_parametric_object_.parameters.empty() ? 0.0 : active_parametric_object_.parameters[0].value;
        if (!document_.FinishLiveRevolveSelectedPolyline()) {
            statusBar()->showMessage("Revolve: выбери Polyline и задай Angle > 0", 1600);
            viewport_->update();
            return;
        }
        ClearActiveProperties();
        RefreshSceneTree();
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi("select");
        viewport_->update();
        statusBar()->showMessage(QString("Revolve: Angle %1").arg(angle, 0, 'f', 1), 1200);
        return;
    }

    active_parametric_object_ = {};
    active_parametric_edit_existing_ = false;
    if (properties_dock_) {
        properties_dock_->hide();
    }
    viewport_->SetTool(ToolMode::Orbit);
    UpdateActiveToolUi("orbit");
    statusBar()->showMessage("Object parameters accepted", 1200);
}

void MainWindow::CancelActiveProperties() {
    if (active_parametric_object_.tool_id == "fillet_edge" || active_parametric_object_.tool_id == "fillet_all_edges") {
        document_.CancelLiveFillet();
        ClearActiveProperties();
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi("select");
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage("Fillet canceled", 1200);
        return;
    }

    if (active_parametric_object_.tool_id == "ChamferSolid") {
        document_.CancelLiveChamfer();
        ClearActiveProperties();
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi("select");
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage("Chamfer canceled", 1200);
        return;
    }

    if (active_parametric_object_.tool_id == "ThickSolidTool") {
        document_.CancelLiveThickSolid();
        ClearActiveProperties();
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi("select");
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage("ThickSolid canceled", 1200);
        return;
    }

    if (active_parametric_object_.tool_id == "SolidExtrudeTool") {
        document_.CancelLiveExtrudeSelectedPolyline();
        ClearActiveProperties();
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi("select");
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage("Extrude canceled", 1200);
        return;
    }

    if (active_parametric_object_.tool_id == "SurfaceOfRevolution") {
        document_.CancelLiveRevolveSelectedPolyline();
        ClearActiveProperties();
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi("select");
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage("Revolve canceled", 1200);
        return;
    }

    if (!active_parametric_edit_existing_) {
        const size_t object_index = active_parametric_object_.object_index;
        auto& objects = document_.GetObjects();
        if (object_index < objects.size()) {
            objects.erase(objects.begin() + static_cast<CAlfaDoc::ObjectList::difference_type>(object_index));
            document_.ClearSelection();
        }
    }

    ClearActiveProperties();
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage("Object creation canceled", 1200);
}

void MainWindow::ShowPropertyPanelAtCursor(const QString& title) {
    if (!properties_dock_) {
        return;
    }

    properties_dock_->setWindowTitle(title + " Parameters");
    properties_dock_->setFloating(true);
    properties_dock_->resize(300, 180);
    properties_dock_->move(QCursor::pos() + QPoint(18, 18));
    properties_dock_->show();
    properties_dock_->raise();
    properties_dock_->activateWindow();
}

void MainWindow::RegisterToolAction(QAction* action, const std::string& key) {
    if (!action) {
        return;
    }

    action->setCheckable(true);
    action->setProperty("toolKey", QString::fromStdString(key));
    action->setIcon(ToolIcon(key));
    tool_actions_.push_back(action);
}

void MainWindow::RegisterToolButton(QPushButton* button, const std::string& key) {
    if (!button) {
        return;
    }

    button->setCheckable(true);
    button->setProperty("toolKey", QString::fromStdString(key));
    button->setIcon(ToolIcon(key));
    button->setIconSize(QSize(22, 22));
    tool_buttons_.push_back(button);
}

void MainWindow::UpdateActiveToolUi(const std::string& key) {
    active_tool_key_ = key;
    const QString active_key = QString::fromStdString(key);

    for (QAction* action : tool_actions_) {
        action->setChecked(action->property("toolKey").toString() == active_key);
    }

    for (QPushButton* button : tool_buttons_) {
        button->setChecked(button->property("toolKey").toString() == active_key);
    }

    UpdateToolAvailability();
}

void MainWindow::UpdateToolAvailability() {
    const bool has_selected_solid = document_.GetSelectedSolid() != nullptr;
    if (edit_texture_button_) {
        edit_texture_button_->setEnabled(document_.HasSelectedSolidFace() || document_.GetSelectedMesh() != nullptr);
    }

    const auto is_enabled = [has_selected_solid](const QString& key) {
        if (key == "fillet_all_edges") {
            return has_selected_solid;
        }
        return true;
    };

    for (QAction* action : tool_actions_) {
        if (action) {
            action->setEnabled(is_enabled(action->property("toolKey").toString()));
        }
    }

    for (QPushButton* button : tool_buttons_) {
        if (button) {
            button->setEnabled(is_enabled(button->property("toolKey").toString()));
        }
    }
}

QIcon MainWindow::ToolIcon(const std::string& key) const {
    QString icon_key = QString::fromStdString(key);
    if (icon_key == "boolean" || icon_key == "boolean_union" || icon_key == "boolean_cut" || icon_key == "boolean_common") {
        icon_key = "BooleanSolid";
    } else if (icon_key == "fillet_edge" || icon_key == "fillet_all_edges") {
        icon_key = "FilletSolid";
    } else if (icon_key == "PolylineCurve" || icon_key == "BSplineCurve") {
        icon_key = "curve";
    } else if (icon_key == "EditPoint") {
        icon_key = "select";
    } else if (icon_key == "NewSketch") {
        return NewSketchIcon();
    } else if (icon_key == "SurfaceLoft") {
        return LoftSurfaceIcon();
    } else if (icon_key == "SurfaceReverseNormals") {
        return ReverseNormalsIcon();
    }
    return QIcon(QString(":/icons/%1.png").arg(icon_key));
}

void MainWindow::NewProject() {
    document_.Clear();
    project_path_.clear();
    ClearActiveProperties();
    RefreshSceneTree();
    viewport_->update();
    UpdateWindowTitle();
    statusBar()->showMessage("New project");
}

void MainWindow::OpenProject() {
    const QString path = QFileDialog::getOpenFileName(this,
                                                       "Open Dom3D Project",
                                                       LastDialogDir(),
                                                       "Dom3D Project (*.dom3d);;Legacy Dom3D Project (*.d3dm);;All files (*.*)");
    if (path.isEmpty()) {
        return;
    }

    OpenProjectFromPath(path);
}

void MainWindow::OpenProjectFromPath(const QString& path) {
    if (path.isEmpty()) {
        return;
    }
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, "Dom3D Pro", QString("File does not exist:\n%1").arg(path));
        recent_project_files_.removeAll(path);
        UpdateRecentFilesMenu();
        QSettings settings;
        settings.setValue("files/recentProjects", recent_project_files_);
        return;
    }

    const bool legacy_project = path.toLower().endsWith(".d3dm");
    bool restored_camera = false;
    if (legacy_project) {
        std::string error;
        if (!project_io_.Load(path.toStdString(), document_, error)) {
            QMessageBox::critical(this, "Dom3D Pro", QString::fromStdString(error));
            return;
        }
    } else {
        QString error;
        QString active_room;
        ProjectViewState view_state;
        if (!dom3d_serializer_.Load(path, document_, active_room, view_state, error)) {
            QMessageBox::critical(this, "Dom3D Pro", error);
            return;
        }
        if (tool_tabs_ && !active_room.isEmpty()) {
            for (int i = 0; i < tool_tabs_->count(); ++i) {
                if (tool_tabs_->tabText(i) == active_room) {
                    tool_tabs_->setCurrentIndex(i);
                    break;
                }
            }
        }
        if (view_state.has_orthographic_projection) {
            viewport_->SetOrthographicProjection(view_state.orthographic_projection);
        }
        if (view_state.has_orbit_mode) {
            viewport_->SetOrbitMode(view_state.orbit_mode);
        }
        if (view_state.has_show_coordinate_axes) {
            SetCoordinateAxesVisible(view_state.show_coordinate_axes);
        }
        if (view_state.has_show_floor_grid) {
            SetFloorGridVisible(view_state.show_floor_grid);
        }
        if (view_state.has_xy_plane_view) {
            SetXYPlaneViewEnabled(view_state.xy_plane_view);
        }
        if (view_state.has_camera) {
            viewport_->SetCamera(view_state.camera);
            restored_camera = true;
        }
    }

    project_path_ = path.toStdString();
    UpdateWindowTitle();
    RememberLastDialogDir(path);
    AddRecentProjectFile(path);
    ClearActiveProperties();
    RefreshSceneTree();
    if (legacy_project || !restored_camera) {
        viewport_->FitToDocument();
    }
    viewport_->update();
    statusBar()->showMessage("Project opened");
}

void MainWindow::SaveProject() {
    QString path = QString::fromStdString(project_path_);
    if (path.isEmpty() || path.toLower().endsWith(".d3dm")) {
        path = QFileDialog::getSaveFileName(this, "Save Dom3D Project", LastDialogDir(), "Dom3D Project (*.dom3d);;All files (*.*)");
    }
    if (path.isEmpty()) {
        return;
    }
    if (QFileInfo(path).suffix().isEmpty()) {
        path += ".dom3d";
    }

    QString error;
    const QString active_room = tool_tabs_ ? tool_tabs_->tabText(tool_tabs_->currentIndex()) : QString("Architecture");
    ProjectViewState view_state;
    view_state.camera = viewport_->GetCamera();
    view_state.has_camera = true;
    view_state.orthographic_projection = viewport_->IsOrthographicProjection();
    view_state.has_orthographic_projection = true;
    view_state.orbit_mode = viewport_->GetOrbitMode();
    view_state.has_orbit_mode = true;
    view_state.show_coordinate_axes = viewport_->IsCoordinateAxesVisible();
    view_state.has_show_coordinate_axes = true;
    view_state.show_floor_grid = viewport_->IsFloorGridVisible();
    view_state.has_show_floor_grid = true;
    view_state.xy_plane_view = viewport_->IsXYPlaneViewEnabled();
    view_state.has_xy_plane_view = true;
    if (!dom3d_serializer_.Save(path, document_, active_room, view_state, error)) {
        QMessageBox::critical(this, "Dom3D Pro", error);
        return;
    }

    project_path_ = path.toStdString();
    UpdateWindowTitle();
    RememberLastDialogDir(path);
    AddRecentProjectFile(path);
    statusBar()->showMessage("Project saved", 1400);
}

void MainWindow::UpdateWindowTitle() {
    QString title = "Dom3D Pro";
    if (!project_path_.empty()) {
        const QString file_name = QFileInfo(QString::fromStdString(project_path_)).completeBaseName();
        if (!file_name.isEmpty()) {
            title += " - [" + file_name + "]";
        }
    }
    setWindowTitle(title);
}

void MainWindow::ShowPreferences() {
    PreferencesDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        statusBar()->showMessage("Preferences applied", 1400);
    }
}

void MainWindow::ImportFile() {
    const QString filter = "3D Studio (*.3ds);;Wavefront OBJ (*.obj);;STEP (*.step *.stp);;IGES (*.iges *.igs);;All files (*.*)";
    QString selected_filter;
    const QString path = QFileDialog::getOpenFileName(this, "Import", LastDialogDir(), filter, &selected_filter);
    if (path.isEmpty()) {
        return;
    }

    std::string error;
    std::map<std::string, unsigned long> imported_material_ids;
    const auto register_imported_material = [this, &imported_material_ids](CMesh3D& mesh) {
        Material material = mesh.GetMaterial();
        if (material.id != 0 || material.name.empty() || material.name == "Imported Mesh") {
            return;
        }
        const std::string key = material.name + "\n"
            + material.color_texture_path + "\n"
            + material.light_texture_path + "\n"
            + material.bump_texture_path;
        const auto existing = imported_material_ids.find(key);
        if (existing != imported_material_ids.end()) {
            if (const Material* saved = document_.FindMaterial(existing->second)) {
                mesh.SetMaterial(*saved);
            }
            return;
        }
        material.id = 0;
        const Material& saved = document_.UpsertMaterial(std::move(material));
        imported_material_ids.emplace(key, saved.id);
        mesh.SetMaterial(saved);
    };
    const QString lower_path = path.toLower();
    if (selected_filter.startsWith("3D Studio") || lower_path.endsWith(".3ds")) {
        std::vector<std::unique_ptr<CMesh3D>> meshes;
        if (!three_ds_io_.Import(path.toStdString(), meshes, error)) {
            QMessageBox::critical(this, "3DS Import", QString::fromStdString(error));
            return;
        }
        const std::string group_name = meshes.size() > 1
            ? (QFileInfo(path).completeBaseName() + " (3DS)").toStdString()
            : std::string{};
        for (auto& mesh : meshes) {
            mesh->SetGroupName(group_name);
            register_imported_material(*mesh);
            document_.AddMesh(std::move(mesh));
        }
    } else if (selected_filter.startsWith("STEP") || lower_path.endsWith(".step") || lower_path.endsWith(".stp")) {
        std::vector<std::unique_ptr<CSolid>> solids;
        if (!step_io_.Import(path.toStdString(), solids, error)) {
            QMessageBox::critical(this, "STEP Import", QString::fromStdString(error));
            return;
        }
        for (auto& solid : solids) {
            document_.AddObject(std::move(solid));
        }
    } else if (selected_filter.startsWith("IGES") || lower_path.endsWith(".iges") || lower_path.endsWith(".igs")) {
        std::vector<std::unique_ptr<CAlfaObject>> objects;
        if (!iges_io_.Import(path.toStdString(), objects, error)) {
            QMessageBox::critical(this, "IGES Import", QString::fromStdString(error));
            return;
        }
        for (auto& object : objects) {
            document_.AddObject(std::move(object));
        }
    } else {
        std::vector<std::unique_ptr<CMesh3D>> meshes;
        if (!obj_io_.Import(path.toStdString(), meshes, error)) {
            QMessageBox::critical(this, "OBJ Import", QString::fromStdString(error));
            return;
        }
        for (auto& mesh : meshes) {
            register_imported_material(*mesh);
            document_.AddMesh(std::move(mesh));
        }
    }

    document_.ClearSelection();
    RememberLastDialogDir(path);
    ClearActiveProperties();
    RefreshSceneTree();
    viewport_->FitToDocument();
    viewport_->update();
    statusBar()->showMessage("File imported", 1400);
}

void MainWindow::ExportFile() {
    const QString filter = "Wavefront OBJ (*.obj);;STEP (*.step *.stp);;IGES (*.iges *.igs);;All files (*.*)";
    QString selected_filter;
    QString path = QFileDialog::getSaveFileName(this, "Export", LastDialogDir(), filter, &selected_filter);
    if (path.isEmpty()) {
        return;
    }

    std::string error;
    const QString lower_path = path.toLower();
    const bool export_step = selected_filter.startsWith("STEP") || lower_path.endsWith(".step") || lower_path.endsWith(".stp");
    const bool export_iges = selected_filter.startsWith("IGES") || lower_path.endsWith(".iges") || lower_path.endsWith(".igs");
    if (QFileInfo(path).suffix().isEmpty()) {
        path += export_step ? ".step" : export_iges ? ".iges" : ".obj";
    }

    bool exported = false;
    if (export_step) {
        exported = step_io_.Export(path.toStdString(), document_, error);
    } else if (export_iges) {
        exported = iges_io_.Export(path.toStdString(), document_, error);
    } else {
        exported = obj_io_.Export(path.toStdString(), document_, error);
    }

    if (!exported) {
        QMessageBox::critical(this, export_step ? "STEP Export" : export_iges ? "IGES Export" : "OBJ Export", QString::fromStdString(error));
        return;
    }

    RememberLastDialogDir(path);
    statusBar()->showMessage("File exported", 1400);
}

void MainWindow::DuplicateSelectedObject() {
    ClearActiveProperties();
    if (!document_.DuplicateSelectedObject()) {
        statusBar()->showMessage("Copy: select an object or group first", 1600);
        return;
    }

    RefreshSceneTree();
    viewport_->update();
    BeginTransformTool(TransformOperation::Move);
    statusBar()->showMessage("Object/group copy created. Move tool is active", 1800);
}

void MainWindow::MirrorSelectedObject() {
    if (!document_.HasSelection()) {
        statusBar()->showMessage("Mirror: select an object or group first", 1600);
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Mirror Object by Plane");
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    auto* plane = new QComboBox(&dialog);
    plane->addItems({"YZ (X = offset)", "XZ (Y = offset)", "XY (Z = offset)"});
    auto* offset = new QDoubleSpinBox(&dialog);
    offset->setRange(-1000000.0, 1000000.0);
    offset->setDecimals(4);
    offset->setSingleStep(0.1);
    form->addRow("Mirror plane", plane);
    form->addRow("Offset", offset);
    layout->addLayout(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    Vec3 plane_point{};
    Vec3 plane_normal{};
    if (plane->currentIndex() == 0) {
        plane_point.x = static_cast<float>(offset->value());
        plane_normal.x = 1.0f;
    } else if (plane->currentIndex() == 1) {
        plane_point.y = static_cast<float>(offset->value());
        plane_normal.y = 1.0f;
    } else {
        plane_point.z = static_cast<float>(offset->value());
        plane_normal.z = 1.0f;
    }

    ClearActiveProperties();
    if (!document_.MirrorSelectedObjects(plane_point, plane_normal)) {
        statusBar()->showMessage("Mirror: could not create mirrored copy", 1800);
        return;
    }
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage("Mirrored object/group copy created", 1800);
}

void MainWindow::LoadUserSettings() {
    QSettings settings;
    last_file_dialog_dir_ = settings.value("files/lastDir", QDir::homePath()).toString();
    recent_project_files_ = settings.value("files/recentProjects").toStringList();
    const int solid_display_mode = settings.value("view/solidDisplayMode", static_cast<int>(SolidDisplayMode::SurfacesAndEdges)).toInt();
    if (solid_display_mode >= static_cast<int>(SolidDisplayMode::SurfacesAndEdges)
        && solid_display_mode <= static_cast<int>(SolidDisplayMode::Wireframe)) {
        CSolid::SetDisplayMode(static_cast<SolidDisplayMode>(solid_display_mode));
    }
    const int mesh_display_mode = settings.value("view/meshDisplayMode", static_cast<int>(MeshDisplayMode::SurfaceGray)).toInt();
    if (mesh_display_mode >= static_cast<int>(MeshDisplayMode::SurfaceGray)
        && mesh_display_mode <= static_cast<int>(MeshDisplayMode::SurfaceMaterial)) {
        CMesh3D::SetDisplayMode(static_cast<MeshDisplayMode>(mesh_display_mode));
    }
    CMesh3D::SetWireOpacity(settings.value("view/meshWireOpacity", 0.76).toFloat());
    CSolid::SetSurfaceTransparencyEnabled(settings.value("view/solidSurfaceTransparency", false).toBool());
    viewport_->SetOrthographicProjection(settings.value("view/orthographicProjection", false).toBool());
    const QString orbit_mode = settings.value("view/orbitMode", "cad").toString();
    viewport_->SetOrbitMode(orbit_mode == "architectural" ? OrbitMode::Architectural : OrbitMode::CAD);
    SetCoordinateAxesVisible(settings.value("view/showCoordinateAxes", true).toBool());
    SetFloorGridVisible(settings.value("view/showFloorGrid", true).toBool());
    SetXYPlaneViewEnabled(settings.value("view/xyPlaneView", false).toBool());
    recent_project_files_.removeAll(QString());
    recent_project_files_.removeDuplicates();
    while (recent_project_files_.size() > kMaxRecentProjectFiles) {
        recent_project_files_.removeLast();
    }
}

void MainWindow::RememberLastDialogDir(const QString& path) {
    const QFileInfo info(path);
    const QString dir = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    if (dir.isEmpty()) {
        return;
    }

    last_file_dialog_dir_ = dir;
    QSettings settings;
    settings.setValue("files/lastDir", last_file_dialog_dir_);
}

QString MainWindow::LastDialogDir() const {
    if (!last_file_dialog_dir_.isEmpty() && QDir(last_file_dialog_dir_).exists()) {
        return last_file_dialog_dir_;
    }
    return QDir::homePath();
}

void MainWindow::AddRecentProjectFile(const QString& path) {
    const QString canonical_path = QFileInfo(path).absoluteFilePath();
    recent_project_files_.removeAll(canonical_path);
    recent_project_files_.prepend(canonical_path);
    while (recent_project_files_.size() > kMaxRecentProjectFiles) {
        recent_project_files_.removeLast();
    }

    QSettings settings;
    settings.setValue("files/recentProjects", recent_project_files_);
    UpdateRecentFilesMenu();
}

void MainWindow::UpdateRecentFilesMenu() {
    if (!recent_files_menu_) {
        return;
    }

    recent_files_menu_->clear();
    QStringList existing_files;
    for (const QString& path : recent_project_files_) {
        if (path.isEmpty()) {
            continue;
        }
        if (!QFileInfo::exists(path)) {
            continue;
        }
        existing_files.push_back(path);
    }

    if (existing_files.isEmpty()) {
        QAction* empty_action = recent_files_menu_->addAction("(Empty)");
        empty_action->setEnabled(false);
        recent_files_menu_->setEnabled(false);
        return;
    }

    recent_files_menu_->setEnabled(true);
    for (const QString& path : existing_files) {
        const QString label = QFileInfo(path).fileName();
        QAction* action = recent_files_menu_->addAction(label, this, [this, path]() {
            OpenProjectFromPath(path);
        });
        action->setToolTip(path);
    }

    recent_files_menu_->addSeparator();
    recent_files_menu_->addAction("Clear Recent Files", this, [this]() {
        ClearRecentProjectFiles();
    });
}

void MainWindow::ClearRecentProjectFiles() {
    recent_project_files_.clear();
    QSettings settings;
    settings.setValue("files/recentProjects", recent_project_files_);
    UpdateRecentFilesMenu();
}

void MainWindow::ShowGreetingDialog(bool force) {
    QSettings settings;
    if (!force && !settings.value("startup/showGreeting", true).toBool()) {
        return;
    }

    QString recent_path;
    for (const QString& path : recent_project_files_) {
        if (QFileInfo::exists(path)) {
            recent_path = path;
            break;
        }
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Greeting");
    dialog.setModal(true);
    dialog.resize(520, 410);
    const QRect parent_rect = geometry();
    dialog.move(parent_rect.center() - QPoint(dialog.width() / 2, dialog.height() / 2));

    auto* root_layout = new QVBoxLayout(&dialog);
    root_layout->setContentsMargins(6, 18, 6, 8);
    root_layout->setSpacing(8);

    auto* content = new QWidget(&dialog);
    content->setObjectName("GreetingContent");
    auto* content_layout = new QGridLayout(content);
    content_layout->setContentsMargins(34, 34, 34, 28);
    content_layout->setHorizontalSpacing(70);
    content_layout->setVerticalSpacing(34);

    auto* group = new QButtonGroup(&dialog);
    group->setExclusive(true);

    int chosen_action = -1;
    const auto make_choice = [&dialog, group](const QString& key, int id, bool enabled) {
        auto* button = new QPushButton(&dialog);
        button->setCheckable(true);
        button->setEnabled(enabled);
        button->setMinimumSize(145, 112);
        button->setIcon(QIcon(GreetingIcon(key)));
        button->setIconSize(QSize(132, 100));
        button->setCursor(enabled ? Qt::PointingHandCursor : Qt::ArrowCursor);
        button->setProperty("choice", true);
        group->addButton(button, id);
        return button;
    };

    auto* new_button = make_choice("new", 0, true);
    auto* open_button = make_choice("open", 1, true);
    new_button->setChecked(true);

    content_layout->addWidget(new_button, 0, 0, Qt::AlignCenter);
    content_layout->addWidget(open_button, 0, 1, Qt::AlignCenter);
    if (!recent_path.isEmpty()) {
        auto* recent_label = new ClickableLabel(QDir::toNativeSeparators(recent_path), content);
        recent_label->setObjectName("GreetingRecentPath");
        recent_label->setWordWrap(false);
        recent_label->setAlignment(Qt::AlignCenter);
        recent_label->on_click = [&dialog, &chosen_action]() {
            chosen_action = 2;
            dialog.accept();
        };
        content_layout->addWidget(recent_label, 2, 0, 1, 2, Qt::AlignCenter);
    }
    content_layout->setColumnStretch(0, 1);
    content_layout->setColumnStretch(1, 1);
    content_layout->setRowStretch(1, 1);
    content_layout->setRowStretch(3, 1);

    root_layout->addWidget(content, 1);

    auto* bottom_layout = new QHBoxLayout();
    auto* dont_show_check = new QCheckBox("Don't show in Future", &dialog);
    auto* button_box = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    bottom_layout->addWidget(dont_show_check);
    bottom_layout->addStretch(1);
    bottom_layout->addWidget(button_box);
    root_layout->addLayout(bottom_layout);

    dialog.setStyleSheet(
        "QWidget#GreetingContent { background: #eef2f8; }"
        "QPushButton[choice=\"true\"] {"
        "  background: transparent;"
        "  border: 0;"
        "  padding: 10px;"
        "}"
        "QPushButton[choice=\"true\"]:checked {"
        "  border-bottom: 2px solid #2850ff;"
        "}"
        "QPushButton[choice=\"true\"]:disabled {"
        "  color: #8e95a3;"
        "}"
        "QPushButton[choice=\"true\"]:hover:!disabled {"
        "  background: rgba(40, 80, 255, 0.08);"
        "}"
        "QLabel#GreetingRecentPath {"
        "  color: #ff2020;"
        "  font-size: 26px;"
        "  border-bottom: 2px solid #2850ff;"
        "  padding: 0 0 4px 0;"
        "}"
        "QLabel#GreetingRecentPath:hover {"
        "  color: #d80000;"
        "}"
    );

    const auto accept_choice = [&dialog, &chosen_action](int choice) {
        chosen_action = choice;
        dialog.accept();
    };
    connect(new_button, &QPushButton::clicked, &dialog, [accept_choice]() { accept_choice(0); });
    connect(open_button, &QPushButton::clicked, &dialog, [accept_choice]() { accept_choice(1); });
    connect(button_box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    settings.setValue("startup/showGreeting", !dont_show_check->isChecked());
    const int choice = chosen_action >= 0 ? chosen_action : group->checkedId();
    if (choice == 1) {
        OpenProject();
    } else if (choice == 2 && !recent_path.isEmpty()) {
        OpenProjectFromPath(recent_path);
    } else {
        NewProject();
    }
}

void MainWindow::DeleteSelected() {
    if (document_.DeleteSelectedPoint() || document_.DeleteSelectedObject()) {
        ClearActiveProperties();
        RefreshSceneTree();
        viewport_->update();
    }
}

void MainWindow::PopulateToolsPanelForTab(int tab_index) {
    if (!tools_dock_ || !tools_layout_ || !tool_tabs_) {
        return;
    }

    ClearActiveProperties();
    tool_buttons_.clear();
    while (QLayoutItem* item = tools_layout_->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    const QString tab = tool_tabs_->tabText(tab_index);
    tools_dock_->setWindowTitle(tab);
    tools_dock_->setVisible(tab == "Architecture" || tab == "Furniture" || tab == "Surfaces" || tab == "Solid" || tab == "Curves");

    std::vector<std::string> tool_ids;
    if (tab == "Architecture") {
        tool_ids = {"stair", "window", "door"};
    } else if (tab == "Furniture") {
        tool_ids = {"cabinet"};
    } else if (tab == "Curves") {
        tool_ids = {"PolylineCurve", "BSplineCurve", "EditPoint"};
    } else if (tab == "Surfaces") {
        tool_ids = {"SurfaceLoft", "SurfaceReverseNormals", "SurfaceOfRevolution"};
    } else if (tab == "Solid") {
        // единый boolean-инструмент вместо трёх отдельных
        tool_ids = {"SolidBox", "SolidCylinder", "SolidPrismTool", "SolidExtrudeTool", "SurfaceOfRevolution", "boolean", "fillet_edge", "fillet_all_edges", "ChamferSolid", "SolidExtrudeFace", "SolidDraft", "ThickSolidTool", "SolidLowPoly"};
    }

    int index = 0;
    for (const std::string& tool_id : tool_ids) {
        AddToolButton(tools_layout_, tools_panel_, tool_id, index / 2, index % 2);
        ++index;
    }

    if (tab == "Solid") {
        const std::vector<std::pair<QString, QString>> placeholders = {
            {"SolidSphereTool", "Sphere"},
            {"SolidTorusTool", "Torus"},
            {"SolidSweptTool", "Swept Solid"},
            {"DeleteFaceOrEdge", "Delete Face or Edge"},
            {"ExtractFaceTool", "Extract Face"},
            {"SewingFaceTool", "Sew Faces"},
            {"SplitRings", "Split Rings"},
            {"SolidInSet", "Inset"},
            {"SolidTransform", "Solid Transform"}
        };
        for (const auto& placeholder : placeholders) {
            AddPlaceholderButton(tools_layout_, tools_panel_, placeholder.first, placeholder.second, index / 2, index % 2);
            ++index;
        }
    } else if (tab == "Surfaces") {
        const std::vector<std::pair<QString, QString>> placeholders = {
            {"SurfaceSweep", "Sweep Surface"},
            {"SurfacePatch", "Patch Surface"},
            {"SurfaceOffset", "Offset Surface"}
        };
        for (const auto& placeholder : placeholders) {
            AddPlaceholderButton(tools_layout_, tools_panel_, placeholder.first, placeholder.second, index / 2, index % 2);
            ++index;
        }
    }

    tools_layout_->setRowStretch((index + 1) / 2, 1);
    UpdateActiveToolUi(active_tool_key_);
}
