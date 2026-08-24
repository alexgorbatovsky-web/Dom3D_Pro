#include "MainWindow.h"

#include "../CadCurve3D.h"
#include "../CBSpline.h"
#include "../BezierSpline.h"
#include "../CMesh3D.h"
#include "../ReferenceImage.h"
#include "../CGroup.h"
#include "../CPart.h"
#include "../CAssembled.h"
#include "../CKitchenCabinet.h"
#include "../CPolyline.h"
#include "../DrawingText.h"
#include "../Line2D.h"
#include "../MaterialLibrary.h"
#include "../SmartLine.h"
#include "../SweptSolidBuilder.h"
#include "../solid/Solid.h"
#include "../solid/SheetBendShapeBuilder.h"
#include "../solid/SurfaceSet.h"
#include "MaterialEditorDialog.h"
#include "MaterialDrag.h"
#include "MeasurementUnits.h"
#include "PreferencesDialog.h"
#include "LightingDialog.h"
#include "BlenderCyclesDialog.h"
#include "NativeRaytraceDialog.h"
#include "../render/RenderScene.h"
#include "LanguageManager.h"
#include "HotkeyManagerDialog.h"
#include "CommandSearchPopup.h"
#include "BooleanDialog.h"
#include "ExtrudeFaceDialog.h"
#include "DragSpinBoxLabel.h"

#include <QAction>
#include <QActionGroup>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QColor>
#include <QCloseEvent>
#include <QComboBox>
#include <QCursor>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QStackedWidget>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDir>
#include <QCoreApplication>
#include <QDropEvent>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFontComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QElapsedTimer>
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
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadialGradient>
#include <QRadioButton>
#include <QRegularExpression>
#include <QScreen>
#include <QSettings>
#include <QShortcut>
#include <QSize>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabBar>
#include <QTimer>
#include <QThreadPool>
#include <QPointer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <BRep_Builder.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopoDS_Compound.hxx>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <vector>

void message_to_file(const char* text);

namespace {
constexpr int kMaxRecentProjectFiles = 18;
constexpr int kSceneTreeObjectIndexRole = Qt::UserRole + 1;
constexpr int kSceneTreeGroupRole = Qt::UserRole + 2;
constexpr int kSceneTreeGroupIdRole = Qt::UserRole + 3;
constexpr int kSceneTreePartsRootRole = Qt::UserRole + 4;

bool IsFurnitureAssemblyTool(const std::string& tool_id) {
    return tool_id == "chair"
        || tool_id == "chair_simple"
        || tool_id == "cabinet"
        || tool_id == "cabinet_advanced"
        || tool_id == "cabinet_advanced_slx"
        || tool_id == "cabinet_showcase"
        || tool_id == "table"
        || tool_id == "desk"
        || tool_id == "drawer_box"
        || tool_id == "single_drawer"
        || tool_id == "single_facade"
        || tool_id == "kitchen_nika_260"
        || tool_id == "kitchen_corner";
}

bool IsCabinetTool(const std::string& tool_id) {
    return tool_id == "cabinet"
        || tool_id == "cabinet_advanced"
        || tool_id == "cabinet_advanced_slx"
        || tool_id == "cabinet_showcase";
}

std::vector<unsigned long> CreateSlxFrameTemplateSketches(CAlfaDoc& document) {
    const auto xy_sketch = [](const std::string& name) {
        auto sketch = std::make_unique<CSmartLine>(name);
        sketch->SetCoordinateSystem(
            CPoint3d(0.0, 0.0, 0.0),
            CPoint3d(1.0, 0.0, 0.0),
            CPoint3d(0.0, 0.0, 1.0));
        return sketch;
    };

    auto frame = xy_sketch("SLX Frame Profile");
    frame->AddLine(std::make_unique<CLinkLine>(
        CPoint3d(0.0, 0.0, 0.0), CPoint3d(0.0, 12.0, 0.0)));
    frame->AddLine(std::make_unique<CLinkLine>(
        CPoint3d(0.0, 12.0, 0.0), CPoint3d(2.0, 12.0, 0.0)));
    frame->AddLine(std::make_unique<CLinkLine>(
        CPoint3d(2.0, 12.0, 0.0), CPoint3d(4.0, 15.0, 0.0)));
    frame->AddLine(std::make_unique<CBezierSpline>(
        CPoint3d(4.0, 15.0, 0.0), CPoint3d(13.9, 20.2226, 0.0),
        CPoint3d(41.881, 20.2226, 0.0), CPoint3d(51.3, 15.0, 0.0)));
    frame->AddLine(std::make_unique<CBezierSpline>(
        CPoint3d(51.3, 15.0, 0.0), CPoint3d(53.0, 17.0, 0.0),
        CPoint3d(55.0, 17.0, 0.0), CPoint3d(57.0, 15.0, 0.0)));
    frame->AddLine(std::make_unique<CLinkLine>(
        CPoint3d(57.0, 15.0, 0.0), CPoint3d(60.0, 15.0, 0.0)));
    frame->AddLine(std::make_unique<CLinkLine>(
        CPoint3d(60.0, 15.0, 0.0), CPoint3d(60.0, 0.0, 0.0)));
    frame->AddLine(std::make_unique<CLinkLine>(
        CPoint3d(60.0, 0.0, 0.0), CPoint3d(0.0, 0.0, 0.0)));
    frame->SetClosed(true);

    auto panel = xy_sketch("SLX Panel Profile");
    // Keep every reusable section in its own local coordinate system.  The
    // facade builder normalizes the profile bounds and places it at the panel
    // inset, so a catalog profile does not need an authoring offset.
    const double panel_x = 0.0;
    panel->AddLine(std::make_unique<CLinkLine>(
        CPoint3d(panel_x, 0.0, 0.0), CPoint3d(panel_x, 7.0, 0.0)));
    panel->AddLine(std::make_unique<CBezierSpline>(
        CPoint3d(panel_x, 7.0, 0.0), CPoint3d(panel_x + 8.0, 7.0, 0.0),
        CPoint3d(panel_x + 24.0, 14.0, 0.0),
        CPoint3d(panel_x + 32.6, 14.0, 0.0)));
    panel->AddLine(std::make_unique<CLinkLine>(
        CPoint3d(panel_x + 32.6, 14.0, 0.0),
        CPoint3d(panel_x + 40.0, 14.0, 0.0)));
    panel->AddLine(std::make_unique<CLinkLine>(
        CPoint3d(panel_x + 40.0, 14.0, 0.0),
        CPoint3d(panel_x + 40.0, 0.0, 0.0)));
    panel->AddLine(std::make_unique<CLinkLine>(
        CPoint3d(panel_x + 40.0, 0.0, 0.0),
        CPoint3d(panel_x, 0.0, 0.0)));
    panel->SetClosed(true);

    std::vector<unsigned long> ids;
    document.AddObject(std::move(frame));
    if (CAlfaObject* added = document.GetSelectedObject()) {
        ids.push_back(added->m_id);
    }
    document.AddObject(std::move(panel));
    if (CAlfaObject* added = document.GetSelectedObject()) {
        ids.push_back(added->m_id);
    }
    document.ClearSelection();
    for (size_t index = 0; index < ids.size(); ++index) {
        document.SelectObjectById(
            ids[index], index == 0 ? SelectionAction::Replace : SelectionAction::Add);
    }
    return ids;
}

void Message_err(const char* message)
{
    message_to_file(message);
}

void DoTest(CPolyline* Plface, CPolyline* PlCut)
{
    if (!Plface || !PlCut) {
        Message_err("ClassifyFaceCut: Plface or PlCut is null\n");
        return;
    }

    Face2D face;
    for (int i = 0; i < static_cast<int>(Plface->np()); ++i) {
        face.verts.push_back(cVec2(Plface->P(i)->x, Plface->P(i)->y));
    }

    std::vector<cVec2> cut;
    for (int i = 0; i < static_cast<int>(PlCut->np()); ++i) {
        cut.push_back(cVec2(PlCut->P(i)->x, PlCut->P(i)->y));
    }

    CellCutInfo info;
    if (!AnalyzeFaceCut(face, cut, info)) {
        Message_err("AnalyzeFaceCut failed\n");
        return;
    }

    ClassifyFaceCut(face, cut, info);
}

BooleanDialog::Operation DialogOperationFromBoolean(BooleanOperation operation) {
    if (operation == BooleanOperation::Cut) {
        return BooleanDialog::Operation::Cut;
    }
    if (operation == BooleanOperation::Common) {
        return BooleanDialog::Operation::Common;
    }
    return BooleanDialog::Operation::Union;
}

BooleanOperation BooleanOperationFromDialog(BooleanDialog::Operation operation) {
    if (operation == BooleanDialog::Operation::Cut) {
        return BooleanOperation::Cut;
    }
    if (operation == BooleanDialog::Operation::Common) {
        return BooleanOperation::Common;
    }
    return BooleanOperation::Union;
}

void CenterDialogOnCursor(QDialog& dialog) {
    dialog.adjustSize();

    const QPoint cursor_pos = QCursor::pos();
    const QSize dialog_size = dialog.sizeHint().expandedTo(dialog.size());
    QPoint top_left = cursor_pos - QPoint(dialog_size.width() / 2, dialog_size.height() / 2);

    if (QScreen* screen = QGuiApplication::screenAt(cursor_pos)) {
        const QRect bounds = screen->availableGeometry();
        top_left.setX(std::clamp(top_left.x(), bounds.left(), bounds.right() - dialog_size.width() + 1));
        top_left.setY(std::clamp(top_left.y(), bounds.top(), bounds.bottom() - dialog_size.height() + 1));
    }

    dialog.move(top_left);
}

void PlaceDialogAtWorkspaceTopLeft(QDialog& dialog, const QWidget& workspace) {
    dialog.adjustSize();

    const QSize dialog_size = dialog.sizeHint().expandedTo(dialog.size());
    QPoint top_left = workspace.mapToGlobal(QPoint(8, 8));
    if (QScreen* screen = QGuiApplication::screenAt(top_left)) {
        const QRect bounds = screen->availableGeometry();
        top_left.setX(std::clamp(
            top_left.x(), bounds.left(),
            bounds.right() - dialog_size.width() + 1));
        top_left.setY(std::clamp(
            top_left.y(), bounds.top(),
            bounds.bottom() - dialog_size.height() + 1));
    }
    dialog.move(top_left);
}

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
        density_->setRange(0.1, 100.0);
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
        const float mesh_step = static_cast<float>(
            1.0 / std::max(density, 0.0001));
        const bool mesh_quadro = mesh_quadro_->isChecked();
        int rebuilt = 0;
        for (CSolid* solid : Solids()) {
            solid->ptchDensity = static_cast<float>(density);
            solid->MeshQuadro = mesh_quadro;
            // Low Poly density has legacy inverse semantics: increasing
            // Density creates more cells. Do not use the adaptive scene
            // tessellation overload here, because it ignores this control.
            if (solid->ReBuldMesh(mesh_step)) {
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

class MeshFillContourDialog : public QDialog {
public:
    MeshFillContourDialog(CAlfaDoc& document,
                          std::function<void()> refresh_scene,
                          std::function<void(const QString&)> set_status,
                          QWidget* parent)
        : QDialog(parent),
          document_(document),
          refresh_scene_(std::move(refresh_scene)),
          set_status_(std::move(set_status)) {
        setWindowTitle("Mesh 3D - Fiill Contour");
        setAttribute(Qt::WA_DeleteOnClose, true);

        auto* root = new QVBoxLayout(this);
        auto* contour_label = new QLabel("Contour CPolyline", this);
        contours_list_ = new QListWidget(this);
        contours_list_->setMinimumSize(280, 120);
        root->addWidget(contour_label);
        root->addWidget(contours_list_);

        auto* use_selected = new QPushButton("Use Selected Contour", this);
        root->addWidget(use_selected);

        auto* form = new QFormLayout();
        density_ = new QDoubleSpinBox(this);
        density_->setRange(0.001, 100000.0);
        density_->setDecimals(3);
        density_->setSingleStep(0.1);
        density_->setValue(1.0);
        form->addRow("Density", density_);
        root->addLayout(form);

        auto* buttons = new QHBoxLayout();
        auto* create = new QPushButton("Create Mesh", this);
        auto* close = new QPushButton("Close", this);
        buttons->addWidget(create);
        buttons->addStretch();
        buttons->addWidget(close);
        root->addLayout(buttons);

        RebuildContoursList();
        SelectDocumentPolyline();

        connect(use_selected, &QPushButton::clicked, this, [this]() {
            SelectDocumentPolyline();
        });
        connect(create, &QPushButton::clicked, this, [this]() {
            CreateMesh();
        });
        connect(close, &QPushButton::clicked, this, &QDialog::close);
    }

    bool HasContours() const
    {
        return contours_list_ && contours_list_->count() > 0;
    }

private:
    void RebuildContoursList()
    {
        contours_list_->clear();
        for (const CAlfaDoc::ObjectPtr& object : document_.GetObjects()) {
            const auto* polyline = dynamic_cast<const CPolyline*>(object.get());
            if (!polyline || polyline->GetPointCount() < 3) {
                continue;
            }
            auto* item = new QListWidgetItem(QString::fromStdString(polyline->GetName()), contours_list_);
            item->setData(Qt::UserRole, static_cast<qulonglong>(polyline->m_id));
            item->setToolTip(polyline->IsClosed() ? "Closed CPolyline" : "Open CPolyline will be closed for fill");
        }
        if (contours_list_->count() > 0 && !contours_list_->currentItem()) {
            contours_list_->setCurrentRow(0);
        }
    }

    void SelectDocumentPolyline()
    {
        const CPolyline* selected = document_.GetSelectedPolyline();
        if (!selected) {
            if (set_status_) {
                set_status_("Fiill Contour:select the required geometry and continue");
            }
            return;
        }
        for (int i = 0; i < contours_list_->count(); ++i) {
            QListWidgetItem* item = contours_list_->item(i);
            if (item && item->data(Qt::UserRole).toULongLong() == selected->m_id) {
                contours_list_->setCurrentItem(item);
                if (set_status_) {
                    set_status_(QString("Fiill Contour: contour %1 selected").arg(QString::fromStdString(selected->GetName())));
                }
                return;
            }
        }
    }

    CPolyline* CurrentPolyline()
    {
        QListWidgetItem* item = contours_list_->currentItem();
        if (!item) {
            return nullptr;
        }
        const unsigned long id = static_cast<unsigned long>(item->data(Qt::UserRole).toULongLong());
        return dynamic_cast<CPolyline*>(document_.FindObjectById(id));
    }

    void CreateMesh()
    {
        CPolyline* contour = CurrentPolyline();
        if (!contour) {
            QMessageBox::warning(this, "Fiill Contour", "Select a CPolyline contour.");
            return;
        }
        auto mesh = std::make_unique<CMesh3D>(contour->GetName() + " Fill Mesh");
        if (!mesh->CreateFromBoundary(contour, static_cast<float>(density_->value()))) {
            QMessageBox::warning(this, "Fiill Contour", "Mesh was not created. Check contour points.");
            return;
        }
        mesh->SetColor({0.16f, 0.52f, 0.82f});
        document_.AddMesh(std::move(mesh));
        if (refresh_scene_) {
            refresh_scene_();
        }
        if (set_status_) {
            set_status_(QString("Fiill Contour: mesh created, Density %1").arg(density_->value(), 0, 'f', 3));
        }
    }

    CAlfaDoc& document_;
    std::function<void()> refresh_scene_;
    std::function<void(const QString&)> set_status_;
    QListWidget* contours_list_ = nullptr;
    QDoubleSpinBox* density_ = nullptr;
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

QIcon CatalogIcon() {
    QPixmap pixmap(22, 22);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(52, 56, 64), 1.2));
    painter.setBrush(QColor(246, 194, 52));
    painter.drawRoundedRect(QRectF(2.5, 4.5, 17.0, 14.0), 1.4, 1.4);
    painter.drawRect(QRectF(4.0, 2.8, 7.0, 3.8));

    painter.setPen(QPen(QColor(55, 61, 70), 1.4, Qt::SolidLine,
                        Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor(238, 241, 245));
    painter.drawRect(QRectF(6.0, 9.0, 3.0, 3.0));
    painter.drawRect(QRectF(13.0, 7.0, 3.0, 3.0));
    painter.drawRect(QRectF(13.0, 14.0, 3.0, 3.0));
    painter.drawLine(QPointF(9.0, 10.5), QPointF(11.0, 10.5));
    painter.drawLine(QPointF(11.0, 8.5), QPointF(11.0, 15.5));
    painter.drawLine(QPointF(11.0, 8.5), QPointF(13.0, 8.5));
    painter.drawLine(QPointF(11.0, 15.5), QPointF(13.0, 15.5));
    painter.end();
    return QIcon(pixmap);
}

QPixmap CatalogOrientationGuidePixmap(
    const QString& parameter_id, bool product) {
    QPixmap pixmap(620, 330);
    pixmap.fill(QColor(248, 249, 251));
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const auto arrow = [&painter](QPointF start, QPointF end,
                                  QColor color, const QString& label) {
        QPen pen(color, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.drawLine(start, end);
        const QLineF line(start, end);
        const double angle = std::atan2(-line.dy(), line.dx());
        constexpr double arrow_size = 11.0;
        const QPointF first = end - QPointF(
            std::cos(angle + 0.55) * arrow_size,
            -std::sin(angle + 0.55) * arrow_size);
        const QPointF second = end - QPointF(
            std::cos(angle - 0.55) * arrow_size,
            -std::sin(angle - 0.55) * arrow_size);
        painter.setBrush(color);
        painter.drawPolygon(QPolygonF{end, first, second});
        painter.setPen(QPen(color.darker(120), 1.0));
        painter.setFont(QFont(painter.font().family(), 11, QFont::Bold));
        painter.drawText(end + QPointF(7.0, -6.0), DomTranslate(label));
    };

    painter.setPen(QColor(40, 44, 52));
    painter.setFont(QFont(painter.font().family(), 14, QFont::Bold));
    if (product) {
        painter.drawText(22, 31, DomTranslate("Catalog handle — local coordinate system"));
        const QPointF origin(305.0, 225.0);

        painter.setPen(QPen(QColor(128, 91, 59), 2.0, Qt::DashLine));
        painter.setBrush(QColor(207, 171, 128, 42));
        painter.drawRect(QRectF(90.0, 70.0, 430.0, 190.0));
        painter.setPen(QColor(104, 75, 51));
        painter.setFont(QFont(painter.font().family(), 10));
        painter.drawText(99, 91, DomTranslate("facade plane (XZ)"));

        QPainterPath handle;
        handle.moveTo(150.0, 191.0);
        handle.cubicTo(220.0, 158.0, 390.0, 158.0, 470.0, 191.0);
        painter.setPen(QPen(QColor(230, 181, 24), 15.0,
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawPath(handle);
        painter.setPen(QPen(QColor(124, 91, 12), 1.2));
        painter.drawPath(handle);

        arrow(origin, QPointF(555.0, 225.0), QColor(220, 45, 45), "+X length");
        arrow(origin, QPointF(205.0, 120.0), QColor(35, 165, 70), "+Y");
        arrow(origin, QPointF(305.0, 48.0), QColor(38, 92, 220), "+Z height");
        arrow(origin, QPointF(400.0, 305.0), QColor(29, 135, 57), "-Y front");

        painter.setPen(QColor(55, 58, 66));
        painter.setFont(QFont(painter.font().family(), 10));
        painter.drawText(22, 315, DomTranslate(
            "Model the handle length along X. The visible/front side faces -Y; Z is up."));
    } else {
        const bool panel = parameter_id == "slx.panel.id";
        const bool cutter = parameter_id == "slx.milling.profile.id";
        const bool milling_guide = parameter_id == "slx.milling.guide.id";
        painter.drawText(22, 31, DomTranslate(
            panel ? "Panel profile sketch — local XY plane"
            : cutter ? "Cutter profile sketch — local XY plane"
            : milling_guide ? "Milling pattern — local XY plane"
            : "Frame profile sketch — local XY plane"));
        const QPointF origin = cutter
            ? QPointF(310.0, 244.0) : QPointF(92.0, 264.0);
        painter.setPen(QPen(QColor(105, 70, 38), 2.0));
        painter.setBrush(QColor(189, 130, 74, 105));
        QPainterPath section;
        section.moveTo(132.0, 244.0);
        if (milling_guide) {
            section.moveTo(175.0, 205.0);
            section.cubicTo(175.0, 128.0, 445.0, 128.0, 445.0, 205.0);
            section.cubicTo(445.0, 254.0, 175.0, 254.0, 175.0, 205.0);
        } else if (cutter) {
            section = QPainterPath();
            section.moveTo(286.0, 82.0);
            section.lineTo(334.0, 82.0);
            section.lineTo(334.0, 220.0);
            section.cubicTo(334.0, 233.0, 326.0, 244.0, 315.0, 244.0);
            section.lineTo(305.0, 244.0);
            section.cubicTo(294.0, 244.0, 286.0, 233.0, 286.0, 220.0);
        } else if (panel) {
            section.lineTo(132.0, 207.0);
            section.cubicTo(190.0, 207.0, 230.0, 152.0, 325.0, 152.0);
            section.lineTo(480.0, 152.0);
            section.lineTo(480.0, 244.0);
        } else {
            section.lineTo(132.0, 176.0);
            section.lineTo(180.0, 176.0);
            section.cubicTo(230.0, 113.0, 376.0, 113.0, 424.0, 176.0);
            section.lineTo(480.0, 176.0);
            section.lineTo(480.0, 244.0);
        }
        if (!milling_guide) {
            section.closeSubpath();
        }
        painter.drawPath(section);
        painter.setPen(QPen(QColor(199, 35, 205), 3.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(section);

        arrow(origin, cutter ? QPointF(555.0, 244.0) : QPointF(555.0, 264.0),
              QColor(220, 45, 45),
              milling_guide ? "+X facade width" : "+X");
        arrow(origin, cutter ? QPointF(310.0, 48.0) : QPointF(92.0, 66.0),
              QColor(35, 165, 70),
              milling_guide ? "+Y facade height" : "+Y outward");
        painter.setPen(QPen(QColor(38, 92, 220), 2.0));
        painter.setBrush(QColor(38, 92, 220));
        painter.drawEllipse(origin, 5.0, 5.0);
        painter.setFont(QFont(painter.font().family(), 10, QFont::Bold));
        painter.drawText(origin + QPointF(-50.0, 21.0), DomTranslate("Z: normal to sketch"));

        painter.setPen(QColor(55, 58, 66));
        painter.setFont(QFont(painter.font().family(), 10));
        painter.drawText(22, 315, DomTranslate(
            milling_guide
                ? "All guides are scaled and centred as one pattern; their proportions and spacing are preserved."
            : cutter
                ? "One closed Sketch: tip Y=0, body Y>=0, symmetric about X=0."
            : panel
                ? "Use one closed contour at the origin. Its position on the door is calculated automatically."
                : "Use one closed contour. Start at the outer/back corner; X goes inward, Y toward the face."));
    }
    painter.end();
    return pixmap;
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

QIcon LayerPropertiesIcon() {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(QColor(252, 252, 252));
    painter.setPen(QPen(QColor(72, 72, 72), 1.2));
    painter.drawRect(QRectF(4.5, 4.5, 35.0, 35.0));

    painter.setPen(QPen(QColor(34, 34, 34), 2.1,
                        Qt::SolidLine, Qt::SquareCap));
    const qreal layer_y[] = {9.0, 15.0, 22.0, 29.0, 36.0};
    for (qreal y : layer_y) {
        painter.drawLine(QPointF(8.0, y), QPointF(36.0, y));
    }

    painter.end();
    return QIcon(pixmap);
}

QIcon ChangeLayerIcon() {
    QPixmap pixmap = LayerPropertiesIcon().pixmap(44, 44);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setPen(QPen(QColor(255, 84, 38), 2.8,
                        Qt::SolidLine, Qt::SquareCap));
    painter.drawLine(QPointF(7.0, 9.0), QPointF(25.0, 9.0));

    painter.setPen(QPen(QColor(30, 45, 235), 2.8,
                        Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter.drawLine(QPointF(8.0, 36.0), QPointF(28.0, 36.0));
    painter.drawLine(QPointF(36.0, 35.0), QPointF(36.0, 11.0));

    QPainterPath arrow;
    arrow.moveTo(36.0, 11.0);
    arrow.lineTo(27.0, 11.0);
    arrow.lineTo(31.0, 7.0);
    arrow.lineTo(25.0, 7.0);
    arrow.lineTo(20.0, 12.0);
    arrow.lineTo(25.0, 17.0);
    arrow.lineTo(31.0, 17.0);
    arrow.lineTo(27.0, 13.0);
    arrow.lineTo(36.0, 13.0);
    arrow.closeSubpath();
    painter.setPen(QPen(QColor(225, 30, 45), 1.1,
                        Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter.setBrush(QColor(255, 55, 70));
    painter.drawPath(arrow);

    painter.end();
    return QIcon(pixmap);
}

QIcon MaterialEditorIcon() {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(45, 48, 52), 1.2));
    painter.setBrush(QColor(8, 11, 14));
    painter.drawRoundedRect(QRectF(4.5, 4.5, 35.0, 35.0), 1.5, 1.5);

    const auto draw_ball = [&painter](const QPointF& center,
                                      const QColor& color) {
        QRadialGradient gradient(center - QPointF(2.4, 2.8), 9.2, center);
        gradient.setColorAt(0.0, color.lighter(175));
        gradient.setColorAt(0.38, color.lighter(112));
        gradient.setColorAt(0.78, color);
        gradient.setColorAt(1.0, color.darker(190));
        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.drawEllipse(center, 8.2, 8.2);
    };

    draw_ball(QPointF(15.0, 15.0), QColor(20, 205, 45));
    draw_ball(QPointF(29.0, 15.0), QColor(205, 35, 205));
    draw_ball(QPointF(15.0, 29.0), QColor(15, 195, 215));
    draw_ball(QPointF(29.0, 29.0), QColor(225, 215, 25));

    painter.end();
    return QIcon(pixmap);
}

QIcon EditTextureIcon() {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(55, 64, 70), 1.1));
    painter.setBrush(QColor(207, 239, 250));
    painter.drawRect(QRectF(4.5, 4.5, 35.0, 35.0));
    painter.fillRect(QRectF(5.0, 33.0, 34.0, 6.0), QColor(45, 205, 235));

    painter.setPen(QPen(QColor(111, 62, 28), 2.4,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(22.0, 34.0), QPointF(22.0, 23.0));
    painter.drawLine(QPointF(22.0, 24.0), QPointF(14.0, 16.0));
    painter.drawLine(QPointF(22.0, 24.0), QPointF(30.5, 14.0));
    painter.drawLine(QPointF(17.5, 19.5), QPointF(11.0, 18.0));
    painter.drawLine(QPointF(15.0, 17.0), QPointF(14.0, 12.0));
    painter.drawLine(QPointF(28.0, 17.0), QPointF(34.0, 16.0));
    painter.drawLine(QPointF(30.5, 14.0), QPointF(33.0, 10.5));

    painter.setPen(QPen(QColor(45, 150, 75), 1.6,
                        Qt::SolidLine, Qt::RoundCap));
    const QLineF leaves[] = {
        {{9.0, 15.5}, {13.0, 17.0}}, {{12.5, 11.0}, {15.0, 14.0}},
        {{15.0, 14.5}, {19.0, 13.0}}, {{29.0, 12.0}, {33.0, 10.0}},
        {{32.0, 14.5}, {36.0, 13.0}}, {{31.0, 17.5}, {35.0, 19.0}}
    };
    for (const QLineF& leaf : leaves) {
        painter.drawLine(leaf);
    }

    painter.setPen(QPen(QColor(225, 35, 35), 2.0,
                        Qt::SolidLine, Qt::SquareCap));
    painter.drawLine(QPointF(11.0, 33.0), QPointF(11.0, 27.0));
    painter.drawLine(QPointF(8.0, 33.0), QPointF(14.0, 33.0));

    painter.end();
    return QIcon(pixmap);
}

QIcon SolidGeometryDisplayIcon(int style) {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QPointF top(22.0, 5.0);
    const QPointF left(7.0, 12.0);
    const QPointF right(37.0, 12.0);
    const QPointF front(22.0, 19.0);
    const QPointF left_bottom(7.0, 31.0);
    const QPointF right_bottom(37.0, 31.0);
    const QPointF front_bottom(22.0, 39.0);
    const QPointF back_bottom(22.0, 25.0);

    if (style == 2 || style == 3 || style == 5) {
        QPainterPath top_face;
        top_face.moveTo(top); top_face.lineTo(right); top_face.lineTo(front);
        top_face.lineTo(left); top_face.closeSubpath();
        QPainterPath left_face;
        left_face.moveTo(left); left_face.lineTo(front); left_face.lineTo(front_bottom);
        left_face.lineTo(left_bottom); left_face.closeSubpath();
        QPainterPath right_face;
        right_face.moveTo(front); right_face.lineTo(right); right_face.lineTo(right_bottom);
        right_face.lineTo(front_bottom); right_face.closeSubpath();
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(225, 228, 232)); painter.drawPath(top_face);
        painter.setBrush(QColor(202, 207, 214)); painter.drawPath(left_face);
        painter.setBrush(QColor(174, 181, 190)); painter.drawPath(right_face);
    }

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(45, 55, 190), 1.35,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(top, left); painter.drawLine(top, right);
    painter.drawLine(left, front); painter.drawLine(front, right);
    painter.drawLine(left, left_bottom); painter.drawLine(right, right_bottom);
    painter.drawLine(front, front_bottom);
    painter.drawLine(left_bottom, front_bottom);
    painter.drawLine(front_bottom, right_bottom);

    if (style == 0 || style == 4) {
        painter.setPen(QPen(QColor(45, 55, 190), 1.15, Qt::DashLine));
        painter.drawLine(top, back_bottom);
        painter.drawLine(back_bottom, left_bottom);
        painter.drawLine(back_bottom, right_bottom);
    } else if (style == 3 || style == 5) {
        painter.setPen(QPen(QColor(45, 55, 190), 0.9));
        for (int i = 1; i <= 3; ++i) {
            const qreal t = static_cast<qreal>(i) / 4.0;
            painter.drawLine(left + (front - left) * t,
                             left_bottom + (front_bottom - left_bottom) * t);
            painter.drawLine(front + (right - front) * t,
                             front_bottom + (right_bottom - front_bottom) * t);
            painter.drawLine(left + (left_bottom - left) * t,
                             front + (front_bottom - front) * t);
            painter.drawLine(front + (front_bottom - front) * t,
                             right + (right_bottom - right) * t);
        }
    }

    painter.end();
    return QIcon(pixmap);
}

QIcon SurfaceAppearanceIcon(int style) {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QPolygonF top_face({QPointF(22, 5), QPointF(38, 12),
                              QPointF(22, 20), QPointF(6, 12)});
    const QPolygonF left_face({QPointF(6, 12), QPointF(22, 20),
                               QPointF(22, 39), QPointF(6, 31)});
    const QPolygonF right_face({QPointF(22, 20), QPointF(38, 12),
                                QPointF(38, 31), QPointF(22, 39)});
    painter.setPen(QPen(QColor(55, 58, 62), 1.1));
    if (style == 1) {
        painter.setBrush(QColor(205, 207, 204)); painter.drawPolygon(top_face);
        painter.setBrush(QColor(175, 178, 175)); painter.drawPolygon(left_face);
        painter.setBrush(QColor(145, 148, 145)); painter.drawPolygon(right_face);
    } else if (style == 2) {
        painter.setBrush(QColor(25, 215, 55)); painter.drawPolygon(top_face);
        painter.setBrush(QColor(25, 75, 225)); painter.drawPolygon(left_face);
        painter.setBrush(QColor(235, 35, 35)); painter.drawPolygon(right_face);
    } else {
        painter.setBrush(QColor(115, 145, 175)); painter.drawPolygon(top_face);
        painter.setBrush(QColor(150, 85, 65)); painter.drawPolygon(left_face);
        painter.setBrush(QColor(75, 145, 80)); painter.drawPolygon(right_face);
        painter.setPen(QPen(QColor(235, 205, 155), 1.1));
        painter.drawLine(QPointF(9, 17), QPointF(20, 23));
        painter.drawLine(QPointF(8, 23), QPointF(20, 29));
        painter.drawLine(QPointF(25, 23), QPointF(36, 18));
        painter.drawLine(QPointF(25, 29), QPointF(36, 24));
        painter.setPen(QPen(QColor(80, 95, 125), 1.0));
        painter.drawEllipse(QRectF(18, 8, 7, 5));
    }

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

QIcon ZoomRectIcon() {
    QPixmap pixmap(22, 22);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(91, 105, 122), 1.2, Qt::DashLine));
    painter.setBrush(QColor(120, 92, 220, 35));
    painter.drawRect(QRectF(3.0, 3.0, 11.0, 10.0));
    painter.setPen(QPen(QColor(52, 58, 68), 2.0, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QRectF(8.0, 8.0, 8.0, 8.0));
    painter.drawLine(QPointF(14.3, 14.3), QPointF(19.0, 19.0));
    painter.end();
    return QIcon(pixmap);
}

QIcon WalkCameraIcon() {
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(48, 52, 57), 1.7, Qt::SolidLine,
                        Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor(214, 222, 228));
    painter.drawRoundedRect(QRectF(3.0, 5.5, 12.0, 10.0), 2.0, 2.0);
    QPainterPath lens;
    lens.moveTo(15.0, 8.0);
    lens.lineTo(21.0, 5.5);
    lens.lineTo(21.0, 15.5);
    lens.lineTo(15.0, 13.0);
    lens.closeSubpath();
    painter.setBrush(QColor(102, 166, 206));
    painter.drawPath(lens);
    painter.setPen(QPen(QColor(218, 63, 54), 1.8,
                        Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(6.0, 19.0), QPointF(10.0, 16.0));
    painter.drawLine(QPointF(10.0, 16.0), QPointF(14.0, 19.0));
    return QIcon(pixmap);
}

QIcon PointDimensionIcon() {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QColor cyan(0, 170, 195);
    painter.setPen(QPen(cyan, 2.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(7.0, 33.0), QPointF(37.0, 11.0));
    painter.drawLine(QPointF(7.0, 33.0), QPointF(14.0, 32.0));
    painter.drawLine(QPointF(7.0, 33.0), QPointF(9.5, 26.5));
    painter.drawLine(QPointF(37.0, 11.0), QPointF(30.0, 12.0));
    painter.drawLine(QPointF(37.0, 11.0), QPointF(34.5, 17.5));
    painter.setPen(QPen(QColor(28, 34, 40), 1.0));
    painter.setBrush(QColor(245, 248, 250));
    painter.drawRoundedRect(QRectF(14.0, 17.0, 17.0, 10.0), 2.0, 2.0);
    painter.setPen(QColor(20, 75, 90));
    QFont font = painter.font();
    font.setPixelSize(7);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(QRectF(14.0, 17.0, 17.0, 10.0), Qt::AlignCenter, "123");
    painter.end();
    return QIcon(pixmap);
}

QIcon SketchPolylineIcon() {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(20, 20, 20), 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPainterPath path;
    path.moveTo(8.0, 34.0);
    path.lineTo(8.0, 10.0);
    path.lineTo(23.0, 10.0);
    path.lineTo(28.0, 23.0);
    path.lineTo(36.0, 23.0);
    painter.drawPath(path);

    painter.setBrush(QColor(255, 62, 78));
    painter.setPen(QPen(QColor(155, 0, 16), 1.0));
    painter.drawEllipse(QPointF(8.0, 34.0), 3.5, 3.5);
    painter.drawEllipse(QPointF(23.0, 10.0), 3.5, 3.5);
    painter.drawEllipse(QPointF(36.0, 23.0), 3.5, 3.5);
    painter.end();
    return QIcon(pixmap);
}

QIcon GroupIcon() {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(45, 95, 220), 2.8, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(10.0, 10.0), QPointF(22.0, 33.0));
    painter.drawLine(QPointF(22.0, 9.0), QPointF(22.0, 33.0));
    painter.drawLine(QPointF(34.0, 10.0), QPointF(22.0, 33.0));

    painter.setPen(QPen(QColor(70, 30, 145), 1.3));
    painter.setBrush(QColor(155, 75, 235));
    painter.drawEllipse(QPointF(10.0, 10.0), 5.0, 5.0);
    painter.drawEllipse(QPointF(22.0, 9.0), 5.0, 5.0);
    painter.drawEllipse(QPointF(34.0, 10.0), 5.0, 5.0);

    painter.setPen(QPen(QColor(15, 75, 155), 1.3));
    painter.setBrush(QColor(55, 165, 245));
    painter.drawEllipse(QPointF(22.0, 33.0), 6.0, 6.0);
    painter.end();
    return QIcon(pixmap);
}

QIcon UngroupIcon() {
    QPixmap pixmap = GroupIcon().pixmap(44, 44);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(230, 35, 45), 4.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(7.0, 7.0), QPointF(37.0, 37.0));
    painter.drawLine(QPointF(37.0, 7.0), QPointF(7.0, 37.0));
    painter.end();
    return QIcon(pixmap);
}

QIcon SketchBezierIcon() {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(82, 150, 210), 1.4, Qt::DashLine));
    painter.drawLine(QPointF(7.0, 32.0), QPointF(15.0, 7.0));
    painter.drawLine(QPointF(15.0, 7.0), QPointF(30.0, 8.0));
    painter.drawLine(QPointF(30.0, 8.0), QPointF(37.0, 31.0));

    QPainterPath curve;
    curve.moveTo(7.0, 32.0);
    curve.cubicTo(15.0, 7.0, 30.0, 8.0, 37.0, 31.0);
    painter.setPen(QPen(QColor(20, 20, 20), 3.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawPath(curve);

    painter.setPen(QPen(QColor(145, 0, 18), 1.0));
    painter.setBrush(QColor(255, 62, 78));
    painter.drawEllipse(QPointF(7.0, 32.0), 3.5, 3.5);
    painter.drawEllipse(QPointF(37.0, 31.0), 3.5, 3.5);
    painter.setPen(QPen(QColor(0, 85, 150), 1.0));
    painter.setBrush(QColor(55, 175, 245));
    painter.drawEllipse(QPointF(15.0, 7.0), 3.2, 3.2);
    painter.drawEllipse(QPointF(30.0, 8.0), 3.2, 3.2);
    painter.end();
    return QIcon(pixmap);
}

QIcon SketchConvertBezierIcon() {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(25, 25, 25), 2.6, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(5.0, 12.0), QPointF(18.0, 12.0));
    painter.setPen(QPen(QColor(55, 120, 185), 2.0));
    painter.drawLine(QPointF(18.0, 22.0), QPointF(27.0, 22.0));
    painter.drawLine(QPointF(23.0, 18.0), QPointF(27.0, 22.0));
    painter.drawLine(QPointF(23.0, 26.0), QPointF(27.0, 22.0));

    QPainterPath curve;
    curve.moveTo(27.0, 34.0);
    curve.cubicTo(30.0, 12.0, 38.0, 12.0, 40.0, 34.0);
    painter.setPen(QPen(QColor(25, 25, 25), 2.6, Qt::SolidLine, Qt::RoundCap));
    painter.drawPath(curve);
    painter.setBrush(QColor(255, 62, 78));
    painter.setPen(QPen(QColor(145, 0, 18), 1.0));
    painter.drawEllipse(QPointF(27.0, 34.0), 3.0, 3.0);
    painter.drawEllipse(QPointF(40.0, 34.0), 3.0, 3.0);
    painter.end();
    return QIcon(pixmap);
}

QIcon SketchArcIcon() {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(20, 20, 20), 3.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(QRectF(7.0, 8.0, 30.0, 30.0), 15 * 16, 150 * 16);
    painter.setPen(QPen(QColor(145, 0, 18), 1.0));
    painter.setBrush(QColor(255, 62, 78));
    painter.drawEllipse(QPointF(8.0, 30.0), 3.5, 3.5);
    painter.drawEllipse(QPointF(35.0, 16.0), 3.5, 3.5);
    painter.end();
    return QIcon(pixmap);
}

QIcon SketchFilletIcon() {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(120, 125, 132), 1.5, Qt::DashLine));
    painter.drawLine(QPointF(8.0, 36.0), QPointF(8.0, 8.0));
    painter.drawLine(QPointF(8.0, 8.0), QPointF(36.0, 8.0));
    QPainterPath rounded_corner;
    rounded_corner.moveTo(8.0, 36.0);
    rounded_corner.lineTo(8.0, 20.0);
    rounded_corner.cubicTo(8.0, 12.0, 12.0, 8.0, 20.0, 8.0);
    rounded_corner.lineTo(36.0, 8.0);
    painter.setPen(QPen(QColor(20, 20, 20), 3.2, Qt::SolidLine, Qt::RoundCap));
    painter.drawPath(rounded_corner);
    painter.setPen(QPen(QColor(145, 0, 18), 1.0));
    painter.setBrush(QColor(255, 62, 78));
    painter.drawEllipse(QPointF(13.0, 13.0), 3.5, 3.5);
    painter.end();
    return QIcon(pixmap);
}

QIcon SketchGeometryConstraintIcon(int kind) {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QPen geometry_pen(QColor(25, 25, 25), 2.8,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(geometry_pen);
    if (kind == 0) {
        painter.drawLine(QPointF(7, 22), QPointF(37, 22));
    } else if (kind == 1) {
        painter.drawLine(QPointF(22, 7), QPointF(22, 37));
    } else {
        QPainterPath curve;
        if (kind == 2) {
            painter.drawLine(QPointF(5, 30), QPointF(20, 22));
            curve.moveTo(20, 22);
            curve.cubicTo(27, 18, 30, 8, 39, 7);
            painter.setPen(QPen(QColor(45, 170, 75), 1.3, Qt::DashLine));
            painter.drawLine(QPointF(7, 29), QPointF(29, 18));
        } else {
            curve.moveTo(5, 37);
            curve.cubicTo(8, 27, 16, 23, 22, 22);
            painter.drawLine(QPointF(22, 22), QPointF(22, 5));
            painter.setPen(QPen(QColor(45, 170, 75), 1.3, Qt::DashLine));
            painter.drawLine(QPointF(22, 36), QPointF(22, 8));
        }
        painter.setPen(geometry_pen);
        painter.drawPath(curve);
        painter.setPen(QPen(QColor(165, 0, 15), 1.0));
        painter.setBrush(QColor(255, 55, 65));
        painter.drawEllipse(QPointF(20 + (kind == 3 ? 2 : 0), 22), 3.2, 3.2);
    }
    painter.end();
    return QIcon(pixmap);
}

QIcon FrameSolidIcon() {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(25, 25, 25), 2.2, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    const QRectF outer(5.0, 6.0, 34.0, 32.0);
    const QRectF inner(12.0, 13.0, 20.0, 18.0);
    painter.drawRect(outer);
    painter.drawRect(inner);
    painter.drawLine(outer.topLeft(), inner.topLeft());
    painter.drawLine(outer.topRight(), inner.topRight());
    painter.drawLine(outer.bottomLeft(), inner.bottomLeft());
    painter.drawLine(outer.bottomRight(), inner.bottomRight());
    painter.end();
    return QIcon(pixmap);
}

QIcon BodyByTwoSketchesIcon() {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(Qt::NoBrush);

    const QPointF top_left(14.0, 8.0);
    const QPointF top_right(34.0, 8.0);
    const QPointF bottom_left(6.0, 36.0);
    const QPointF bottom_right(39.0, 37.0);

    painter.setPen(QPen(QColor(20, 20, 20), 2.2,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(top_left, bottom_left);
    painter.drawLine(top_right, bottom_right);

    painter.setPen(QPen(QColor(35, 35, 35), 1.5,
                        Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(19.0, 9.0), QPointF(18.0, 35.0));

    painter.setPen(QPen(QColor(240, 20, 30), 2.4,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPainterPath top_sketch;
    top_sketch.moveTo(top_left);
    top_sketch.cubicTo(19.0, 6.8, 28.0, 6.8, 34.0, 8.0);
    top_sketch.cubicTo(31.0, 11.0, 22.0, 12.0, 14.0, 11.2);
    top_sketch.closeSubpath();
    painter.drawPath(top_sketch);

    QPainterPath bottom_sketch;
    bottom_sketch.moveTo(bottom_left);
    bottom_sketch.cubicTo(15.0, 39.5, 30.0, 40.5, 39.0, 37.0);
    bottom_sketch.cubicTo(31.0, 34.5, 16.0, 34.0, 6.0, 36.0);
    bottom_sketch.closeSubpath();
    painter.drawPath(bottom_sketch);

    painter.end();
    return QIcon(pixmap);
}

QIcon ChairFurnitureIcon() {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QPen wood_pen(QColor(92, 48, 24), 3.2,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(wood_pen);
    painter.setBrush(QColor(176, 96, 45));
    painter.drawPolygon(QPolygonF()
        << QPointF(10.0, 23.0) << QPointF(29.0, 20.0)
        << QPointF(36.0, 24.0) << QPointF(17.0, 28.0));
    painter.drawLine(QPointF(11.0, 23.0), QPointF(9.0, 40.0));
    painter.drawLine(QPointF(17.0, 28.0), QPointF(16.0, 42.0));
    painter.drawLine(QPointF(35.0, 24.0), QPointF(34.0, 38.0));
    painter.drawLine(QPointF(29.0, 21.0), QPointF(31.0, 5.0));
    painter.drawLine(QPointF(10.0, 23.0), QPointF(9.0, 8.0));
    painter.drawLine(QPointF(9.0, 8.0), QPointF(31.0, 5.0));
    painter.setPen(QPen(QColor(113, 59, 27), 2.1,
                        Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(12.0, 12.0), QPointF(28.0, 10.0));
    painter.drawLine(QPointF(12.0, 17.0), QPointF(28.0, 15.0));
    painter.end();
    return QIcon(pixmap);
}

QIcon TableFurnitureIcon() {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(75, 45, 25), 1.5, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(QColor(184, 112, 58));
    QPolygonF top;
    top << QPointF(5.0, 13.0) << QPointF(27.0, 7.0)
        << QPointF(39.0, 14.0) << QPointF(16.0, 21.0);
    painter.drawPolygon(top);
    painter.setBrush(QColor(138, 78, 38));
    painter.drawPolygon(QPolygonF()
        << QPointF(5.0, 13.0) << QPointF(16.0, 21.0)
        << QPointF(39.0, 14.0) << QPointF(39.0, 18.0)
        << QPointF(16.0, 25.0) << QPointF(5.0, 17.0));
    painter.setPen(QPen(QColor(70, 42, 24), 3.4, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(10.0, 20.0), QPointF(10.0, 38.0));
    painter.drawLine(QPointF(17.0, 24.0), QPointF(17.0, 40.0));
    painter.drawLine(QPointF(34.0, 19.0), QPointF(34.0, 35.0));
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
                                                       CAlfaDoc& document,
                                                       size_t object_index,
                                                       const ToolRegistry& registry,
                                                       const std::function<void()>& name_changed,
                                                       const std::function<void(int, bool)>& dimensions_changed) {
    const auto current_solid = [&document, object_index]() -> CSolid* {
        if (object_index >= document.GetObjects().size()) {
            return nullptr;
        }
        return dynamic_cast<CSolid*>(document.GetObjects()[object_index].get());
    };
    CSolid* solid = current_solid();
    if (!solid) {
        return {};
    }

    const std::string original_name = solid->GetName();
    QDialog dialog(parent);
    dialog.setWindowTitle(
        dynamic_cast<CSurfaceSet*>(solid)
            ? "Surface Editor"
            : "Solid Editor");
    dialog.setModal(false);
    dialog.setWindowModality(Qt::NonModal);
    dialog.resize(210, 300);

    auto* root_layout = new QVBoxLayout(&dialog);
    root_layout->setContentsMargins(8, 8, 8, 8);
    root_layout->setSpacing(8);

    auto* undo_label = new QLabel("Operation history", &dialog);
    undo_label->setAlignment(Qt::AlignCenter);
    root_layout->addWidget(undo_label);

    auto* name_layout = new QHBoxLayout();
    auto* name_label = new QLabel("Name", &dialog);
    auto* name_edit = new QLineEdit(QString::fromStdString(solid->GetName()), &dialog);
    name_layout->addWidget(name_label);
    name_layout->addWidget(name_edit, 1);
    root_layout->addLayout(name_layout);

    auto* list = new QListWidget(&dialog);
    list->setMinimumHeight(150);
    for (int i = 0; i < solid->GetNumOperations(); ++i) {
        const ParametricFunction* operation = solid->GetOperation(i);
        if (!operation) {
            continue;
        }
        // Keep the stable internal tool id in project files, while allowing a
        // renamed tool to update its user-facing history label immediately.
        QString label = operation->ToolId == "SolidSketchFeature"
            ? QString::fromStdString(registry.LabelFor(operation->ToolId))
            : QString::fromStdString(operation->Name);
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

    auto* all_dimensions = new QCheckBox("All Dimensions", &dialog);
    all_dimensions->setToolTip(
        "Show editable dimensions for every supported operation");
    root_layout->addWidget(all_dimensions);

    auto* delete_button = new QPushButton("Delete", &dialog);
    root_layout->addWidget(delete_button);

    auto* buttons_layout = new QHBoxLayout();
    auto* ok_button = new QPushButton("OK", &dialog);
    auto* cancel_button = new QPushButton("Cancel", &dialog);
    buttons_layout->addWidget(ok_button);
    buttons_layout->addWidget(cancel_button);
    root_layout->addLayout(buttons_layout);

    SolidOperationsDialogResult result;
    QObject::connect(name_edit, &QLineEdit::textChanged, &dialog, [current_solid, name_changed](const QString& text) {
        if (CSolid* edited_solid = current_solid()) {
            edited_solid->SetName(text.toStdString());
        }
        if (name_changed) {
            name_changed();
        }
    });
    const auto update_delete_state = [current_solid, list, delete_button]() {
        const int index = list->currentItem()
            ? list->currentItem()->data(Qt::UserRole).toInt() : -1;
        const CSolid* edited_solid = current_solid();
        const ParametricFunction* operation =
            edited_solid && index >= 0 ? edited_solid->GetOperation(index) : nullptr;
        delete_button->setEnabled(
            index > 0 || (operation && operation->ToolId == "SurfaceFilmCoating"));
    };
    update_delete_state();

    const auto update_operation_highlight = [current_solid, list, all_dimensions,
                                             name_changed, dimensions_changed]() {
        CSolid* edited_solid = current_solid();
        const QListWidgetItem* item = list->currentItem();
        const ParametricFunction* operation =
            edited_solid && item
                ? edited_solid->GetOperation(item->data(Qt::UserRole).toInt())
                : nullptr;
        if (edited_solid) {
            edited_solid->SetOperationHighlightedSurfaces(
                operation ? operation->CreatedSurfaceIndices : std::vector<int>{});
        }
        if (name_changed) {
            name_changed();
        }
        if (dimensions_changed && item) {
            dimensions_changed(
                item->data(Qt::UserRole).toInt(),
                all_dimensions->isChecked());
        }
    };
    QObject::connect(list, &QListWidget::currentItemChanged, &dialog, [update_delete_state, update_operation_highlight](QListWidgetItem*, QListWidgetItem*) {
        update_delete_state();
        update_operation_highlight();
    });
    update_operation_highlight();
    QObject::connect(all_dimensions, &QCheckBox::toggled, &dialog,
                     [update_operation_highlight](bool) {
        update_operation_highlight();
    });
    QObject::connect(ok_button, &QPushButton::clicked, &dialog, [&dialog, list, name_edit, &result]() {
        result.action = SolidOperationsDialogAction::Accept;
        result.object_name = name_edit->text().trimmed().toStdString();
        dialog.accept();
    });
    QObject::connect(cancel_button, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(delete_button, &QPushButton::clicked, &dialog, [&dialog, current_solid, list, name_edit, &result]() {
        if (!list->currentItem()) {
            return;
        }
        const int operation_index = list->currentItem()->data(Qt::UserRole).toInt();
        const CSolid* edited_solid = current_solid();
        const ParametricFunction* operation =
            edited_solid ? edited_solid->GetOperation(operation_index) : nullptr;
        if (operation_index <= 0
            && (!operation || operation->ToolId != "SurfaceFilmCoating")) {
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

    QEventLoop dialog_loop;
    QObject::connect(&dialog, &QDialog::finished, &dialog_loop, &QEventLoop::quit);
    dialog.show();
    dialog.raise();
    dialog.activateWindow();
    dialog_loop.exec();
    const int dialog_result = dialog.result();

    if (CSolid* edited_solid = current_solid()) {
        edited_solid->ClearOperationHighlightedSurfaces();
    }
    if (name_changed) {
        name_changed();
    }
    if (dialog_result != QDialog::Accepted) {
        if (CSolid* edited_solid = current_solid()) {
            edited_solid->SetName(original_name);
        }
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

bool IsEditableCurve(const CAlfaObject* object) {
    return dynamic_cast<const CPolyline*>(object)
        || dynamic_cast<const CBSpline*>(object);
}

double CurvePointDistanceSquared(const CPoint3d& first, const CPoint3d& second) {
    const double dx = first.x - second.x;
    const double dy = first.y - second.y;
    const double dz = first.z - second.z;
    return dx * dx + dy * dy + dz * dz;
}

const std::vector<CPoint3d>* CurveControlPoints(const CAlfaObject* object) {
    if (const auto* polyline = dynamic_cast<const CPolyline*>(object)) {
        return &polyline->GetPoints();
    }
    if (const auto* spline = dynamic_cast<const CBSpline*>(object)) {
        return &spline->GetPoints();
    }
    return nullptr;
}

void CopyCurveAppearance(const CAlfaObject& source, CAlfaObject& target) {
    target.SetColor(source.GetColor());
    target.SetMaterial(source.GetMaterial());
    target.SetMaterialId(source.GetMaterialId());
    target.SetGroupName(source.GetGroupName());
    target.SetLineWidth(source.GetLineWidth());
    target.SetLineStyle(source.GetLineStyle());
    target.SetVisible(source.IsVisible());
    target.m_LayerID = source.m_LayerID;
    target.SetParametricDefinition(
        source.GetParametricToolId(), source.GetParametricParameters());
}

std::unique_ptr<CAlfaObject> MakeCurvePiece(
    const CAlfaObject& source,
    const std::vector<CPoint3d>& points,
    const std::vector<double>& weights,
    const std::string& suffix) {
    if (dynamic_cast<const CPolyline*>(&source)) {
        auto result = std::make_unique<CPolyline>(source.GetName() + suffix);
        for (const CPoint3d& point : points) result->AddPoint(point);
        CopyCurveAppearance(source, *result);
        return result;
    }
    const auto* source_spline = dynamic_cast<const CBSpline*>(&source);
    if (!source_spline) return {};
    auto result = std::make_unique<CBSpline>(source.GetName() + suffix);
    result->SetCurveType(source_spline->GetCurveType());
    result->SetDegree(std::min(
        source_spline->GetDegree(), std::max(1, static_cast<int>(points.size()) - 1)));
    for (const CPoint3d& point : points) result->AddPoint(point);
    result->SetWeights(weights);
    CopyCurveAppearance(source, *result);
    return result;
}

bool ReplaceCurveGeometry(CAlfaObject& target,
                          const std::vector<CPoint3d>& points,
                          const std::vector<double>& weights = {}) {
    if (auto* polyline = dynamic_cast<CPolyline*>(&target)) {
        polyline->Clear();
        for (const CPoint3d& point : points) polyline->AddPoint(point);
        return points.size() >= 2;
    }
    if (auto* spline = dynamic_cast<CBSpline*>(&target)) {
        spline->Clear();
        for (const CPoint3d& point : points) spline->AddPoint(point);
        spline->SetWeights(weights);
        spline->SetDegree(std::min(
            spline->GetDegree(), std::max(1, static_cast<int>(points.size()) - 1)));
        return points.size() >= 2;
    }
    return false;
}

struct CurveProjection {
    size_t segment = 0;
    CPoint3d point;
    double distance_squared = std::numeric_limits<double>::max();
};

CurveProjection ProjectToControlPolygon(const std::vector<CPoint3d>& points,
                                        CPoint3d query) {
    CurveProjection best;
    if (points.size() < 2) return best;
    for (size_t segment = 0; segment + 1 < points.size(); ++segment) {
        const CPoint3d& start = points[segment];
        const CPoint3d& end = points[segment + 1];
        const double dx = end.x - start.x;
        const double dy = end.y - start.y;
        const double dz = end.z - start.z;
        const double length_squared = dx * dx + dy * dy + dz * dz;
        const double t = length_squared <= 1.0e-18 ? 0.0 : std::clamp(
            ((query.x - start.x) * dx + (query.y - start.y) * dy
             + (query.z - start.z) * dz) / length_squared,
            0.0, 1.0);
        const CPoint3d projected(
            start.x + dx * t, start.y + dy * t, start.z + dz * t);
        const double distance_squared = CurvePointDistanceSquared(projected, query);
        if (distance_squared < best.distance_squared) {
            best = {segment, projected, distance_squared};
        }
    }
    return best;
}

std::vector<CPoint3d> CurveSamples(const CAlfaObject& object) {
    if (const auto* polyline = dynamic_cast<const CPolyline*>(&object)) {
        return polyline->GetRoundedPathPoints();
    }
    const auto* spline = dynamic_cast<const CBSpline*>(&object);
    if (!spline) return {};
    const int count = std::max(64, static_cast<int>(spline->GetPointCount()) * 32);
    std::vector<CPoint3d> samples;
    samples.reserve(static_cast<size_t>(count + 1));
    for (int i = 0; i <= count; ++i) {
        samples.push_back(spline->Evaluate(static_cast<float>(i) / count));
    }
    return samples;
}

struct SegmentPairProjection {
    CPoint3d first;
    CPoint3d second;
    double distance_squared = std::numeric_limits<double>::max();
};

SegmentPairProjection ClosestSegmentPair(CPoint3d p1, CPoint3d q1,
                                         CPoint3d p2, CPoint3d q2) {
    const Vec3 a{static_cast<float>(p1.x), static_cast<float>(p1.y), static_cast<float>(p1.z)};
    const Vec3 b{static_cast<float>(q1.x), static_cast<float>(q1.y), static_cast<float>(q1.z)};
    const Vec3 c{static_cast<float>(p2.x), static_cast<float>(p2.y), static_cast<float>(p2.z)};
    const Vec3 d{static_cast<float>(q2.x), static_cast<float>(q2.y), static_cast<float>(q2.z)};
    const Vec3 u = b - a;
    const Vec3 v = d - c;
    const Vec3 w = a - c;
    const double aa = dot(u, u);
    const double bb = dot(u, v);
    const double cc = dot(v, v);
    const double dd = dot(u, w);
    const double ee = dot(v, w);
    const double denominator = aa * cc - bb * bb;
    double s = denominator <= 1.0e-18 ? 0.0 : (bb * ee - cc * dd) / denominator;
    double t = denominator <= 1.0e-18
        ? (cc <= 1.0e-18 ? 0.0 : ee / cc)
        : (aa * ee - bb * dd) / denominator;
    s = std::clamp(s, 0.0, 1.0);
    t = std::clamp(t, 0.0, 1.0);
    // Reproject once after clamping to handle segment endpoints.
    if (aa > 1.0e-18) s = std::clamp((bb * t - dd) / aa, 0.0, 1.0);
    if (cc > 1.0e-18) t = std::clamp((bb * s + ee) / cc, 0.0, 1.0);
    const CPoint3d first(
        p1.x + (q1.x - p1.x) * s,
        p1.y + (q1.y - p1.y) * s,
        p1.z + (q1.z - p1.z) * s);
    const CPoint3d second(
        p2.x + (q2.x - p2.x) * t,
        p2.y + (q2.y - p2.y) * t,
        p2.z + (q2.z - p2.z) * t);
    return {first, second, CurvePointDistanceSquared(first, second)};
}

std::vector<CPoint3d> PlanePointsFromOriginNormal(CPoint3d origin, Vec3 normal) {
    normal = normalize(normal);
    if (dot(normal, normal) <= 1.0e-12f) return {};
    const Vec3 reference = std::abs(normal.z) < 0.9f
        ? Vec3{0.0f, 0.0f, 1.0f} : Vec3{0.0f, 1.0f, 0.0f};
    const Vec3 first_axis = normalize(cross(normal, reference));
    const Vec3 second_axis = normalize(cross(normal, first_axis));
    return {
        origin,
        CPoint3d(origin.x + first_axis.x, origin.y + first_axis.y, origin.z + first_axis.z),
        CPoint3d(origin.x + second_axis.x, origin.y + second_axis.y, origin.z + second_axis.z)};
}

std::array<double, 4> LoadRememberedPlaneFactors() {
    QSettings settings;
    return {
        settings.value("modeling/lastPlane/a", 0.0).toDouble(),
        settings.value("modeling/lastPlane/b", 0.0).toDouble(),
        settings.value("modeling/lastPlane/c", 1.0).toDouble(),
        settings.value("modeling/lastPlane/d", 0.0).toDouble()};
}

void SaveRememberedPlaneFactors(const std::array<double, 4>& factors) {
    QSettings settings;
    settings.setValue("modeling/lastPlane/a", factors[0]);
    settings.setValue("modeling/lastPlane/b", factors[1]);
    settings.setValue("modeling/lastPlane/c", factors[2]);
    settings.setValue("modeling/lastPlane/d", factors[3]);
}

std::array<double, 4> PlaneFactorsFromPoints(
    const CPoint3d& first, const CPoint3d& second, const CPoint3d& third) {
    Vec3 normal = normalize(cross(
        Vec3{static_cast<float>(second.x - first.x),
             static_cast<float>(second.y - first.y),
             static_cast<float>(second.z - first.z)},
        Vec3{static_cast<float>(third.x - first.x),
             static_cast<float>(third.y - first.y),
             static_cast<float>(third.z - first.z)}));
    return {normal.x, normal.y, normal.z,
            -(normal.x * first.x + normal.y * first.y + normal.z * first.z)};
}

double PlaneParameterValue(const std::vector<ToolParameter>& parameters,
                           const char* id, double fallback) {
    const auto found = std::find_if(parameters.begin(), parameters.end(),
        [id](const ToolParameter& parameter) { return parameter.id == id; });
    return found == parameters.end() ? fallback : found->value;
}

struct FilletRadiusValues {
    int mode = 0;
    double start = 2.0;
    double end = 2.0;
    std::vector<double> law;
};

FilletRadiusValues FilletRadii(const std::vector<ToolParameter>& parameters) {
    FilletRadiusValues values;
    values.mode = std::clamp(
        static_cast<int>(PlaneParameterValue(parameters, "radius_type", 0.0)),
        0, 1);
    const double constant = PlaneParameterValue(parameters, "radius", 2.0);
    values.start = values.mode == 1
        ? PlaneParameterValue(parameters, "radius_start", constant) : constant;
    values.end = values.mode == 1
        ? PlaneParameterValue(parameters, "radius_end", values.start) : constant;
    return values;
}

QString FilletPrimaryParameter(const std::vector<ToolParameter>& parameters) {
    const FilletRadiusValues radii = FilletRadii(parameters);
    if (radii.mode == 1) return QStringLiteral("radius_start");
    return QStringLiteral("radius");
}

std::vector<double> FilletLaw(
    const std::vector<ToolParameter>& parameters) {
    const FilletRadiusValues radii = FilletRadii(parameters);
    return std::vector<double>{radii.start, radii.end};
}

std::array<double, 4> PlaneFactorsFromParameters(
    const std::vector<ToolParameter>& parameters) {
    const int mode = static_cast<int>(PlaneParameterValue(parameters, "mode", 1.0));
    if (mode == 0) return {
        PlaneParameterValue(parameters, "a", 0.0),
        PlaneParameterValue(parameters, "b", 0.0),
        PlaneParameterValue(parameters, "c", 1.0),
        PlaneParameterValue(parameters, "d", 0.0)};
    if (mode == 1) {
        const double x = PlaneParameterValue(parameters, "plane.origin.x", 0.0);
        const double y = PlaneParameterValue(parameters, "plane.origin.y", 0.0);
        const double z = PlaneParameterValue(parameters, "plane.origin.z", 0.0);
        const double a = PlaneParameterValue(parameters, "plane.normal.x", 0.0);
        const double b = PlaneParameterValue(parameters, "plane.normal.y", 0.0);
        const double c = PlaneParameterValue(parameters, "plane.normal.z", 1.0);
        return {a, b, c, -(a * x + b * y + c * z)};
    }
    if (mode == 2) return PlaneFactorsFromPoints(
        CPoint3d(PlaneParameterValue(parameters, "p1.x", 0.0),
                 PlaneParameterValue(parameters, "p1.y", 0.0),
                 PlaneParameterValue(parameters, "p1.z", 0.0)),
        CPoint3d(PlaneParameterValue(parameters, "p2.x", 100.0),
                 PlaneParameterValue(parameters, "p2.y", 0.0),
                 PlaneParameterValue(parameters, "p2.z", 0.0)),
        CPoint3d(PlaneParameterValue(parameters, "p3.x", 0.0),
                 PlaneParameterValue(parameters, "p3.y", 100.0),
                 PlaneParameterValue(parameters, "p3.z", 0.0)));
    const double offset = PlaneParameterValue(parameters, "offset", 0.0);
    if (mode == 3) return {0.0, 0.0, 1.0, -offset};
    if (mode == 4) return {0.0, 1.0, 0.0, -offset};
    return {1.0, 0.0, 0.0, -offset};
}

// Shared plane-definition dialog. Reference Plane creation and curve trimming
// must use the same UI and the same result codes so the two workflows cannot
// drift into separate dialog implementations again.
int ShowPlaneDefinitionDialog(
    QWidget* parent, std::array<double, 4>& factors) {
    QDialog dialog(parent);
    dialog.setWindowTitle("Plane box");
    dialog.setModal(true);
    dialog.setFixedWidth(232);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(3);
    auto* factors_group = new QGroupBox("Factors  A, B, C, D", &dialog);
    auto* factors_layout = new QVBoxLayout(factors_group);
    factors_layout->setContentsMargins(4, 5, 4, 5);
    factors_layout->setSpacing(4);
    auto* factors_editor = new QLineEdit(factors_group);
    const QLocale number_locale = NumberInputLocale();
    factors_editor->setText(QString("%1 %2 %3 %4")
        .arg(number_locale.toString(factors[0], 'f', 6))
        .arg(number_locale.toString(factors[1], 'f', 6))
        .arg(number_locale.toString(factors[2], 'f', 6))
        .arg(number_locale.toString(factors[3], 'f', 6)));
    factors_editor->setToolTip("A B C D");
    factors_layout->addWidget(factors_editor);
    auto* factors_ok = new QPushButton("OK", factors_group);
    factors_layout->addWidget(factors_ok, 0, Qt::AlignHCenter);
    layout->addWidget(factors_group);
    QObject::connect(factors_ok, &QPushButton::clicked, &dialog,
                     [&dialog, &factors, factors_editor, number_locale]() {
        const QStringList values = factors_editor->text().split(
            QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (values.size() != 4) {
            factors_editor->setStyleSheet("QLineEdit { background: #ffd6d6; }");
            factors_editor->setToolTip("Enter four numbers: A B C D");
            factors_editor->setFocus();
            factors_editor->selectAll();
            return;
        }
        std::array<double, 4> parsed{};
        for (int index = 0; index < 4; ++index) {
            bool ok = false;
            parsed[static_cast<size_t>(index)] =
                number_locale.toDouble(values[index], &ok);
            if (!ok) {
                factors_editor->setStyleSheet("QLineEdit { background: #ffd6d6; }");
                factors_editor->setToolTip("Enter four numbers: A B C D");
                factors_editor->setFocus();
                factors_editor->selectAll();
                return;
            }
        }
        factors = parsed;
        dialog.done(5);
    });
    auto add_method = [&dialog, layout](const QString& text, int result) {
        auto* button = new QPushButton(text, &dialog);
        button->setMinimumWidth(210);
        layout->addWidget(button);
        QObject::connect(button, &QPushButton::clicked, &dialog,
                         [&dialog, result]() { dialog.done(result); });
    };
    add_method("Face of Solid", 7);
    add_method("Plane / Planar Sketch / Polyline", 8);
    add_method("3 Points", 1);
    add_method("Point + Normal", 6);
    add_method("Plane XY", 2);
    add_method("Plane XZ", 3);
    add_method("Plane YZ", 4);
    auto* cancel = new QPushButton("Cancel", &dialog);
    layout->addSpacing(7);
    layout->addWidget(cancel, 0, Qt::AlignHCenter);
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    CenterDialogOnCursor(dialog);
    return dialog.exec();
}

bool ShowPlaneValuesDialog(QWidget* parent, const QString& title,
                           const QStringList& labels,
                           std::vector<double>& values) {
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    auto* root = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();
    std::vector<QDoubleSpinBox*> editors;
    for (int i = 0; i < labels.size(); ++i) {
        auto* editor = new QDoubleSpinBox(&dialog);
        editor->setRange(-1000000.0, 1000000.0);
        editor->setDecimals(4);
        editor->setValue(i < static_cast<int>(values.size()) ? values[i] : 0.0);
        form->addRow(labels[i], editor);
        editors.push_back(editor);
    }
    root->addLayout(form);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    root->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    CenterDialogOnCursor(dialog);
    if (dialog.exec() != QDialog::Accepted) return false;
    values.clear();
    for (QDoubleSpinBox* editor : editors) values.push_back(editor->value());
    return true;
}
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      viewport_(new OpenGLViewport(this)),
      property_panel_(new PropertyPanel(this)),
      undo_redo_(document_) {
    const auto pump_startup_events = []() {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    };
    UpdateWindowTitle();
    resize(1280, 760);
    setAcceptDrops(true);

    viewport_->SetDocument(&document_);
    setCentralWidget(viewport_);

    LoadUserSettings();
    pump_startup_events();
    CreateActions();
    pump_startup_events();
    CreateDocks();
    pump_startup_events();
    HotkeyManagerDialog::InitializeActions(this);
    UpdateRecentFilesMenu();
    // Create the optional dock before restoreState(), otherwise Qt cannot
    // restore its floating/docked geometry from the previous session.
    ShowSketchPanel();
    sketch_dock_->hide();
    RestoreUserInterfaceSettings();
    statusBar()->showMessage("Ready");

    cursor_x_label_ = new QLabel("X=0.0", this);
    cursor_y_label_ = new QLabel("Y=0.0", this);
    cursor_z_label_ = new QLabel("Z=0.0", this);
    for (QLabel* label : {cursor_x_label_, cursor_y_label_, cursor_z_label_}) {
        label->setMinimumWidth(92);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        label->setFrameStyle(QFrame::Panel | QFrame::Sunken);
        statusBar()->addPermanentWidget(label);
    }
    connect(viewport_, &OpenGLViewport::CursorWorldPositionChanged,
            this, [this](double x, double y, double z, bool valid) {
        if (!cursor_x_label_ || !cursor_y_label_ || !cursor_z_label_) return;
        if (!valid) {
            cursor_x_label_->setText("X=--");
            cursor_y_label_->setText("Y=--");
            cursor_z_label_->setText("Z=--");
            return;
        }
        cursor_x_label_->setText(QString("X=%1").arg(x, 0, 'f', 1));
        cursor_y_label_->setText(QString("Y=%1").arg(y, 0, 'f', 1));
        cursor_z_label_->setText(QString("Z=%1").arg(z, 0, 'f', 1));
    });

    live_fillet_update_timer_ = new QTimer(this);
    live_fillet_update_timer_->setSingleShot(true);
    live_fillet_update_timer_->setInterval(280);
    connect(live_fillet_update_timer_, &QTimer::timeout,
            this, &MainWindow::StartPendingLiveFilletRebuild);

    furniture_animation_timer_ = new QTimer(this);
    furniture_animation_timer_->setInterval(30);
    connect(furniture_animation_timer_, &QTimer::timeout,
            this, &MainWindow::AdvanceFurnitureAnimation);

    connect(viewport_, &OpenGLViewport::DocumentChanged, this, [this]() {
        unsigned long moved_curve_id = 0;
        std::vector<CPoint3d> curve_points_before;
        std::vector<CPoint3d> curve_points_after;
        if (viewport_->TakeCurvePointDragChange(
                moved_curve_id, curve_points_before, curve_points_after)) {
            const auto apply_curve_points = [this, moved_curve_id](
                CAlfaDoc& document, const std::vector<CPoint3d>& points) {
                CAlfaObject* object = document.FindObjectById(moved_curve_id);
                if (auto* polyline = dynamic_cast<CPolyline*>(object)) {
                    if (polyline->GetPoints().size() != points.size()) return false;
                    polyline->GetPoints() = points;
                } else if (auto* spline = dynamic_cast<CBSpline*>(object)) {
                    if (spline->GetPoints().size() != points.size()) return false;
                    spline->GetPoints() = points;
                } else {
                    return false;
                }
                // Rebuild only objects that really reference this curve.
                tool_registry_.ReplayProfileDependents(
                    moved_curve_id, document);
                tool_registry_.ReplayAllTrimDependents(
                    document, moved_curve_id);
                document.RebuildAssociativeClones(moved_curve_id);
                return true;
            };
            // The live drag already contains the final coordinates. Only its
            // actual dependents need updating now; no document snapshot.
            tool_registry_.ReplayProfileDependents(
                moved_curve_id, document_);
            tool_registry_.ReplayAllTrimDependents(
                document_, moved_curve_id);
            document_.RebuildAssociativeClones(moved_curve_id);
            undo_redo_.RecordCommand(
                "Move curve nodes",
                [curve_points_before, apply_curve_points](CAlfaDoc& document) {
                    return apply_curve_points(document, curve_points_before);
                },
                [curve_points_after, apply_curve_points](CAlfaDoc& document) {
                    return apply_curve_points(document, curve_points_after);
                });
            UpdateUndoRedoActions();
            RefreshSceneTree();
            viewport_->update();
            return;
        }
        std::vector<unsigned long> moved_object_ids;
        Vec3 object_move_delta{};
        if (viewport_->TakeObjectMoveChange(
                moved_object_ids, object_move_delta)) {
            const auto apply_move = [](
                CAlfaDoc& document,
                const std::vector<unsigned long>& object_ids,
                Vec3 delta) {
                bool changed = false;
                for (unsigned long object_id : object_ids) {
                    CAlfaObject* object = document.FindObjectById(object_id);
                    if (auto* group = dynamic_cast<CGroup*>(object)) {
                        group->PreviewTranslate(delta);
                        changed = group->CommitTranslate(delta) || changed;
                    } else if (auto* solid = dynamic_cast<CSolid*>(object)) {
                        solid->PreviewTranslate(delta);
                        changed = solid->CommitPreviewTranslate(delta)
                            || changed;
                    } else if (object) {
                        object->Translate(delta);
                        changed = true;
                    }
                }
                return changed;
            };
            const Vec3 undo_delta{
                -object_move_delta.x,
                -object_move_delta.y,
                -object_move_delta.z};
            undo_redo_.RecordCommand(
                "Move objects",
                [moved_object_ids, undo_delta, apply_move](
                    CAlfaDoc& document) {
                    return apply_move(
                        document, moved_object_ids, undo_delta);
                },
                [moved_object_ids, object_move_delta, apply_move](
                    CAlfaDoc& document) {
                    return apply_move(
                        document, moved_object_ids, object_move_delta);
                });
            UpdateUndoRedoActions();
            RefreshSceneTree();
            viewport_->update();
            return;
        }
        unsigned long material_object_id = 0;
        Material material_before;
        unsigned long material_before_id = 0;
        Material material_after;
        unsigned long material_after_id = 0;
        if (viewport_->TakeMaterialDropChange(
                material_object_id,
                material_before,
                material_before_id,
                material_after,
                material_after_id)) {
            const auto apply_material = [material_object_id](
                CAlfaDoc& document,
                const Material& material,
                unsigned long material_id) {
                CAlfaObject* object = document.FindObjectById(
                    material_object_id);
                if (!object) return false;
                object->SetMaterial(material);
                object->SetMaterialId(material_id);
                return true;
            };
            undo_redo_.RecordCommand(
                "Apply material",
                [material_before, material_before_id, apply_material](
                    CAlfaDoc& document) {
                    return apply_material(
                        document, material_before, material_before_id);
                },
                [material_after, material_after_id, apply_material](
                    CAlfaDoc& document) {
                    return apply_material(
                        document, material_after, material_after_id);
                });
            UpdateUndoRedoActions();
            RefreshSceneTree();
            viewport_->update();
            return;
        }
        // Point transforms have their own Undo command.  Finish the current
        // NURBS parameter transaction first, then reopen the editor after the
        // viewport change has been recorded.
        const bool reopen_nurbs_editor =
            active_parametric_object_.tool_id == "NurbsParameters";
        if (reopen_nurbs_editor) {
            AcceptNurbsParameterChanges();
        }
        // A live fillet is only a preview until the property dialog is
        // accepted. Replaying profile dependents here rebuilds a Boss-created
        // solid from its committed operation tree and discards that preview
        // as soon as a dimension grip is released.
        const bool live_fillet_preview = !active_parametric_edit_existing_
            && (active_parametric_object_.tool_id == "fillet_edge"
                || active_parametric_object_.tool_id == "fillet_all_edges")
            && document_.HasLiveFillet();
        if (live_fillet_preview) {
            RefreshSceneTree();
            viewport_->update();
            return;
        }

        bool selected_sketch_found = false;
        for (size_t object_index : document_.GetSelectedObjectIndices()) {
            if (object_index >= document_.GetObjects().size()) {
                continue;
            }
            const auto* sketch = dynamic_cast<const CSmartLine*>(
                document_.GetObjects()[object_index].get());
            if (!sketch) {
                continue;
            }
            selected_sketch_found = true;
            tool_registry_.ReplayProfileDependents(sketch->m_id, document_);
        }
        if (!selected_sketch_found) {
            tool_registry_.ReplayAllProfileDependents(document_);
        }
        tool_registry_.ReplayAllTrimDependents(document_);
        document_.RebuildAssociativeClones();
        RefreshSceneTree();
        viewport_->update();
        RecordDocumentChange("Viewport edit");
        if (reopen_nurbs_editor) {
            QTimer::singleShot(0, this, [this]() {
                UpdateNurbsParameterEditor();
            });
        }
    });
    connect(viewport_, &OpenGLViewport::SelectionChanged, this, [this]() {
        if (pending_group_command_ == PendingGroupCommand::PlaneIntersection
            && pending_body_section_target_id_ == 0) {
            const CSolid* solid = document_.GetSelectedSolid();
            if (solid) {
                pending_body_section_target_id_ = solid->m_id;
                document_.ClearSelection();
                RefreshSceneTree();
                QTimer::singleShot(0, this, [this]() {
                    BeginBodySectionPlaneInput();
                });
                return;
            }
        }
        if (pending_body_section_plane_object_pick_) {
            if (CompleteBodySectionPlaneObjectPick()) return;
        }
        if (pending_group_command_ == PendingGroupCommand::ChangeLayer
            && document_.HasSelection()) {
            pending_group_command_ = PendingGroupCommand::None;
            viewport_->SetSelectionConfirmationMode(false);
            RefreshSceneTree();
            UpdateToolAvailability();
            QTimer::singleShot(0, this, [this]() {
                ChangeSelectedObjectLayer();
            });
            return;
        }
        if (pending_reference_plane_face_pick_ || pending_trim_plane_face_pick_
            || pending_body_section_plane_face_pick_) {
            if (CompletePendingPlaneFacePick()) return;
        }
        if (!pending_trim_tool_id_.empty()) {
            RefreshSceneTree();
            UpdateToolAvailability();
            QTimer::singleShot(0, this, [this]() {
                TryApplyPendingTrim();
            });
            return;
        }
        if (pending_precise_transform_ != PendingPreciseTransform::None
            && document_.HasSelection()) {
            const PendingPreciseTransform command = pending_precise_transform_;
            pending_precise_transform_ = PendingPreciseTransform::None;
            RefreshSceneTree();
            UpdateToolAvailability();
            QTimer::singleShot(0, this, [this, command]() {
                if (command == PendingPreciseTransform::Move) {
                    ShowPreciseMoveDialog();
                } else if (command == PendingPreciseTransform::Rotate) {
                    ShowPreciseRotateDialog();
                } else if (command == PendingPreciseTransform::Scale) {
                    ShowPreciseScaleDialog();
                }
            });
            return;
        }
        if (!active_parametric_edit_existing_
            && IsFurnitureAssemblyTool(active_parametric_object_.tool_id)) {
            document_.ClearSelection();
            RefreshSceneTree();
            UpdateToolAvailability();
            return;
        }
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
        // Selection alone must never open an editor. NURBS parameters are
        // shown only by the explicit tool in the Curves panel.
        const bool nurbs_parameter_editor =
            active_parametric_object_.tool_id == "NurbsParameters";
        const bool active_edge_tool = active_parametric_object_.tool_id == "fillet_edge"
            || active_parametric_object_.tool_id == "ChamferSolid";
        if (active_edge_tool) {
            TryStartLiveEdgeToolFromSelection();
        } else if (active_parametric_object_.tool_id == "SolidExtrudeTool") {
            TryStartLivePolylineExtrudeFromSelection();
        } else if (active_parametric_object_.tool_id == "SurfaceOfRevolution") {
            TryStartLivePolylineRevolveFromSelection();
        } else if (active_parametric_object_.tool_id == "SurfaceRuled"
                   || active_parametric_object_.tool_id == "SolidShell") {
            // The generated surface/solid is the live preview. Keep its
            // parameter panel open while the user inspects the result.
        } else if (active_parametric_object_.tool_id != "ThickSolidTool"
            && active_parametric_object_.tool_id != "TrimByPlane"
            && active_parametric_object_.tool_id != "TrimBySketch"
            && active_parametric_object_.tool_id != "TrimBySurface"
            && !nurbs_parameter_editor) {
            ClearActiveProperties();
        }
        RefreshSceneTree();
        UpdateToolAvailability();
        const auto* reference = dynamic_cast<const CReferenceImage*>(
            document_.GetSelectedObject());
        const int opacity_percent = static_cast<int>(std::round(
            (reference ? reference->GetMaterial().alpha
                       : CMesh3D::GetSurfaceOpacity()) * 100.0f));
        if (mesh_opacity_slider_
            && mesh_opacity_slider_->value() != opacity_percent) {
            const QSignalBlocker blocker(mesh_opacity_slider_);
            mesh_opacity_slider_->setValue(opacity_percent);
        }
        if (mesh_opacity_value_label_) {
            mesh_opacity_value_label_->setText(
                QString("%1%").arg(opacity_percent));
        }
        if (pending_group_command_ == PendingGroupCommand::Create) {
            const size_t count = document_.GetSelectedObjectCount();
            statusBar()->showMessage(count >= 2
                ? QString("Create Group: selected %1 object(s). Press Enter to create.").arg(count)
                : "Create Group: select at least two objects, then press Enter");
        } else if (pending_group_command_ == PendingGroupCommand::Ungroup) {
            statusBar()->showMessage(HasSelectedGroup()
                ? "UnGroup: group selected. Press Enter to ungroup."
                : "UnGroup: select a group, then press Enter");
        } else if (pending_group_command_ == PendingGroupCommand::CreateAssembly) {
            statusBar()->showMessage("Create Assembly: select at least two objects, then press Enter");
        } else if (pending_group_command_ == PendingGroupCommand::TwoSketchBody) {
            statusBar()->showMessage("Body by Two Sketches: select exactly two closed sketches, then press Enter");
        } else if (pending_group_command_ == PendingGroupCommand::AssociativeClone) {
            statusBar()->showMessage("Associative Clone: select one solid, then press Enter");
        } else if (pending_group_command_ == PendingGroupCommand::JoinSurfaces) {
            statusBar()->showMessage(
                "Join Surfaces:select the required geometry and continue");
        } else if (pending_group_command_ == PendingGroupCommand::PlaneIntersection) {
            statusBar()->showMessage(
                pending_body_section_target_id_ == 0
                    ? "Body Section by Plane: click the Solid body"
                    : "Body Section by Plane: select the plane geometry");
        } else if (pending_group_command_ == PendingGroupCommand::SurfaceIntersection) {
            statusBar()->showMessage(
                "Surface Intersection:select the required geometry and continue");
        } else if (pending_group_command_ == PendingGroupCommand::ProjectCurveToSurface) {
            statusBar()->showMessage(
                "Project Curve:select the required geometry and continue");
        } else if (pending_group_command_ == PendingGroupCommand::ExtractSurfaceEdge) {
            statusBar()->showMessage(
                "Extract Edge:select the required geometry and continue");
        } else if (pending_group_command_ == PendingGroupCommand::FourSplineSurface) {
            statusBar()->showMessage("Surface by 4 Splines:select the required geometry and continue");
        } else if (pending_group_command_ == PendingGroupCommand::TwoRailSweepSurface) {
            statusBar()->showMessage("Sweep Surface (2 Rails):select the required geometry and continue");
        } else if (pending_group_command_ == PendingGroupCommand::TwoRailSweepSolid) {
            statusBar()->showMessage("Sweep Solid (2 Rails):select the required geometry and continue");
        } else if (pending_group_command_ == PendingGroupCommand::ChangeLayer) {
            statusBar()->showMessage(
                "Change Layer: pick an object or select objects by rectangle");
        }
    });
    connect(viewport_, &OpenGLViewport::Point3DPicked, this, [this](CPoint3d point) {
        if (pending_body_section_three_point_pick_) {
            AppendBodySectionThreePointPick(point);
            return;
        }
        if (plane_three_point_pick_active_) {
            AppendPlaneThreePointPick(point);
            return;
        }
        if (pending_curve_edit_command_ != CurveEditCommand::None) {
            CompleteCurveEditPoint(point);
            return;
        }
        if (spatial_curve_kind_ != SpatialCurveKind::None) {
            AppendSpatialCurvePoint(point);
            return;
        }
        const Vec3 value{static_cast<float>(point.x),
                         static_cast<float>(point.y),
                         static_cast<float>(point.z)};
        if (pending_transform_point_pick_ == PendingTransformPointPick::ScaleBasePoint) {
            precise_scale_base_point_ = value;
            precise_scale_base_point_ready_ = true;
            pending_transform_point_pick_ = PendingTransformPointPick::None;
            QTimer::singleShot(0, this, [this]() { ShowPreciseScaleDialog(); });
        } else if (pending_transform_point_pick_ == PendingTransformPointPick::RotationPivot) {
            pending_transform_point_pick_ = PendingTransformPointPick::None;
            viewport_->SetRotationPivot(point);
            statusBar()->showMessage(
                QString("Pivot of Rotation: X %1, Y %2, Z %3")
                    .arg(point.x, 0, 'f', 3)
                    .arg(point.y, 0, 'f', 3)
                    .arg(point.z, 0, 'f', 3),
                2200);
        }
    });
    connect(viewport_, &OpenGLViewport::ArchitectureWallPicked,
            this, &MainWindow::CompleteArchitectureOpeningPlacement);
    connect(viewport_, &OpenGLViewport::ArchitectureWallPickCanceled,
            this, &MainWindow::CancelArchitectureOpeningPlacement);
    connect(viewport_, &OpenGLViewport::XYPointPicked, this, [this](CPoint3d point) {
        if (drawing_text_placement_pending_) {
            CompleteDrawingTextPlacement(point);
        }
    });
    connect(viewport_, &OpenGLViewport::XYPointPickCanceled, this, [this]() {
        if (!drawing_text_placement_pending_) return;
        drawing_text_placement_pending_ = false;
        UpdateActiveToolUi("select");
        statusBar()->showMessage("Text creation canceled", 1400);
    });
    connect(viewport_, &OpenGLViewport::Point3DPickFinished,
            this, &MainWindow::FinishSpatialCurve);
    connect(viewport_, &OpenGLViewport::Point3DPickCloseRequested,
            this, &MainWindow::CloseSpatialCurve);
    connect(viewport_, &OpenGLViewport::RotationAxisPicked,
            this, [this](CPoint3d start, CPoint3d end) {
        precise_rotate_axis_start_ = {
            static_cast<float>(start.x),
            static_cast<float>(start.y),
            static_cast<float>(start.z)};
        precise_rotate_axis_end_ = {
            static_cast<float>(end.x),
            static_cast<float>(end.y),
            static_cast<float>(end.z)};
        precise_rotate_axis_ready_ = true;
        QTimer::singleShot(0, this, [this]() { ShowPreciseRotateDialog(); });
    });
    connect(viewport_, &OpenGLViewport::RotationAxisPickCanceled,
            this, [this]() {
        precise_rotate_axis_ready_ = false;
        statusBar()->showMessage("Rotate canceled", 1400);
    });
    connect(viewport_, &OpenGLViewport::Point3DPickCanceled, this, [this]() {
        if (pending_body_section_three_point_pick_) {
            CancelBodySectionThreePointPick(
                "Body Section by Plane: operation canceled");
            return;
        }
        if (plane_three_point_pick_active_) {
            CancelPlaneThreePointPick("Plane:operation canceled");
            return;
        }
        if (pending_curve_edit_command_ != CurveEditCommand::None) {
            CancelCurveEditCommand("Curve edit canceled");
            return;
        }
        if (spatial_curve_kind_ != SpatialCurveKind::None) {
            CancelSpatialCurve();
            return;
        }
        pending_transform_point_pick_ = PendingTransformPointPick::None;
        precise_rotate_axis_ready_ = false;
        precise_scale_base_point_ready_ = false;
        statusBar()->showMessage("Point 3D canceled", 1400);
    });
    connect(viewport_, &OpenGLViewport::SelectionConfirmed, this, [this]() {
        if (pending_curve_edit_command_ != CurveEditCommand::None) {
            PrepareCurveEditCommandSelection();
        } else if (pending_group_command_ == PendingGroupCommand::Create) {
            CreateSelectedGroup();
        } else if (pending_group_command_ == PendingGroupCommand::Ungroup) {
            UngroupSelectedGroup();
        } else if (pending_group_command_ == PendingGroupCommand::CreateAssembly) {
            CreateSelectedAssembly();
        } else if (pending_group_command_ == PendingGroupCommand::TwoSketchBody) {
            CreateBodyFromTwoSketches();
        } else if (pending_group_command_ == PendingGroupCommand::AssociativeClone) {
            CreateAssociativeClone();
        } else if (pending_group_command_ == PendingGroupCommand::JoinSurfaces) {
            JoinSelectedSurfaces();
        } else if (pending_group_command_ == PendingGroupCommand::PlaneIntersection) {
            if (pending_body_section_target_id_ == 0) CreatePlaneIntersection();
            else if (pending_body_section_plane_object_pick_) {
                CompleteBodySectionPlaneObjectPick();
            }
        } else if (pending_group_command_ == PendingGroupCommand::SurfaceIntersection) {
            CreateSurfaceIntersection();
        } else if (pending_group_command_ == PendingGroupCommand::ProjectCurveToSurface) {
            ProjectCurveToSurface();
        } else if (pending_group_command_ == PendingGroupCommand::ExtractSurfaceEdge) {
            ExtractSurfaceEdge();
        } else if (pending_group_command_ == PendingGroupCommand::FourSplineSurface) {
            CreateFourSplineSurface();
        } else if (pending_group_command_ == PendingGroupCommand::TwoRailSweepSurface) {
            CreateTwoRailSweepSurface();
        } else if (pending_group_command_ == PendingGroupCommand::TwoRailSweepSolid) {
            CreateTwoRailSweepSolid();
        } else if (pending_group_command_ == PendingGroupCommand::ChangeLayer) {
            ChangeSelectedObjectLayer();
        }
    });
    connect(viewport_, &OpenGLViewport::SelectionCommandCanceled, this, [this]() {
        if (pending_curve_edit_command_ != CurveEditCommand::None) {
            CancelCurveEditCommand("Curve edit canceled");
        } else if (!pending_trim_tool_id_.empty()) {
            CancelPendingTrim("Trim command canceled");
        } else if (pending_group_command_ == PendingGroupCommand::JoinSurfaces) {
            CancelPendingGroupCommand("Join Surfaces:operation canceled");
        } else if (pending_group_command_ == PendingGroupCommand::PlaneIntersection) {
            pending_body_section_target_id_ = 0;
            pending_body_section_plane_face_pick_ = false;
            pending_body_section_plane_object_pick_ = false;
            pending_body_section_three_point_pick_ = false;
            pending_body_section_plane_points_.clear();
            viewport_->ClearPointPickMarkers();
            CancelPendingGroupCommand("Body Section by Plane: operation canceled");
        } else if (pending_group_command_ == PendingGroupCommand::SurfaceIntersection) {
            CancelPendingGroupCommand("Surface Intersection:operation canceled");
        } else if (pending_group_command_ == PendingGroupCommand::ProjectCurveToSurface) {
            CancelPendingGroupCommand("Project Curve:operation canceled");
        } else if (pending_group_command_ == PendingGroupCommand::ExtractSurfaceEdge) {
            CancelPendingGroupCommand("Extract Edge:operation canceled");
        } else if (pending_group_command_ == PendingGroupCommand::FourSplineSurface) {
            CancelPendingGroupCommand("Surface by 4 Splines:operation canceled");
        } else if (pending_group_command_ == PendingGroupCommand::TwoRailSweepSurface) {
            CancelPendingGroupCommand("Sweep Surface (2 Rails):operation canceled");
        } else if (pending_group_command_ == PendingGroupCommand::TwoRailSweepSolid) {
            CancelPendingGroupCommand("Sweep Solid (2 Rails):operation canceled");
        } else if (pending_group_command_ == PendingGroupCommand::ChangeLayer) {
            CancelPendingGroupCommand("Change Layer: operation canceled");
        } else {
            CancelPendingGroupCommand("Group command canceled");
        }
    });
    connect(viewport_, &OpenGLViewport::ObjectDoubleClicked, this, [this]() {
        if (auto* text = dynamic_cast<CDrawingText*>(document_.GetSelectedObject())) {
            EditDrawingText(*text);
            return;
        }
        EditSelectedParametricObject();
    });
    connect(viewport_, &OpenGLViewport::FurnitureInteractionRequested,
            this, &MainWindow::StartFurnitureInteraction);
    connect(viewport_, &OpenGLViewport::SolidDimensionEditRequested,
            this,
            [this](int operation_index, const QString& parameter_id, double current_value) {
        ActiveParametricObject solid_dimension_object;
        ActiveParametricObject* dimension_object = nullptr;
        if (solid_body_edit_mode_) {
            if (solid_body_edit_object_index_ < document_.GetObjects().size()) {
                auto* solid = dynamic_cast<CSolid*>(
                    document_.GetObjects()[solid_body_edit_object_index_].get());
                if (solid && operation_index >= 0
                    && operation_index < solid->GetNumOperations()) {
                    solid_dimension_object = tool_registry_.ActiveObjectFromDocument(
                        solid_body_edit_object_index_, *solid,
                        static_cast<size_t>(operation_index), &document_);
                    dimension_object = &solid_dimension_object;
                }
            }
        } else if (active_parametric_edit_existing_) {
            dimension_object = &active_parametric_object_;
        } else if (!active_parametric_object_.tool_id.empty()) {
            dimension_object = &active_parametric_object_;
        }
        if (!dimension_object
            || (dimension_object->tool_id != "SolidBox"
                && dimension_object->tool_id != "SolidCylinder"
                && dimension_object->tool_id != "SolidPrismTool"
                && dimension_object->tool_id != "fillet_edge"
                && dimension_object->tool_id != "fillet_all_edges"
                && !IsCabinetTool(dimension_object->tool_id))) {
            return;
        }
        const auto parameter = std::find_if(
            dimension_object->parameters.begin(),
            dimension_object->parameters.end(),
            [&parameter_id](const ToolParameter& candidate) {
                return QString::fromStdString(candidate.id) == parameter_id;
            });
        if (parameter == dimension_object->parameters.end()) {
            return;
        }

        const DisplayLengthUnit unit = LoadDisplayLengthUnit();
        const double factor = MillimetersToDisplay(1.0, unit);
        bool accepted = false;
        const double display_value = QInputDialog::getDouble(
            this,
            IsCabinetTool(dimension_object->tool_id)
                ? "Edit Cabinet Dimension"
                : "Edit Solid Dimension",
            QString("%1 (%2)").arg(
                QString::fromStdString(parameter->label),
                DisplayLengthUnitSuffix(unit)),
            MillimetersToDisplay(current_value, unit),
            parameter->minimum * factor,
            parameter->maximum * factor,
            3,
            &accepted,
            Qt::WindowFlags(),
            parameter->step * factor);
        if (!accepted) {
            return;
        }

        parameter->value = DisplayToMillimeters(display_value, unit);
        if (solid_body_edit_mode_) {
            solid_body_dimensions_modified_ = true;
        }
        if (!solid_body_edit_mode_) {
            property_panel_->SetActiveObject(*dimension_object);
        }
        if (IsCabinetTool(dimension_object->tool_id)) {
            active_cabinet_parameters_dirty_ = true;
            viewport_->SetSolidDimensionEdit(*dimension_object, parameter_id);
            viewport_->SetCabinetPreviewVisible(true);
            viewport_->update();
            statusBar()->showMessage(
                "Cabinet dimension changed. Press Apply to rebuild.", 2200);
            return;
        } else if ((dimension_object->tool_id == "fillet_edge"
             || dimension_object->tool_id == "fillet_all_edges")
            && !active_parametric_edit_existing_
            && document_.HasLiveFillet()) {
            ScheduleLiveFilletRebuild(true);
        } else {
            tool_registry_.Rebuild(*dimension_object, document_);
        }
        if (solid_body_edit_mode_) {
            UpdateSolidBodyDimensions();
        } else {
            viewport_->SetSolidDimensionEdit(
                *dimension_object,
                !active_parametric_edit_existing_
                        && (dimension_object->tool_id == "SolidBox"
                            || dimension_object->tool_id == "SolidCylinder")
                    ? QString::fromLatin1(
                        dimension_object->tool_id == "SolidBox" ? "depth" : "height")
                    : QString());
        }
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage(
            QString("%1: %2 = %3%4")
                .arg(QString::fromStdString(
                    tool_registry_.LabelFor(dimension_object->tool_id)))
                .arg(QString::fromStdString(parameter->label))
                .arg(display_value, 0, 'f', 3)
                 .arg(DisplayLengthUnitSuffix(unit)),
             1400);
    });
    connect(viewport_, &OpenGLViewport::SolidDimensionGripChanged,
            this,
            [this](int operation_index, const QString& parameter_id,
                   double requested_value, bool finished) {
        const auto supports_dimension_grips = [](const std::string& tool_id) {
            return tool_id == "SolidBox"
                || tool_id == "SolidCylinder"
                || tool_id == "SolidPrismTool"
                || tool_id == "fillet_edge"
                || tool_id == "fillet_all_edges"
                || IsCabinetTool(tool_id);
        };
        ActiveParametricObject solid_dimension_object;
        ActiveParametricObject* dimension_object = nullptr;
        if (solid_body_edit_mode_
            && solid_body_edit_object_index_ < document_.GetObjects().size()) {
            auto* solid = dynamic_cast<CSolid*>(
                document_.GetObjects()[solid_body_edit_object_index_].get());
            if (solid && operation_index >= 0
                && operation_index < solid->GetNumOperations()) {
                solid_dimension_object = tool_registry_.ActiveObjectFromDocument(
                    solid_body_edit_object_index_, *solid,
                    static_cast<size_t>(operation_index), &document_);
                if (supports_dimension_grips(solid_dimension_object.tool_id)) {
                    dimension_object = &solid_dimension_object;
                }
            }
        } else if (supports_dimension_grips(active_parametric_object_.tool_id)) {
            dimension_object = &active_parametric_object_;
        }
        if (!dimension_object) {
            return;
        }

        const auto parameter = std::find_if(
            dimension_object->parameters.begin(),
            dimension_object->parameters.end(),
            [&parameter_id](const ToolParameter& candidate) {
                return QString::fromStdString(candidate.id) == parameter_id;
            });
        if (parameter == dimension_object->parameters.end()) {
            return;
        }

        double value = std::clamp(
            requested_value, parameter->minimum, parameter->maximum);
        if (parameter->id == "depth" && std::abs(value) < 0.01) {
            value = value < 0.0 ? -0.01 : 0.01;
        }
        const bool changed = std::abs(parameter->value - value) > 1.0e-8;
        if (changed) {
            parameter->value = value;
            if (solid_body_edit_mode_) {
                solid_body_dimensions_modified_ = true;
            } else if (dimension_object == &active_parametric_object_) {
                property_panel_->UpdateParameterValue(parameter->id, value);
            }
            if (IsCabinetTool(dimension_object->tool_id)) {
                active_cabinet_parameters_dirty_ = true;
                // Keep the drag responsive; the expensive assembly rebuild is
                // performed explicitly by Apply or OK.
            } else if ((dimension_object->tool_id == "fillet_edge"
                 || dimension_object->tool_id == "fillet_all_edges")
                && !active_parametric_edit_existing_
                && document_.HasLiveFillet()) {
                ScheduleLiveFilletRebuild(finished);
            } else {
                tool_registry_.Rebuild(*dimension_object, document_);
            }
        }

        if (solid_body_edit_mode_) {
            UpdateSolidBodyDimensions();
        } else {
            viewport_->SetSolidDimensionEdit(*dimension_object, parameter_id);
            if (IsCabinetTool(dimension_object->tool_id)) {
                viewport_->SetCabinetPreviewVisible(true);
            }
        }
        if (finished) {
            RefreshSceneTree();
            const DisplayLengthUnit unit = LoadDisplayLengthUnit();
            statusBar()->showMessage(
                IsCabinetTool(dimension_object->tool_id)
                    ? QString("Cabinet: %1 = %2%3. Press Apply to rebuild.")
                        .arg(QString::fromStdString(parameter->label))
                        .arg(MillimetersToDisplay(value, unit), 0, 'f', 3)
                        .arg(DisplayLengthUnitSuffix(unit))
                    : QString("%1: %2 = %3%4")
                    .arg(QString::fromStdString(
                        tool_registry_.LabelFor(dimension_object->tool_id)))
                    .arg(QString::fromStdString(parameter->label))
                    .arg(MillimetersToDisplay(value, unit), 0, 'f', 3)
                    .arg(DisplayLengthUnitSuffix(unit)),
                1400);
        }
        viewport_->update();
    });
    connect(viewport_, &OpenGLViewport::EdgeQuickMenuRequested,
            this,
            [this](const QPoint& global_position) {
        if (!active_parametric_object_.tool_id.empty()
            || !document_.HasSelectedSolidEdge()) {
            return;
        }
        QMenu menu(this);
        QAction* fillet = menu.addAction(ToolIcon("fillet_edge"), "Solid Fillet");
        QAction* chamfer = menu.addAction(ToolIcon("ChamferSolid"), "Solid Chamfer");
        QAction* selected = menu.exec(global_position);
        if (selected == fillet) {
            ActivateParametricTool("fillet_edge");
        } else if (selected == chamfer) {
            ActivateParametricTool("ChamferSolid");
        }
    });
    connect(viewport_, &OpenGLViewport::FaceQuickMenuRequested,
            this,
            [this](const QPoint& global_position) {
        if (!active_parametric_object_.tool_id.empty()
            || !document_.HasSelectedSolidFace()) {
            return;
        }

        QMenu menu(this);
        QAction* fillet = menu.addAction(ToolIcon("fillet_edge"), "Fillet");
        QAction* chamfer = menu.addAction(ToolIcon("ChamferSolid"), "Chamfer");
        menu.addSeparator();
            QAction* extrude = menu.addAction(ToolIcon("SolidExtrudeFace"), "Extrude");
            QAction* offset = menu.addAction(ToolIcon("SolidOffsetFace"), "Offset");
            QAction* draft = menu.addAction(ToolIcon("SolidDraft"), "Draft");
            menu.addSeparator();
            QAction* edit_texture = menu.addAction(EditTextureIcon(), "Edit Texture");
            QAction* apply_film = menu.addAction("Apply Film...");
            QAction* selected = menu.exec(global_position);
        if (selected == fillet || selected == chamfer) {
            // SetSelectionMode performs the Face -> Edges conversion itself.
            // Converting here first would leave no selected face for that
            // method and it would clear the newly selected edges.
            edge_tool_started_from_face_quick_menu_ = true;
            viewport_->SetSelectionMode(SelectionMode::Edge);
            if (!document_.HasSelectedSolidEdge()) {
                edge_tool_started_from_face_quick_menu_ = false;
                viewport_->SetSelectionMode(SelectionMode::Face);
                statusBar()->showMessage(
                    "Face quick menu:operation failed; check the selected geometry and parameters", 1600);
                return;
            }
            ActivateParametricTool(
                selected == fillet ? "fillet_edge" : "ChamferSolid");
            if (active_parametric_object_.tool_id.empty()) {
                edge_tool_started_from_face_quick_menu_ = false;
                viewport_->SetSelectionMode(SelectionMode::Face);
            }
        } else if (selected == extrude) {
            ActivateParametricTool("SolidExtrudeFace");
        } else if (selected == offset) {
            ActivateParametricTool("SolidOffsetFace");
            } else if (selected == draft) {
                ActivateParametricTool("SolidDraft");
            } else if (selected == edit_texture) {
                ShowSurfaceTextureEditor();
            } else if (selected == apply_film) {
                ShowSurfaceFilmDialog();
            }
        });
    connect(viewport_, &OpenGLViewport::ObjectQuickMenuRequested,
            this,
            [this](const QPoint& global_position) {
        if (!active_parametric_object_.tool_id.empty()
            || !document_.HasSelection()) {
            return;
        }

        QMenu menu(this);
        QAction* move = menu.addAction(ToolIcon("move"), "Move");
        QAction* rotate = menu.addAction(ToolIcon("rotate"), "Rotate");
        QAction* scale = menu.addAction(ToolIcon("scale"), "Scale");
        menu.addSeparator();
        QAction* move_point_to_point = menu.addAction("P2P");
        menu.addSeparator();
        QAction* rotate_plus_90 = menu.addAction(ToolIcon("rotate"), "Rotate 90°");
        QAction* rotate_minus_90 = menu.addAction(ToolIcon("rotate"), "Rotate -90°");
        QAction* edit_text = nullptr;
        if (dynamic_cast<CDrawingText*>(document_.GetSelectedObject())) {
            menu.addSeparator();
            edit_text = menu.addAction("Edit Text...");
        }

        QTimer dismiss_timer(&menu);
        dismiss_timer.setInterval(50);
        connect(&dismiss_timer, &QTimer::timeout, &menu, [&menu]() {
            if (!menu.isVisible()) {
                return;
            }
            const QRect bounds = menu.frameGeometry();
            const QPoint cursor = QCursor::pos();
            const int dx = cursor.x() < bounds.left()
                ? bounds.left() - cursor.x()
                : cursor.x() > bounds.right()
                    ? cursor.x() - bounds.right()
                    : 0;
            const int dy = cursor.y() < bounds.top()
                ? bounds.top() - cursor.y()
                : cursor.y() > bounds.bottom()
                    ? cursor.y() - bounds.bottom()
                    : 0;
            const int dismiss_distance = menu.width();
            if (dx * dx + dy * dy > dismiss_distance * dismiss_distance) {
                menu.close();
            }
        });
        dismiss_timer.start();
        QAction* selected = menu.exec(global_position);
        if (selected == move) {
            BeginTransformTool(TransformOperation::Move);
        } else if (selected == rotate) {
            BeginTransformTool(TransformOperation::Rotate);
        } else if (selected == scale) {
            BeginTransformTool(TransformOperation::Scale);
        } else if (selected == move_point_to_point) {
            viewport_->BeginMovePointToPoint();
        } else if (selected == rotate_plus_90 || selected == rotate_minus_90) {
            Vec3 center{};
            if (!document_.GetSelectionCenter(center)) {
                statusBar()->showMessage("Cannot determine the object center", 1800);
                return;
            }
            const float angle = selected == rotate_plus_90
                ? kPi * 0.5f
                : -kPi * 0.5f;
            if (viewport_->ApplyPreciseRotate(center, {0.0f, 0.0f, 1.0f}, angle)) {
                RefreshSceneTree();
                statusBar()->showMessage(
                    selected == rotate_plus_90
                        ? "Object rotated 90° around Z"
                        : "Object rotated -90° around Z",
                    1800);
            }
        } else if (selected == edit_text) {
            if (auto* text = dynamic_cast<CDrawingText*>(document_.GetSelectedObject())) {
                EditDrawingText(*text);
            }
        }
    });
    connect(viewport_, &OpenGLViewport::FilesDropped, this, [this](const QStringList& paths) {
        HandleDroppedFiles(paths);
    });
    connect(viewport_, &OpenGLViewport::ViewportPopupMenuRequested,
            this, &MainWindow::ShowViewportPopupMenu);
    connect(viewport_, &OpenGLViewport::StatusTextChanged, this, [this](const QString& text) {
        statusBar()->showMessage(text);
    });
    connect(viewport_, &OpenGLViewport::PointToPointMeasurementFinished,
            this, [this](double distance_mm) {
        QTimer::singleShot(0, this, [this, distance_mm]() {
            const DisplayLengthUnit unit = LoadDisplayLengthUnit();
            QMessageBox message(QMessageBox::Question,
                                "The information",
                                QString("Distance = %1 %2")
                                    .arg(MillimetersToDisplay(distance_mm, unit),
                                         0, 'f', 3)
                                    .arg(DisplayLengthUnitSuffix(unit)),
                                QMessageBox::NoButton,
                                this);
            QPushButton* repeat = message.addButton(
                "Repeat", QMessageBox::AcceptRole);
            QPushButton* cancel = message.addButton(
                "Cancel", QMessageBox::RejectRole);
            message.setDefaultButton(repeat);
            message.exec();
            if (message.clickedButton() == repeat) {
                viewport_->BeginMeasurePointToPoint();
                UpdateActiveToolUi("MeasurePointToPoint");
            } else {
                Q_UNUSED(cancel);
                viewport_->SetTool(ToolMode::Select);
                statusBar()->showMessage(
                    "Point-to-Point Dimension completed", 1400);
            }
        });
    });
    connect(viewport_, &OpenGLViewport::ToolModeChanged, this, [this](ToolMode tool) {
        if (tool != ToolMode::SketchFillet
            && sketch_fillet_dialog_
            && sketch_fillet_dialog_->isVisible()) {
            sketch_fillet_dialog_->hide();
        }
        if (tool != ToolMode::DrawSpline
            && draw_spline_dialog_
            && draw_spline_dialog_->isVisible()) {
            draw_spline_dialog_->hide();
        }
        if (tool != ToolMode::Select && pending_group_command_ != PendingGroupCommand::None) {
            pending_group_command_ = PendingGroupCommand::None;
            viewport_->SetSelectionConfirmationMode(false);
        }
        if (tool == ToolMode::Orbit) {
            UpdateActiveToolUi("orbit");
        } else if (tool == ToolMode::Walk) {
            UpdateActiveToolUi("walk");
        } else if (tool == ToolMode::ZoomRect) {
            UpdateActiveToolUi("zoom_rect");
        } else if (tool == ToolMode::Select) {
            UpdateActiveToolUi("select");
        } else if (tool == ToolMode::DrawCurve) {
            UpdateActiveToolUi("PolylineCurve");
        } else if (tool == ToolMode::DrawBSpline) {
            UpdateActiveToolUi("BSplineCurve");
        } else if (tool == ToolMode::DrawSpline) {
            UpdateActiveToolUi("DrawSpline");
        } else if (tool == ToolMode::EditPoint) {
            UpdateActiveToolUi("EditPoint");
        } else if (tool == ToolMode::SketchRectangle
                   || tool == ToolMode::SketchPolyline
                   || tool == ToolMode::SketchBezier
                   || tool == ToolMode::SketchConvertBezier
                   || tool == ToolMode::SketchConvertArc
                   || tool == ToolMode::SketchFillet) {
            UpdateActiveToolUi("NewSketch");
        } else if (tool == ToolMode::SolidFillet) {
            UpdateActiveToolUi(active_parametric_object_.tool_id);
        } else if (tool == ToolMode::SolidBoxRectangle) {
            UpdateActiveToolUi("SolidBox");
        } else if (tool == ToolMode::SolidCylinderCircle) {
            UpdateActiveToolUi("SolidCylinder");
        } else if (tool == ToolMode::MeasurePointToPoint) {
            UpdateActiveToolUi("MeasurePointToPoint");
        }
    });
    connect(viewport_, &OpenGLViewport::SolidBoxRectangleFinished, this, [this](std::vector<ToolParameter> parameters) {
        active_parametric_edit_existing_ = false;
        active_parametric_object_ = tool_registry_.CreateParametricObject("SolidBox", document_, parameters);
        if (active_parametric_object_.tool_id.empty()) {
            ClearActiveProperties();
            statusBar()->showMessage("BOX: could not create solid", 1600);
            return;
        }
        property_panel_->SetActiveObject(active_parametric_object_);
        ShowPropertyPanelAtCursor("BOX");
        viewport_->SetSolidDimensionEdit(
            active_parametric_object_, QStringLiteral("depth"));
        property_panel_->FocusParameter("depth");
        UpdateActiveToolUi("SolidBox");
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage("BOX: set Height or press OK");
    });
    connect(viewport_, &OpenGLViewport::SolidCylinderCircleFinished, this, [this](std::vector<ToolParameter> parameters) {
        active_parametric_edit_existing_ = false;
        active_parametric_object_ = tool_registry_.CreateParametricObject(
            "SolidCylinder", document_, parameters);
        if (active_parametric_object_.tool_id.empty()) {
            ClearActiveProperties();
            statusBar()->showMessage("CYLINDER: could not create solid", 1600);
            return;
        }
        property_panel_->SetActiveObject(active_parametric_object_);
        ShowPropertyPanelAtCursor("CYLINDER");
        viewport_->SetSolidDimensionEdit(
            active_parametric_object_, QStringLiteral("height"));
        property_panel_->FocusParameter("height");
        UpdateActiveToolUi("SolidCylinder");
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage("CYLINDER: set Height or press OK");
    });
    connect(viewport_, &OpenGLViewport::BooleanFinished, this, [this]() {
        UpdateActiveToolUi("select");
    });
    connect(property_panel_, &PropertyPanel::ParametersChanged, this, [this]() {
        active_parametric_object_ = property_panel_->ActiveObject();
        if (active_parametric_object_.tool_id == "NurbsParameters") {
            ApplyNurbsParameterChanges();
            return;
        }
        if (active_parametric_object_.tool_id == "PlaneTool") {
            const auto mode = std::find_if(
                active_parametric_object_.parameters.begin(),
                active_parametric_object_.parameters.end(),
                [](const ToolParameter& parameter) { return parameter.id == "mode"; });
            if (mode != active_parametric_object_.parameters.end()
                && static_cast<int>(mode->value) == 2
                && !plane_three_point_method_selected_) {
                plane_three_point_method_selected_ = true;
                BeginPlaneThreePointPick();
            } else if (mode != active_parametric_object_.parameters.end()
                       && static_cast<int>(mode->value) != 2) {
                plane_three_point_method_selected_ = false;
                if (plane_three_point_pick_active_) {
                    CancelPlaneThreePointPick();
                }
            }
            const std::array<double, 4> factors =
                PlaneFactorsFromParameters(active_parametric_object_.parameters);
            if (factors[0] * factors[0] + factors[1] * factors[1]
                    + factors[2] * factors[2] > 1.0e-18) {
                SaveRememberedPlaneFactors(factors);
            }
        }
        bool corrected_corner_single_door = false;
        if (IsCabinetTool(active_parametric_object_.tool_id)) {
            ToolParameter* body_type = nullptr;
            ToolParameter* facade_type = nullptr;
            for (ToolParameter& parameter : active_parametric_object_.parameters) {
                if (parameter.id == "body_type") {
                    body_type = &parameter;
                } else if (parameter.id == "facade_type") {
                    facade_type = &parameter;
                }
            }
            if (body_type && facade_type
                && static_cast<int>(body_type->value) == 1
                && static_cast<int>(facade_type->value) == 1) {
                facade_type->value = 2.0;
                corrected_corner_single_door = true;
                property_panel_->SetActiveObject(active_parametric_object_);
            }
        }
        if (active_parametric_edit_existing_
            && (active_parametric_object_.tool_id == "fillet_edge"
                || active_parametric_object_.tool_id == "fillet_all_edges"
                || active_parametric_object_.tool_id == "ChamferSolid")) {
            tool_registry_.Rebuild(active_parametric_object_, document_);
            if (active_parametric_object_.tool_id == "fillet_edge"
                || active_parametric_object_.tool_id == "fillet_all_edges") {
                viewport_->SetSolidDimensionEdit(
                    active_parametric_object_,
                    FilletPrimaryParameter(active_parametric_object_.parameters));
            }
            RefreshSceneTree();
            viewport_->update();
            const double value = active_parametric_object_.parameters.empty() ? 2.0 : active_parametric_object_.parameters[0].value;
            const QString label = active_parametric_object_.tool_id == "ChamferSolid"
                ? "Chamfer"
                : (active_parametric_object_.tool_id == "fillet_edge" ? "Fillet Edge" : "Fillet All");
            statusBar()->showMessage(QString("%1: %2").arg(label).arg(value, 0, 'f', 2));
            return;
        }
        if (active_parametric_object_.tool_id == "fillet_edge" || active_parametric_object_.tool_id == "fillet_all_edges") {
            const FilletRadiusValues radii = FilletRadii(active_parametric_object_.parameters);
            if (!document_.HasLiveFillet()) {
                statusBar()->showMessage("Fillet:select the required geometry and continue");
                return;
            }
            ScheduleLiveFilletRebuild();
            viewport_->SetSolidDimensionEdit(
                active_parametric_object_,
                FilletPrimaryParameter(active_parametric_object_.parameters));
            viewport_->update();
            statusBar()->showMessage(radii.mode == 2
                    ? QString("Fillet: radius law with %1 points").arg(radii.law.size())
                    : (radii.mode == 1
                        ? QString("Fillet: radius %1 → %2").arg(radii.start, 0, 'f', 2).arg(radii.end, 0, 'f', 2)
                        : QString("Fillet: radius %1").arg(radii.start, 0, 'f', 2)));
            return;
        }
        if (active_parametric_object_.tool_id == "ChamferSolid") {
            const double distance = active_parametric_object_.parameters.empty() ? 2.0 : active_parametric_object_.parameters[0].value;
            if (!document_.HasLiveChamfer()) {
                statusBar()->showMessage("Chamfer:select the required geometry and continue");
                return;
            }
            const bool rebuilt = document_.UpdateLiveChamfer(distance);
            RefreshSceneTree();
            viewport_->update();
            statusBar()->showMessage(rebuilt
                ? QString("Chamfer: Distance %1").arg(distance, 0, 'f', 2)
                : "Chamfer:operation failed; check the selected geometry and parameters");
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
            statusBar()->showMessage(QString("ThickSolid: Thick %1, select the required geometry and continue").arg(thickness, 0, 'f', 2));
            return;
        }
        if (active_parametric_object_.tool_id == "SolidExtrudeTool") {
            const auto& parameters = active_parametric_object_.parameters;
            const double distance = parameters.size() > 0 ? parameters[0].value : 1.0;
            const bool reverse = parameters.size() > 1 && parameters[1].value >= 0.5;
            const double taper_angle = parameters.size() > 2 ? parameters[2].value : 0.0;
            if (active_parametric_edit_existing_) {
                tool_registry_.Rebuild(active_parametric_object_, document_);
                RefreshSceneTree();
                viewport_->update();
                statusBar()->showMessage(
                    QString("Extrude: Distance %1, Angle %2")
                        .arg(distance, 0, 'f', 2)
                        .arg(taper_angle, 0, 'f', 2));
                return;
            }
            if (!document_.HasLivePolylineExtrude()) {
                statusBar()->showMessage("Extrude:select the required geometry and continue");
                return;
            }
            const bool rebuilt = document_.UpdateLiveExtrudeSelectedPolyline(distance, reverse, taper_angle);
            RefreshSceneTree();
            viewport_->update();
            statusBar()->showMessage(rebuilt
                ? QString("Extrude: Distance %1, Angle %2").arg(distance, 0, 'f', 2).arg(taper_angle, 0, 'f', 2)
                : "Extrude:operation status");
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
                statusBar()->showMessage(
                    "Revolve:select the required geometry and continue");
                return;
            }
            const bool rebuilt = document_.UpdateLiveRevolveSelectedPolyline(angle, axis_index);
            RefreshSceneTree();
            viewport_->update();
            statusBar()->showMessage(rebuilt
                ? QString("Revolve: Angle %1").arg(angle, 0, 'f', 1)
                : "Revolve:operation status");
            return;
        }
        // Cabinet rebuilds can contain expensive milling booleans. Keep all
        // edits pending until Apply or OK, both for a new cabinet and while
        // editing an existing assembly.
        if (IsCabinetTool(active_parametric_object_.tool_id)) {
            active_cabinet_parameters_dirty_ = true;
            viewport_->SetSolidDimensionEdit(active_parametric_object_);
            viewport_->SetCabinetPreviewVisible(true);
            viewport_->update();
            if (corrected_corner_single_door) {
                statusBar()->showMessage(
                    "Corner cabinet supports only Double Door", 3000);
                QTimer::singleShot(0, this, [this]() {
                    QMessageBox::information(
                        this,
                        "Corner Cabinet",
                        "Only a double-door front is available for a Corner cabinet.\n"
                        "The parameter was changed to Double Door.");
                });
            } else {
                statusBar()->showMessage(
                    "Cabinet parameters changed. Press Apply to rebuild.");
            }
            return;
        }

        const bool rebuilding_assembly = ActiveParametricObjectIsAssembly();
        tool_registry_.Rebuild(active_parametric_object_, document_);
        if (rebuilding_assembly) {
            document_.ClearSelection();
        }
        if (active_parametric_object_.tool_id == "SolidBox"
            || active_parametric_object_.tool_id == "SolidCylinder"
            || active_parametric_object_.tool_id == "SolidPrismTool") {
            viewport_->SetSolidDimensionEdit(
                active_parametric_object_,
                !active_parametric_edit_existing_
                        && (active_parametric_object_.tool_id == "SolidBox"
                            || active_parametric_object_.tool_id == "SolidCylinder")
                    ? QString::fromLatin1(
                        active_parametric_object_.tool_id == "SolidBox"
                            ? "depth"
                            : "height")
                    : QString());
        }
        if (active_parametric_object_.tool_id == "PlaneTool"
            && active_parametric_object_.object_index
                   < document_.GetObjects().size()
            && document_.GetObjects()[
                   active_parametric_object_.object_index]) {
            tool_registry_.ReplayAllTrimDependents(
                document_,
                document_.GetObjects()[
                    active_parametric_object_.object_index]->m_id);
        }
        RefreshSceneTree();
        viewport_->update();
        if (corrected_corner_single_door) {
            statusBar()->showMessage(
                "Corner cabinet supports only Double Door", 3000);
            QTimer::singleShot(0, this, [this]() {
                QMessageBox::information(
                    this,
                    "Corner Cabinet",
                    "Only a double-door front is available for a Corner cabinet.\n"
                    "The parameter was changed to Double Door.");
            });
        } else {
            statusBar()->showMessage("Object rebuilt", 1200);
        }
    });
    connect(property_panel_, &PropertyPanel::MaterialParameterChanged, this,
            [this](const QString& parameter_id) {
        active_parametric_object_ = property_panel_->ActiveObject();
        const std::string id = parameter_id.toStdString();

        if (!IsCabinetTool(active_parametric_object_.tool_id)
            || !active_parametric_edit_existing_
            || active_parametric_object_.object_index
                   >= document_.GetObjects().size()) {
            if (IsCabinetTool(active_parametric_object_.tool_id)) {
                active_cabinet_parameters_dirty_ = true;
                viewport_->SetSolidDimensionEdit(active_parametric_object_);
                viewport_->SetCabinetPreviewVisible(true);
                viewport_->update();
                statusBar()->showMessage(
                    "Cabinet material selected. Press Apply to build.");
            } else {
                tool_registry_.Rebuild(active_parametric_object_, document_);
                RefreshSceneTree();
                viewport_->update();
            }
            return;
        }

        auto* assembly = dynamic_cast<CAssembled*>(
            document_.GetObjects()[active_parametric_object_.object_index].get());
        if (!assembly) {
            return;
        }

        struct MaterialState {
            unsigned long object_id = 0;
            Material material;
            unsigned long material_id = 0;
        };
        const auto capture_materials = [this](const CAssembled& source) {
            std::vector<MaterialState> states;
            states.reserve(source.GetElementIds().size());
            for (unsigned long object_id : source.GetElementIds()) {
                const CAlfaObject* object = document_.FindObjectById(object_id);
                if (object) {
                    states.push_back({
                        object_id, object->GetMaterial(), object->GetMaterialId()});
                }
            }
            return states;
        };

        const unsigned long assembly_id = assembly->m_id;
        const std::string tool_id = assembly->GetParametricToolId();
        const std::vector<ParametricParameterValue> before_parameters =
            assembly->GetParametricParameters();
        const std::vector<MaterialState> before_materials =
            capture_materials(*assembly);

        if (!tool_registry_.ApplyFurnitureMaterialParameter(
                active_parametric_object_, document_, id)) {
            active_cabinet_parameters_dirty_ = true;
            statusBar()->showMessage(
                "Material will be applied when the cabinet is rebuilt", 1800);
            return;
        }

        assembly = dynamic_cast<CAssembled*>(
            document_.FindObjectById(assembly_id));
        if (!assembly) {
            return;
        }
        const std::vector<ParametricParameterValue> after_parameters =
            assembly->GetParametricParameters();
        const std::vector<MaterialState> after_materials =
            capture_materials(*assembly);
        const auto apply_state = [assembly_id, tool_id](
                                     CAlfaDoc& document,
                                     const std::vector<MaterialState>& materials,
                                     const std::vector<ParametricParameterValue>& parameters) {
            auto* target = dynamic_cast<CAssembled*>(
                document.FindObjectById(assembly_id));
            if (!target) {
                return false;
            }
            target->SetParametricDefinition(tool_id, parameters);
            for (const MaterialState& state : materials) {
                CAlfaObject* object = document.FindObjectById(state.object_id);
                if (!object) {
                    continue;
                }
                object->SetMaterial(state.material);
                object->SetMaterialId(state.material_id);
            }
            return true;
        };
        undo_redo_.RecordCommand(
            "Change cabinet material",
            [before_materials, before_parameters, apply_state](CAlfaDoc& document) {
                return apply_state(document, before_materials, before_parameters);
            },
            [after_materials, after_parameters, apply_state](CAlfaDoc& document) {
                return apply_state(document, after_materials, after_parameters);
            });
        UpdateUndoRedoActions();
        viewport_->update();
        statusBar()->showMessage("Cabinet material applied", 1200);
    });
    connect(property_panel_, &PropertyPanel::MaterialLibraryRequested, this,
            [this](const QString& parameter_id) {
        pending_material_parameter_id_ = parameter_id;
        if (refresh_material_library_) {
            refresh_material_library_();
        }
        if (material_library_dock_) {
            material_library_dock_->show();
            material_library_dock_->raise();
        }
        statusBar()->showMessage(
            "Select a material sphere from the library", 3000);
    });
    connect(property_panel_, &PropertyPanel::CatalogSelectionRequested, this,
            [this](const QString& parameter_id, bool product) {
        const QString catalog_root = QDir(
            QCoreApplication::applicationDirPath()).filePath(
                product ? "Catalog/Products" : "Catalog/Sketches");
        QDir().mkpath(catalog_root);
        const bool milling_pattern_request = !product
            && parameter_id == "slx.milling.guide.id";
        const bool milling_cutter_request = !product
            && parameter_id == "slx.milling.profile.id";
        const QString path = QFileDialog::getOpenFileName(
            this,
            product ? "Choose Handle from Catalog"
                    : milling_pattern_request
                        ? "Choose Milling Pattern from Catalog"
                        : "Choose Profile Sketch from Catalog",
            catalog_root,
            "Dom3D Catalog (*.dom3d)");
        if (path.isEmpty()) {
            return;
        }

        CAlfaDoc catalog_document;
        QString catalog_room;
        ProjectViewState catalog_view;
        QString error;
        if (!dom3d_serializer_.Load(
                path, catalog_document, catalog_room, catalog_view, error)) {
            SetAlfaDoc(&document_);
            QMessageBox::critical(this, "Catalog", error);
            return;
        }
        SetAlfaDoc(&document_);

        std::unique_ptr<CAlfaObject> resource;
        std::vector<std::unique_ptr<CAlfaObject>> pattern_sketches;
        QString catalog_validation_error;
        const bool milling_pattern = milling_pattern_request;
        const auto clone_catalog_sketch = [](const CAlfaObject& object)
                -> std::unique_ptr<CAlfaObject> {
            if (dynamic_cast<const CSmartLine*>(&object)) {
                return object.Clone();
            }
            const auto* polyline = dynamic_cast<const CPolyline*>(&object);
            if (!polyline) {
                return {};
            }
            auto sketch = std::make_unique<CSmartLine>();
            if (!sketch->Create(*polyline)) {
                return {};
            }
            sketch->SetColor(polyline->GetColor());
            sketch->SetLineWidth(polyline->GetLineWidth());
            sketch->SetLineStyle(polyline->GetLineStyle());
            return sketch;
        };
        Material catalog_product_material;
        bool has_catalog_product_material = false;
        if (product) {
            BRep_Builder builder;
            TopoDS_Compound compound;
            builder.MakeCompound(compound);
            bool has_shape = false;
            for (const auto& object : catalog_document.GetObjects()) {
                const auto* solid = dynamic_cast<const CSolid*>(object.get());
                if (!solid || solid->m_Shape.IsNull()) {
                    continue;
                }
                if (!has_catalog_product_material) {
                    catalog_product_material = solid->GetMaterial();
                    has_catalog_product_material = true;
                }
                builder.Add(compound, solid->m_Shape);
                has_shape = true;
            }
            if (has_shape) {
                auto solid = std::make_unique<CSolid>();
                solid->m_Shape = compound;
                solid->InitSurfaces();
                solid->ReBuldMesh();
                resource = std::move(solid);
            }
        } else if (milling_pattern) {
            for (const auto& object : catalog_document.GetObjects()) {
                if (std::unique_ptr<CAlfaObject> sketch =
                        clone_catalog_sketch(*object)) {
                    pattern_sketches.push_back(std::move(sketch));
                }
            }
        } else if (milling_cutter_request) {
            std::string validation_error;
            const CSmartLine* cutter_sketch =
                ResolveMillingCutterCatalogProfile(
                    catalog_document, validation_error);
            if (!cutter_sketch) {
                catalog_validation_error =
                    QString::fromStdString(validation_error);
            } else {
                resource = cutter_sketch->Clone();
            }
        } else {
            for (const auto& object : catalog_document.GetObjects()) {
                if (std::unique_ptr<CAlfaObject> sketch =
                        clone_catalog_sketch(*object)) {
                    resource = std::move(sketch);
                    break;
                }
            }
        }

        if (milling_pattern && !pattern_sketches.empty()) {
            std::vector<unsigned long> sketch_ids;
            sketch_ids.reserve(pattern_sketches.size());
            for (auto& sketch : pattern_sketches) {
                sketch->m_id = 0;
                sketch->SetName(
                    "Catalog Milling Guide: "
                    + QFileInfo(path).completeBaseName().toStdString());
                sketch->SetParametricDefinition("CatalogProfileResource", {});
                sketch->SetVisible(false);
                document_.AddObject(std::move(sketch));
                const std::vector<size_t> selected_indices =
                    document_.GetSelectedObjectIndices();
                if (!selected_indices.empty()
                    && selected_indices.back() < document_.GetObjects().size()
                    && document_.GetObjects()[selected_indices.back()]) {
                    sketch_ids.push_back(
                        document_.GetObjects()[selected_indices.back()]->m_id);
                    document_.GetObjects()[selected_indices.back()]->SetVisible(false);
                }
            }
            if (!sketch_ids.empty()) {
                resource = std::make_unique<CGroup>(
                    "Catalog Milling Pattern", std::move(sketch_ids));
            }
        }

        if (!resource) {
            QMessageBox::information(
                this,
                "Catalog",
                !catalog_validation_error.isEmpty()
                    ? catalog_validation_error
                    : product
                    ? "The selected catalog item does not contain a solid handle."
                    : milling_pattern
                        ? "The selected catalog item does not contain milling guide sketches."
                        : "The selected catalog item does not contain a profile sketch.");
            return;
        }

        resource->m_id = 0;
        resource->SetName(
            "Catalog Resource: " + QFileInfo(path).completeBaseName().toStdString());
        resource->SetParametricDefinition(
            product ? "CatalogHandleResource" : "CatalogProfileResource", {});
        if (product && has_catalog_product_material) {
            Material imported_material = catalog_product_material;
            // Catalog and document material IDs are independent.  Allocate a
            // new document ID so an unrelated material cannot replace it.
            imported_material.id = 0;
            imported_material.name =
                "Catalog Handle: " + QFileInfo(path).completeBaseName().toStdString();
            Material& stored_material = document_.UpsertMaterial(
                std::move(imported_material));
            resource->SetMaterial(stored_material);
            resource->SetMaterialId(stored_material.id);
        }
        resource->SetVisible(false);
        document_.AddObject(std::move(resource));
        const auto selected = document_.GetSelectedObjectIndices();
        if (selected.empty() || selected.back() >= document_.GetObjects().size()
            || !document_.GetObjects()[selected.back()]) {
            return;
        }
        const unsigned long resource_id =
            document_.GetObjects()[selected.back()]->m_id;
        document_.GetObjects()[selected.back()]->SetVisible(false);
        document_.ClearSelection();
        property_panel_->SetCatalogParameterValue(
            parameter_id.toStdString(), static_cast<double>(resource_id));
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage(
            QString("Catalog resource selected: %1")
                .arg(QFileInfo(path).completeBaseName()),
            1800);
    });
    connect(property_panel_, &PropertyPanel::CatalogOrientationHelpRequested,
            this, [this](const QString& parameter_id, bool product) {
        auto* dialog = new QDialog(
            this, Qt::Tool | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
        dialog->setAttribute(Qt::WA_DeleteOnClose, true);
        dialog->setWindowTitle(product
            ? "Catalog Handle Orientation"
            : parameter_id == "slx.panel.id"
                ? "Panel Profile Orientation"
            : parameter_id == "slx.milling.profile.id"
                ? "Cutter Profile Orientation"
            : parameter_id == "slx.milling.guide.id"
                ? "Milling Pattern Orientation"
                : "Frame Profile Orientation");
        auto* layout = new QVBoxLayout(dialog);
        auto* guide = new QLabel(dialog);
        guide->setPixmap(CatalogOrientationGuidePixmap(parameter_id, product));
        layout->addWidget(guide);
        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Close, dialog);
        connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
        layout->addWidget(buttons);
        dialog->setFixedSize(dialog->sizeHint());
        const QPoint cursor = QCursor::pos();
        dialog->move(cursor + QPoint(18, 18));
        dialog->show();
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
    connect(property_panel_, &PropertyPanel::Applied, this, [this]() {
        active_parametric_object_ = property_panel_->ActiveObject();
        ApplyActiveCabinetProperties();
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
}

void MainWindow::CompleteStartup(const QString& startup_project_path) {
    // Startup splash is already hidden when this queued call runs, so the
    // project loader or greeting can never appear underneath it.
    if (!startup_project_path.isEmpty()) {
        const QString lower_path = startup_project_path.toLower();
        if (lower_path.endsWith(".step") || lower_path.endsWith(".stp")) {
            ImportFileFromPath(startup_project_path);
        } else {
            OpenProjectFromPath(startup_project_path);
        }
        return;
    }
    ShowGreetingDialog();
}

void MainWindow::StartFurnitureInteraction(unsigned long object_id) {
    if (!furniture_animation_timer_ || furniture_animation_timer_->isActive()) {
        return;
    }

    const CAlfaObject* clicked_object = document_.FindObjectById(object_id);
    if (!clicked_object) {
        return;
    }

    size_t assembly_index = document_.GetObjects().size();
    const CAssembled* assembly = nullptr;
    size_t fallback_assembly_index = document_.GetObjects().size();
    const CAssembled* fallback_assembly = nullptr;
    const auto contains_recursive = [this](const CGroup& root,
                                            unsigned long target_id) {
        std::set<unsigned long> visited;
        std::function<bool(const CGroup&)> contains;
        contains = [&](const CGroup& group) {
            if (!visited.insert(group.m_id).second) return false;
            for (unsigned long child_id : group.GetElementIds()) {
                if (child_id == target_id) return true;
                const auto* child_group = dynamic_cast<const CGroup*>(
                    document_.FindObjectById(child_id));
                if (child_group && contains(*child_group)) return true;
            }
            return false;
        };
        return contains(root);
    };
    for (size_t index = 0; index < document_.GetObjects().size(); ++index) {
        const auto* candidate = dynamic_cast<const CAssembled*>(
            document_.GetObjects()[index].get());
        if (!candidate || !contains_recursive(*candidate, object_id)) {
            continue;
        }
        const std::string& tool_id = candidate->GetParametricToolId();
        if (tool_id == "kitchen_nika_260" || tool_id == "kitchen_corner") {
            assembly_index = index;
            assembly = candidate;
            break;
        }
        if (!fallback_assembly
            && (IsCabinetTool(tool_id) || tool_id == "desk"
                || tool_id == "drawer_box" || tool_id == "single_drawer"
                || tool_id == "single_facade")) {
            fallback_assembly_index = index;
            fallback_assembly = candidate;
        }
    }
    if (!assembly && fallback_assembly) {
        assembly_index = fallback_assembly_index;
        assembly = fallback_assembly;
    }
    if (!assembly) {
        return;
    }

    std::vector<unsigned long> animation_element_ids;
    std::set<unsigned long> visited_animation_groups;
    std::function<void(const CGroup&)> collect_animation_elements;
    collect_animation_elements = [&](const CGroup& group) {
        if (!visited_animation_groups.insert(group.m_id).second) return;
        for (unsigned long child_id : group.GetElementIds()) {
            const auto* child_group = dynamic_cast<const CGroup*>(
                document_.FindObjectById(child_id));
            if (child_group) {
                collect_animation_elements(*child_group);
            } else {
                animation_element_ids.push_back(child_id);
            }
        }
    };
    collect_animation_elements(*assembly);

    furniture_animation_selection_ids_.clear();
    for (size_t selected_index : document_.GetSelectedObjectIndices()) {
        if (selected_index < document_.GetObjects().size()
            && document_.GetObjects()[selected_index]) {
            furniture_animation_selection_ids_.push_back(
                document_.GetObjects()[selected_index]->m_id);
        }
    }

    ActiveParametricObject animation = tool_registry_.ActiveObjectFromDocument(
        assembly_index, *assembly, 0, &document_);
    if (animation.tool_id.empty()) {
        return;
    }
    const auto find_parameter = [&animation](const std::string& id) -> ToolParameter* {
        const auto found = std::find_if(
            animation.parameters.begin(), animation.parameters.end(),
            [&id](const ToolParameter& parameter) { return parameter.id == id; });
        return found == animation.parameters.end() ? nullptr : &*found;
    };

    furniture_animation_final_drawer_ = -1.0;
    furniture_animation_saved_distance_ = 0.0;
    furniture_animation_preview_value_ = 0.0;
    furniture_animation_preview_rotation_sign_ = 0.0f;
    furniture_animation_preview_ids_.clear();
    if (IsCabinetTool(animation.tool_id)) {
        const std::string& facade_name = clicked_object->GetName();
        if (facade_name.find("Facade") == std::string::npos) {
            return;
        }
        const ToolParameter* facade_type = find_parameter("facade_type");
        const ToolParameter* body_type = find_parameter("body_type");
        const bool independent_double_doors = facade_type && body_type
            && static_cast<int>(std::lround(facade_type->value)) == 2
            && static_cast<int>(std::lround(body_type->value)) != 1;
        if (independent_double_doors) {
            if (facade_name.find("Left Facade") != std::string::npos) {
                furniture_animation_parameter_id_ = "left_door_open_angle";
            } else if (facade_name.find("Right Facade") != std::string::npos) {
                furniture_animation_parameter_id_ = "right_door_open_angle";
            } else {
                return;
            }
        } else {
            furniture_animation_parameter_id_ = "door_open_angle";
        }
        ToolParameter* angle = find_parameter(furniture_animation_parameter_id_);
        if (!angle) {
            return;
        }
        furniture_animation_start_value_ = angle->value;
        furniture_animation_end_value_ = angle->value > 1.0 ? 0.0 : 90.0;

        const auto* cabinet = dynamic_cast<const CKitchenCabinet*>(assembly);
        KitchenCabinetDoorAnimation door_animation;
        if (cabinet
            && cabinet->GetDoorAnimation(facade_name, &door_animation)) {
            const auto& matrix = cabinet->GetAssemblyTransform();
            const Vec3 local_center = door_animation.hinge;
            furniture_animation_preview_center_ = {
                static_cast<float>(matrix[0] * local_center.x
                    + matrix[1] * local_center.y
                    + matrix[2] * local_center.z + matrix[3]),
                static_cast<float>(matrix[4] * local_center.x
                    + matrix[5] * local_center.y
                    + matrix[6] * local_center.z + matrix[7]),
                static_cast<float>(matrix[8] * local_center.x
                    + matrix[9] * local_center.y
                    + matrix[10] * local_center.z + matrix[11])};
            furniture_animation_preview_axis_ = normalize({
                static_cast<float>(matrix[2]),
                static_cast<float>(matrix[6]),
                static_cast<float>(matrix[10])});
            furniture_animation_preview_rotation_sign_ =
                door_animation.angle_sign;

            const bool left_door =
                facade_name.find("Left Facade") != std::string::npos;
            const bool right_door =
                facade_name.find("Right Facade") != std::string::npos;
            std::set<unsigned long> preview_leaf_ids;
            std::function<void(unsigned long)> append_preview_leaves;
            append_preview_leaves = [&](unsigned long object_id) {
                const CAlfaObject* object =
                    document_.FindObjectById(object_id);
                if (!object) {
                    return;
                }
                if (const auto* group = dynamic_cast<const CGroup*>(object)) {
                    for (unsigned long detail_id : group->GetElementIds()) {
                        append_preview_leaves(detail_id);
                    }
                    return;
                }
                if (dynamic_cast<const CSolid*>(object)
                    && preview_leaf_ids.insert(object_id).second) {
                    furniture_animation_preview_ids_.push_back(object_id);
                }
            };
            for (unsigned long child_id : cabinet->GetElementIds()) {
                const CAlfaObject* child = document_.FindObjectById(child_id);
                if (!child) {
                    continue;
                }
                const std::string& child_name = child->GetName();
                const bool same_door = left_door
                    ? child_name.find("Left Facade") != std::string::npos
                    : right_door
                        ? child_name.find("Right Facade") != std::string::npos
                        : child_name.find("Facade") != std::string::npos;
                if (same_door) {
                    // Milano radius facades are CFacadeFurniture groups made
                    // from five solids. PreviewRotate operates on CSolid, so
                    // expand matching facade groups to their leaf details;
                    // direct handle solids are collected by the same path.
                    append_preview_leaves(child_id);
                }
            }
            furniture_animation_preview_value_ =
                furniture_animation_start_value_;
        }
    } else if (animation.tool_id == "single_facade") {
        if (clicked_object->GetName().find("Single Facade")
            == std::string::npos) {
            return;
        }
        furniture_animation_parameter_id_ = "open_angle";
        ToolParameter* angle = find_parameter("open_angle");
        ToolParameter* width = find_parameter("width");
        ToolParameter* hinge = find_parameter("hinge_side");
        if (!angle || !width || !hinge) return;
        furniture_animation_start_value_ = angle->value;
        furniture_animation_end_value_ = angle->value > 1.0 ? 0.0 : 90.0;
        const bool hinge_right = hinge->value >= 0.5;
        const Vec3 local_center{
            static_cast<float>((hinge_right ? 0.5 : -0.5) * width->value),
            0.0f, 0.0f};
        const auto& matrix = assembly->GetAssemblyTransform();
        furniture_animation_preview_center_ = {
            static_cast<float>(matrix[0] * local_center.x
                + matrix[1] * local_center.y + matrix[3]),
            static_cast<float>(matrix[4] * local_center.x
                + matrix[5] * local_center.y + matrix[7]),
            static_cast<float>(matrix[8] * local_center.x
                + matrix[9] * local_center.y + matrix[11])};
        furniture_animation_preview_axis_ = normalize({
            static_cast<float>(matrix[2]),
            static_cast<float>(matrix[6]),
            static_cast<float>(matrix[10])});
        furniture_animation_preview_rotation_sign_ =
            hinge_right ? 1.0f : -1.0f;
        furniture_animation_preview_ids_ = animation_element_ids;
        furniture_animation_preview_value_ =
            furniture_animation_start_value_;
    } else if (animation.tool_id == "kitchen_nika_260"
               && clicked_object->GetName().find(" Door ") != std::string::npos) {
        const std::string& clicked_name = clicked_object->GetName();
        const size_t handle_position = clicked_name.find(" Handle");
        if (handle_position == std::string::npos) {
            return;
        }
        const std::string door_prefix = clicked_name.substr(0, handle_position);
        struct NikaDoor {
            const char* prefix;
            const char* parameter;
            int module;
            bool upper;
            bool hinge_on_right;
        };
        static const std::array<NikaDoor, 8> doors{{
            {"Nika Lower 1 Door", "lower_1_door_angle", 1, false, false},
            {"Nika Lower 4 Left Door", "lower_4_left_door_angle", 4, false, false},
            {"Nika Lower 4 Right Door", "lower_4_right_door_angle", 4, false, true},
            {"Nika Upper 1 Door", "upper_1_door_angle", 1, true, false},
            {"Nika Upper 2 Door", "upper_2_door_angle", 2, true, false},
            {"Nika Upper 3 Door", "upper_3_door_angle", 3, true, false},
            {"Nika Upper 4 Left Door", "upper_4_left_door_angle", 4, true, false},
            {"Nika Upper 4 Right Door", "upper_4_right_door_angle", 4, true, true}
        }};
        const auto door = std::find_if(
            doors.begin(), doors.end(), [&door_prefix](const NikaDoor& item) {
                return door_prefix == item.prefix;
            });
        if (door == doors.end()) {
            return;
        }
        furniture_animation_parameter_id_ = door->parameter;
        ToolParameter* angle = find_parameter(furniture_animation_parameter_id_);
        if (!angle) {
            return;
        }
        furniture_animation_start_value_ = angle->value;
        furniture_animation_end_value_ = angle->value > 1.0 ? 0.0 : 90.0;
        furniture_animation_preview_rotation_sign_ =
            door->hinge_on_right ? 1.0f : -1.0f;

        const double kitchen_width = find_parameter("width")
            ? find_parameter("width")->value : 2600.0;
        const double depth = find_parameter(
            door->upper ? "upper_depth" : "base_depth")
            ? find_parameter(door->upper ? "upper_depth" : "base_depth")->value
            : (door->upper ? 300.0 : 500.0);
        const double module_start = -kitchen_width * 0.5
            + kitchen_width * (door->module - 1) * 600.0 / 2600.0;
        const double module_width = kitchen_width
            * (door->module == 4 ? 800.0 : 600.0) / 2600.0;
        const double local_hinge_x = door->hinge_on_right
            ? module_start + module_width - 3.0
            : module_start + 3.0;
        const Vec3 local_center{
            static_cast<float>(local_hinge_x),
            static_cast<float>(-depth * 0.5), 0.0f};
        const auto& matrix = assembly->GetAssemblyTransform();
        furniture_animation_preview_center_ = {
            static_cast<float>(matrix[0] * local_center.x
                + matrix[1] * local_center.y + matrix[3]),
            static_cast<float>(matrix[4] * local_center.x
                + matrix[5] * local_center.y + matrix[7]),
            static_cast<float>(matrix[8] * local_center.x
                + matrix[9] * local_center.y + matrix[11])};
        furniture_animation_preview_axis_ = normalize({
            static_cast<float>(matrix[2]),
            static_cast<float>(matrix[6]),
            static_cast<float>(matrix[10])});
        for (unsigned long child_id : animation_element_ids) {
            const CAlfaObject* child = document_.FindObjectById(child_id);
            if (child && child->GetName().rfind(door_prefix, 0) == 0) {
                furniture_animation_preview_ids_.push_back(child_id);
            }
        }
        furniture_animation_preview_value_ = furniture_animation_start_value_;
    } else if (animation.tool_id == "kitchen_corner"
               && clicked_object->GetName().find("Facade Handle")
                    != std::string::npos
               && (clicked_object->GetName().rfind("Corner Base ", 0) == 0
                   || clicked_object->GetName().rfind("Corner Upper ", 0) == 0)) {
        const bool upper = clicked_object->GetName().rfind(
            "Corner Upper ", 0) == 0;
        furniture_animation_parameter_id_ = upper
            ? "upper_corner_door_angle" : "lower_corner_door_angle";
        ToolParameter* angle = find_parameter(furniture_animation_parameter_id_);
        if (!angle) return;
        furniture_animation_start_value_ = angle->value;
        furniture_animation_end_value_ = angle->value > 1.0 ? 0.0 : 90.0;
        furniture_animation_preview_rotation_sign_ = 1.0f;
        const double depth = find_parameter(upper ? "upper_depth" : "base_depth")
            ? find_parameter(upper ? "upper_depth" : "base_depth")->value
            : (upper ? 300.0 : 570.0);
        const Vec3 local_center{0.0f,
                                static_cast<float>(-depth + 2.0), 0.0f};
        const auto& matrix = assembly->GetAssemblyTransform();
        furniture_animation_preview_center_ = {
            static_cast<float>(matrix[0] * local_center.x
                + matrix[1] * local_center.y + matrix[3]),
            static_cast<float>(matrix[4] * local_center.x
                + matrix[5] * local_center.y + matrix[7]),
            static_cast<float>(matrix[8] * local_center.x
                + matrix[9] * local_center.y + matrix[11])};
        furniture_animation_preview_axis_ = normalize({
            static_cast<float>(matrix[2]), static_cast<float>(matrix[6]),
            static_cast<float>(matrix[10])});
        const std::string prefix = upper ? "Corner Upper " : "Corner Base ";
        for (unsigned long child_id : animation_element_ids) {
            const CAlfaObject* child = document_.FindObjectById(child_id);
            if (child && child->GetName().rfind(prefix, 0) == 0
                && child->GetName().find("Facade") != std::string::npos) {
                furniture_animation_preview_ids_.push_back(child_id);
            }
        }
        furniture_animation_preview_value_ = furniture_animation_start_value_;
    } else if (animation.tool_id == "kitchen_corner"
               && clicked_object->GetName().find(" Door ") != std::string::npos) {
        const std::string& clicked_name = clicked_object->GetName();
        const size_t handle_position = clicked_name.find(" Handle");
        if (handle_position == std::string::npos) return;
        const std::string door_prefix = clicked_name.substr(0, handle_position);
        const bool right_wing = door_prefix.rfind("Corner Right ", 0) == 0;
        const bool left_wing = door_prefix.rfind("Corner Left ", 0) == 0;
        if (!left_wing && !right_wing) return;
        struct Door { const char* suffix; const char* parameter; int module; bool upper; bool right_hinge; };
        static const std::array<Door, 8> doors{{
            {"Lower 1 Door", "lower_1_door_angle", 1, false, false},
            {"Lower 4 Left Door", "lower_4_left_door_angle", 4, false, false},
            {"Lower 4 Right Door", "lower_4_right_door_angle", 4, false, true},
            {"Upper 1 Door", "upper_1_door_angle", 1, true, false},
            {"Upper 2 Door", "upper_2_door_angle", 2, true, false},
            {"Upper 3 Door", "upper_3_door_angle", 3, true, false},
            {"Upper 4 Left Door", "upper_4_left_door_angle", 4, true, false},
            {"Upper 4 Right Door", "upper_4_right_door_angle", 4, true, true}
        }};
        const std::string suffix = door_prefix.substr(
            right_wing ? std::string("Corner Right ").size()
                       : std::string("Corner Left ").size());
        const auto door = std::find_if(doors.begin(), doors.end(),
            [&suffix](const Door& item) { return suffix == item.suffix; });
        if (door == doors.end()) return;
        furniture_animation_parameter_id_ = std::string(
            right_wing ? "right_" : "left_") + door->parameter;
        ToolParameter* angle = find_parameter(furniture_animation_parameter_id_);
        if (!angle) return;
        furniture_animation_start_value_ = angle->value;
        furniture_animation_end_value_ = angle->value > 1.0 ? 0.0 : 90.0;
        furniture_animation_preview_rotation_sign_ = door->right_hinge ? 1.0f : -1.0f;
        const double base_depth = find_parameter("base_depth")
            ? find_parameter("base_depth")->value : 570.0;
        const double depth = find_parameter(door->upper ? "upper_depth" : "base_depth")
            ? find_parameter(door->upper ? "upper_depth" : "base_depth")->value
            : (door->upper ? 300.0 : base_depth);
        const double total_length = find_parameter(
            right_wing ? "right_length" : "left_length")
            ? find_parameter(right_wing ? "right_length" : "left_length")->value
            : (right_wing ? 1800.0 : 2700.0);
        const double run = std::max(900.0, total_length - base_depth);
        const double module_start = -run * 0.5
            + run * (door->module - 1) * 600.0 / 2600.0;
        const double module_width = run
            * (door->module == 4 ? 800.0 : 600.0) / 2600.0;
        const double source_x = door->right_hinge
            ? module_start + module_width - 3.0 : module_start + 3.0;
        const double source_y = -depth * 0.5;
        const double corner_depth = door->upper
            ? depth : base_depth;
        Vec3 local_center;
        if (right_wing) {
            local_center = {
                static_cast<float>(source_y + depth * 0.5),
                static_cast<float>(-source_x - corner_depth - run * 0.5),
                0.0f};
        } else {
            local_center = {
                static_cast<float>(source_x - corner_depth - run * 0.5),
                static_cast<float>(source_y + depth * 0.5),
                0.0f};
        }
        const auto& matrix = assembly->GetAssemblyTransform();
        furniture_animation_preview_center_ = {
            static_cast<float>(matrix[0] * local_center.x + matrix[1] * local_center.y + matrix[3]),
            static_cast<float>(matrix[4] * local_center.x + matrix[5] * local_center.y + matrix[7]),
            static_cast<float>(matrix[8] * local_center.x + matrix[9] * local_center.y + matrix[11])};
        furniture_animation_preview_axis_ = normalize({
            static_cast<float>(matrix[2]), static_cast<float>(matrix[6]),
            static_cast<float>(matrix[10])});
        for (unsigned long child_id : animation_element_ids) {
            const CAlfaObject* child = document_.FindObjectById(child_id);
            if (child && child->GetName().rfind(door_prefix, 0) == 0) {
                furniture_animation_preview_ids_.push_back(child_id);
            }
        }
        furniture_animation_preview_value_ = furniture_animation_start_value_;
    } else {
        const std::string& name = clicked_object->GetName();
        if (name.find("Handle") == std::string::npos) {
            return;
        }
        int drawer = 0;
        std::string nika_drawer_prefix;
        if (animation.tool_id == "kitchen_nika_260") {
            for (int module = 2; module <= 3 && drawer == 0; ++module) {
                for (int local_drawer = 1; local_drawer <= 4; ++local_drawer) {
                    const std::string prefix = "Nika Lower "
                        + std::to_string(module) + " Drawer "
                        + std::to_string(local_drawer);
                    if (name.rfind(prefix, 0) == 0) {
                        drawer = (module == 2 ? 0 : 4) + local_drawer;
                        nika_drawer_prefix = prefix;
                        break;
                    }
                }
            }
        } else if (animation.tool_id == "drawer_box"
                   && name.rfind("Nika Lower ", 0) == 0) {
            const size_t drawer_marker = name.find(" Drawer ");
            if (drawer_marker == std::string::npos) return;
            const size_t number_begin = drawer_marker
                + std::string(" Drawer ").size();
            size_t number_end = number_begin;
            while (number_end < name.size()
                   && std::isdigit(static_cast<unsigned char>(
                       name[number_end])) != 0) {
                ++number_end;
            }
            if (number_end == number_begin) return;
            drawer = std::stoi(name.substr(
                number_begin, number_end - number_begin));
            nika_drawer_prefix = name.substr(0, number_end);
        } else {
            const auto digit = std::find_if(
                name.begin(), name.end(), [](unsigned char ch) {
                    return std::isdigit(ch) != 0;
                });
            if (digit == name.end()) {
                return;
            }
            for (auto cursor = digit;
                 cursor != name.end() && std::isdigit(
                     static_cast<unsigned char>(*cursor)) != 0;
                 ++cursor) {
                drawer = drawer * 10 + (*cursor - '0');
            }
        }
        if (drawer <= 0) {
            return;
        }
        ToolParameter* open_drawer = find_parameter("open_drawer");
        ToolParameter* distance = find_parameter("pullout_distance");
        if (!open_drawer || !distance || drawer > static_cast<int>(open_drawer->maximum)) {
            return;
        }

        const int current_open = static_cast<int>(std::lround(open_drawer->value));
        furniture_animation_parameter_id_ = "pullout_distance";
        furniture_animation_saved_distance_ = distance->value;
        furniture_animation_start_value_ = current_open == drawer
            ? distance->value : 0.0;
        furniture_animation_end_value_ = current_open == drawer
            ? 0.0 : distance->value;
        furniture_animation_final_drawer_ = current_open == drawer
            ? 0.0 : static_cast<double>(drawer);
        open_drawer->value = static_cast<double>(drawer);

        const std::string drawer_number = std::to_string(drawer);
        const std::string drawer_prefix = "Drawer " + drawer_number + " ";
        const std::string desk_drawer_prefix =
            "Desk Drawer " + drawer_number + " ";
        const std::string desk_facade = "Desk Drawer Facade " + drawer_number;
        const std::string desk_handle = "Desk Drawer Handle " + drawer_number;
        for (unsigned long child_id : animation_element_ids) {
            const CAlfaObject* child = document_.FindObjectById(child_id);
            if (!child) {
                continue;
            }
            const std::string& child_name = child->GetName();
            const bool desk_part = child_name == desk_facade
                || child_name == desk_handle
                || child_name.rfind(desk_drawer_prefix, 0) == 0;
            const bool drawer_part = child_name.rfind(drawer_prefix, 0) == 0
                && child_name.find(" Guide") == std::string::npos;
            const bool nika_part = !nika_drawer_prefix.empty()
                && child_name.rfind(nika_drawer_prefix, 0) == 0;
            if (desk_part || drawer_part || nika_part) {
                furniture_animation_preview_ids_.push_back(child_id);
            }
        }
        furniture_animation_preview_value_ = furniture_animation_start_value_;
    }

    furniture_animation_object_ = std::move(animation);
    furniture_animation_frame_ = 0;
    furniture_animation_timer_->start();
    statusBar()->showMessage(
        IsCabinetTool(furniture_animation_object_.tool_id)
            || furniture_animation_object_.tool_id == "single_facade"
            || (furniture_animation_object_.tool_id == "kitchen_nika_260"
                && furniture_animation_final_drawer_ < 0.0)
            || furniture_animation_object_.tool_id == "kitchen_corner"
            ? "Door animation"
            : "Drawer animation");
}

void MainWindow::RestoreFurnitureAnimationSelection() {
    document_.ClearSelection();
    for (size_t index = 0;
         index < furniture_animation_selection_ids_.size(); ++index) {
        document_.SelectObjectById(
            furniture_animation_selection_ids_[index],
            index == 0 ? SelectionAction::Replace : SelectionAction::Add);
    }
}

void MainWindow::AdvanceFurnitureAnimation() {
    if (!furniture_animation_timer_
        || furniture_animation_object_.tool_id.empty()
        || furniture_animation_object_.object_index >= document_.GetObjects().size()
        || !document_.GetObjects()[furniture_animation_object_.object_index]) {
        if (furniture_animation_timer_) {
            furniture_animation_timer_->stop();
        }
        furniture_animation_object_ = {};
        return;
    }

    // Ten eased frames keep the animation responsive.  The ready meshes and
    // edges move during the frames.  At the end only the corresponding BReps
    // receive the same rigid transform; the cabinet/kitchen is not rebuilt.
    constexpr int frame_count = 10;
    ++furniture_animation_frame_;
    const double time = std::clamp(
        static_cast<double>(furniture_animation_frame_) / frame_count,
        0.0,
        1.0);
    const double eased = time * time * (3.0 - 2.0 * time);
    const double value = furniture_animation_start_value_
        + (furniture_animation_end_value_ - furniture_animation_start_value_) * eased;

    auto find_parameter = [this](const std::string& id) -> ToolParameter* {
        const auto found = std::find_if(
            furniture_animation_object_.parameters.begin(),
            furniture_animation_object_.parameters.end(),
            [&id](const ToolParameter& parameter) { return parameter.id == id; });
        return found == furniture_animation_object_.parameters.end()
            ? nullptr : &*found;
    };
    ToolParameter* animated_parameter = find_parameter(
        furniture_animation_parameter_id_);
    if (!animated_parameter) {
        furniture_animation_timer_->stop();
        furniture_animation_object_ = {};
        return;
    }
    animated_parameter->value = value;
    const bool door_mesh_preview =
        (IsCabinetTool(furniture_animation_object_.tool_id)
         || furniture_animation_object_.tool_id == "single_facade"
         || furniture_animation_object_.tool_id == "kitchen_nika_260"
         || furniture_animation_object_.tool_id == "kitchen_corner")
        && std::fabs(furniture_animation_preview_rotation_sign_) > 0.5f
        && !furniture_animation_preview_ids_.empty();
    if (door_mesh_preview) {
        constexpr double degrees_to_radians =
            3.14159265358979323846 / 180.0;
        const float delta_angle = static_cast<float>(
            (value - furniture_animation_preview_value_)
            * furniture_animation_preview_rotation_sign_
            * degrees_to_radians);
        for (unsigned long id : furniture_animation_preview_ids_) {
            if (auto* solid = dynamic_cast<CSolid*>(document_.FindObjectById(id))) {
                solid->PreviewRotate(
                    furniture_animation_preview_center_,
                    furniture_animation_preview_axis_,
                    delta_angle);
            }
        }
        furniture_animation_preview_value_ = value;
    } else if (furniture_animation_final_drawer_ >= 0.0
        && !furniture_animation_preview_ids_.empty()) {
        const float delta = static_cast<float>(
            furniture_animation_preview_value_ - value);
        for (unsigned long id : furniture_animation_preview_ids_) {
            if (auto* solid = dynamic_cast<CSolid*>(document_.FindObjectById(id))) {
                solid->PreviewTranslate({0.0f, delta, 0.0f});
            }
        }
        furniture_animation_preview_value_ = value;
    } else {
        tool_registry_.Rebuild(furniture_animation_object_, document_);
        RestoreFurnitureAnimationSelection();
    }
    viewport_->update();

    if (furniture_animation_frame_ < frame_count) {
        return;
    }

    const auto store_animation_parameters = [this]() {
        if (furniture_animation_object_.object_index
                >= document_.GetObjects().size()
            || !document_.GetObjects()[
                furniture_animation_object_.object_index]) {
            return;
        }
        std::vector<ParametricParameterValue> values;
        values.reserve(furniture_animation_object_.parameters.size());
        for (const ToolParameter& parameter :
             furniture_animation_object_.parameters) {
            values.push_back({parameter.id, parameter.value});
        }
        document_.GetObjects()[furniture_animation_object_.object_index]
            ->SetParametricDefinition(
                furniture_animation_object_.tool_id, std::move(values));
    };

    if (furniture_animation_final_drawer_ >= 0.0) {
        if (ToolParameter* open_drawer = find_parameter("open_drawer")) {
            open_drawer->value = furniture_animation_final_drawer_;
        }
        if (ToolParameter* distance = find_parameter("pullout_distance")) {
            distance->value = furniture_animation_saved_distance_;
        }
        const Vec3 total_delta{
            0.0f,
            static_cast<float>(furniture_animation_start_value_
                               - furniture_animation_end_value_),
            0.0f};
        for (unsigned long id : furniture_animation_preview_ids_) {
            if (auto* solid = dynamic_cast<CSolid*>(
                    document_.FindObjectById(id))) {
                // Drawer animation is part of the furniture parameters, not
                // a user-authored transform of each generated solid.
                solid->CommitPreviewTranslate(total_delta, false);
            }
        }
        store_animation_parameters();
        RestoreFurnitureAnimationSelection();
    } else if (door_mesh_preview) {
        constexpr double degrees_to_radians =
            3.14159265358979323846 / 180.0;
        const float total_angle = static_cast<float>(
            (furniture_animation_end_value_
             - furniture_animation_start_value_)
            * furniture_animation_preview_rotation_sign_
            * degrees_to_radians);
        for (unsigned long id : furniture_animation_preview_ids_) {
            if (auto* solid = dynamic_cast<CSolid*>(
                    document_.FindObjectById(id))) {
                solid->CommitPreviewRotate(
                    furniture_animation_preview_center_,
                    furniture_animation_preview_axis_, total_angle);
            }
        }
        store_animation_parameters();
        RestoreFurnitureAnimationSelection();
    }
    furniture_animation_timer_->stop();

    if (active_parametric_object_.object_index
            == furniture_animation_object_.object_index
        && active_parametric_object_.tool_id
            == furniture_animation_object_.tool_id) {
        active_parametric_object_ = furniture_animation_object_;
        property_panel_->SetActiveObject(active_parametric_object_);
    }
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage(
        (IsCabinetTool(furniture_animation_object_.tool_id)
         || furniture_animation_object_.tool_id == "single_facade"
         || furniture_animation_object_.tool_id == "kitchen_corner")
            ? (furniture_animation_end_value_ > 0.0
                ? "Door opened" : "Door closed")
            : (furniture_animation_final_drawer_ > 0.0
                ? "Drawer opened" : "Drawer closed"),
        1400);
    furniture_animation_object_ = {};
    furniture_animation_preview_rotation_sign_ = 0.0f;
    furniture_animation_preview_ids_.clear();
    furniture_animation_selection_ids_.clear();
}

void MainWindow::CreateActions() {
    auto* file_menu = menuBar()->addMenu("&File");
    auto* view_menu = menuBar()->addMenu("&View");
    auto* tools_menu = menuBar()->addMenu("&Tools");
    auto* edit_menu = menuBar()->addMenu("&Edit");
    auto* render_menu = menuBar()->addMenu("&Render");
    auto* help_menu = menuBar()->addMenu("&Help");
    auto& languages = LanguageManager::Instance();
    languages.BindText(file_menu, "MenuFile", "&File");
    languages.BindText(view_menu, "MenuView", "&View");
    languages.BindText(tools_menu, "MenuTools", "&Tools");
    languages.BindText(edit_menu, "MenuEdit", "&Edit");
    languages.BindText(render_menu, "MenuRender", "&Render");
    languages.BindText(help_menu, "MenuHelp", "&Help");
    connect(&languages, &LanguageManager::LanguageChanged, this, [this, &languages]() {
        languages.Apply(this);
        statusBar()->showMessage(
            languages.Text("LanguageChangedStatus", "Interface language changed"),
            1600);
    });

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
    tool_tabs_->addTab("Mesh 3D");
    tool_tabs_->addTab("Sketch");
    tool_tabs_->addTab("Assemblies");
    const QStringList tool_tab_keys = {
        "Architecture", "Furniture", "Surfaces", "Solid",
        "Curves", "Mesh 3D", "Sketch", "Assemblies"};
    for (int index = 0; index < tool_tabs_->count(); ++index) {
        tool_tabs_->setTabData(index, tool_tab_keys[index]);
    }
    tool_tabs_->setCurrentIndex(0);
    tab_toolbar_->addWidget(tool_tabs_);
    connect(tool_tabs_, &QTabBar::currentChanged, this, [this](int index) {
        if (!tool_tabs_ || index < 0) {
            return;
        }
        PopulateToolsPanelForTab(index);
        if (tool_tabs_->tabData(index).toString() == "Sketch"
            && document_.GetSelectedSketch()
            && viewport_->BeginEditSelectedSketch()) {
            ShowSketchPanel();
            UpdateActiveToolUi("NewSketch");
            viewport_->update();
            statusBar()->showMessage("Sketch edit panel opened", 1200);
            return;
        }
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
    main_toolbar_->setObjectName("MainToolbar");
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

    undo_action_ = add_action("&Undo", QKeySequence::Undo,
                              [this]() { UndoDocumentChange(); });
    redo_action_ = add_action("&Redo", QKeySequence::Redo,
                              [this]() { RedoDocumentChange(); });
    edit_menu->addAction(undo_action_);
    edit_menu->addAction(redo_action_);
    edit_menu->addSeparator();
    UpdateUndoRedoActions();

    file_menu->addAction(add_action("&New", QKeySequence::New, [this]() { NewProject(); }));
    file_menu->addAction(add_action("&Open...", QKeySequence::Open, [this]() { OpenProject(); }));
    recent_files_menu_ = file_menu->addMenu("Open &Recent");
    file_menu->addAction(add_action("&Save", QKeySequence::Save, [this]() { SaveProject(); }));
    file_menu->addAction(add_action("Save &As...", QKeySequence::SaveAs, [this]() { SaveProjectAs(); }));
    file_menu->addSeparator();
    file_menu->addAction(add_action("&Preferences...", {}, [this]() { ShowPreferences(); }));
    file_menu->addAction(add_action("&Hot Keys...", {}, [this]() {
        HotkeyManagerDialog dialog(this, this);
        dialog.exec();
    }));
    file_menu->addSeparator();
    file_menu->addAction(add_action("&Import...", QKeySequence(Qt::CTRL | Qt::Key_I), [this]() { ImportFile(); }));
    QAction* catalog_action = add_action("&Catalog...", {}, [this]() { ShowCatalogDialog(); });
    catalog_action->setIcon(CatalogIcon());
    catalog_action->setToolTip("Catalog");
    file_menu->addAction(catalog_action);
    file_menu->addAction(add_action("&Export...", QKeySequence(Qt::CTRL | Qt::Key_E), [this]() { ExportFile(); }));
    file_menu->addSeparator();
    file_menu->addAction(add_action("E&xit", QKeySequence::Quit, [this]() { close(); }));

    render_menu->addAction("Blender Cycles...", this, [this]() {
        ShowBlenderCyclesDialog();
    });
    render_menu->addAction("Dom3D Native Raytrace...", this, [this]() {
        ShowNativeRaytraceDialog();
    });
    render_menu->addAction("View Last Render", this, [this]() {
        ViewLastRenderResult();
    });

    view_menu->addAction("Update Scene", this, [this]() {
        viewport_->RefreshSurfaceMeshQuality();
    });
    auto* lighting_action = view_menu->addAction("Lighting...", this, [this]() {
        ShowLightingDialog();
    });
    languages.BindText(lighting_action, "LightingMenuAction", "Lighting...");
    view_menu->addSeparator();
    auto* reference_image_menu = view_menu->addMenu("Add Ref image");
    reference_image_menu->addAction("Ref image for X-axis", this, [this]() {
        AddReferenceImage(ReferenceImageAxis::X);
    });
    reference_image_menu->addAction("Ref image for Y-axis", this, [this]() {
        AddReferenceImage(ReferenceImageAxis::Y);
    });
    reference_image_menu->addAction("Ref image for Z-axis", this, [this]() {
        AddReferenceImage(ReferenceImageAxis::Z);
    });
    auto* grid_density_menu = view_menu->addMenu("Grid Density");
    auto* grid_density_group = new QActionGroup(this);
    grid_density_group->setExclusive(true);
    QAction* custom_grid_action = nullptr;
    const auto add_grid_density_action =
        [this, grid_density_menu, grid_density_group, &custom_grid_action](
            const QString& text, const QString& mode) {
            auto* action = grid_density_menu->addAction(text);
            action->setCheckable(true);
            action->setData(mode);
            grid_density_group->addAction(action);
            if (mode == "custom") {
                custom_grid_action = action;
            }
            connect(action, &QAction::triggered, this, [this, mode]() {
                QSettings settings("Dom3D", "Dom3D_Pro");
                settings.setValue("view/gridDensityMode", mode);
                viewport_->ReloadModelingPreferences();
                statusBar()->showMessage(
                    QString("Grid Density: %1").arg(mode), 1400);
            });
            return action;
        };
    add_grid_density_action("Small", "small");
    add_grid_density_action("Medium", "medium");
    add_grid_density_action("Large", "large");
    add_grid_density_action("Custom", "custom");
    {
        QSettings settings("Dom3D", "Dom3D_Pro");
        const QString current_mode = settings.value(
            "view/gridDensityMode", QStringLiteral("medium")).toString();
        for (QAction* action : grid_density_group->actions()) {
            action->setChecked(action->data().toString() == current_mode);
        }
    }
    grid_density_menu->addSeparator();
    grid_density_menu->addAction("Customize Grid...", this,
        [this, custom_grid_action]() {
            QSettings settings("Dom3D", "Dom3D_Pro");
            const DisplayLengthUnit unit = LoadDisplayLengthUnit();

            QDialog dialog(this);
            dialog.setWindowTitle("Customize Grid");
            dialog.setModal(true);
            dialog.setMinimumWidth(420);

            auto* description = new QLabel(
                "Set custom grid parameters. They are used when the Custom density is selected.",
                &dialog);
            description->setWordWrap(true);

            auto* grid_step = new QDoubleSpinBox(&dialog);
            grid_step->setDecimals(unit == DisplayLengthUnit::Inches ? 4 : 3);
            grid_step->setRange(
                MillimetersToDisplay(0.001, unit),
                MillimetersToDisplay(1000000.0, unit));
            grid_step->setSingleStep(
                MillimetersToDisplay(unit == DisplayLengthUnit::Inches ? 1.27 : 10.0, unit));
            grid_step->setValue(MillimetersToDisplay(
                settings.value("view/customGridStep", 100.0).toDouble(), unit));

            auto* subdivisions = new QSpinBox(&dialog);
            subdivisions->setRange(1, 100);
            subdivisions->setValue(settings.value(
                "view/customGridSubdivisions", 2).toInt());

            auto* division_count = new QSpinBox(&dialog);
            division_count->setRange(1, 1000);
            division_count->setValue(settings.value(
                "view/customGridDivisionCount", 10).toInt());

            auto* form = new QFormLayout();
            form->addRow("Grid Step", grid_step);
            form->addRow("Grid Subdivisions", subdivisions);
            form->addRow("Division Number", division_count);

            auto* buttons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
            connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
            connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

            auto* layout = new QVBoxLayout(&dialog);
            layout->addWidget(description);
            layout->addLayout(form);
            layout->addWidget(buttons);

            if (dialog.exec() != QDialog::Accepted) {
                return;
            }
            settings.setValue(
                "view/customGridStep",
                DisplayToMillimeters(grid_step->value(), unit));
            settings.setValue(
                "view/customGridSubdivisions", subdivisions->value());
            settings.setValue(
                "view/customGridDivisionCount", division_count->value());
            settings.setValue("view/gridDensityMode", "custom");
            if (custom_grid_action) {
                custom_grid_action->setChecked(true);
            }
            viewport_->ReloadModelingPreferences();
            statusBar()->showMessage("Custom grid parameters applied", 1600);
        });
    auto* zebra_analysis_action = view_menu->addAction("Zebra Analysis");
    zebra_analysis_action->setCheckable(true);
    {
        QSettings settings;
        const bool enabled =
            settings.value("view/zebraAnalysis", false).toBool();
        CMesh3D::SetZebraAnalysisEnabled(enabled);
        zebra_analysis_action->setChecked(enabled);
    }
    connect(
        zebra_analysis_action, &QAction::toggled, this,
        [this](bool enabled) {
            CMesh3D::SetZebraAnalysisEnabled(enabled);
            QSettings settings;
            settings.setValue("view/zebraAnalysis", enabled);
            viewport_->update();
            statusBar()->showMessage(
                enabled
                    ? "Zebra Analysis: selected object, or all visible bodies when nothing is selected"
                    : "Zebra Analysis disabled",
                2400);
        });
    view_menu->addSeparator();

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
    solid_hidden_line_action_ = add_solid_display_action("Hidden Lines", SolidDisplayMode::HiddenLine);
    solid_hidden_line_hatch_action_ = add_solid_display_action("Hidden Lines Hatch", SolidDisplayMode::HiddenLineHatch);
    view_menu->addAction("BackGround Color...", this, [this]() {
        QSettings settings;
        QColor current(
            settings.value("view/backgroundColor", QStringLiteral("#0e1114")).toString());
        if (!current.isValid()) current = QColor(QStringLiteral("#0e1114"));
        const QColor chosen = QColorDialog::getColor(
            current, this, QStringLiteral("Background Color"));
        if (!chosen.isValid()) return;
        settings.setValue(
            "view/backgroundColor", chosen.name(QColor::HexRgb));
        viewport_->SetBackgroundColor(chosen);
        statusBar()->showMessage("Background color changed", 1400);
    });
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
    auto* xy_view_action = add_action("View XY", QKeySequence(Qt::Key_2 | Qt::KeypadModifier), [this]() {
        viewport_->SetXYView();
    });
    view_menu->addAction(xy_view_action);
    projection_status_label_ = new QLabel(this);
    projection_status_label_->setMinimumWidth(108);
    projection_status_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    projection_status_label_->setStyleSheet("QLabel { color: #777777; padding-right: 4px; }");
    statusBar()->addPermanentWidget(projection_status_label_);
    camera_fov_spin_ = new QDoubleSpinBox(this);
    camera_fov_spin_->setRange(20.0, 100.0);
    camera_fov_spin_->setDecimals(0);
    camera_fov_spin_->setSingleStep(1.0);
    camera_fov_spin_->setPrefix("FOV ");
    camera_fov_spin_->setSuffix(QString::fromUtf8("\u00b0"));
    camera_fov_spin_->setValue(viewport_->GetVerticalFovDegrees());
    camera_fov_spin_->setMinimumWidth(88);
    camera_fov_spin_->setToolTip(
        "Vertical camera field of view. Default: 50 degrees. Used by Walk and Render.");
    statusBar()->addPermanentWidget(camera_fov_spin_);
    connect(camera_fov_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double value) {
                viewport_->SetVerticalFovDegrees(static_cast<float>(value));
            });
    connect(viewport_, &OpenGLViewport::CameraFieldOfViewChanged,
            this, [this](float value) {
                if (!camera_fov_spin_) return;
                const QSignalBlocker blocker(camera_fov_spin_);
                camera_fov_spin_->setValue(value);
            });
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
    auto* walk_action = add_action("Walk Through Room", {}, [this]() {
        SetTool(ToolMode::Walk,
                "Walk: click inside a room on the map; arrows move; left mouse rotates; Esc exits");
    });
    walk_action->setIcon(WalkCameraIcon());
    auto* select_action = add_action("Select", {}, [this]() { SetTool(ToolMode::Select, "Select objects"); });
    auto* zoom_rect_action = add_action("Zoom By Rect", QKeySequence(Qt::Key_F2), [this]() {
        SetTool(ToolMode::ZoomRect, "Zoom By Rect: drag the area to enlarge");
    });
    auto* curve_action = add_action("Curve", {}, [this]() {
        ActivateParametricTool("PolylineCurve");
    });
    auto* transform_action = add_action("Transform", {}, [this]() { BeginTransformTool(TransformOperation::Move); });
    auto* move_action = add_action("Move", QKeySequence(Qt::Key_M), [this]() { BeginTransformTool(TransformOperation::Move); });
    auto* rotate_action = add_action("Rotate", {}, [this]() { BeginTransformTool(TransformOperation::Rotate); });
    auto* scale_action = add_action("Scale", {}, [this]() { BeginTransformTool(TransformOperation::Scale); });
    auto* precise_move_action = add_action("Move with Dialog...", {}, [this]() { ShowPreciseMoveDialog(); });
    auto* precise_rotate_action = add_action("Rotate with Dialog...", {}, [this]() { ShowPreciseRotateDialog(); });
    auto* precise_scale_action = add_action("Scale with Dialog...", {}, [this]() { ShowPreciseScaleDialog(); });
    auto* new_sketch_action = add_action("New Sketch", {}, [this]() { BeginNewSketch(); });
    auto* drawing_text_action = add_action("Text...", {}, [this]() { BeginCreateDrawingText(); });
    auto* duplicate_action = add_action("Make Object/Group Copy", {}, [this]() { DuplicateSelectedObject(); });
    duplicate_action->setToolTip("Make a copy of the selected object or whole group and move it");
    duplicate_action->setIcon(DuplicateObjectIcon());
    auto* mirror_action = add_action("Mirror Object by Plane...", {}, [this]() { MirrorSelectedObject(); });
    mirror_action->setToolTip("Create a mirrored copy of the selected object or group");
    mirror_action->setIcon(MirrorObjectIcon());
    auto* all_scene_action = add_action("All Scene", QKeySequence(Qt::Key_F4), [this]() {
        viewport_->FitToDocument();
        statusBar()->showMessage("All scene fitted", 1400);
    });
    auto* material_editor_action = add_action("Material Editor...", {}, [this]() {
        ShowMaterialEditor();
    });
    auto* apply_film_action = add_action("Apply Film...", {}, [this]() {
        ShowSurfaceFilmDialog();
    });
    auto* layer_properties_action = add_action("Layer Properties...", {}, [this]() {
        ShowLayerProperties();
    });
    layer_properties_action->setToolTip("Layer visibility and selectability");
    layer_properties_action->setIcon(LayerPropertiesIcon());
    auto* change_layer_action = add_action("Change Layer...", {}, [this]() {
        ChangeSelectedObjectLayer();
    });
    change_layer_action->setToolTip("Move selected objects to another layer");
    change_layer_action->setIcon(ChangeLayerIcon());
    auto* create_group_action = add_action("Create Group", {}, [this]() {
        CreateSelectedGroup();
    });
    create_group_action->setIcon(GroupIcon());
    auto* ungroup_action = add_action("UnGroup", {}, [this]() {
        UngroupSelectedGroup();
    });
    ungroup_action->setIcon(UngroupIcon());
    auto* assembly_action = add_action("Create Assembly", {}, [this]() { CreateSelectedAssembly(); });
    auto* two_sketch_action = add_action("Body by Two Sketches", {}, [this]() { CreateBodyFromTwoSketches(); });
    auto* point_move_action = add_action("Move Point to Point", {}, [this]() { viewport_->BeginMovePointToPoint(); });
    auto* point_dimension_action = add_action(
        "Point-to-Point Dimension", {}, [this]() {
            ClearActiveProperties();
            viewport_->BeginMeasurePointToPoint();
        });
    point_dimension_action->setIcon(PointDimensionIcon());
    auto* linked_clone_action = add_action("Associative Clone", {}, [this]() { CreateAssociativeClone(); });
    auto* find_command_action = add_action(
        "Find Command...", QKeySequence(Qt::CTRL | Qt::Key_F), [this]() {
            CommandSearchPopup::Show(this);
        });
    RegisterToolAction(orbit_action, "orbit");
    RegisterToolAction(walk_action, "walk");
    RegisterToolAction(select_action, "select");
    RegisterToolAction(zoom_rect_action, "zoom_rect");
    zoom_rect_action->setIcon(ZoomRectIcon());
    RegisterToolAction(curve_action, "PolylineCurve");
    RegisterToolAction(transform_action, "transform");
    RegisterToolAction(move_action, "move");
    RegisterToolAction(rotate_action, "rotate");
    RegisterToolAction(scale_action, "scale");
    RegisterToolAction(precise_move_action, "move_dialog");
    RegisterToolAction(precise_rotate_action, "rotate_dialog");
    RegisterToolAction(precise_scale_action, "scale_dialog");
    RegisterToolAction(new_sketch_action, "NewSketch");

    tools_menu->addAction(material_editor_action);
    tools_menu->addAction(apply_film_action);
    tools_menu->addAction(layer_properties_action);
    tools_menu->addAction(change_layer_action);
    tools_menu->addAction(create_group_action);
    tools_menu->addAction(ungroup_action);
    tools_menu->addAction(assembly_action);
    tools_menu->addAction(two_sketch_action);
    tools_menu->addAction(point_move_action);
    tools_menu->addAction(point_dimension_action);
    tools_menu->addAction(linked_clone_action);
    tools_menu->addAction(find_command_action);
    tools_menu->addSeparator();
    tools_menu->addAction(orbit_action);
    tools_menu->addAction(walk_action);
    tools_menu->addAction(select_action);
    tools_menu->addAction(zoom_rect_action);
    tools_menu->addAction(curve_action);
    tools_menu->addAction(transform_action);
    tools_menu->addSeparator();
    tools_menu->addAction(move_action);
    tools_menu->addAction(rotate_action);
    tools_menu->addAction(scale_action);
    tools_menu->addSeparator();
    tools_menu->addAction(precise_move_action);
    tools_menu->addAction(precise_rotate_action);
    tools_menu->addAction(precise_scale_action);
    tools_menu->addAction(mirror_action);
    tools_menu->addAction(new_sketch_action);
    tools_menu->addAction(drawing_text_action);
    tools_menu->addSeparator();
    QAction* facade_manager_action = add_action(
        "Facade Manager...", {}, [this]() { ShowFacadeManager(); });
    tools_menu->addAction(facade_manager_action);
    tools_menu->addSeparator();
    for (const ToolDefinition& tool : tool_registry_.Tools()) {
        // Kept in the registry for reopening old documents. The public Fillet
        // Solid command dispatches to it automatically when a body is selected.
        if (tool.id == "fillet_all_edges") {
            continue;
        }
        auto* action = tools_menu->addAction(QString::fromStdString(tool.label), this, [this, id = tool.id]() {
            ActivateParametricTool(id);
        });
        RegisterToolAction(action, tool.id);
    }

    edit_menu->addAction(facade_manager_action);
    edit_menu->addSeparator();
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
    edit_menu->addAction(add_action("Select All Visible", QKeySequence::SelectAll, [this]() {
        const size_t count = document_.SelectAllVisibleObjects();
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage(QString("Selected %1 visible object(s)").arg(count), 1200);
    }));

    auto* help_topics_action = add_action("Help &Topics", QKeySequence::HelpContents, [this]() {
        QMessageBox::information(this, "Help Topics", "Help system will be added here.");
    });
    languages.BindText(help_topics_action, "HelpTopics", "Help &Topics");
    help_menu->addAction(help_topics_action);

    auto* language_menu = help_menu->addMenu("Language");
    languages.BindText(language_menu, "MenuLanguage", "Language");
    auto* language_group = new QActionGroup(language_menu);
    language_group->setExclusive(true);
    for (const QString& language : languages.AvailableLanguages()) {
        auto* action = language_menu->addAction(languages.LanguageDisplayName(language));
        action->setCheckable(true);
        action->setChecked(language.compare(
            languages.CurrentLanguage(), Qt::CaseInsensitive) == 0);
        language_group->addAction(action);
        connect(action, &QAction::triggered, this, [language]() {
            LanguageManager::Instance().SetLanguage(language);
        });
    }
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

    main_toolbar_->addAction(zoom_rect_action);
    main_toolbar_->addAction(all_scene_action);
    main_toolbar_->addSeparator();
    main_toolbar_->addAction(catalog_action);
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

    auto* opacity_label = new QLabel("Opacity", main_toolbar_);
    opacity_label->setStyleSheet("QLabel { padding-left: 6px; padding-right: 2px; }");
    main_toolbar_->addWidget(opacity_label);
    mesh_opacity_slider_ = new QSlider(Qt::Horizontal, main_toolbar_);
    mesh_opacity_slider_->setToolTip(
        "Opacity of the selected reference image or mesh surfaces");
    mesh_opacity_slider_->setRange(0, 100);
    mesh_opacity_slider_->setSingleStep(1);
    mesh_opacity_slider_->setPageStep(5);
    mesh_opacity_slider_->setFixedWidth(96);
    mesh_opacity_slider_->setValue(static_cast<int>(std::round(CMesh3D::GetSurfaceOpacity() * 100.0f)));
    connect(mesh_opacity_slider_, &QSlider::valueChanged, this, [this](int value) {
        SetMeshSurfaceOpacity(static_cast<float>(value) / 100.0f);
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
    auto* line_weight_button = new QPushButton("Weight", main_toolbar_);
    line_weight_button->setToolTip("Set selected curve line weight");
    line_weight_button->setMinimumWidth(66);
    connect(line_weight_button, &QPushButton::clicked,
            this, &MainWindow::ShowLineWeightDialog);
    main_toolbar_->addWidget(line_weight_button);
    auto* line_style_button = new QPushButton("Style", main_toolbar_);
    line_style_button->setToolTip("Set selected curve line style");
    line_style_button->setMinimumWidth(60);
    connect(line_style_button, &QPushButton::clicked,
            this, &MainWindow::ShowLineStyleDialog);
    main_toolbar_->addWidget(line_style_button);
    auto* draw_edges_button = new QPushButton("Draw Edges", main_toolbar_);
    draw_edges_button->setToolTip("Draw body edges");
    draw_edges_button->setCheckable(true);
    draw_edges_button->setChecked(CSolid::IsEdgeDrawingEnabled());
    draw_edges_button->setMinimumWidth(86);
    connect(draw_edges_button, &QPushButton::toggled, this, [this](bool checked) {
        CSolid::SetEdgeDrawingEnabled(checked);
        QSettings settings;
        settings.setValue("view/drawSolidEdges", checked);
        viewport_->update();
        statusBar()->showMessage(
            checked ? "Body edges: visible" : "Body edges: hidden", 1200);
    });
    main_toolbar_->addWidget(draw_edges_button);
    UpdateActiveToolUi(active_tool_key_);
}

void MainWindow::CreateVerticalToolBar() {
    vertical_tools_dock_ = new QDockWidget(this);
    vertical_tools_dock_->setObjectName("VerticalToolsDock");
    vertical_tools_dock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    vertical_tools_dock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    vertical_tools_dock_->setMinimumWidth(36);
    vertical_tools_dock_->setMaximumWidth(46);
    vertical_tools_dock_->setTitleBarWidget(new QWidget(vertical_tools_dock_));

    vertical_toolbar_ = new QToolBar("Vertical Tools", vertical_tools_dock_);
    vertical_toolbar_->setObjectName("VerticalTools");
    vertical_toolbar_->setOrientation(Qt::Vertical);
    vertical_toolbar_->setMovable(false);
    vertical_toolbar_->setFloatable(false);
    vertical_toolbar_->setIconSize(QSize(22, 22));
    vertical_toolbar_->setStyleSheet(
        "QToolBar#VerticalTools {"
        "  background: #d7d7d7;"
        "  border: 1px solid #9a9a9a;"
        "  spacing: 2px;"
        "  padding: 2px;"
        "}"
        "QToolButton {"
        "  background: #eeeeee;"
        "  border: 1px solid #8f8f8f;"
        "  border-radius: 2px;"
        "  padding: 2px;"
        "}"
        "QToolButton:hover {"
        "  background: #ffffff;"
        "  border-color: #3f84d8;"
        "}"
        "QToolButton:checked {"
        "  background: #1f7ae0;"
        "  border-color: #0f4f9a;"
        "}"
        "QToolButton::menu-indicator {"
        "  image: none;"
        "  width: 0px;"
        "}"
    );

    const auto add_flyout = [this](const std::string& initial_tool_key,
                                  const std::vector<std::pair<std::string, QString>>& tools) {
        auto* button = new QToolButton(vertical_toolbar_);
        button->setIconSize(QSize(22, 22));
        button->setFixedSize(30, 28);
        button->setPopupMode(QToolButton::DelayedPopup);
        button->setCheckable(true);
        button->setProperty("persistentToolButton", true);
        button->setProperty("selectedToolKey", QString::fromStdString(initial_tool_key));
        QStringList group_keys;
        for (const auto& tool : tools) {
            group_keys.push_back(QString::fromStdString(tool.first));
        }
        button->setProperty("toolGroupKeys", group_keys);
        tool_buttons_.push_back(button);

        const auto set_selected_tool = [this, button, tools](const std::string& key) {
            const auto selected = std::find_if(
                tools.begin(), tools.end(), [&key](const auto& tool) { return tool.first == key; });
            if (selected == tools.end()) {
                return;
            }
            button->setProperty("selectedToolKey", QString::fromStdString(selected->first));
            button->setIcon(ToolIcon(selected->first));
            button->setToolTip(selected->second);
            if (button->icon().isNull()) {
                button->setText(selected->second.left(4));
            } else {
                button->setText({});
            }
        };
        set_selected_tool(initial_tool_key);

        connect(button, &QToolButton::clicked, this, [this, button]() {
            const std::string key = button->property("selectedToolKey").toString().toStdString();
            if (!key.empty()) {
                ActivateParametricTool(key);
            }
        });

        auto* menu = new QMenu(button);
        menu->setStyleSheet(
            "QMenu {"
            "  background: #ffffff;"
            "  border: 1px solid #b6b6b6;"
            "  border-radius: 6px;"
            "  padding: 4px;"
            "}"
        );

        auto* action = new QWidgetAction(menu);
        auto* panel = new QWidget(menu);
        auto* row = new QHBoxLayout(panel);
        row->setContentsMargins(4, 3, 4, 3);
        row->setSpacing(4);

        for (const auto& tool : tools) {
            auto* child = new QToolButton(panel);
            child->setToolTip(tool.second);
            child->setIcon(ToolIcon(tool.first));
            child->setIconSize(QSize(22, 22));
            child->setFixedSize(30, 28);
            RegisterToolButton(child, tool.first);
            child->setProperty("persistentToolButton", true);
            if (child->icon().isNull()) {
                child->setText(tool.second.left(4));
            }
            // QMenu grabs the mouse while a delayed popup is open. Handle the
            // press, because in this configuration it may consume the release
            // before the embedded tool button can emit clicked().
            connect(child, &QToolButton::pressed, this, [this, button, menu, set_selected_tool,
                                                         id = tool.first]() {
                set_selected_tool(id);
                menu->close();
                ActivateParametricTool(id);
            });
            row->addWidget(child);
        }

        action->setDefaultWidget(panel);
        menu->addAction(action);
        button->setMenu(menu);
        vertical_toolbar_->addWidget(button);
    };

    struct DisplayChoice {
        QString title;
        QIcon icon;
        std::function<void()> apply;
        QString fallback_text;
    };
    const auto add_display_flyout = [this](int initial_index,
                                           const std::vector<DisplayChoice>& choices,
                                           int hidden_choice_index)
                                           -> QToolButton* {
        if (choices.empty()) {
            return nullptr;
        }
        initial_index = std::clamp(initial_index, 0, static_cast<int>(choices.size()) - 1);
        auto* button = new QToolButton(vertical_toolbar_);
        button->setIconSize(QSize(22, 22));
        button->setFixedSize(30, 28);
        button->setPopupMode(QToolButton::DelayedPopup);
        button->setProperty("selectedDisplayIndex", initial_index);

        const auto select_choice = [button, choices](int index) {
            if (index < 0 || index >= static_cast<int>(choices.size())) {
                return;
            }
            button->setProperty("selectedDisplayIndex", index);
            button->setIcon(choices[static_cast<size_t>(index)].icon);
            button->setToolTip(choices[static_cast<size_t>(index)].title);
            button->setText(button->icon().isNull()
                ? choices[static_cast<size_t>(index)].fallback_text
                : QString());
        };
        select_choice(initial_index);

        connect(button, &QToolButton::clicked, this, [button, choices]() {
            const int index = button->property("selectedDisplayIndex").toInt();
            if (index >= 0 && index < static_cast<int>(choices.size())) {
                choices[static_cast<size_t>(index)].apply();
            }
        });

        auto* menu = new QMenu(button);
        menu->setStyleSheet(
            "QMenu { background: #ffffff; border: 1px solid #b6b6b6;"
            " border-radius: 6px; padding: 4px; }"
        );
        auto* action = new QWidgetAction(menu);
        auto* panel = new QWidget(menu);
        auto* row = new QHBoxLayout(panel);
        row->setContentsMargins(4, 3, 4, 3);
        row->setSpacing(4);
        for (int index = 0; index < static_cast<int>(choices.size()); ++index) {
            if (index == hidden_choice_index) {
                continue;
            }
            const DisplayChoice& choice = choices[static_cast<size_t>(index)];
            auto* child = new QToolButton(panel);
            child->setToolTip(choice.title);
            child->setIcon(choice.icon);
            child->setIconSize(QSize(22, 22));
            child->setFixedSize(30, 28);
            if (child->icon().isNull()) child->setText(choice.fallback_text);
            connect(child, &QToolButton::pressed, this,
                    [menu, select_choice, choices, index]() {
                select_choice(index);
                menu->close();
                choices[static_cast<size_t>(index)].apply();
            });
            row->addWidget(child);
        }
        action->setDefaultWidget(panel);
        menu->addAction(action);
        button->setMenu(menu);
        vertical_toolbar_->addWidget(button);
        return button;
    };

    const auto add_direct_button = [this](const QString& title,
                                          const std::string& key,
                                          const QIcon& icon,
                                          const QString& fallback_text,
                                          const std::function<void()>& handler,
                                          bool mode_button = true) {
        auto* button = new QToolButton(vertical_toolbar_);
        button->setToolTip(title);
        button->setIcon(icon);
        button->setIconSize(QSize(22, 22));
        button->setFixedSize(30, 28);
        if (mode_button) {
            RegisterToolButton(button, key);
            button->setProperty("persistentToolButton", true);
        }
        if (button->icon().isNull()) {
            button->setText(fallback_text);
        }
        connect(button, &QToolButton::clicked, this, [handler]() {
            handler();
        });
        vertical_toolbar_->addWidget(button);
        return button;
    };

    add_direct_button("Orbit camera", "orbit", ToolIcon("orbit"), "Or", [this]() {
        SetTool(ToolMode::Orbit, "Orbit camera");
    });
    add_direct_button("Walk through room", "walk", WalkCameraIcon(), "Walk", [this]() {
        SetTool(ToolMode::Walk,
                "Walk: click inside a room on the map; arrows move; left mouse rotates; Esc exits");
    });
    add_direct_button("Select objects", "select", ToolIcon("select"), "Sel", [this]() {
        SetTool(ToolMode::Select, "Select objects");
    });
    add_direct_button("Layer Properties", "LayerProperties", LayerPropertiesIcon(), "Ly", [this]() {
        ShowLayerProperties();
    });
    add_direct_button("Change Layer", "ChangeLayer", ChangeLayerIcon(), "CL", [this]() {
        ChangeSelectedObjectLayer();
    });
    add_direct_button("Create Group", "CreateGroup", GroupIcon(), "Grp", [this]() {
        CreateSelectedGroup();
    });
    add_direct_button("UnGroup", "UnGroup", UngroupIcon(), "UnG", [this]() {
        UngroupSelectedGroup();
    });
    add_direct_button("Create Assembly", "CreateAssembly", QIcon(), "Asm", [this]() {
        CreateSelectedAssembly();
    });
    add_direct_button("Move Point to Point", "MovePointToPoint", QIcon(), "P2P", [this]() {
        viewport_->BeginMovePointToPoint();
    });
    add_direct_button(
        "Point-to-Point Dimension", "MeasurePointToPoint",
        PointDimensionIcon(), "Dim", [this]() {
            ClearActiveProperties();
            viewport_->BeginMeasurePointToPoint();
        });
    add_direct_button("Associative Clone", "AssociativeClone", QIcon(), "LnC", [this]() {
        CreateAssociativeClone();
    });
    add_direct_button("Transform", "move", ToolIcon("transform"), "Tr", [this]() {
        BeginTransformTool(TransformOperation::Move);
    });
    add_direct_button("Make Object/Group Copy", "duplicate", DuplicateObjectIcon(), "Cp", [this]() {
        DuplicateSelectedObject();
    });
    add_direct_button("Mirror Object by Plane", "mirror", MirrorObjectIcon(), "Mr", [this]() {
        MirrorSelectedObject();
    });

    vertical_toolbar_->addSeparator();

    QToolButton* move_flyout = add_display_flyout(
        0,
        {
            {"Move", ToolIcon("move"),
             [this]() { BeginTransformTool(TransformOperation::Move); }, "Mv"},
            {"Move with Dialog", QIcon(),
             [this]() { ShowPreciseMoveDialog(); }, "MD"}
        }, -1);
    QToolButton* rotate_flyout = add_display_flyout(
        0,
        {
            {"Rotate", ToolIcon("rotate"),
             [this]() { BeginTransformTool(TransformOperation::Rotate); }, "Rt"},
            {"Rotate with Dialog", QIcon(),
             [this]() { ShowPreciseRotateDialog(); }, "RD"}
        }, -1);
    QToolButton* scale_flyout = add_display_flyout(
        0,
        {
            {"Scale", ToolIcon("scale"),
             [this]() { BeginTransformTool(TransformOperation::Scale); }, "Sc"},
            {"Scale with Dialog", QIcon(),
             [this]() { ShowPreciseScaleDialog(); }, "SD"}
        }, -1);
    const auto register_transform_flyout = [this](
        QToolButton* button, const std::string& primary_key,
        const QString& dialog_key) {
        if (!button) return;
        RegisterToolButton(button, primary_key);
        button->setProperty(
            "toolGroupKeys",
            QStringList{QString::fromStdString(primary_key), dialog_key});
        button->setProperty("persistentToolButton", true);
    };
    register_transform_flyout(move_flyout, "move", "move_dialog");
    register_transform_flyout(rotate_flyout, "rotate", "rotate_dialog");
    register_transform_flyout(scale_flyout, "scale", "scale_dialog");
    add_direct_button("Material Editor", "MaterialEditor", MaterialEditorIcon(), "Mat", [this]() {
        ShowMaterialEditor(has_selected_library_material_ ? &selected_library_material_ : nullptr);
    }, false);
    edit_texture_button_ = add_direct_button(
        "Edit Texture", "EditTexture", EditTextureIcon(), "Tex", [this]() {
            ShowSurfaceTextureEditor();
        }, false);
    add_direct_button("Apply Film", "ApplyFilm", QIcon(), "Film", [this]() {
        ShowSurfaceFilmDialog();
    }, false);
    add_direct_button("View Last Render", "ViewRender", QIcon(), "Rend", [this]() {
        ViewLastRenderResult();
    }, false);
    add_direct_button("New Sketch", "NewSketch", NewSketchIcon(), "Sk", [this]() {
        BeginNewSketch();
    });
    add_direct_button("Polyline", "PolylineCurve", ToolIcon("PolylineCurve"), "Pl", [this]() {
        ActivateParametricTool("PolylineCurve");
    });
    add_direct_button("Text", "DrawingText", QIcon(), "T", [this]() {
        BeginCreateDrawingText();
    }, false);

    vertical_toolbar_->addSeparator();

    int solid_display_index = 3;
    if (CSolid::GetDisplayMode() == SolidDisplayMode::Wireframe) {
        solid_display_index = 0;
    } else if (CSolid::GetDisplayMode() == SolidDisplayMode::HiddenLine) {
        solid_display_index = 1;
    } else if (CSolid::GetDisplayMode() == SolidDisplayMode::HiddenLineHatch) {
        solid_display_index = 2;
    } else if (CSolid::GetDisplayMode() == SolidDisplayMode::SurfacesAndRaisedMesh) {
        solid_display_index = 4;
    } else if (CSolid::GetDisplayMode() == SolidDisplayMode::MeshOnly) {
        solid_display_index = 5;
    }
    QToolButton* solid_display_button = add_display_flyout(
        solid_display_index,
        {
            {"Wireframe", SolidGeometryDisplayIcon(0),
             [this]() { SetSolidDisplayMode(SolidDisplayMode::Wireframe); }},
            {"Hidden Lines", SolidGeometryDisplayIcon(1),
             [this]() { SetSolidDisplayMode(SolidDisplayMode::HiddenLine); }},
            {"Hidden Lines Hatch", SolidGeometryDisplayIcon(4),
             [this]() { SetSolidDisplayMode(SolidDisplayMode::HiddenLineHatch); }},
            {"Surfaces and Edges", SolidGeometryDisplayIcon(2),
             [this]() { SetSolidDisplayMode(SolidDisplayMode::SurfacesAndEdges); }},
            {"Surfaces and Raised Mesh", SolidGeometryDisplayIcon(5),
             [this]() { SetSolidDisplayMode(SolidDisplayMode::SurfacesAndRaisedMesh); }},
            {"Mesh", SolidGeometryDisplayIcon(3),
             [this]() { SetSolidDisplayMode(SolidDisplayMode::MeshOnly); }}
        }, 3);

    int surface_display_index = 0;
    if (CMesh3D::GetDisplayMode() == MeshDisplayMode::SurfaceGray) {
        surface_display_index = 1;
    } else if (CMesh3D::GetDisplayMode() == MeshDisplayMode::SurfaceColored) {
        surface_display_index = 2;
    }
    const auto activate_surface_display = [this, solid_display_button](MeshDisplayMode mode) {
        const SolidDisplayMode solid_mode = CSolid::GetDisplayMode();
        if (solid_mode == SolidDisplayMode::Wireframe
            || solid_mode == SolidDisplayMode::HiddenLine
            || solid_mode == SolidDisplayMode::HiddenLineHatch
            || solid_mode == SolidDisplayMode::MeshOnly) {
            SetSolidDisplayMode(SolidDisplayMode::SurfacesAndEdges);
            if (solid_display_button) {
                solid_display_button->setProperty("selectedDisplayIndex", 3);
                solid_display_button->setIcon(SolidGeometryDisplayIcon(2));
                solid_display_button->setToolTip("Surfaces and Edges");
            }
        }
        SetMeshDisplayMode(mode);
    };
    add_display_flyout(
        surface_display_index,
        {
            {"Texture", SurfaceAppearanceIcon(0),
             [activate_surface_display]() {
                 activate_surface_display(MeshDisplayMode::SurfaceMaterial);
             }},
            {"Gray", SurfaceAppearanceIcon(1),
             [activate_surface_display]() {
                 activate_surface_display(MeshDisplayMode::SurfaceGray);
             }},
            {"RGB", SurfaceAppearanceIcon(2),
             [activate_surface_display]() {
                 activate_surface_display(MeshDisplayMode::SurfaceColored);
             }}
        }, -1);

    vertical_toolbar_->addSeparator();

    add_flyout(
        "SolidLowPoly",
        {
            {"MeshFillContour", "Fiill Contour"},
            {"SolidLowPoly", "Low Poly"}
        });

    vertical_tools_dock_->setWidget(vertical_toolbar_);
    addDockWidget(Qt::LeftDockWidgetArea, vertical_tools_dock_);
    if (tools_dock_) {
        splitDockWidget(tools_dock_, vertical_tools_dock_, Qt::Horizontal);
        resizeDocks({tools_dock_, vertical_tools_dock_}, {104, 40}, Qt::Horizontal);
    }
}

void MainWindow::CreateDocks() {
    tools_dock_ = new QDockWidget("Architecture", this);
    tools_dock_->setObjectName("ToolsDock");
    tools_dock_->setMinimumWidth(104);
    CreateToolsPanel(tools_dock_);
    addDockWidget(Qt::LeftDockWidgetArea, tools_dock_);
    CreateVerticalToolBar();

    scene_tree_dock_ = new QDockWidget("Scene Tree", this);
    scene_tree_dock_->setObjectName("SceneTreeDock");
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
    connect(scene_tree_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int column) {
        OnSceneTreeItemDoubleClicked(item, column);
    });

    properties_dock_ = new QDockWidget("Property Panel", this);
    properties_dock_->setObjectName("PropertiesDock");
    properties_dock_->setWidget(property_panel_);
    properties_dock_->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, properties_dock_);
    properties_dock_->hide();

    CreateMaterialLibraryDock();
}

void MainWindow::CreateMaterialLibraryDock() {
    material_library_dock_ = new QDockWidget("Materials Library", this);
    material_library_dock_->setObjectName("MaterialLibraryDock");
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
    category_combo->addItem("Default", "Default");
    category_combo->addItem("Document", "Document");
    for (const QString& default_category : MaterialLibrary::DefaultCategories()) {
        if (default_category != "Default" && categories.find(default_category) != categories.end()) {
            category_combo->addItem(default_category, default_category);
            categories.erase(default_category);
        }
    }
    categories.erase("Default");
    for (const QString& category : categories) {
        category_combo->addItem(category, category);
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
            if ((i % 6) == 0) {
                QCoreApplication::processEvents(
                    QEventLoop::ExcludeUserInputEvents);
            }
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
        populate_materials(category_combo->currentData().toString(), search_edit->text());
    };

    populate_materials(category_combo->currentData().toString(), search_edit->text());
    connect(category_combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [populate_materials, search_edit, category_combo](int) {
        populate_materials(category_combo->currentData().toString(), search_edit->text());
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
        populate_materials(category_combo->currentData().toString(), query);
    });
    connect(material_list, &QListWidget::itemClicked, this, [this, material_from_item](QListWidgetItem* item) {
        Material material;
        if (!material_from_item(item, &material)) {
            has_selected_library_material_ = false;
            return;
        }
        selected_library_material_ = material;
        has_selected_library_material_ = true;
        if (!pending_material_parameter_id_.isEmpty()) {
            const QString parameter_id = pending_material_parameter_id_;
            pending_material_parameter_id_.clear();

            const auto source = static_cast<MaterialListSource>(
                item->data(kMaterialSourceRole).toInt());
            if (source == MaterialListSource::Library) {
                // Library IDs belong to a different namespace.  Let the
                // document allocate a safe ID instead of replacing an
                // unrelated material with the same numeric ID.
                material.id = 0;
            }
            const Material& stored = document_.UpsertMaterial(material);
            const unsigned long stored_id = stored.id;
            const std::string stored_name = stored.name;
            property_panel_->SetMaterialParameterValue(
                parameter_id.toStdString(),
                static_cast<double>(stored_id),
                stored_name);
            statusBar()->showMessage(
                QString("Material assigned: %1")
                    .arg(QString::fromStdString(stored_name)),
                1800);
            return;
        }
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
            populate_materials(category_combo->currentData().toString(), search_edit->text());
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
            category_combo->setCurrentIndex(category_combo->findData("Document"));
            populate_materials(category_combo->currentData().toString(), search_edit->text());
        } else if (chosen == delete_action) {
            if (document_.DeleteMaterial(material.id)) {
                RefreshSceneTree();
                viewport_->update();
                populate_materials(category_combo->currentData().toString(), search_edit->text());
                statusBar()->showMessage(QString("Material deleted from document: %1").arg(QString::fromStdString(material.name)), 1400);
            }
        }
    });
    connect(add_button, &QPushButton::clicked, this, [this, library, material_list, material_from_item, populate_materials, category_combo, search_edit]() {
        QString category = category_combo->currentData().toString();
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
        if (category_combo->findData(category) < 0) {
            category_combo->addItem(category, category);
        }
        category_combo->setCurrentIndex(category_combo->findData(category));
        populate_materials(category_combo->currentData().toString(), search_edit->text());
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
    if (key == "TrimMeshTest") {
        button->setIcon(QIcon());
        button->setText("Trim M");
    } else if (key == "ClassifyFaceCut") {
        button->setIcon(QIcon());
        button->setText("Face Cut");
    } else if (key == "MeshFillContour") {
        button->setIcon(QIcon());
        button->setText("Fill");
    } else if (key == "DrawSpline") {
        button->setIcon(ToolIcon(key));
        button->setText("Draw");
    } else if (key == "CurveJoin") {
        button->setIcon(QIcon()); button->setText("Join");
    } else if (key == "CurveSplit") {
        button->setIcon(QIcon()); button->setText("Split");
    } else if (key == "CurveExtend") {
        button->setIcon(QIcon()); button->setText("Extend");
    } else if (key == "CurveTrimByPlane") {
        button->setIcon(QIcon()); button->setText("Trim P");
    } else if (key == "CurveSimplifyByPoint") {
        button->setIcon(QIcon()); button->setText("Split P");
    } else if (key == "CurveReverse") {
        button->setIcon(QIcon()); button->setText("Reverse");
    } else if (key == "NurbsParametersTool") {
        button->setIcon(QIcon()); button->setText("NURBS P");
    }
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
    std::set<QString> expanded_legacy_groups;
    std::set<QString> seen_legacy_groups;
    std::set<unsigned long> expanded_object_groups;
    std::set<unsigned long> seen_object_groups;
    std::function<void(QTreeWidgetItem*)> remember_expansion;
    remember_expansion = [&](QTreeWidgetItem* item) {
        if (!item) {
            return;
        }
        const QVariant group_id = item->data(0, kSceneTreeGroupIdRole);
        if (group_id.isValid()) {
            const unsigned long id = static_cast<unsigned long>(group_id.toULongLong());
            seen_object_groups.insert(id);
            if (item->isExpanded()) {
                expanded_object_groups.insert(id);
            }
        } else {
            const QVariant group_name = item->data(0, kSceneTreeGroupRole);
            if (group_name.isValid()) {
                seen_legacy_groups.insert(group_name.toString());
                if (item->isExpanded()) {
                    expanded_legacy_groups.insert(group_name.toString());
                }
            }
        }
        for (int child_index = 0; child_index < item->childCount(); ++child_index) {
            remember_expansion(item->child(child_index));
        }
    };
    for (int i = 0; i < scene_tree_->topLevelItemCount(); ++i) {
        remember_expansion(scene_tree_->topLevelItem(i));
    }

    scene_tree_->clear();
    const auto& objects = document_.GetObjects();
    const auto is_catalog_resource = [](const CAlfaObject* object) {
        if (!object) {
            return false;
        }
        const std::string& tool_id = object->GetParametricToolId();
        return tool_id == "CatalogProfileResource"
            || tool_id == "CatalogHandleResource";
    };
    auto* parts_root = new QTreeWidgetItem;
    parts_root->setText(1, "Parts");
    parts_root->setText(2, "Catalog / imported");
    parts_root->setData(0, kSceneTreePartsRootRole, true);
    parts_root->setExpanded(true);
    std::map<unsigned long, size_t> object_indices_by_id;
    for (size_t i = 0; i < objects.size(); ++i) {
        if (objects[i] && objects[i]->m_id != 0) {
            object_indices_by_id[objects[i]->m_id] = i;
        }
    }

    std::map<size_t, size_t> member_group_indices;
    for (size_t group_index = 0; group_index < objects.size(); ++group_index) {
        const auto* group = dynamic_cast<const CGroup*>(objects[group_index].get());
        if (!group) {
            continue;
        }
        for (unsigned long id : group->GetElementIds()) {
            const auto element = object_indices_by_id.find(id);
            if (element != object_indices_by_id.end() && element->second != group_index) {
                member_group_indices[element->second] = group_index;
            }
        }
    }

    const auto object_type = [](const CAlfaObject& object) {
        if (dynamic_cast<const CAssembled*>(&object)) {
            return QString("Assembly");
        }
        if (dynamic_cast<const CPart*>(&object)) {
            return QString("Part");
        }
        if (dynamic_cast<const CGroup*>(&object)) {
            return QString("Group");
        }
        if (dynamic_cast<const CPolyline*>(&object)) {
            return QString("Curve");
        }
        if (dynamic_cast<const CDrawingText*>(&object)) {
            return QString("Text");
        }
        if (dynamic_cast<const CBSpline*>(&object)) {
            return QString("B-Spline");
        }
        if (dynamic_cast<const CCadCurve3D*>(&object)) {
            return QString("CAD Curve");
        }
        if (dynamic_cast<const CReferenceImage*>(&object)) {
            return QString("Reference Image");
        }
        if (dynamic_cast<const CMesh3D*>(&object)) {
            return QString("Mesh");
        }
        if (dynamic_cast<const CSurfaceSet*>(&object)) {
            return QString("Surface Set");
        }
        if (dynamic_cast<const CSolid*>(&object)) {
            return QString("Solid");
        }
        return QString("Object");
    };

    const auto fill_object_item = [this, &objects, &object_type](QTreeWidgetItem& item, size_t index) {
        const CAlfaObject& object = *objects[index];
        item.setIcon(0, SceneVisibilityIcon(document_.IsObjectVisible(object)));
        item.setToolTip(0, document_.IsObjectVisible(object) ? "Hide object" : "Show object");
        item.setText(1, QString::fromStdString(object.GetName()));
        item.setText(2, object_type(object));
        item.setData(0, kSceneTreeObjectIndexRole, static_cast<qulonglong>(index));
        if (document_.IsObjectSelected(index)) {
            item.setSelected(true);
        }
    };

    std::map<size_t, QTreeWidgetItem*> object_group_items;
    for (size_t i = 0; i < objects.size(); ++i) {
        const auto* group = dynamic_cast<const CGroup*>(objects[i].get());
        if (!group) {
            continue;
        }
        auto* item = new QTreeWidgetItem;
        fill_object_item(*item, i);
        item->setToolTip(0, document_.IsObjectVisible(*group) ? "Hide group" : "Show group");
        item->setData(0, kSceneTreeGroupIdRole, static_cast<qulonglong>(group->m_id));
        item->setExpanded(seen_object_groups.count(group->m_id) == 0
            || expanded_object_groups.count(group->m_id) > 0);
        object_group_items[i] = item;
    }

    const auto creates_group_cycle = [&member_group_indices](size_t child_index, size_t parent_index) {
        std::set<size_t> visited{child_index};
        size_t current = parent_index;
        while (visited.insert(current).second) {
            const auto parent = member_group_indices.find(current);
            if (parent == member_group_indices.end()) {
                return false;
            }
            current = parent->second;
        }
        return true;
    };
    for (const auto& entry : object_group_items) {
        const size_t group_index = entry.first;
        QTreeWidgetItem* item = entry.second;
        const auto parent = member_group_indices.find(group_index);
        if (parent != member_group_indices.end() && !creates_group_cycle(group_index, parent->second)) {
            const auto parent_item = object_group_items.find(parent->second);
            if (parent_item != object_group_items.end()) {
                parent_item->second->addChild(item);
                continue;
            }
        }
        if (dynamic_cast<const CPart*>(objects[group_index].get())) {
            parts_root->addChild(item);
        } else {
            scene_tree_->addTopLevelItem(item);
        }
    }

    if (parts_root->childCount() > 0) {
        bool any_part_visible = false;
        for (const auto& object : objects) {
            if (const auto* part = dynamic_cast<const CPart*>(object.get())) {
                any_part_visible = any_part_visible || document_.IsObjectVisible(*part);
            }
        }
        parts_root->setIcon(0, SceneVisibilityIcon(any_part_visible));
        parts_root->setToolTip(0, any_part_visible ? "Hide all parts" : "Show all parts");
        scene_tree_->insertTopLevelItem(0, parts_root);
    } else {
        delete parts_root;
    }

    std::map<QString, QTreeWidgetItem*> legacy_group_items;

    for (size_t i = 0; i < objects.size(); ++i) {
        const CAlfaObject* object = objects[i].get();
        if (!object || dynamic_cast<const CGroup*>(object)
            || is_catalog_resource(object)) {
            continue;
        }
        if (const auto* polyline = dynamic_cast<const CPolyline*>(object); polyline && polyline->IsEmpty()) {
            continue;
        }
        if (const auto* spline = dynamic_cast<const CBSpline*>(object); spline && spline->IsEmpty()) {
            continue;
        }

        QTreeWidgetItem* parent = nullptr;
        const auto member_group = member_group_indices.find(i);
        if (member_group != member_group_indices.end()) {
            const auto group_item = object_group_items.find(member_group->second);
            if (group_item != object_group_items.end()) {
                parent = group_item->second;
            }
        }
        const QString legacy_group_name = QString::fromStdString(object->GetGroupName());
        if (!parent && !legacy_group_name.isEmpty()) {
            auto existing_group = legacy_group_items.find(legacy_group_name);
            if (existing_group == legacy_group_items.end()) {
                auto* group_item = new QTreeWidgetItem(scene_tree_);
                group_item->setText(1, legacy_group_name);
                group_item->setText(2, "Group");
                group_item->setData(0, kSceneTreeGroupRole, legacy_group_name);
                group_item->setExpanded(seen_legacy_groups.count(legacy_group_name) == 0
                    || expanded_legacy_groups.count(legacy_group_name) > 0);
                existing_group = legacy_group_items.emplace(legacy_group_name, group_item).first;
            }
            parent = existing_group->second;
        }

        auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(scene_tree_);
        fill_object_item(*item, i);
    }

    for (auto& group_entry : legacy_group_items) {
        QTreeWidgetItem* group_item = group_entry.second;
        bool any_visible = false;
        for (int i = 0; i < group_item->childCount(); ++i) {
            const QVariant object_index = group_item->child(i)->data(0, kSceneTreeObjectIndexRole);
            const size_t index = static_cast<size_t>(object_index.toULongLong());
            const bool child_visible = index < objects.size() && objects[index] && document_.IsObjectVisible(*objects[index]);
            any_visible = any_visible || child_visible;
        }
        group_item->setIcon(0, SceneVisibilityIcon(any_visible));
        group_item->setToolTip(0, any_visible ? "Hide group" : "Show group");
    }

    UpdateToolAvailability();
}

void MainWindow::OnSceneTreeItemClicked(QTreeWidgetItem* item, int column) {
    if (!item) {
        return;
    }

    auto& objects = document_.GetObjects();
    const QVariant object_index = item->data(0, kSceneTreeObjectIndexRole);
    if (column != 0) {
        if (!object_index.isValid()) {
            return;
        }
        const size_t index = static_cast<size_t>(object_index.toULongLong());
        if (index >= objects.size() || !objects[index]) {
            return;
        }

        SelectionAction action = SelectionAction::Replace;
        const Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();
        if (modifiers.testFlag(Qt::ControlModifier)) {
            action = document_.IsObjectSelected(index)
                ? SelectionAction::Remove
                : SelectionAction::Add;
        } else if (modifiers.testFlag(Qt::ShiftModifier)) {
            action = SelectionAction::Add;
        }
        document_.SelectObjectById(objects[index]->m_id, action);
        ClearActiveProperties();
        RefreshSceneTree();
        UpdateToolAvailability();
        viewport_->update();
        return;
    }

    const auto refresh_visibility_icons = [this, &objects]() {
        QSignalBlocker blocker(scene_tree_);
        std::function<bool(QTreeWidgetItem*)> refresh_item;
        refresh_item = [&](QTreeWidgetItem* tree_item) {
            bool any_child_visible = false;
            for (int child_index = 0; child_index < tree_item->childCount(); ++child_index) {
                any_child_visible = refresh_item(tree_item->child(child_index)) || any_child_visible;
            }

            bool visible = any_child_visible;
            bool has_visibility = false;
            const bool parts_root = tree_item->data(
                0, kSceneTreePartsRootRole).toBool();
            const QVariant row_object_index = tree_item->data(0, kSceneTreeObjectIndexRole);
            if (parts_root) {
                visible = any_child_visible;
                has_visibility = true;
            } else if (row_object_index.isValid()) {
                const size_t index = static_cast<size_t>(row_object_index.toULongLong());
                if (index < objects.size() && objects[index]) {
                    visible = document_.IsObjectVisible(*objects[index]);
                    has_visibility = true;
                }
            } else {
                const QVariant row_group_name = tree_item->data(0, kSceneTreeGroupRole);
                if (row_group_name.isValid()) {
                    const std::string group = row_group_name.toString().toStdString();
                    visible = false;
                    for (const auto& object : objects) {
                        visible = visible || (object && object->GetGroupName() == group
                                              && document_.IsObjectVisible(*object));
                    }
                    has_visibility = true;
                }
            }
            if (has_visibility) {
                tree_item->setIcon(0, SceneVisibilityIcon(visible));
                tree_item->setToolTip(0, parts_root
                    ? (visible ? "Hide all parts" : "Show all parts")
                    : (visible ? "Hide object" : "Show object"));
            }
            return visible;
        };
        for (int top_index = 0; top_index < scene_tree_->topLevelItemCount(); ++top_index) {
            refresh_item(scene_tree_->topLevelItem(top_index));
        }
    };

    if (object_index.isValid()) {
        const size_t index = static_cast<size_t>(object_index.toULongLong());
        if (index < objects.size() && objects[index]) {
            objects[index]->SetVisible(!objects[index]->IsVisible());
        }
        document_.ClearSelection();
        scene_tree_->clearSelection();
        refresh_visibility_icons();
        ClearActiveProperties();
        UpdateToolAvailability();
        viewport_->update();
        return;
    }

    if (item->data(0, kSceneTreePartsRootRole).toBool()) {
        bool any_part_visible = false;
        for (const auto& object : objects) {
            if (const auto* part = dynamic_cast<const CPart*>(object.get())) {
                any_part_visible = any_part_visible || document_.IsObjectVisible(*part);
            }
        }
        const bool parts_visible = !any_part_visible;
        for (auto& object : objects) {
            if (auto* part = dynamic_cast<CPart*>(object.get())) {
                part->SetVisible(parts_visible);
            }
        }
        document_.ClearSelection();
        scene_tree_->clearSelection();
        refresh_visibility_icons();
        ClearActiveProperties();
        UpdateToolAvailability();
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
        scene_tree_->clearSelection();
        refresh_visibility_icons();
        ClearActiveProperties();
        UpdateToolAvailability();
        viewport_->update();
    }
}

void MainWindow::OnSceneTreeItemDoubleClicked(QTreeWidgetItem* item, int column) {
    (void)column;
    if (!item) {
        return;
    }

    const QVariant object_index = item->data(0, kSceneTreeObjectIndexRole);
    if (!object_index.isValid()) {
        return;
    }

    auto& objects = document_.GetObjects();
    const size_t index = static_cast<size_t>(object_index.toULongLong());
    if (index >= objects.size() || !objects[index]) {
        return;
    }

    CAlfaObject& object = *objects[index];
    if (auto* text = dynamic_cast<CDrawingText*>(&object)) {
        EditDrawingText(*text);
        return;
    }
    bool accepted = false;
    const QString name = QInputDialog::getText(
        this,
        "Rename Object",
        "Object name",
        QLineEdit::Normal,
        QString::fromStdString(object.GetName()),
        &accepted).trimmed();
    if (!accepted) {
        return;
    }
    if (name.isEmpty()) {
        statusBar()->showMessage("Object name cannot be empty", 1400);
        return;
    }

    object.SetName(name.toStdString());
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage(QString("Object renamed: %1").arg(name), 1400);
}

void MainWindow::SetTool(ToolMode tool, const QString& status_text) {
    low_poly_pick_pending_ = false;
    if (pending_group_command_ != PendingGroupCommand::None) {
        pending_group_command_ = PendingGroupCommand::None;
        viewport_->SetSelectionConfirmationMode(false);
    }
    ClearActiveProperties();
    if (tool == ToolMode::Walk) {
        viewport_->SetOrthographicProjection(false);
        viewport_->SetXYPlaneViewEnabled(false);
        viewport_->SetOrbitMode(OrbitMode::Architectural);
        UpdateProjectionStatus();
    }
    viewport_->SetTool(tool);
    if (tool == ToolMode::Orbit) {
        UpdateActiveToolUi("orbit");
    } else if (tool == ToolMode::Walk) {
        UpdateActiveToolUi("walk");
    } else if (tool == ToolMode::ZoomRect) {
        UpdateActiveToolUi("zoom_rect");
    } else if (tool == ToolMode::Select) {
        UpdateActiveToolUi("select");
    } else if (tool == ToolMode::DrawCurve) {
        UpdateActiveToolUi("PolylineCurve");
    } else if (tool == ToolMode::DrawBSpline) {
        UpdateActiveToolUi("BSplineCurve");
    } else if (tool == ToolMode::DrawSpline) {
        UpdateActiveToolUi("DrawSpline");
    } else if (tool == ToolMode::EditPoint) {
        UpdateActiveToolUi("EditPoint");
    } else if (tool == ToolMode::SketchRectangle
               || tool == ToolMode::SketchPolyline
               || tool == ToolMode::SketchBezier
               || tool == ToolMode::SketchConvertBezier
               || tool == ToolMode::SketchConvertArc) {
        UpdateActiveToolUi("NewSketch");
    } else if (tool == ToolMode::SolidBoxRectangle) {
        UpdateActiveToolUi("SolidBox");
    } else if (tool == ToolMode::SolidCylinderCircle) {
        UpdateActiveToolUi("SolidCylinder");
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
    if (solid_hidden_line_action_) {
        solid_hidden_line_action_->setChecked(mode == SolidDisplayMode::HiddenLine);
    }
    if (solid_hidden_line_hatch_action_) {
        solid_hidden_line_hatch_action_->setChecked(mode == SolidDisplayMode::HiddenLineHatch);
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
    } else if (mode == SolidDisplayMode::HiddenLine) {
        message = "Solid display: hidden lines removed";
    } else if (mode == SolidDisplayMode::HiddenLineHatch) {
        message = "Solid display: hidden lines hatch";
    }
    viewport_->update();
    statusBar()->showMessage(message, 1400);
}

void MainWindow::ToggleWireShadedDisplay() {
    const bool wire_enabled =
        CSolid::GetDisplayMode() == SolidDisplayMode::Wireframe;
    if (wire_enabled) {
        SetSolidDisplayMode(SolidDisplayMode::SurfacesAndEdges);
        statusBar()->showMessage("Display: shaded", 1200);
    } else {
        SetSolidDisplayMode(SolidDisplayMode::Wireframe);
        statusBar()->showMessage("Display: wired", 1200);
    }
}

void MainWindow::SetMeshDisplayMode(MeshDisplayMode mode) {
    CMesh3D::SetDisplayMode(mode);
    QSettings settings;
    settings.setValue("view/meshDisplayMode", static_cast<int>(mode));
    viewport_->update();

    QString message = "Mesh display: surface gray";
    if (mode == MeshDisplayMode::SurfaceColored) {
        message = "Mesh display: surface colored";
    } else if (mode == MeshDisplayMode::SurfaceMaterial) {
        message = "Surface display: texture";
    } else if (mode == MeshDisplayMode::Wire) {
        message = "Mesh display: wire";
    }
    statusBar()->showMessage(message, 1200);
}

void MainWindow::SetMeshSurfaceOpacity(float opacity) {
    opacity = std::clamp(opacity, 0.0f, 1.0f);
    if (auto* reference =
            dynamic_cast<CReferenceImage*>(document_.GetSelectedObject())) {
        Material material = reference->GetMaterial();
        material.alpha = opacity;
        reference->SetMaterial(material);
        const int percent = static_cast<int>(std::round(opacity * 100.0f));
        if (mesh_opacity_slider_ && mesh_opacity_slider_->value() != percent) {
            const QSignalBlocker blocker(mesh_opacity_slider_);
            mesh_opacity_slider_->setValue(percent);
        }
        if (mesh_opacity_value_label_) {
            mesh_opacity_value_label_->setText(QString("%1%").arg(percent));
        }
        viewport_->update();
        statusBar()->showMessage(
            QString("Reference image opacity: %1%").arg(percent), 900);
        return;
    }

    CMesh3D::SetSurfaceOpacity(opacity);
    const int percent = static_cast<int>(std::round(CMesh3D::GetSurfaceOpacity() * 100.0f));
    if (mesh_opacity_slider_ && mesh_opacity_slider_->value() != percent) {
        const QSignalBlocker blocker(mesh_opacity_slider_);
        mesh_opacity_slider_->setValue(percent);
    }
    if (mesh_opacity_value_label_) {
        mesh_opacity_value_label_->setText(QString("%1%").arg(percent));
    }

    QSettings settings;
    settings.setValue("view/meshSurfaceOpacity", CMesh3D::GetSurfaceOpacity());
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
    statusBar()->showMessage("Color:select the required geometry and continue");
}

void MainWindow::EditSelectedObjectColor() {
    if (!document_.HasSelection()) {
        object_color_pick_pending_ = true;
        statusBar()->showMessage("Color:select the required geometry and continue");
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

void MainWindow::ShowLineWeightDialog() {
    if (!document_.HasSelection()) {
        statusBar()->showMessage("Line weight: select one or more objects", 1600);
        return;
    }
    const CAlfaObject* selected = document_.GetSelectedObject();
    bool accepted = false;
    const double width = QInputDialog::getDouble(
        this, "Line Weight", "Width (mm):",
        selected ? selected->GetLineWidth() : 0.5,
        0.01, 25.0, 2, &accepted, Qt::WindowFlags(), 0.05);
    if (!accepted) return;
    int changed = 0;
    auto& objects = document_.GetObjects();
    for (size_t index : document_.GetSelectedObjectIndices()) {
        if (index < objects.size() && objects[index]) {
            objects[index]->SetLineWidth(width);
            ++changed;
        }
    }
    if (changed > 0) RecordDocumentChange("Change line weight");
    viewport_->update();
    statusBar()->showMessage(
        QString("Line weight %1 mm applied to %2 object(s)")
            .arg(width, 0, 'f', 2).arg(changed), 1600);
}

void MainWindow::ShowLineStyleDialog() {
    if (!document_.HasSelection()) {
        statusBar()->showMessage("Line style: select one or more objects", 1600);
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle("Line Style");
    auto* layout = new QVBoxLayout(&dialog);
    auto* combo = new QComboBox(&dialog);
    combo->addItem("Solid  ━━━━━━━━━━━━━", "CONTINUOUS");
    combo->addItem("Hidden  ━ ━ ━ ━ ━ ━", "HIDDEN");
    combo->addItem("Center  ━━━ · ━━━ · ━━━", "CENTER");
    combo->addItem("Phantom  ━━━ · · ━━━ · ·", "PHANTOM");
    combo->addItem("Dash-dot  ━━ · ━━ · ━━", "DASHDOT");
    combo->addItem("Dot  · · · · · · · · ·", "DOT");
    combo->addItem("Divide  ━ · · ━ · · ━", "DIVIDE");
    combo->addItem("Thin continuous  ─────────", "THIN");
    if (const CAlfaObject* selected = document_.GetSelectedObject()) {
        const int current = combo->findData(
            QString::fromStdString(selected->GetLineStyle()));
        if (current >= 0) combo->setCurrentIndex(current);
    }
    layout->addWidget(combo);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) return;

    const std::string style = combo->currentData().toString().toStdString();
    int changed = 0;
    auto& objects = document_.GetObjects();
    for (size_t index : document_.GetSelectedObjectIndices()) {
        if (index < objects.size() && objects[index]) {
            objects[index]->SetLineStyle(style);
            ++changed;
        }
    }
    if (changed > 0) RecordDocumentChange("Change line style");
    viewport_->update();
    statusBar()->showMessage(
        QString("Line style %1 applied to %2 object(s)")
            .arg(combo->currentText()).arg(changed), 1600);
}

bool MainWindow::EditDrawingText(CDrawingText& text, bool creating) {
    QDialog dialog(this);
    dialog.setWindowTitle(creating ? "Create Text" : "Edit Text");
    auto* root = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();

    auto* content = new QPlainTextEdit(&dialog);
    content->setPlainText(QString::fromUtf8(text.GetText().c_str()));
    content->setMinimumWidth(330);
    content->setMaximumHeight(110);
    form->addRow("Text:", content);

    auto* font = new QFontComboBox(&dialog);
    font->setFontFilters(QFontComboBox::ScalableFonts);
    font->setCurrentFont(QFont(QString::fromUtf8(text.GetFontFamily().c_str())));
    font->setMinimumWidth(260);
    form->addRow("Font:", font);

    auto* height = new QDoubleSpinBox(&dialog);
    height->setRange(0.01, 1000000.0);
    height->setDecimals(3);
    height->setValue(text.GetHeight());
    form->addRow("Height:", height);

    auto* rotation = new QDoubleSpinBox(&dialog);
    rotation->setRange(-36000.0, 36000.0);
    rotation->setDecimals(2);
    rotation->setSuffix(" deg");
    rotation->setValue(text.GetRotationDegrees());
    form->addRow("Rotation:", rotation);
    root->addLayout(form);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    root->addWidget(buttons);
    content->setFocus();
    content->selectAll();
    CenterDialogOnCursor(dialog);
    if (dialog.exec() != QDialog::Accepted) return false;

    const QString value = content->toPlainText();
    if (value.trimmed().isEmpty()) {
        statusBar()->showMessage("Text cannot be empty", 1600);
        return false;
    }
    text.SetText(value.toUtf8().toStdString());
    text.SetHeight(height->value());
    text.SetRotationDegrees(rotation->value());
    text.SetFontFamily(font->currentFont().family().toUtf8().toStdString());

    if (!creating) {
        RecordDocumentChange("Edit text");
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage("Text updated", 1400);
    }
    return true;
}

void MainWindow::BeginCreateDrawingText() {
    drawing_text_placement_pending_ = true;
    viewport_->BeginPickXYPoint(
        "Text: click the insertion point on the XY plane; Esc — cancel");
    UpdateActiveToolUi("DrawingText");
}

void MainWindow::CompleteDrawingTextPlacement(CPoint3d point) {
    if (!drawing_text_placement_pending_) return;
    drawing_text_placement_pending_ = false;
    CDrawingText prototype(
        pending_drawing_text_.empty() ? "Text" : pending_drawing_text_, point,
        pending_drawing_text_height_, pending_drawing_text_rotation_,
        pending_drawing_text_font_);
    if (!EditDrawingText(prototype, true)) {
        UpdateActiveToolUi("select");
        statusBar()->showMessage("Text creation canceled", 1400);
        return;
    }
    pending_drawing_text_ = prototype.GetText();
    pending_drawing_text_height_ = prototype.GetHeight();
    pending_drawing_text_rotation_ = prototype.GetRotationDegrees();
    pending_drawing_text_font_ = prototype.GetFontFamily();
    auto text = std::make_unique<CDrawingText>(
        pending_drawing_text_, point, pending_drawing_text_height_,
        pending_drawing_text_rotation_, pending_drawing_text_font_);
    document_.AddObject(std::move(text));
    RecordDocumentChange("Create text");
    RefreshSceneTree();
    viewport_->update();
    UpdateActiveToolUi("select");
    statusBar()->showMessage("Text created", 1400);
}

void MainWindow::ShowLayerProperties() {
    document_.EnsureDefaultLayer();

    auto* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setWindowTitle("A Property of Layers");
    auto* root = new QVBoxLayout(dialog);

    auto* title = new QLabel("The List of Layers", dialog);
    root->addWidget(title);

    auto* list = new QListWidget(dialog);
    list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    list->setMinimumSize(260, 300);
    list->setStyleSheet(
        "QListWidget::item {"
        "  padding: 4px 6px;"
        "}"
        "QListWidget::item:selected,"
        "QListWidget::item:selected:!active {"
        "  background: #0a84ff;"
        "  color: #ffffff;"
        "}"
    );
    root->addWidget(list);

    auto refresh = std::make_shared<std::function<void()>>();
    *refresh = [this, list]() {
        const QList<int> selected_ids = [list]() {
            QList<int> ids;
            for (QListWidgetItem* item : list->selectedItems()) {
                ids.push_back(item->data(Qt::UserRole).toInt());
            }
            return ids;
        }();
        list->clear();
        for (const CLayer* layer : document_.m_Layers) {
            if (!layer) {
                continue;
            }
            const QString flags = QString("%1 %2 ")
                .arg(layer->Visible ? "[V]" : "[-]")
                .arg(layer->Selectable ? "[S]" : "[X]");
            auto* item = new QListWidgetItem(flags + QString::fromStdString(layer->Name), list);
            item->setData(Qt::UserRole, layer->ID());
            if (layer->ID() == document_.GetWorkLayerID()) {
                QFont font = item->font();
                font.setBold(true);
                item->setFont(font);
            }
            if (selected_ids.contains(layer->ID())) {
                item->setSelected(true);
            }
        }
        if (list->selectedItems().empty() && list->count() > 0) {
            list->item(0)->setSelected(true);
            list->setCurrentRow(0);
        }
    };

    (*refresh)();

    auto* visibility_box = new QGroupBox("Visibility of a Layer", dialog);
    auto* visibility_layout = new QHBoxLayout(visibility_box);
    auto* visible_radio = new QRadioButton("Visible", visibility_box);
    auto* invisible_radio = new QRadioButton("Invisible", visibility_box);
    visibility_layout->addWidget(visible_radio);
    visibility_layout->addWidget(invisible_radio);
    root->addWidget(visibility_box);

    auto* selectable_box = new QGroupBox("Selectability of a Layer", dialog);
    auto* selectable_layout = new QHBoxLayout(selectable_box);
    auto* selectable_radio = new QRadioButton("Selectable", selectable_box);
    auto* unselectable_radio = new QRadioButton("Unselectable", selectable_box);
    selectable_layout->addWidget(selectable_radio);
    selectable_layout->addWidget(unselectable_radio);
    root->addWidget(selectable_box);

    auto sync_radios = std::make_shared<std::function<void()>>();
    *sync_radios = [this, list, visible_radio, invisible_radio, selectable_radio, unselectable_radio]() {
        const QListWidgetItem* item = list->currentItem();
        const CLayer* layer = item ? document_.GetLayerByID(item->data(Qt::UserRole).toInt()) : nullptr;
        if (!layer) {
            return;
        }
        QSignalBlocker visible_blocker(visible_radio);
        QSignalBlocker invisible_blocker(invisible_radio);
        QSignalBlocker selectable_blocker(selectable_radio);
        QSignalBlocker unselectable_blocker(unselectable_radio);
        visible_radio->setChecked(layer->Visible);
        invisible_radio->setChecked(!layer->Visible);
        selectable_radio->setChecked(layer->Selectable);
        unselectable_radio->setChecked(!layer->Selectable);
    };
    (*sync_radios)();
    connect(list, &QListWidget::currentItemChanged, dialog, [sync_radios](QListWidgetItem*, QListWidgetItem*) {
        (*sync_radios)();
    });

    auto selected_layers = std::make_shared<std::function<std::vector<CLayer*>()>>();
    *selected_layers = [this, list]() {
        std::vector<CLayer*> layers;
        for (QListWidgetItem* item : list->selectedItems()) {
            if (CLayer* layer = document_.GetLayerByID(item->data(Qt::UserRole).toInt())) {
                layers.push_back(layer);
            }
        }
        return layers;
    };

    auto apply_visibility = std::make_shared<std::function<void(bool)>>();
    *apply_visibility = [this, selected_layers, refresh, sync_radios](bool visible) {
        std::vector<CLayer*> layers = (*selected_layers)();
        if (layers.empty()) {
            return;
        }
        for (CLayer* layer : layers) {
            layer->Visible = visible;
        }
        document_.ClearSelection();
        (*refresh)();
        (*sync_radios)();
        RefreshSceneTree();
        viewport_->update();
    };

    auto apply_selectability = std::make_shared<std::function<void(bool)>>();
    *apply_selectability = [this, selected_layers, refresh, sync_radios](bool selectable) {
        std::vector<CLayer*> layers = (*selected_layers)();
        if (layers.empty()) {
            return;
        }
        for (CLayer* layer : layers) {
            layer->Selectable = selectable;
        }
        document_.ClearSelection();
        (*refresh)();
        (*sync_radios)();
        RefreshSceneTree();
        viewport_->update();
    };

    connect(visible_radio, &QRadioButton::toggled, dialog, [apply_visibility](bool checked) {
        if (checked) {
            (*apply_visibility)(true);
        }
    });
    connect(invisible_radio, &QRadioButton::toggled, dialog, [apply_visibility](bool checked) {
        if (checked) {
            (*apply_visibility)(false);
        }
    });
    connect(selectable_radio, &QRadioButton::toggled, dialog, [apply_selectability](bool checked) {
        if (checked) {
            (*apply_selectability)(true);
        }
    });
    connect(unselectable_radio, &QRadioButton::toggled, dialog, [apply_selectability](bool checked) {
        if (checked) {
            (*apply_selectability)(false);
        }
    });

    auto* buttons_row = new QHBoxLayout();
    auto* work_button = new QPushButton("Set Work", dialog);
    auto* new_button = new QPushButton("New Layer", dialog);
    auto* ok_button = new QPushButton("OK", dialog);
    buttons_row->addWidget(work_button);
    buttons_row->addWidget(new_button);
    buttons_row->addStretch();
    buttons_row->addWidget(ok_button);
    root->addLayout(buttons_row);

    connect(work_button, &QPushButton::clicked, dialog, [this, list, refresh]() {
        QListWidgetItem* item = list->currentItem();
        if (!item) {
            return;
        }
        document_.SetWorkLayer(item->data(Qt::UserRole).toInt());
        (*refresh)();
    });
    connect(new_button, &QPushButton::clicked, dialog, [this, dialog, list, refresh]() {
        bool ok = false;
        const QString name = QInputDialog::getText(dialog, "Name Layer", "Name", QLineEdit::Normal, QString(), &ok);
        if (!ok) {
            return;
        }
        CLayer* layer = document_.AddLayer(name.trimmed().toStdString());
        (*refresh)();
        for (int row = 0; row < list->count(); ++row) {
            if (list->item(row)->data(Qt::UserRole).toInt() == layer->ID()) {
                list->setCurrentRow(row);
                list->item(row)->setSelected(true);
                break;
            }
        }
    });
    connect(ok_button, &QPushButton::clicked, dialog, &QDialog::accept);
    connect(dialog, &QDialog::finished, this, [this]() {
        // Layer Properties is a dialog command, not a persistent viewport
        // tool. Re-apply the actual active tool so its toolbar/menu button is
        // released for OK, the window close button, and every other exit path.
        UpdateActiveToolUi(active_tool_key_);
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage("Layer properties updated", 1200);
    });

    CenterDialogOnCursor(*dialog);
    dialog->show();
}

void MainWindow::ShowFacadeManager() {
    if (auto* existing = findChild<QDialog*>("Dom3DFacadeManager")) {
        existing->show();
        existing->raise();
        existing->activateWindow();
        return;
    }

    struct FacadePreset {
        const char* name;
        int style;
    };
    const std::array<FacadePreset, 5> presets{{
        {"Chipboard panel", static_cast<int>(KitchenCabinetFacadeStyle::Plain)},
        {"Frame", static_cast<int>(KitchenCabinetFacadeStyle::Frame)},
        {"Screen", static_cast<int>(KitchenCabinetFacadeStyle::Screen)},
        {"Facade Milled", static_cast<int>(KitchenCabinetFacadeStyle::Milled)},
        {"MDF Profile Milano", static_cast<int>(KitchenCabinetFacadeStyle::Milano)}
    }};

    std::vector<unsigned long> target_ids;
    const auto& initial_objects = document_.GetObjects();
    for (size_t index = 0; index < initial_objects.size(); ++index) {
        if (!initial_objects[index] || !initial_objects[index]->IsParametric()) {
            continue;
        }
        const ActiveParametricObject active =
            tool_registry_.ActiveObjectFromDocument(
                index, *initial_objects[index], 0, &document_);
        const bool has_facade_style = std::any_of(
            active.parameters.begin(), active.parameters.end(),
            [](const ToolParameter& parameter) {
                return parameter.id == "facade_style";
            });
        if (has_facade_style) {
            target_ids.push_back(initial_objects[index]->m_id);
        }
    }

    if (target_ids.empty()) {
        QMessageBox::information(
            this, "Facade Manager",
            "The document does not contain editable furniture facades.");
        return;
    }

    auto original_styles = std::make_shared<std::map<unsigned long, double>>();
    for (unsigned long id : target_ids) {
        const CAlfaObject* object = document_.FindObjectById(id);
        if (!object) {
            continue;
        }
        const size_t index = document_.FindObjectIndexById(id);
        const ActiveParametricObject active =
            tool_registry_.ActiveObjectFromDocument(index, *object, 0, &document_);
        const auto parameter = std::find_if(
            active.parameters.begin(), active.parameters.end(),
            [](const ToolParameter& candidate) {
                return candidate.id == "facade_style";
            });
        if (parameter != active.parameters.end()) {
            (*original_styles)[id] = parameter->value;
        }
    }
    const int initial_style = original_styles->empty()
        ? 0 : static_cast<int>(std::lround(original_styles->begin()->second));

    auto* dialog = new QDialog(
        this, Qt::Tool | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    dialog->setObjectName("Dom3DFacadeManager");
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setWindowTitle("Facade Manager");
    dialog->setMinimumWidth(390);
    dialog->setModal(false);
    auto* layout = new QVBoxLayout(dialog);
    auto* form = new QFormLayout;
    auto* facade_type = new QComboBox(dialog);
    for (const FacadePreset& preset : presets) {
        facade_type->addItem(preset.name, preset.style);
    }
    const int initial_index = facade_type->findData(initial_style);
    facade_type->setCurrentIndex(initial_index >= 0 ? initial_index : 0);
    form->addRow("Type of Facade", facade_type);
    layout->addLayout(form);
    auto* affected = new QLabel(
        QString("Editable furniture assemblies: %1").arg(target_ids.size()),
        dialog);
    affected->setStyleSheet("QLabel { color: #666666; padding-top: 8px; }");
    layout->addWidget(affected);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel,
        dialog);
    layout->addWidget(buttons);

    struct FacadeBatchResult {
        int rebuilt = 0;
        bool canceled = false;
    };
    auto rebuilding = std::make_shared<bool>(false);
    auto cancel_requested = std::make_shared<bool>(false);
    auto last_applied_style = std::make_shared<int>(-1);
    auto* stop_shortcut = new QShortcut(QKeySequence(Qt::Key_Escape), dialog);
    stop_shortcut->setContext(Qt::ApplicationShortcut);
    connect(stop_shortcut, &QShortcut::activated, dialog,
            [rebuilding, cancel_requested]() {
        if (*rebuilding) {
            *cancel_requested = true;
        }
    });

    if (!undo_redo_.BeginChange()) {
        QMessageBox::critical(
            this, "Facade Manager", "Could not start facade editing.");
        dialog->deleteLater();
        return;
    }

    const auto apply_style =
        std::make_shared<std::function<FacadeBatchResult(int, bool)>>();
    *apply_style = [this, dialog, facade_type, affected, buttons, target_ids,
                    rebuilding, cancel_requested, last_applied_style](
                        int requested_style, bool allow_cancel) {
        FacadeBatchResult result;
        *rebuilding = true;
        *cancel_requested = false;
        facade_type->setEnabled(false);
        buttons->setEnabled(false);
        QApplication::setOverrideCursor(Qt::WaitCursor);

        const int total = static_cast<int>(target_ids.size());
        int rebuilt = 0;
        for (unsigned long id : target_ids) {
            if (allow_cancel && *cancel_requested) {
                result.canceled = true;
                break;
            }
            const size_t index = document_.FindObjectIndexById(id);
            auto& objects = document_.GetObjects();
            if (index >= objects.size() || !objects[index]) {
                continue;
            }
            ActiveParametricObject active =
                tool_registry_.ActiveObjectFromDocument(
                    index, *objects[index], 0, &document_);
            auto parameter = std::find_if(
                active.parameters.begin(), active.parameters.end(),
                [](const ToolParameter& candidate) {
                    return candidate.id == "facade_style";
                });
            if (parameter == active.parameters.end()) {
                continue;
            }
            // Simple drawer tools support Plain/Frame only. Decorative presets
            // map to Frame there; advanced cabinets retain all five styles.
            parameter->value = requested_style <= parameter->maximum
                ? requested_style
                : (requested_style == 0
                    ? 0.0
                    : std::min(1.0, parameter->maximum));
            tool_registry_.Rebuild(active, document_);
            // Furniture rebuild selects the rebuilt assembly so its parameter
            // editor can continue working. Facade Manager is a document-wide
            // batch command and must not expose that internal selection.
            document_.ClearSelection();
            ++rebuilt;

            affected->setText(QString("Rebuilt %1 of %2 — Esc to stop")
                                  .arg(rebuilt).arg(total));
            viewport_->update();
            viewport_->repaint();
            QApplication::processEvents(QEventLoop::AllEvents);
        }
        result.rebuilt = rebuilt;
        if (allow_cancel && *cancel_requested && rebuilt < total) {
            result.canceled = true;
        }
        *last_applied_style = result.canceled ? -1 : requested_style;
        document_.ClearSelection();
        RefreshSceneTree();
        viewport_->update();
        QApplication::restoreOverrideCursor();
        buttons->setEnabled(true);
        facade_type->setEnabled(true);
        affected->setText(result.canceled
            ? QString("Stopped after %1 of %2 assemblies")
                  .arg(rebuilt).arg(total)
            : QString("Editable furniture assemblies: %1").arg(total));
        *rebuilding = false;
        *cancel_requested = false;
        return result;
    };

    const auto restore_original = std::make_shared<std::function<void()>>();
    *restore_original = [this, original_styles]() {
        for (const auto& entry : *original_styles) {
            const size_t index = document_.FindObjectIndexById(entry.first);
            auto& objects = document_.GetObjects();
            if (index >= objects.size() || !objects[index]) {
                continue;
            }
            ActiveParametricObject active =
                tool_registry_.ActiveObjectFromDocument(
                    index, *objects[index], 0, &document_);
            auto parameter = std::find_if(
                active.parameters.begin(), active.parameters.end(),
                [](const ToolParameter& candidate) {
                    return candidate.id == "facade_style";
                });
            if (parameter != active.parameters.end()) {
                parameter->value = entry.second;
                tool_registry_.Rebuild(active, document_);
            }
        }
        document_.ClearSelection();
        RefreshSceneTree();
        viewport_->update();
    };

    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            dialog, [this, facade_type, apply_style]() {
        const FacadeBatchResult result =
            (*apply_style)(facade_type->currentData().toInt(), true);
        statusBar()->showMessage(
            result.canceled
                ? QString("Facade Manager: stopped after %1 assemblies")
                      .arg(result.rebuilt)
                : QString("Facade Manager: updated %1 furniture assemblies")
                      .arg(result.rebuilt),
            2200);
    });
    connect(buttons, &QDialogButtonBox::accepted,
            dialog, [this, dialog, facade_type, apply_style,
                     last_applied_style]() {
        const int style = facade_type->currentData().toInt();
        if (*last_applied_style != style) {
            const FacadeBatchResult result = (*apply_style)(style, true);
            if (result.canceled) {
                statusBar()->showMessage(
                    "Facade Manager: operation stopped; press OK to continue",
                    2400);
                return;
            }
        }
        undo_redo_.CommitChange("Change furniture facades");
        UpdateUndoRedoActions();
        statusBar()->showMessage("Facade Manager: operation completed", 1800);
        dialog->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected,
            dialog, &QDialog::reject);
    connect(dialog, &QDialog::rejected,
            this, [this, restore_original]() {
        (*restore_original)();
        undo_redo_.CancelChange();
        UpdateUndoRedoActions();
        ClearActiveProperties();
        statusBar()->showMessage("Facade Manager: changes canceled", 1600);
    });

    CenterDialogOnCursor(*dialog);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void MainWindow::ChangeSelectedObjectLayer() {
    document_.EnsureDefaultLayer();
    const int original_work_layer = document_.GetWorkLayerID();
    if (!document_.HasSelection()) {
        pending_group_command_ = PendingGroupCommand::ChangeLayer;
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionMode(SelectionMode::Object);
        viewport_->SetSelectionConfirmationMode(true);
        UpdateActiveToolUi("ChangeLayer");
        statusBar()->showMessage(
            "Change Layer: pick an object or select objects by rectangle");
        return;
    }

    if (pending_group_command_ == PendingGroupCommand::ChangeLayer) {
        pending_group_command_ = PendingGroupCommand::None;
        viewport_->SetSelectionConfirmationMode(false);
    }

    QDialog dialog(this);
    dialog.setWindowTitle("The list of Layers");
    auto* root = new QHBoxLayout(&dialog);
    auto* list = new QListWidget(&dialog);
    list->setMinimumSize(260, 280);
    root->addWidget(list);

    int current_layer = document_.GetWorkLayerID();
    const std::vector<size_t>& selected_indices =
        document_.GetSelectedObjectIndices();
    const auto& scene_objects = document_.GetObjects();
    if (!selected_indices.empty()
        && selected_indices.front() < scene_objects.size()
        && scene_objects[selected_indices.front()]) {
        current_layer = scene_objects[selected_indices.front()]->m_LayerID;
    }
    for (const CLayer* layer : document_.m_Layers) {
        if (!layer) {
            continue;
        }
        auto* item = new QListWidgetItem(QString::fromStdString(layer->Name), list);
        item->setData(Qt::UserRole, layer->ID());
        if (layer->ID() == current_layer) {
            item->setSelected(true);
            list->setCurrentItem(item);
        }
    }

    auto* buttons = new QVBoxLayout();
    auto* ok_button = new QPushButton("OK", &dialog);
    auto* new_button = new QPushButton("To\nNew\nLayer", &dialog);
    new_button->setStyleSheet("QPushButton { background: #fff200; color: black; font-weight: 700; }");
    auto* cancel_button = new QPushButton("Cancel", &dialog);
    buttons->addWidget(ok_button);
    buttons->addStretch();
    buttons->addWidget(new_button);
    buttons->addStretch();
    buttons->addWidget(cancel_button);
    root->addLayout(buttons);

    connect(ok_button, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancel_button, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(new_button, &QPushButton::clicked, &dialog, [&]() {
        bool ok = false;
        const QString name = QInputDialog::getText(&dialog, "Name Layer", "Name", QLineEdit::Normal, QString(), &ok);
        if (!ok) {
            return;
        }
        CLayer* layer = document_.AddLayer(name.trimmed().toStdString());
        document_.SetWorkLayer(original_work_layer);
        auto* item = new QListWidgetItem(QString::fromStdString(layer->Name), list);
        item->setData(Qt::UserRole, layer->ID());
        list->setCurrentItem(item);
        // Confirming the new layer name is the final confirmation for this
        // branch of Change Layer.  Close the parent dialog immediately so the
        // selected objects are assigned without requiring a second OK click.
        dialog.accept();
    });
    connect(list, &QListWidget::itemDoubleClicked, &dialog, [&dialog](QListWidgetItem*) {
        dialog.accept();
    });

    CenterDialogOnCursor(dialog);
    const int dialog_result = dialog.exec();
    UpdateActiveToolUi("select");
    if (dialog_result != QDialog::Accepted || !list->currentItem()) {
        statusBar()->showMessage("Layer change canceled", 900);
        return;
    }

    const int layer_id = list->currentItem()->data(Qt::UserRole).toInt();
    auto& objects = document_.GetObjects();
    std::vector<std::pair<unsigned long, int>> layers_before;
    layers_before.reserve(objects.size());
    for (const auto& object : objects) {
        if (object) layers_before.emplace_back(object->m_id, object->m_LayerID);
    }
    int changed = 0;
    for (size_t index : document_.GetSelectedObjectIndices()) {
        if (index < objects.size() && objects[index]) {
            if (auto* group = dynamic_cast<CGroup*>(objects[index].get())) {
                group->SetLayer(static_cast<unsigned long>(layer_id));
            } else {
                objects[index]->m_LayerID = layer_id;
            }
            ++changed;
        }
    }
    document_.SetWorkLayer(original_work_layer);
    if (changed > 0) {
        struct LayerChange {
            unsigned long object_id = 0;
            int before = 0;
            int after = 0;
        };
        std::vector<LayerChange> changes;
        for (const auto& [object_id, before] : layers_before) {
            if (const CAlfaObject* object = document_.FindObjectById(object_id);
                object && object->m_LayerID != before) {
                changes.push_back({object_id, before, object->m_LayerID});
            }
        }
        const auto apply_layers = [](CAlfaDoc& document,
                                     const std::vector<LayerChange>& values,
                                     bool use_after) {
            for (const LayerChange& value : values) {
                CAlfaObject* object = document.FindObjectById(value.object_id);
                if (!object) return false;
                object->m_LayerID = use_after ? value.after : value.before;
            }
            document.ClearSelection();
            return true;
        };
        undo_redo_.RecordCommand(
            "Change layer",
            [changes, apply_layers](CAlfaDoc& document) {
                return apply_layers(document, changes, false);
            },
            [changes, apply_layers](CAlfaDoc& document) {
                return apply_layers(document, changes, true);
            });
        UpdateUndoRedoActions();
    }
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage(QString("Layer changed for %1 object(s)").arg(changed), 1200);
}

QString MainWindow::SpatialCurvePrompt() const {
    QString curve_name;
    switch (spatial_curve_kind_) {
    case SpatialCurveKind::Polyline:
        curve_name = "Polyline 3D";
        break;
    case SpatialCurveKind::BSpline:
        curve_name = "B-Spline 3D";
        break;
    case SpatialCurveKind::Bezier:
        curve_name = "Bezier 3D";
        break;
    case SpatialCurveKind::Nurbs:
        curve_name = "NURBS 3D";
        break;
    case SpatialCurveKind::None:
        return {};
    }
    if (spatial_curve_kind_ == SpatialCurveKind::Bezier)
        return QString("Bezier 3D:operation canceled")
            .arg(spatial_curve_point_count_);
    return QString("%1:operation canceled")
        .arg(curve_name)
        .arg(spatial_curve_point_count_);
}

void MainWindow::BeginSpatialCurve(SpatialCurveKind kind) {
    if (kind == SpatialCurveKind::None) {
        return;
    }
    if (spatial_curve_kind_ != SpatialCurveKind::None) {
        CancelSpatialCurve();
    }

    ClearActiveProperties();
    spatial_curve_kind_ = kind;
    spatial_curve_object_id_ = 0;
    spatial_curve_point_count_ = 0;
    spatial_curve_interpolation_points_.clear();

    switch (kind) {
    case SpatialCurveKind::Polyline:
        UpdateActiveToolUi("PolylineCurve");
        break;
    case SpatialCurveKind::BSpline:
        UpdateActiveToolUi("BSplineCurve");
        break;
    case SpatialCurveKind::Bezier:
        UpdateActiveToolUi("BezierCurve3D");
        break;
    case SpatialCurveKind::Nurbs:
        UpdateActiveToolUi("NurbsCurve3D");
        break;
    case SpatialCurveKind::None:
        break;
    }
    viewport_->BeginPick3DPoint(SpatialCurvePrompt());
}

void MainWindow::AppendSpatialCurvePoint(CPoint3d point) {
    if (spatial_curve_kind_ == SpatialCurveKind::None) {
        return;
    }

    // Picking the first node again is the mouse equivalent of the C command.
    // Point3D picking snaps to an existing node, while the small relative
    // tolerance also covers coordinates restored from serialized projects.
    if (spatial_curve_point_count_ >= 3 && spatial_curve_object_id_ != 0) {
        const size_t index = document_.FindObjectIndexById(spatial_curve_object_id_);
        const auto& objects = document_.GetObjects();
        const CPoint3d* first = nullptr;
        if (index < objects.size() && objects[index]) {
            if (const auto* polyline = dynamic_cast<const CPolyline*>(objects[index].get())) {
                if (!polyline->GetPoints().empty()) first = &polyline->GetPoints().front();
            } else if (const auto* spline = dynamic_cast<const CBSpline*>(objects[index].get())) {
                if (!spline->GetPoints().empty()) first = &spline->GetPoints().front();
            }
        }
        if (first) {
            const double dx = point.x - first->x;
            const double dy = point.y - first->y;
            const double dz = point.z - first->z;
            const double coordinate_scale = std::max(
                {1.0, std::abs(first->x), std::abs(first->y), std::abs(first->z)});
            if (std::sqrt(dx * dx + dy * dy + dz * dz)
                <= coordinate_scale * 1.0e-7) {
                CloseSpatialCurve();
                return;
            }
        }
    }

    if (spatial_curve_object_id_ == 0) {
        if (spatial_curve_kind_ == SpatialCurveKind::Polyline) {
            auto curve = std::make_unique<CPolyline>("Polyline 3D");
            curve->AddPoint(point);
            curve->SetParametricDefinition("PolylineCurve", {});
            document_.AddObject(std::move(curve));
        } else {
            std::string name;
            std::string tool_id;
            SplineCurveType curve_type = SplineCurveType::BSpline;
            if (spatial_curve_kind_ == SpatialCurveKind::Bezier) {
                name = "Bezier 3D";
                tool_id = "BezierCurve3D";
                curve_type = SplineCurveType::Bezier;
            } else if (spatial_curve_kind_ == SpatialCurveKind::Nurbs) {
                name = "NURBS 3D";
                tool_id = "NurbsCurve3D";
                curve_type = SplineCurveType::Nurbs;
            } else {
                name = "B-Spline 3D";
                tool_id = "BSplineCurve";
            }
            auto curve = std::make_unique<CBSpline>(name);
            curve->SetCurveType(curve_type);
            curve->SetDegree(3);
            if (spatial_curve_kind_ == SpatialCurveKind::Bezier) {
                spatial_curve_interpolation_points_.push_back(point);
                curve->SetBezierInterpolationPoints(
                    spatial_curve_interpolation_points_);
            } else {
                curve->AddPoint(point);
            }
            curve->SetParametricDefinition(tool_id, {});
            document_.AddObject(std::move(curve));
        }
        const auto& objects = document_.GetObjects();
        if (!objects.empty() && objects.back()) {
            spatial_curve_object_id_ = objects.back()->m_id;
        }
    } else {
        const size_t index = document_.FindObjectIndexById(spatial_curve_object_id_);
        auto& objects = document_.GetObjects();
        if (index >= objects.size() || !objects[index]) {
            CancelSpatialCurve();
            return;
        }
        if (auto* polyline = dynamic_cast<CPolyline*>(objects[index].get())) {
            polyline->AddPoint(point);
        } else if (auto* spline = dynamic_cast<CBSpline*>(objects[index].get())) {
            if (spatial_curve_kind_ == SpatialCurveKind::Bezier) {
                spatial_curve_interpolation_points_.push_back(point);
                spline->SetBezierInterpolationPoints(
                    spatial_curve_interpolation_points_);
            } else {
                spline->AddPoint(point);
            }
        }
    }

    ++spatial_curve_point_count_;
    RefreshSceneTree();
    viewport_->update();
    QTimer::singleShot(0, this, [this]() {
        if (spatial_curve_kind_ != SpatialCurveKind::None) {
            viewport_->BeginPick3DPoint(SpatialCurvePrompt());
        }
    });
}

void MainWindow::FinishSpatialCurve() {
    if (spatial_curve_kind_ == SpatialCurveKind::None) {
        return;
    }
    if (spatial_curve_point_count_ < 2) {
        CancelSpatialCurve();
        statusBar()->showMessage("3D curve:additional geometry is required", 2200);
        return;
    }

    const SpatialCurveKind completed_kind = spatial_curve_kind_;
    if (completed_kind != SpatialCurveKind::Polyline) {
        const size_t index = document_.FindObjectIndexById(spatial_curve_object_id_);
        auto& objects = document_.GetObjects();
        if (index < objects.size()) {
            if (auto* spline = dynamic_cast<CBSpline*>(objects[index].get())) {
                spline->SetDegree(completed_kind == SpatialCurveKind::Bezier
                    ? 3
                    : static_cast<int>(
                        std::min<size_t>(3, spatial_curve_point_count_ - 1)));
            }
        }
    }

    QString name;
    switch (completed_kind) {
    case SpatialCurveKind::Polyline: name = "Polyline 3D"; break;
    case SpatialCurveKind::BSpline: name = "B-Spline 3D"; break;
    case SpatialCurveKind::Bezier: name = "Bezier 3D"; break;
    case SpatialCurveKind::Nurbs: name = "NURBS 3D"; break;
    case SpatialCurveKind::None: break;
    }
    const unsigned long completed_object_id = spatial_curve_object_id_;
    std::shared_ptr<CAlfaObject> curve_prototype;
    if (const CAlfaObject* completed =
            document_.FindObjectById(completed_object_id)) {
        std::unique_ptr<CAlfaObject> clone = completed->Clone();
        if (clone) {
            clone->SetName(completed->GetName());
            clone->m_id = completed->m_id;
            clone->m_LayerID = completed->m_LayerID;
            clone->SetParametricDefinition(
                completed->GetParametricToolId(),
                completed->GetParametricParameters());
            curve_prototype.reset(clone.release());
        }
    }
    spatial_curve_kind_ = SpatialCurveKind::None;
    spatial_curve_object_id_ = 0;
    spatial_curve_point_count_ = 0;
    spatial_curve_interpolation_points_.clear();
    if (curve_prototype) {
        undo_redo_.RecordCommand(
            QString("Create %1").arg(name).toStdString(),
            [completed_object_id](CAlfaDoc& document) {
                const size_t index = document.FindObjectIndexById(
                    completed_object_id);
                auto& objects = document.GetObjects();
                if (index >= objects.size()) return false;
                objects.erase(objects.begin()
                    + static_cast<std::ptrdiff_t>(index));
                document.ClearSelection();
                return true;
            },
            [curve_prototype](CAlfaDoc& document) {
                if (document.FindObjectById(curve_prototype->m_id)) {
                    return false;
                }
                std::unique_ptr<CAlfaObject> clone = curve_prototype->Clone();
                if (!clone) return false;
                clone->SetName(curve_prototype->GetName());
                clone->m_id = curve_prototype->m_id;
                clone->m_LayerID = curve_prototype->m_LayerID;
                clone->SetParametricDefinition(
                    curve_prototype->GetParametricToolId(),
                    curve_prototype->GetParametricParameters());
                const int layer_id = clone->m_LayerID;
                const unsigned long object_id = clone->m_id;
                document.AddObject(std::move(clone));
                if (CAlfaObject* restored = document.FindObjectById(object_id)) {
                    restored->m_LayerID = layer_id;
                }
                return true;
            });
    }
    UpdateUndoRedoActions();
    viewport_->SetTool(ToolMode::Select);
    UpdateActiveToolUi("select");
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage(QString("%1 operation completed").arg(name), 1800);
}

void MainWindow::CloseSpatialCurve() {
    if (spatial_curve_kind_ == SpatialCurveKind::None) return;
    if (spatial_curve_point_count_ < 3 || spatial_curve_object_id_ == 0) {
        statusBar()->showMessage(
            "3D curve:additional geometry is required", 2200);
        viewport_->BeginPick3DPoint(SpatialCurvePrompt());
        return;
    }

    const SpatialCurveKind kind = spatial_curve_kind_;
    const size_t index = document_.FindObjectIndexById(spatial_curve_object_id_);
    auto& objects = document_.GetObjects();
    bool closed = false;
    if (index < objects.size() && objects[index]) {
        if (auto* polyline = dynamic_cast<CPolyline*>(objects[index].get())) {
            closed = polyline->Close();
        } else if (auto* spline = dynamic_cast<CBSpline*>(objects[index].get())) {
            if (kind == SpatialCurveKind::Bezier
                && !spatial_curve_interpolation_points_.empty()) {
                spline->SetClosedBezierInterpolationPoints(
                    spatial_curve_interpolation_points_);
                ++spatial_curve_point_count_;
            }
            closed = spline->Close();
        }
    }
    if (!closed) {
        statusBar()->showMessage("3D curve:operation failed; check the selected geometry and parameters", 2200);
        viewport_->BeginPick3DPoint(SpatialCurvePrompt());
        return;
    }

    FinishSpatialCurve();
    const QString name = kind == SpatialCurveKind::Polyline ? "Polyline 3D"
        : kind == SpatialCurveKind::BSpline ? "B-Spline 3D"
        : kind == SpatialCurveKind::Bezier ? "Bezier 3D" : "NURBS 3D";
    statusBar()->showMessage(QString("%1 operation status").arg(name), 1800);
}

void MainWindow::CancelSpatialCurve() {
    if (spatial_curve_kind_ == SpatialCurveKind::None) {
        return;
    }
    if (spatial_curve_object_id_ != 0) {
        const size_t index = document_.FindObjectIndexById(spatial_curve_object_id_);
        auto& objects = document_.GetObjects();
        if (index < objects.size()) {
            objects.erase(objects.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }
    document_.ClearSelection();
    spatial_curve_kind_ = SpatialCurveKind::None;
    spatial_curve_object_id_ = 0;
    spatial_curve_point_count_ = 0;
    spatial_curve_interpolation_points_.clear();
    UpdateUndoRedoActions();
    viewport_->SetTool(ToolMode::Select);
    UpdateActiveToolUi("select");
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage("3D curve creation canceled", 1600);
}

void MainWindow::BeginCurveEditCommand(CurveEditCommand command) {
    ClearActiveProperties();
    CancelCurveEditCommand();
    pending_curve_edit_command_ = command;
    viewport_->SetSelectionMode(SelectionMode::Object);
    viewport_->SetTool(ToolMode::Select);
    viewport_->SetSelectionConfirmationMode(false);
    if (PrepareCurveEditCommandSelection()) return;

    viewport_->SetSelectionConfirmationMode(true);
    QString prompt;
    switch (command) {
    case CurveEditCommand::Join:
        prompt = "Join:select the required geometry and continue";
        break;
    case CurveEditCommand::Split:
        prompt = "Split:select the required geometry and continue";
        break;
    case CurveEditCommand::Extend:
        prompt = "Extend:select the required geometry and continue";
        break;
    case CurveEditCommand::TrimByPlane:
        prompt = "Trim by Plane:select the required geometry and continue";
        break;
    case CurveEditCommand::SimplifyByPoint:
        prompt = "Split by Point:select the required geometry and continue";
        break;
    case CurveEditCommand::Reverse:
        prompt = "Reverse:select the required geometry and continue";
        break;
    case CurveEditCommand::None:
        return;
    }
    statusBar()->showMessage(prompt);
}

bool MainWindow::PrepareCurveEditCommandSelection() {
    if (pending_curve_edit_command_ == CurveEditCommand::None) return false;
    const auto& objects = document_.GetObjects();
    std::vector<size_t> curves;
    size_t plane_count = 0;
    for (size_t index : document_.GetSelectedObjectIndices()) {
        if (index >= objects.size() || !objects[index]) continue;
        if (IsEditableCurve(objects[index].get())) curves.push_back(index);
        if (objects[index]->GetParametricToolId() == "PlaneTool") ++plane_count;
    }

    switch (pending_curve_edit_command_) {
    case CurveEditCommand::Join:
        if (curves.size() < 2) return false;
        if (!JoinSelectedCurves()) return false;
        CancelCurveEditCommand("Join: curves joined");
        return true;
    case CurveEditCommand::Reverse:
        if (curves.empty()) return false;
        if (!ReverseSelectedCurves()) return false;
        CancelCurveEditCommand("Reverse: direction changed");
        return true;
    case CurveEditCommand::Split:
        if (curves.size() != 2) return false;
        break;
    case CurveEditCommand::TrimByPlane:
        if (curves.size() != 1 || plane_count > 1) return false;
        pending_curve_trim_plane_points_.clear();
        if (plane_count == 0) {
            std::array<double, 4> remembered_factors = LoadRememberedPlaneFactors();
            const int method = ShowPlaneDefinitionDialog(this, remembered_factors);
            if (method == QDialog::Rejected) {
                CancelCurveEditCommand("Trim by Plane:operation canceled");
                return true;
            }
            if (method == 7) {
                pending_trim_plane_face_pick_ = true;
                pending_trim_plane_curve_id_ = objects[curves.front()]->m_id;
                viewport_->SetSelectionConfirmationMode(false);
                viewport_->SetTool(ToolMode::Select);
                viewport_->SetSelectionMode(SelectionMode::Face);
                statusBar()->showMessage(
                    "Trim by Plane — Face of Solid:select the required geometry and continue");
                return true;
            } else if (method == 2) {
                pending_curve_trim_plane_points_ = {
                    CPoint3d(0, 0, 0), CPoint3d(1, 0, 0), CPoint3d(0, 1, 0)};
                SaveRememberedPlaneFactors({0.0, 0.0, 1.0, 0.0});
            } else if (method == 3) {
                pending_curve_trim_plane_points_ = {
                    CPoint3d(0, 0, 0), CPoint3d(1, 0, 0), CPoint3d(0, 0, 1)};
                SaveRememberedPlaneFactors({0.0, 1.0, 0.0, 0.0});
            } else if (method == 4) {
                pending_curve_trim_plane_points_ = {
                    CPoint3d(0, 0, 0), CPoint3d(0, 1, 0), CPoint3d(0, 0, 1)};
                SaveRememberedPlaneFactors({1.0, 0.0, 0.0, 0.0});
            } else if (method == 5) {
                const double squared = remembered_factors[0] * remembered_factors[0]
                    + remembered_factors[1] * remembered_factors[1]
                    + remembered_factors[2] * remembered_factors[2];
                if (squared <= 1.0e-18) {
                    statusBar()->showMessage("Plane:operation failed; check the selected geometry and parameters", 2600);
                    return false;
                }
                SaveRememberedPlaneFactors(remembered_factors);
                pending_curve_trim_plane_points_ = PlanePointsFromOriginNormal(
                    CPoint3d(-remembered_factors[0] * remembered_factors[3] / squared,
                             -remembered_factors[1] * remembered_factors[3] / squared,
                             -remembered_factors[2] * remembered_factors[3] / squared),
                    Vec3{static_cast<float>(remembered_factors[0]),
                         static_cast<float>(remembered_factors[1]),
                         static_cast<float>(remembered_factors[2])});
            } else if (method == 6) {
                std::vector<double> values{0.0, 0.0, 0.0, 0.0, 0.0, 1.0};
                if (!ShowPlaneValuesDialog(
                        this, "Point + Normal", {"Point X", "Point Y", "Point Z",
                        "Normal X", "Normal Y", "Normal Z"}, values)) {
                    CancelCurveEditCommand("Trim by Plane:operation canceled");
                    return true;
                }
                pending_curve_trim_plane_points_ = PlanePointsFromOriginNormal(
                    CPoint3d(values[0], values[1], values[2]),
                    Vec3{static_cast<float>(values[3]), static_cast<float>(values[4]),
                         static_cast<float>(values[5])});
                if (pending_curve_trim_plane_points_.empty()) {
                    statusBar()->showMessage("Plane:operation failed; check the selected geometry and parameters", 2600);
                    return false;
                }
                const CPoint3d& origin = pending_curve_trim_plane_points_[0];
                SaveRememberedPlaneFactors({
                    values[3], values[4], values[5],
                    -(values[3] * origin.x + values[4] * origin.y + values[5] * origin.z)});
            }
        }
        break;
    case CurveEditCommand::Extend:
    case CurveEditCommand::SimplifyByPoint:
        if (curves.size() != 1) return false;
        break;
    case CurveEditCommand::None:
        return false;
    }

    viewport_->SetSelectionConfirmationMode(false);
    QString prompt;
    if (pending_curve_edit_command_ == CurveEditCommand::Split) {
        prompt = "Split:select the required geometry and continue";
    } else if (pending_curve_edit_command_ == CurveEditCommand::Extend) {
        prompt = "Extend:select the required geometry and continue";
    } else if (pending_curve_edit_command_ == CurveEditCommand::TrimByPlane) {
        prompt = plane_count == 1 || pending_curve_trim_plane_points_.size() == 3
            ? "Trim by Plane:select the required geometry and continue"
            : "Trim by Plane — Point 1 of 3:adjust the required parameters and continue";
    } else {
        prompt = "Split by Point:select the required geometry and continue";
    }
    if (pending_curve_edit_command_ == CurveEditCommand::TrimByPlane
        && (plane_count == 1 || pending_curve_trim_plane_points_.size() == 3)) {
        viewport_->BeginPick3DPointOnObject(
            objects[curves.front()]->m_id, prompt + "; Esc —operation canceled");
    } else {
        viewport_->BeginPick3DPoint(prompt + "; Esc —operation canceled");
    }
    return true;
}

void MainWindow::CompleteCurveEditPoint(CPoint3d point) {
    bool changed = false;
    QString message;
    switch (pending_curve_edit_command_) {
    case CurveEditCommand::Split:
        changed = SplitSelectedCurves(point);
        message = changed ? "Split: curves divided" : "Split:operation failed; check the selected geometry and parameters";
        break;
    case CurveEditCommand::Extend:
        changed = ExtendSelectedCurve(point);
        message = changed ? "Extend: curve extended" : "Extend: operation canceled";
        break;
    case CurveEditCommand::TrimByPlane:
        {
        bool has_plane_object = false;
        for (size_t index : document_.GetSelectedObjectIndices()) {
            if (index < document_.GetObjects().size()
                && document_.GetObjects()[index]
                && document_.GetObjects()[index]->GetParametricToolId() == "PlaneTool") {
                has_plane_object = true;
                break;
            }
        }
        if (!has_plane_object && pending_curve_trim_plane_points_.size() < 3) {
            pending_curve_trim_plane_points_.push_back(point);
            viewport_->SetPointPickMarkers(pending_curve_trim_plane_points_);
            if (pending_curve_trim_plane_points_.size() == 3) {
                const CPoint3d& first = pending_curve_trim_plane_points_[0];
                const CPoint3d& second = pending_curve_trim_plane_points_[1];
                const CPoint3d& third = pending_curve_trim_plane_points_[2];
                const Vec3 a{static_cast<float>(second.x - first.x),
                             static_cast<float>(second.y - first.y),
                             static_cast<float>(second.z - first.z)};
                const Vec3 b{static_cast<float>(third.x - first.x),
                             static_cast<float>(third.y - first.y),
                             static_cast<float>(third.z - first.z)};
                const Vec3 normal = cross(a, b);
                if (dot(normal, normal) <= 1.0e-12f) {
                    pending_curve_trim_plane_points_.pop_back();
                    viewport_->SetPointPickMarkers(pending_curve_trim_plane_points_);
                    statusBar()->showMessage(
                        "Trim by Plane:operation status", 2400);
                    viewport_->BeginPick3DPoint(
                        "Trim by Plane — Point 3 of 3:select the required geometry and continue");
                    return;
                }
                SaveRememberedPlaneFactors(PlaneFactorsFromPoints(
                    first, second, third));
                for (size_t index : document_.GetSelectedObjectIndices()) {
                    if (index < document_.GetObjects().size()
                        && IsEditableCurve(document_.GetObjects()[index].get())) {
                        viewport_->BeginPick3DPointOnObject(
                            document_.GetObjects()[index]->m_id,
                            "Trim by Plane:select the required geometry and continue");
                        break;
                    }
                }
            } else {
                viewport_->BeginPick3DPoint(
                    QString("Trim by Plane — Point %1 of 3:adjust the required parameters and continue")
                        .arg(pending_curve_trim_plane_points_.size() + 1));
            }
            return;
        }
        changed = TrimSelectedCurveByPlane(point);
        if (!changed) {
            for (size_t index : document_.GetSelectedObjectIndices()) {
                if (index < document_.GetObjects().size()
                    && IsEditableCurve(document_.GetObjects()[index].get())) {
                    viewport_->BeginPick3DPointOnObject(
                        document_.GetObjects()[index]->m_id,
                        "Trim by Plane:select the required geometry and continue");
                    break;
                }
            }
            return;
        }
        message = "Trim by Plane: curve trimmed";
        break;
        }
    case CurveEditCommand::SimplifyByPoint:
        changed = SimplifySelectedCurveByPoint(point);
        message = changed ? "Split by Point: two curve pieces created" : "Split by Point:operation failed; check the selected geometry and parameters";
        break;
    default:
        break;
    }
    CancelCurveEditCommand(message);
}

void MainWindow::CancelCurveEditCommand(const QString& message) {
    pending_curve_edit_command_ = CurveEditCommand::None;
    pending_curve_trim_plane_points_.clear();
    pending_trim_plane_face_pick_ = false;
    pending_trim_plane_curve_id_ = 0;
    if (viewport_) viewport_->ClearPointPickMarkers();
    if (viewport_) viewport_->SetSelectionConfirmationMode(false);
    if (!message.isEmpty()) statusBar()->showMessage(message, 2200);
}

bool MainWindow::ReverseSelectedCurves() {
    undo_redo_.BeginChange();
    bool changed = false;
    std::vector<unsigned long> changed_ids;
    for (size_t index : document_.GetSelectedObjectIndices()) {
        if (index >= document_.GetObjects().size()) continue;
        if (auto* polyline = dynamic_cast<CPolyline*>(
                document_.GetObjects()[index].get())) {
            polyline->Revers();
            changed = true;
            changed_ids.push_back(polyline->m_id);
        } else if (auto* spline = dynamic_cast<CBSpline*>(
                       document_.GetObjects()[index].get())) {
            spline->Reverse();
            changed = true;
            changed_ids.push_back(spline->m_id);
        }
    }
    if (!changed) {
        undo_redo_.CancelChange();
        return false;
    }
    for (unsigned long id : changed_ids) {
        tool_registry_.ReplayProfileDependents(id, document_);
    }
    undo_redo_.CommitChange("Reverse curves");
    UpdateUndoRedoActions();
    RefreshSceneTree();
    viewport_->update();
    return true;
}

bool MainWindow::JoinSelectedCurves() {
    std::vector<size_t> indices;
    for (size_t index : document_.GetSelectedObjectIndices()) {
        if (index < document_.GetObjects().size()
            && IsEditableCurve(document_.GetObjects()[index].get())) {
            indices.push_back(index);
        }
    }
    if (indices.size() < 2) return false;
    auto& objects = document_.GetObjects();
    const bool polyline_family = dynamic_cast<CPolyline*>(objects[indices[0]].get());
    const auto* first_spline = dynamic_cast<CBSpline*>(objects[indices[0]].get());
    for (size_t index : indices) {
        if (polyline_family != static_cast<bool>(dynamic_cast<CPolyline*>(objects[index].get()))) {
            statusBar()->showMessage("Join: Polyline operation failed; check the selected geometry and parameters", 2600);
            return false;
        }
        const auto* spline = dynamic_cast<CBSpline*>(objects[index].get());
        if (first_spline && (!spline
            || spline->GetCurveType() != first_spline->GetCurveType())) {
            statusBar()->showMessage("Join:operation failed; check the selected geometry and parameters", 2600);
            return false;
        }
    }

    std::vector<CPoint3d> joined = *CurveControlPoints(objects[indices[0]].get());
    std::vector<double> joined_weights = first_spline
        ? first_spline->GetWeights() : std::vector<double>{};
    std::set<size_t> remaining(indices.begin() + 1, indices.end());
    constexpr double tolerance_squared = 1.0;
    while (!remaining.empty()) {
        size_t best_index = *remaining.begin();
        int best_mode = -1;
        double best_distance = std::numeric_limits<double>::max();
        for (size_t index : remaining) {
            const auto* candidate = CurveControlPoints(objects[index].get());
            if (!candidate || candidate->size() < 2) continue;
            const double distances[4] = {
                CurvePointDistanceSquared(joined.back(), candidate->front()),
                CurvePointDistanceSquared(joined.back(), candidate->back()),
                CurvePointDistanceSquared(joined.front(), candidate->back()),
                CurvePointDistanceSquared(joined.front(), candidate->front())};
            for (int mode = 0; mode < 4; ++mode) {
                if (distances[mode] < best_distance) {
                    best_distance = distances[mode];
                    best_index = index;
                    best_mode = mode;
                }
            }
        }
        if (best_mode < 0 || best_distance > tolerance_squared) {
            statusBar()->showMessage("Join:operation failed; check the selected geometry and parameters", 2600);
            return false;
        }
        std::vector<CPoint3d> next = *CurveControlPoints(objects[best_index].get());
        std::vector<double> next_weights;
        if (const auto* spline = dynamic_cast<CBSpline*>(objects[best_index].get())) {
            next_weights = spline->GetWeights();
        }
        if (best_mode == 1 || best_mode == 3) {
            std::reverse(next.begin(), next.end());
            std::reverse(next_weights.begin(), next_weights.end());
        }
        if (best_mode <= 1) {
            joined.insert(joined.end(), next.begin() + 1, next.end());
            if (!joined_weights.empty()) joined_weights.insert(
                joined_weights.end(), next_weights.begin() + 1, next_weights.end());
        } else {
            next.pop_back();
            next.insert(next.end(), joined.begin(), joined.end());
            joined.swap(next);
            if (!joined_weights.empty()) {
                next_weights.pop_back();
                next_weights.insert(next_weights.end(), joined_weights.begin(), joined_weights.end());
                joined_weights.swap(next_weights);
            }
        }
        remaining.erase(best_index);
    }

    undo_redo_.BeginChange();
    const unsigned long retained_curve_id = objects[indices[0]]->m_id;
    ReplaceCurveGeometry(*objects[indices[0]], joined, joined_weights);
    std::sort(indices.begin() + 1, indices.end(), std::greater<size_t>());
    for (auto it = indices.begin() + 1; it != indices.end(); ++it) {
        objects.erase(objects.begin() + static_cast<CAlfaDoc::ObjectList::difference_type>(*it));
    }
    document_.ClearSelection();
    tool_registry_.ReplayProfileDependents(retained_curve_id, document_);
    undo_redo_.CommitChange("Join curves");
    UpdateUndoRedoActions();
    RefreshSceneTree();
    viewport_->update();
    return true;
}

bool MainWindow::SimplifySelectedCurveByPoint(CPoint3d split_point) {
    size_t object_index = document_.GetObjects().size();
    for (size_t index : document_.GetSelectedObjectIndices()) {
        if (index < document_.GetObjects().size()
            && IsEditableCurve(document_.GetObjects()[index].get())) {
            if (object_index != document_.GetObjects().size()) return false;
            object_index = index;
        }
    }
    auto& objects = document_.GetObjects();
    if (object_index >= objects.size()) return false;
    CAlfaObject& source = *objects[object_index];
    const auto* source_points = CurveControlPoints(&source);
    if (!source_points || source_points->size() < 3) return false;

    std::vector<CPoint3d> points = *source_points;
    std::vector<double> weights;
    if (const auto* spline = dynamic_cast<const CBSpline*>(&source)) {
        weights = spline->GetWeights();
        weights.resize(points.size(), 1.0);
    }
    CurveProjection projection = ProjectToControlPolygon(points, split_point);
    if (projection.segment == 0
        && CurvePointDistanceSquared(projection.point, points.front()) < 1.0e-10) return false;
    if (projection.segment + 2 == points.size()
        && CurvePointDistanceSquared(projection.point, points.back()) < 1.0e-10) return false;

    size_t split_index = projection.segment + 1;
    if (CurvePointDistanceSquared(projection.point, points[projection.segment]) < 1.0e-10) {
        split_index = projection.segment;
    } else if (CurvePointDistanceSquared(
                   projection.point, points[projection.segment + 1]) < 1.0e-10) {
        split_index = projection.segment + 1;
    } else {
        points.insert(points.begin() + static_cast<std::ptrdiff_t>(split_index), projection.point);
        if (!weights.empty()) {
            const double weight = 0.5 * (weights[projection.segment]
                                       + weights[projection.segment + 1]);
            weights.insert(weights.begin() + static_cast<std::ptrdiff_t>(split_index), weight);
        }
    }
    if (split_index == 0 || split_index + 1 >= points.size()) return false;

    const std::vector<CPoint3d> first_points(points.begin(), points.begin() + split_index + 1);
    const std::vector<CPoint3d> second_points(points.begin() + split_index, points.end());
    const std::vector<double> first_weights = weights.empty() ? std::vector<double>{}
        : std::vector<double>(weights.begin(), weights.begin() + split_index + 1);
    const std::vector<double> second_weights = weights.empty() ? std::vector<double>{}
        : std::vector<double>(weights.begin() + split_index, weights.end());
    std::unique_ptr<CAlfaObject> second = MakeCurvePiece(
        source, second_points, second_weights, " Part 2");
    if (!second) return false;

    undo_redo_.BeginChange();
    const unsigned long source_id = source.m_id;
    source.SetName(source.GetName() + " Part 1");
    ReplaceCurveGeometry(source, first_points, first_weights);
    document_.AddObject(std::move(second));
    tool_registry_.ReplayProfileDependents(source_id, document_);
    undo_redo_.CommitChange("Split curve by point");
    UpdateUndoRedoActions();
    RefreshSceneTree();
    viewport_->update();
    return true;
}

bool MainWindow::SplitSelectedCurves(CPoint3d near_point) {
    std::vector<size_t> indices;
    for (size_t index : document_.GetSelectedObjectIndices()) {
        if (index < document_.GetObjects().size()
            && IsEditableCurve(document_.GetObjects()[index].get())) indices.push_back(index);
    }
    if (indices.size() != 2) return false;
    auto& objects = document_.GetObjects();
    const std::vector<CPoint3d> first_samples = CurveSamples(*objects[indices[0]]);
    const std::vector<CPoint3d> second_samples = CurveSamples(*objects[indices[1]]);
    if (first_samples.size() < 2 || second_samples.size() < 2) return false;

    CPoint3d intersection;
    double best_score = std::numeric_limits<double>::max();
    double best_separation = std::numeric_limits<double>::max();
    for (size_t first = 0; first + 1 < first_samples.size(); ++first) {
        for (size_t second = 0; second + 1 < second_samples.size(); ++second) {
            const SegmentPairProjection pair = ClosestSegmentPair(
                first_samples[first], first_samples[first + 1],
                second_samples[second], second_samples[second + 1]);
            if (pair.distance_squared > 1.0) continue;
            const CPoint3d middle(
                0.5 * (pair.first.x + pair.second.x),
                0.5 * (pair.first.y + pair.second.y),
                0.5 * (pair.first.z + pair.second.z));
            const double score = CurvePointDistanceSquared(middle, near_point);
            if (score < best_score) {
                best_score = score;
                best_separation = pair.distance_squared;
                intersection = middle;
            }
        }
    }
    if (best_separation > 1.0) return false;

    // Split the higher index first so adding the second half cannot invalidate
    // the other selected object's index.
    std::sort(indices.begin(), indices.end(), std::greater<size_t>());
    undo_redo_.BeginChange();
    bool split_any = false;
    std::vector<unsigned long> changed_ids;
    for (size_t object_index : indices) {
        CAlfaObject& source = *objects[object_index];
        const unsigned long source_id = source.m_id;
        std::vector<CPoint3d> points = *CurveControlPoints(&source);
        std::vector<double> weights;
        if (const auto* spline = dynamic_cast<const CBSpline*>(&source)) {
            weights = spline->GetWeights();
            weights.resize(points.size(), 1.0);
        }
        CurveProjection projection = ProjectToControlPolygon(points, intersection);
        size_t split_index = projection.segment + 1;
        points.insert(points.begin() + static_cast<std::ptrdiff_t>(split_index), intersection);
        if (!weights.empty()) weights.insert(
            weights.begin() + static_cast<std::ptrdiff_t>(split_index),
            0.5 * (weights[projection.segment] + weights[projection.segment + 1]));
        if (split_index == 0 || split_index + 1 >= points.size()) continue;
        const std::vector<CPoint3d> first_points(points.begin(), points.begin() + split_index + 1);
        const std::vector<CPoint3d> second_points(points.begin() + split_index, points.end());
        const std::vector<double> first_weights = weights.empty() ? std::vector<double>{}
            : std::vector<double>(weights.begin(), weights.begin() + split_index + 1);
        const std::vector<double> second_weights = weights.empty() ? std::vector<double>{}
            : std::vector<double>(weights.begin() + split_index, weights.end());
        std::unique_ptr<CAlfaObject> second_piece = MakeCurvePiece(
            source, second_points, second_weights, " Part 2");
        source.SetName(source.GetName() + " Part 1");
        ReplaceCurveGeometry(source, first_points, first_weights);
        document_.AddObject(std::move(second_piece));
        changed_ids.push_back(source_id);
        split_any = true;
    }
    if (!split_any) {
        undo_redo_.CancelChange();
        return false;
    }
    for (unsigned long id : changed_ids) {
        tool_registry_.ReplayProfileDependents(id, document_);
    }
    undo_redo_.CommitChange("Split curves");
    UpdateUndoRedoActions();
    RefreshSceneTree();
    viewport_->update();
    return true;
}

bool MainWindow::ExtendSelectedCurve(CPoint3d endpoint_hint) {
    CAlfaObject* curve = nullptr;
    for (size_t index : document_.GetSelectedObjectIndices()) {
        if (index < document_.GetObjects().size()
            && IsEditableCurve(document_.GetObjects()[index].get())) {
            if (curve) return false;
            curve = document_.GetObjects()[index].get();
        }
    }
    const auto* points = CurveControlPoints(curve);
    if (!curve || !points || points->size() < 2) return false;
    const bool at_start = CurvePointDistanceSquared(endpoint_hint, points->front())
        <= CurvePointDistanceSquared(endpoint_hint, points->back());
    bool ok = false;
    const double distance = QInputDialog::getDouble(
        this, "Extend Curve", "Length, mm:", 10.0, 0.001, 1000000.0, 3, &ok);
    if (!ok) return false;

    undo_redo_.BeginChange();
    bool changed = false;
    if (auto* polyline = dynamic_cast<CPolyline*>(curve)) {
        const size_t endpoint = at_start ? 0 : polyline->GetPointCount() - 1;
        const size_t neighbor = at_start ? 1 : polyline->GetPointCount() - 2;
        CPoint3d point = polyline->GetPoints()[endpoint];
        const CPoint3d& adjacent = polyline->GetPoints()[neighbor];
        const double dx = point.x - adjacent.x;
        const double dy = point.y - adjacent.y;
        const double dz = point.z - adjacent.z;
        const double length = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (length > 1.0e-12 && !polyline->IsClosed()) {
            point.x += dx * distance / length;
            point.y += dy * distance / length;
            point.z += dz * distance / length;
            changed = polyline->SetPoint(endpoint, point);
        }
    } else if (auto* spline = dynamic_cast<CBSpline*>(curve)) {
        changed = spline->ExtendEndpoint(at_start, distance);
    }
    if (!changed) {
        undo_redo_.CancelChange();
        return false;
    }
    tool_registry_.ReplayProfileDependents(curve->m_id, document_);
    undo_redo_.CommitChange("Extend curve");
    UpdateUndoRedoActions();
    RefreshSceneTree();
    viewport_->update();
    return true;
}

bool MainWindow::TrimSelectedCurveByPlane(CPoint3d keep_point) {
    size_t curve_index = document_.GetObjects().size();
    const CAlfaObject* plane = nullptr;
    for (size_t index : document_.GetSelectedObjectIndices()) {
        if (index >= document_.GetObjects().size()) continue;
        CAlfaObject* object = document_.GetObjects()[index].get();
        if (IsEditableCurve(object)) curve_index = index;
        if (object && object->GetParametricToolId() == "PlaneTool") plane = object;
    }
    auto& objects = document_.GetObjects();
    if (curve_index >= objects.size()) return false;
    if (!plane && pending_curve_trim_plane_points_.size() != 3) return false;

    const auto value = [plane](const char* id, double fallback) {
        if (!plane) return fallback;
        const auto& parameters = plane->GetParametricParameters();
        const auto found = std::find_if(parameters.begin(), parameters.end(),
            [id](const ParametricParameterValue& parameter) {
                return parameter.id == id;
            });
        return found == parameters.end() ? fallback : found->value;
    };
    CPoint3d origin;
    Vec3 normal{};
    const int mode = plane ? static_cast<int>(value("mode", 1.0)) : -1;
    if (!plane) {
        const CPoint3d& first = pending_curve_trim_plane_points_[0];
        const CPoint3d& second = pending_curve_trim_plane_points_[1];
        const CPoint3d& third = pending_curve_trim_plane_points_[2];
        origin = first;
        normal = cross(
            Vec3{static_cast<float>(second.x - first.x),
                 static_cast<float>(second.y - first.y),
                 static_cast<float>(second.z - first.z)},
            Vec3{static_cast<float>(third.x - first.x),
                 static_cast<float>(third.y - first.y),
                 static_cast<float>(third.z - first.z)});
    } else if (mode == 0) {
        const double a = value("a", 0.0), b = value("b", 0.0);
        const double c = value("c", 1.0), d = value("d", 0.0);
        const double squared = a * a + b * b + c * c;
        if (squared <= 1.0e-18) return false;
        origin = CPoint3d(-a * d / squared, -b * d / squared, -c * d / squared);
        normal = {static_cast<float>(a), static_cast<float>(b), static_cast<float>(c)};
    } else if (mode == 1) {
        origin = CPoint3d(value("plane.origin.x", 0.0),
                          value("plane.origin.y", 0.0),
                          value("plane.origin.z", 0.0));
        normal = {static_cast<float>(value("plane.normal.x", 0.0)),
                  static_cast<float>(value("plane.normal.y", 0.0)),
                  static_cast<float>(value("plane.normal.z", 1.0))};
    } else if (mode == 2) {
        const Vec3 first{static_cast<float>(value("p1.x", 0.0)),
                         static_cast<float>(value("p1.y", 0.0)),
                         static_cast<float>(value("p1.z", 0.0))};
        const Vec3 second{static_cast<float>(value("p2.x", 100.0)),
                          static_cast<float>(value("p2.y", 0.0)),
                          static_cast<float>(value("p2.z", 0.0))};
        const Vec3 third{static_cast<float>(value("p3.x", 0.0)),
                         static_cast<float>(value("p3.y", 100.0)),
                         static_cast<float>(value("p3.z", 0.0))};
        origin = CPoint3d(first.x, first.y, first.z);
        normal = cross(second - first, third - first);
    } else {
        const double offset = value("offset", 0.0);
        if (mode == 3) { origin = CPoint3d(0.0, 0.0, offset); normal = {0, 0, 1}; }
        else if (mode == 4) { origin = CPoint3d(0.0, offset, 0.0); normal = {0, 1, 0}; }
        else { origin = CPoint3d(offset, 0.0, 0.0); normal = {1, 0, 0}; }
    }
    normal = normalize(normal);
    if (dot(normal, normal) <= 1.0e-12f) return false;
    const auto signed_distance = [&](const CPoint3d& point) {
        return (point.x - origin.x) * normal.x
             + (point.y - origin.y) * normal.y
             + (point.z - origin.z) * normal.z;
    };
    CAlfaObject& source = *objects[curve_index];
    const std::vector<CPoint3d> side_samples = CurveSamples(source);
    const CurveProjection side_projection =
        ProjectToControlPolygon(side_samples, keep_point);
    const CPoint3d side_point = side_samples.size() >= 2
        ? side_projection.point : keep_point;
    const double keep_sign = signed_distance(side_point);
    if (std::abs(keep_sign) <= 1.0e-9) return false;

    const std::vector<CPoint3d> points = *CurveControlPoints(&source);
    if (points.size() < 2) return false;
    bool has_positive_side = false;
    bool has_negative_side = false;
    for (const CPoint3d& point : points) {
        const double distance = signed_distance(point);
        has_positive_side = has_positive_side || distance > 1.0e-9;
        has_negative_side = has_negative_side || distance < -1.0e-9;
    }
    if (!has_positive_side || !has_negative_side) return false;
    std::vector<double> weights;
    if (const auto* spline = dynamic_cast<const CBSpline*>(&source)) {
        weights = spline->GetWeights();
        weights.resize(points.size(), 1.0);
    }
    std::vector<std::vector<CPoint3d>> pieces;
    std::vector<std::vector<double>> piece_weights;
    std::vector<CPoint3d> current;
    std::vector<double> current_weights;
    const auto inside = [keep_sign](double distance) {
        return keep_sign > 0.0 ? distance >= -1.0e-9 : distance <= 1.0e-9;
    };
    for (size_t i = 0; i + 1 < points.size(); ++i) {
        const double first_distance = signed_distance(points[i]);
        const double second_distance = signed_distance(points[i + 1]);
        const bool first_inside = inside(first_distance);
        const bool second_inside = inside(second_distance);
        if (first_inside && current.empty()) {
            current.push_back(points[i]);
            if (!weights.empty()) current_weights.push_back(weights[i]);
        }
        if (first_inside != second_inside) {
            const double t = first_distance / (first_distance - second_distance);
            const CPoint3d crossing(
                points[i].x + (points[i + 1].x - points[i].x) * t,
                points[i].y + (points[i + 1].y - points[i].y) * t,
                points[i].z + (points[i + 1].z - points[i].z) * t);
            const double crossing_weight = weights.empty() ? 1.0
                : weights[i] + (weights[i + 1] - weights[i]) * t;
            current.push_back(crossing);
            if (!weights.empty()) current_weights.push_back(crossing_weight);
            if (first_inside) {
                if (current.size() >= 2) {
                    pieces.push_back(current);
                    piece_weights.push_back(current_weights);
                }
                current.clear();
                current_weights.clear();
            }
        }
        if (second_inside) {
            if (current.empty() && !first_inside) {
                const double t = first_distance / (first_distance - second_distance);
                current.emplace_back(
                    points[i].x + (points[i + 1].x - points[i].x) * t,
                    points[i].y + (points[i + 1].y - points[i].y) * t,
                    points[i].z + (points[i + 1].z - points[i].z) * t);
                if (!weights.empty()) current_weights.push_back(
                    weights[i] + (weights[i + 1] - weights[i]) * t);
            }
            current.push_back(points[i + 1]);
            if (!weights.empty()) current_weights.push_back(weights[i + 1]);
        }
    }
    if (current.size() >= 2) {
        pieces.push_back(current);
        piece_weights.push_back(current_weights);
    }
    if (pieces.empty()) return false;
    std::vector<std::unique_ptr<CAlfaObject>> additional;
    for (size_t piece = 1; piece < pieces.size(); ++piece) {
        additional.push_back(MakeCurvePiece(
            source, pieces[piece], piece_weights[piece],
            " Trim " + std::to_string(piece + 1)));
    }
    undo_redo_.BeginChange();
    ReplaceCurveGeometry(source, pieces.front(), piece_weights.front());
    for (auto& object : additional) document_.AddObject(std::move(object));
    tool_registry_.ReplayProfileDependents(source.m_id, document_);
    undo_redo_.CommitChange("Trim curve by plane");
    UpdateUndoRedoActions();
    RefreshSceneTree();
    viewport_->update();
    return true;
}

void MainWindow::BeginPlaneThreePointPick() {
    if (active_parametric_object_.tool_id != "PlaneTool") return;
    plane_three_point_picks_.clear();
    viewport_->ClearPointPickMarkers();
    plane_three_point_pick_active_ = true;
    viewport_->BeginPick3DPoint("Plane — Point 1 of 3:select the required geometry and continue");
}

void MainWindow::AppendPlaneThreePointPick(CPoint3d point) {
    if (!plane_three_point_pick_active_
        || active_parametric_object_.tool_id != "PlaneTool") return;
    plane_three_point_picks_.push_back(point);
    viewport_->SetPointPickMarkers(plane_three_point_picks_);
    if (plane_three_point_picks_.size() == 3) {
        const CPoint3d& first = plane_three_point_picks_[0];
        const CPoint3d& second = plane_three_point_picks_[1];
        const CPoint3d& third = plane_three_point_picks_[2];
        const Vec3 first_edge{static_cast<float>(second.x - first.x),
                              static_cast<float>(second.y - first.y),
                              static_cast<float>(second.z - first.z)};
        const Vec3 second_edge{static_cast<float>(third.x - first.x),
                               static_cast<float>(third.y - first.y),
                               static_cast<float>(third.z - first.z)};
        if (dot(cross(first_edge, second_edge), cross(first_edge, second_edge))
            <= 1.0e-12f) {
            plane_three_point_picks_.pop_back();
            viewport_->SetPointPickMarkers(plane_three_point_picks_);
            statusBar()->showMessage(
                "Plane:operation failed; check the selected geometry and parameters", 2600);
            viewport_->BeginPick3DPoint(
                "Plane — Point 3 of 3:select the required geometry and continue");
            return;
        }
    }

    const size_t point_index = plane_three_point_picks_.size() - 1;
    const CPoint3d& picked = plane_three_point_picks_.back();
    const std::string prefix = "p" + std::to_string(point_index + 1) + ".";
    for (ToolParameter& parameter : active_parametric_object_.parameters) {
        if (parameter.id == prefix + "x") parameter.value = picked.x;
        else if (parameter.id == prefix + "y") parameter.value = picked.y;
        else if (parameter.id == prefix + "z") parameter.value = picked.z;
    }
    property_panel_->SetActiveObject(active_parametric_object_);
    tool_registry_.Rebuild(active_parametric_object_, document_);
    if (active_parametric_object_.object_index < document_.GetObjects().size()
        && document_.GetObjects()[active_parametric_object_.object_index]) {
        tool_registry_.ReplayAllTrimDependents(
            document_,
            document_.GetObjects()[active_parametric_object_.object_index]->m_id);
    }
    RefreshSceneTree();
    viewport_->update();

    if (plane_three_point_picks_.size() < 3) {
        const size_t next = plane_three_point_picks_.size() + 1;
        viewport_->BeginPick3DPoint(
            QString("Plane — Point %1 of 3:select the required geometry and continue").arg(next));
        return;
    }
    SaveRememberedPlaneFactors(PlaneFactorsFromPoints(
        plane_three_point_picks_[0],
        plane_three_point_picks_[1],
        plane_three_point_picks_[2]));
    plane_three_point_pick_active_ = false;
    viewport_->ClearPointPickMarkers();
    statusBar()->showMessage(
        "Plane:operation completed",
        3000);
}

void MainWindow::CancelPlaneThreePointPick(const QString& message) {
    plane_three_point_pick_active_ = false;
    plane_three_point_picks_.clear();
    if (viewport_) viewport_->ClearPointPickMarkers();
    if (!message.isEmpty()) statusBar()->showMessage(message, 2200);
}

bool MainWindow::CompletePendingPlaneFacePick() {
    Vec3 center{};
    Vec3 x_axis{};
    Vec3 y_axis{};
    Vec3 normal{};
    unsigned long body_id = 0;
    int face_index = -1;
    if (!document_.HasSelectedSolidFace()) {
        return false;
    }
    if (!document_.GetSelectedSolidFaceSketchPlane(
            center, x_axis, y_axis, normal, body_id, face_index)) {
        statusBar()->showMessage(
            "Face of Solid:select the required geometry and continue", 2500);
        return true;
    }
    normal = normalize(normal);
    if (dot(normal, normal) <= 1.0e-12f) return false;
    const CPoint3d origin(center.x, center.y, center.z);
    const std::array<double, 4> factors{
        normal.x, normal.y, normal.z,
        -(normal.x * center.x + normal.y * center.y + normal.z * center.z)};
    SaveRememberedPlaneFactors(factors);

    if (pending_body_section_plane_face_pick_) {
        pending_body_section_plane_face_pick_ = false;
        viewport_->SetSelectionMode(SelectionMode::Object);
        CompleteBodySectionByPlane(center, normal);
        return true;
    }

    if (pending_trim_plane_face_pick_) {
        const unsigned long curve_id = pending_trim_plane_curve_id_;
        pending_trim_plane_face_pick_ = false;
        pending_trim_plane_curve_id_ = 0;
        pending_curve_trim_plane_points_ = PlanePointsFromOriginNormal(origin, normal);
        // SetSelectionMode clears the current selection.  Switch out of Face
        // mode before restoring the curve; otherwise the restored Polyline is
        // immediately deselected and the following trim has no source object.
        viewport_->SetSelectionMode(SelectionMode::Object);
        document_.ClearSelection();
        document_.SelectObjectById(curve_id, SelectionAction::Replace);
        viewport_->BeginPick3DPointOnObject(
            curve_id,
            "Trim by Plane:select the required geometry and continue");
        RefreshSceneTree();
        viewport_->update();
        return true;
    }

    if (!pending_reference_plane_face_pick_) return false;
    pending_reference_plane_face_pick_ = false;
    // Face -> Object clears selection, so perform it before creating the
    // parametric plane that becomes the active object.
    viewport_->SetSelectionMode(SelectionMode::Object);
    const ToolDefinition* definition = tool_registry_.Find("PlaneTool");
    if (!definition) return true;
    std::vector<ToolParameter> parameters = definition->defaults;
    const auto set_value = [&parameters](const char* id, double value) {
        const auto found = std::find_if(parameters.begin(), parameters.end(),
            [id](const ToolParameter& parameter) { return parameter.id == id; });
        if (found != parameters.end()) found->value = value;
    };
    set_value("mode", 1.0);
    set_value("plane.origin.x", center.x);
    set_value("plane.origin.y", center.y);
    set_value("plane.origin.z", center.z);
    set_value("plane.normal.x", normal.x);
    set_value("plane.normal.y", normal.y);
    set_value("plane.normal.z", normal.z);
    active_parametric_object_ = tool_registry_.CreateParametricObject(
        "PlaneTool", document_, parameters);
    active_parametric_edit_existing_ = false;
    if (!active_parametric_object_.tool_id.empty()) {
        property_panel_->SetActiveObject(active_parametric_object_);
        ShowPropertyPanelAtCursor("Plane");
    }
    UpdateActiveToolUi("PlaneTool");
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage("Plane:operation completed");
    return true;
}

bool MainWindow::UpdateNurbsParameterEditor() {
    const size_t object_index = document_.GetSelectedObjectIndex();
    auto& objects = document_.GetObjects();
    auto* spline = document_.HasSelection() && object_index < objects.size()
        ? dynamic_cast<CBSpline*>(objects[object_index].get()) : nullptr;
    if (!spline || spline->GetCurveType() != SplineCurveType::Nurbs) {
        if (active_parametric_object_.tool_id == "NurbsParameters") {
            CancelNurbsParameterChanges();
        }
        return false;
    }

    if (active_parametric_object_.tool_id != "NurbsParameters"
        || nurbs_parameter_object_id_ != spline->m_id) {
        ClearActiveProperties();
        nurbs_parameter_object_id_ = spline->m_id;
        nurbs_parameter_original_degree_ = spline->GetDegree();
        nurbs_parameter_original_weights_ = spline->GetWeights();
        nurbs_parameter_original_knots_ = spline->GetKnots();
        nurbs_parameters_modified_ = false;
        undo_redo_.BeginChange();
    }

    std::vector<size_t> selected_points;
    for (const auto& selected : document_.GetSelectedCurvePoints()) {
        if (selected.first == object_index
            && selected.second < spline->GetPointCount()) {
            selected_points.push_back(selected.second);
        }
    }

    const int maximum_degree = std::max(
        1, static_cast<int>(spline->GetPointCount()) - 1);
    std::vector<ToolParameter> parameters{
        {"spline.degree", "Degree", static_cast<double>(spline->GetDegree()),
         1.0, static_cast<double>(maximum_degree), 1.0}
    };
    if (!selected_points.empty()) {
        const std::vector<double>& weights = spline->GetWeights();
        const double first_weight = selected_points.front() < weights.size()
            ? weights[selected_points.front()] : 1.0;
        nurbs_parameter_displayed_weight_ = first_weight;
        const bool mixed = std::any_of(
            selected_points.begin(), selected_points.end(),
            [&weights, first_weight](size_t point_index) {
                const double weight = point_index < weights.size()
                    ? weights[point_index] : 1.0;
                return std::abs(weight - first_weight) > 1.0e-9;
            });
        const std::string label = "Weight (" + std::to_string(selected_points.size())
            + (mixed ? ", mixed)" : ")");
        parameters.push_back(
            {"spline.weight", label, first_weight, 0.01, 1000.0, 0.01});
    }

    active_parametric_object_ = {
        "NurbsParameters", object_index, 0, std::move(parameters)};
    active_parametric_edit_existing_ = true;
    property_panel_->SetActiveObject(active_parametric_object_);
    if (!properties_dock_->isVisible()) {
        ShowPropertyPanelAtCursor("NURBS");
    } else {
        properties_dock_->setWindowTitle("NURBS Parameters");
    }
    statusBar()->showMessage(selected_points.empty()
        ? "NURBS:select the required geometry and continue"
        : QString("NURBS: Weight operation status")
              .arg(selected_points.size()));
    return true;
}

void MainWindow::ApplyNurbsParameterChanges() {
    const size_t object_index = document_.FindObjectIndexById(
        nurbs_parameter_object_id_);
    auto& objects = document_.GetObjects();
    auto* spline = object_index < objects.size()
        ? dynamic_cast<CBSpline*>(objects[object_index].get()) : nullptr;
    if (!spline || spline->GetCurveType() != SplineCurveType::Nurbs) {
        return;
    }

    const auto parameter_value = [this](const char* id, double fallback) {
        const auto found = std::find_if(
            active_parametric_object_.parameters.begin(),
            active_parametric_object_.parameters.end(),
            [id](const ToolParameter& parameter) { return parameter.id == id; });
        return found == active_parametric_object_.parameters.end()
            ? fallback : found->value;
    };

    const int degree = static_cast<int>(std::lround(
        parameter_value("spline.degree", spline->GetDegree())));
    if (degree != spline->GetDegree()) {
        spline->SetDegree(degree);
        nurbs_parameters_modified_ = true;
    }

    const auto weight_parameter = std::find_if(
        active_parametric_object_.parameters.begin(),
        active_parametric_object_.parameters.end(),
        [](const ToolParameter& parameter) {
            return parameter.id == "spline.weight";
        });
    size_t changed_weight_count = 0;
    if (weight_parameter != active_parametric_object_.parameters.end()
        && std::abs(weight_parameter->value
                    - nurbs_parameter_displayed_weight_) > 1.0e-9) {
        for (const auto& selected : document_.GetSelectedCurvePoints()) {
            if (selected.first != object_index
                || selected.second >= spline->GetPointCount()) {
                continue;
            }
            const std::vector<double>& weights = spline->GetWeights();
            const double old_weight = selected.second < weights.size()
                ? weights[selected.second] : 1.0;
            if (std::abs(old_weight - weight_parameter->value) <= 1.0e-9) {
                continue;
            }
            spline->SetWeight(selected.second, weight_parameter->value);
            ++changed_weight_count;
            nurbs_parameters_modified_ = true;
        }
        nurbs_parameter_displayed_weight_ = weight_parameter->value;
    }

    tool_registry_.ReplayProfileDependents(spline->m_id, document_);
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage(changed_weight_count > 0
        ? QString("NURBS: Weight %1 operation completed")
              .arg(weight_parameter->value, 0, 'f', 3)
              .arg(changed_weight_count)
        : QString("NURBS: Degree %1").arg(spline->GetDegree()));
}

void MainWindow::AcceptNurbsParameterChanges() {
    if (active_parametric_object_.tool_id != "NurbsParameters") {
        return;
    }
    if (nurbs_parameters_modified_) {
        undo_redo_.CommitChange("Edit NURBS parameters");
    } else {
        undo_redo_.CancelChange();
    }
    nurbs_parameter_object_id_ = 0;
    nurbs_parameter_original_weights_.clear();
    nurbs_parameter_original_knots_.clear();
    nurbs_parameter_displayed_weight_ = 1.0;
    nurbs_parameters_modified_ = false;
    active_parametric_object_ = {};
    active_parametric_edit_existing_ = false;
    property_panel_->Clear();
    if (properties_dock_) {
        properties_dock_->hide();
    }
    UpdateUndoRedoActions();
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage("NURBS parameters accepted", 1400);
}

void MainWindow::CancelNurbsParameterChanges() {
    if (active_parametric_object_.tool_id != "NurbsParameters") {
        return;
    }
    const size_t object_index = document_.FindObjectIndexById(
        nurbs_parameter_object_id_);
    auto& objects = document_.GetObjects();
    auto* spline = object_index < objects.size()
        ? dynamic_cast<CBSpline*>(objects[object_index].get()) : nullptr;
    if (spline) {
        spline->SetDegree(nurbs_parameter_original_degree_);
        spline->SetWeights(nurbs_parameter_original_weights_);
        if (!nurbs_parameter_original_knots_.empty()) {
            spline->SetKnots(nurbs_parameter_original_knots_);
        }
        tool_registry_.ReplayProfileDependents(spline->m_id, document_);
    }
    undo_redo_.CancelChange();
    nurbs_parameter_object_id_ = 0;
    nurbs_parameter_original_weights_.clear();
    nurbs_parameter_original_knots_.clear();
    nurbs_parameter_displayed_weight_ = 1.0;
    nurbs_parameters_modified_ = false;
    active_parametric_object_ = {};
    active_parametric_edit_existing_ = false;
    property_panel_->Clear();
    if (properties_dock_) {
        properties_dock_->hide();
    }
    UpdateUndoRedoActions();
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage("NURBS parameter changes canceled", 1400);
}

void MainWindow::CreateSelectedGroup() {
    if (document_.GetSelectedObjectCount() < 2) {
        pending_group_command_ = PendingGroupCommand::Create;
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionMode(SelectionMode::Object);
        viewport_->SetSelectionConfirmationMode(true);
        UpdateActiveToolUi("CreateGroup");
        statusBar()->showMessage("Create Group: select at least two objects, then press Enter");
        return;
    }
    if (!document_.CreateGroupFromSelection()) {
        statusBar()->showMessage("Create Group: could not create group", 1600);
        return;
    }
    RecordDocumentChange("Create group");
    pending_group_command_ = PendingGroupCommand::None;
    viewport_->SetSelectionConfirmationMode(false);
    viewport_->SetTool(ToolMode::Select);
    viewport_->SetSelectionMode(SelectionMode::Object);
    UpdateActiveToolUi("select");
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage("Group created", 1200);
}

void MainWindow::UngroupSelectedGroup() {
    if (!document_.UngroupSelection()) {
        pending_group_command_ = PendingGroupCommand::Ungroup;
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionMode(SelectionMode::Object);
        viewport_->SetSelectionConfirmationMode(true);
        UpdateActiveToolUi("UnGroup");
        statusBar()->showMessage("UnGroup: select a group, then press Enter");
        return;
    }
    RecordDocumentChange("Ungroup");
    pending_group_command_ = PendingGroupCommand::None;
    viewport_->SetSelectionConfirmationMode(false);
    viewport_->SetTool(ToolMode::Select);
    viewport_->SetSelectionMode(SelectionMode::Object);
    UpdateActiveToolUi("select");
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage("Group dissolved", 1200);
}

void MainWindow::CreateSelectedAssembly() {
    if (!document_.CreateAssemblyFromSelection()) {
        pending_group_command_ = PendingGroupCommand::CreateAssembly;
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionConfirmationMode(true);
        UpdateActiveToolUi("CreateAssembly");
        statusBar()->showMessage("Create Assembly: select at least two objects, then press Enter");
        return;
    }
    RecordDocumentChange("Create assembly");
    pending_group_command_ = PendingGroupCommand::None;
    viewport_->SetSelectionConfirmationMode(false);
    viewport_->SetTool(ToolMode::Select);
    viewport_->update();
    RefreshSceneTree();
    statusBar()->showMessage("Assembly created", 1400);
}

void MainWindow::CreateBodyFromTwoSketches() {
    if (!document_.CreateSolidFromTwoSelectedSketches()) {
        pending_group_command_ = PendingGroupCommand::TwoSketchBody;
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionConfirmationMode(true);
        UpdateActiveToolUi("SolidTwoSketches");
        statusBar()->showMessage("Body by Two Sketches: select exactly two closed sketches, then press Enter");
        return;
    }
    RecordDocumentChange("Body by two sketches");
    pending_group_command_ = PendingGroupCommand::None;
    viewport_->SetSelectionConfirmationMode(false);
    viewport_->SetTool(ToolMode::Select);
    viewport_->update();
    RefreshSceneTree();
    statusBar()->showMessage("Body by Two Sketches created", 1400);
}

void MainWindow::CreateAssociativeClone() {
    if (!document_.CreateAssociativeCloneFromSelection()) {
        pending_group_command_ = PendingGroupCommand::AssociativeClone;
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionConfirmationMode(true);
        UpdateActiveToolUi("AssociativeClone");
        statusBar()->showMessage("Associative Clone: select one solid, then press Enter");
        return;
    }
    RecordDocumentChange("Associative clone");
    pending_group_command_ = PendingGroupCommand::None;
    viewport_->SetSelectionConfirmationMode(false);
    viewport_->update();
    RefreshSceneTree();
    viewport_->BeginMovePointToPoint();
    statusBar()->showMessage("Associative clone created. Pick source point, then target point");
}

void MainWindow::CreateTwoRailSweepSurface() {
    if (!document_.CreateTwoRailSweepSurfaceFromSelection()) {
        pending_group_command_ = PendingGroupCommand::TwoRailSweepSurface;
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionMode(SelectionMode::Object);
        viewport_->SetSelectionConfirmationMode(true);
        UpdateActiveToolUi("SurfaceSweepTwoRails");
        statusBar()->showMessage(
            "Sweep Surface (2 Rails):select the required geometry and continue");
        return;
    }
    RecordDocumentChange("Two-rail sweep surface");
    pending_group_command_ = PendingGroupCommand::None;
    viewport_->SetSelectionConfirmationMode(false);
    viewport_->SetTool(ToolMode::Select);
    UpdateActiveToolUi("select");
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage("Sweep Surface (2 Rails) operation completed", 1600);
}

void MainWindow::JoinSelectedSurfaces() {
    const bool enough_surfaces_selected =
        document_.GetSelectedObjectCount() >= 2;
    if (!document_.JoinSelectedSurfaces()) {
        if (enough_surfaces_selected) {
            pending_group_command_ = PendingGroupCommand::None;
            viewport_->SetSelectionConfirmationMode(false);
            viewport_->SetTool(ToolMode::Select);
            UpdateActiveToolUi("select");
            statusBar()->showMessage(
                "Join:operation failed; check the selected geometry and parameters",
                2600);
            return;
        }
        pending_group_command_ = PendingGroupCommand::JoinSurfaces;
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionMode(SelectionMode::Object);
        viewport_->SetSelectionConfirmationMode(true);
        UpdateActiveToolUi("SurfaceJoin");
        statusBar()->showMessage(
            "Join Surfaces:select the required geometry and continue");
        return;
    }

    RecordDocumentChange("Join surfaces");
    pending_group_command_ = PendingGroupCommand::None;
    viewport_->SetSelectionConfirmationMode(false);
    viewport_->SetTool(ToolMode::Select);
    UpdateActiveToolUi("select");
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage("Surfaces joined", 1600);
}

void MainWindow::CreatePlaneIntersection() {
    const CSolid* solid = document_.GetSelectedSolid();
    pending_group_command_ = PendingGroupCommand::PlaneIntersection;
    pending_body_section_target_id_ = solid ? solid->m_id : 0;
    pending_body_section_plane_face_pick_ = false;
    pending_body_section_plane_object_pick_ = false;
    pending_body_section_three_point_pick_ = false;
    pending_body_section_plane_points_.clear();
    viewport_->SetTool(ToolMode::Select);
    viewport_->SetSelectionMode(SelectionMode::Object);
    viewport_->SetSelectionConfirmationMode(false);
    UpdateActiveToolUi("PlaneIntersection");
    if (pending_body_section_target_id_ != 0) {
        document_.ClearSelection();
        RefreshSceneTree();
        BeginBodySectionPlaneInput();
        return;
    }
    statusBar()->showMessage("Body Section by Plane: click the Solid body");
}

void MainWindow::BeginBodySectionPlaneInput() {
    if (pending_body_section_target_id_ == 0) return;
    std::array<double, 4> factors = LoadRememberedPlaneFactors();
    const int method = ShowPlaneDefinitionDialog(this, factors);
    if (method == QDialog::Rejected) {
        pending_body_section_target_id_ = 0;
        CancelPendingGroupCommand("Body Section by Plane: operation canceled");
        return;
    }
    if (method == 7) {
        pending_body_section_plane_face_pick_ = true;
        viewport_->SetSelectionMode(SelectionMode::Face);
        statusBar()->showMessage("Body Section by Plane: click a planar face");
        return;
    }
    if (method == 8) {
        pending_body_section_plane_object_pick_ = true;
        viewport_->SetSelectionMode(SelectionMode::Object);
        statusBar()->showMessage(
            "Body Section by Plane: click a reference Plane, planar Sketch, or planar Polyline");
        return;
    }
    if (method == 1) {
        BeginBodySectionThreePointPick();
        return;
    }
    Vec3 origin{};
    Vec3 normal{};
    if (method == 2) normal = {0.0f, 0.0f, 1.0f};
    else if (method == 3) normal = {0.0f, 1.0f, 0.0f};
    else if (method == 4) normal = {1.0f, 0.0f, 0.0f};
    else if (method == 6) {
        std::vector<double> values{0.0, 0.0, 0.0, 0.0, 0.0, 1.0};
        if (!ShowPlaneValuesDialog(
                this, "Point + Normal",
                {"Point X", "Point Y", "Point Z",
                 "Normal X", "Normal Y", "Normal Z"}, values)) {
            pending_body_section_target_id_ = 0;
            CancelPendingGroupCommand(
                "Body Section by Plane: operation canceled");
            return;
        }
        origin = {static_cast<float>(values[0]),
                  static_cast<float>(values[1]),
                  static_cast<float>(values[2])};
        normal = {static_cast<float>(values[3]),
                  static_cast<float>(values[4]),
                  static_cast<float>(values[5])};
    } else {
        normal = {static_cast<float>(factors[0]),
                  static_cast<float>(factors[1]),
                  static_cast<float>(factors[2])};
        const double length_squared = dot(normal, normal);
        if (length_squared <= 1.0e-18) {
            statusBar()->showMessage("Body Section by Plane: invalid plane factors", 2600);
            BeginBodySectionPlaneInput();
            return;
        }
        origin = normal * static_cast<float>(-factors[3] / length_squared);
    }
    if (dot(normal, normal) <= 1.0e-18f) {
        statusBar()->showMessage(
            "Body Section by Plane: invalid plane normal", 2600);
        BeginBodySectionPlaneInput();
        return;
    }
    SaveRememberedPlaneFactors({normal.x, normal.y, normal.z,
        -(normal.x * origin.x + normal.y * origin.y + normal.z * origin.z)});
    CompleteBodySectionByPlane(origin, normal);
}

void MainWindow::BeginBodySectionThreePointPick() {
    pending_body_section_plane_points_.clear();
    pending_body_section_three_point_pick_ = true;
    viewport_->ClearPointPickMarkers();
    viewport_->BeginPick3DPoint(
        "Body Section by Plane — Point 1 of 3: select a point");
}

void MainWindow::AppendBodySectionThreePointPick(CPoint3d point) {
    if (!pending_body_section_three_point_pick_
        || pending_body_section_target_id_ == 0) {
        return;
    }
    pending_body_section_plane_points_.push_back(point);
    viewport_->SetPointPickMarkers(pending_body_section_plane_points_);
    if (pending_body_section_plane_points_.size() < 3) {
        viewport_->BeginPick3DPoint(
            QString("Body Section by Plane — Point %1 of 3: select a point")
                .arg(pending_body_section_plane_points_.size() + 1));
        return;
    }

    const CPoint3d& first = pending_body_section_plane_points_[0];
    const CPoint3d& second = pending_body_section_plane_points_[1];
    const CPoint3d& third = pending_body_section_plane_points_[2];
    const Vec3 first_edge{
        static_cast<float>(second.x - first.x),
        static_cast<float>(second.y - first.y),
        static_cast<float>(second.z - first.z)};
    const Vec3 second_edge{
        static_cast<float>(third.x - first.x),
        static_cast<float>(third.y - first.y),
        static_cast<float>(third.z - first.z)};
    const Vec3 normal = normalize(cross(first_edge, second_edge));
    if (dot(normal, normal) <= 1.0e-12f) {
        pending_body_section_plane_points_.pop_back();
        viewport_->SetPointPickMarkers(pending_body_section_plane_points_);
        statusBar()->showMessage(
            "Body Section by Plane: the three points must not be collinear",
            2600);
        viewport_->BeginPick3DPoint(
            "Body Section by Plane — Point 3 of 3: select a point");
        return;
    }

    const Vec3 origin{static_cast<float>(first.x),
                      static_cast<float>(first.y),
                      static_cast<float>(first.z)};
    SaveRememberedPlaneFactors({
        normal.x, normal.y, normal.z,
        -(normal.x * origin.x + normal.y * origin.y + normal.z * origin.z)});
    pending_body_section_three_point_pick_ = false;
    pending_body_section_plane_points_.clear();
    viewport_->ClearPointPickMarkers();
    CompleteBodySectionByPlane(origin, normal);
}

void MainWindow::CancelBodySectionThreePointPick(const QString& message) {
    pending_body_section_three_point_pick_ = false;
    pending_body_section_plane_points_.clear();
    pending_body_section_target_id_ = 0;
    if (viewport_)
        viewport_->ClearPointPickMarkers();
    CancelPendingGroupCommand(message);
}

bool MainWindow::CompleteBodySectionPlaneObjectPick() {
    if (!pending_body_section_plane_object_pick_ || !document_.HasSelection()) {
        return false;
    }
    const CAlfaObject* object = document_.GetSelectedObject();
    if (!object) return false;
    Vec3 origin{};
    Vec3 normal{};
    std::string error_message;
    if (!document_.GetObjectPlane(object->m_id, origin, normal, &error_message)) {
        statusBar()->showMessage(QString::fromStdString(error_message), 2800);
        return true;
    }
    pending_body_section_plane_object_pick_ = false;
    CompleteBodySectionByPlane(origin, normal);
    return true;
}

void MainWindow::CompleteBodySectionByPlane(Vec3 origin, Vec3 normal) {
    std::string error_message;
    const size_t created = document_.CreatePlaneIntersectionCurves(
        pending_body_section_target_id_, origin, normal, &error_message);
    if (created == 0) {
        statusBar()->showMessage(QString::fromStdString(error_message), 3000);
        BeginBodySectionPlaneInput();
        return;
    }
    RecordDocumentChange("Body section by plane");
    pending_body_section_target_id_ = 0;
    pending_body_section_plane_face_pick_ = false;
    pending_body_section_plane_object_pick_ = false;
    pending_body_section_three_point_pick_ = false;
    pending_body_section_plane_points_.clear();
    viewport_->ClearPointPickMarkers();
    pending_group_command_ = PendingGroupCommand::None;
    viewport_->SetSelectionConfirmationMode(false);
    viewport_->SetTool(ToolMode::Select);
    UpdateActiveToolUi("select");
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage(
        QString("Body Section by Plane: created %1 section curve(s)").arg(created), 1800);
}

void MainWindow::CreateSurfaceIntersection() {
    const size_t created = document_.CreateSurfaceIntersectionCurves();
    if (created == 0) {
        pending_group_command_ = PendingGroupCommand::SurfaceIntersection;
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionMode(SelectionMode::Object);
        viewport_->SetSelectionConfirmationMode(true);
        UpdateActiveToolUi("SurfaceIntersection");
        statusBar()->showMessage(
            "Surface Intersection:select the required geometry and continue");
        return;
    }
    RecordDocumentChange("Surface intersection curves");
    pending_group_command_ = PendingGroupCommand::None;
    viewport_->SetSelectionConfirmationMode(false);
    viewport_->SetTool(ToolMode::Select);
    UpdateActiveToolUi("select");
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage(
        QString("Surface Intersection:operation completed").arg(created), 1800);
}

void MainWindow::ProjectCurveToSurface() {
    const auto& objects = document_.GetObjects();
    size_t curve_count = 0;
    size_t surface_count = 0;
    for (size_t index : document_.GetSelectedObjectIndices()) {
        if (index >= objects.size() || !objects[index]) continue;
        if (dynamic_cast<const CSurfaceSet*>(objects[index].get())) {
            ++surface_count;
        } else if (IsEditableCurve(objects[index].get())
                   || dynamic_cast<const CCadCurve3D*>(objects[index].get())
                   || dynamic_cast<const CSmartLine*>(objects[index].get())) {
            ++curve_count;
        }
    }
    if (curve_count != 1 || surface_count != 1) {
        pending_group_command_ = PendingGroupCommand::ProjectCurveToSurface;
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionMode(SelectionMode::Object);
        viewport_->SetSelectionConfirmationMode(true);
        UpdateActiveToolUi("ProjectCurveToSurface");
        statusBar()->showMessage(
            "Project Curve:select the required geometry and continue");
        return;
    }

    std::vector<double> values{0.0, 0.0, 1.0};
    if (!ShowPlaneValuesDialog(
            this, "Projection Direction", {"Vector X", "Vector Y", "Vector Z"},
            values)) {
        CancelPendingGroupCommand("Project Curve:operation canceled");
        return;
    }
    const Vec3 direction{static_cast<float>(values[0]),
                         static_cast<float>(values[1]),
                         static_cast<float>(values[2])};
    const size_t created = document_.ProjectSelectedCurveToSurface(direction);
    if (created == 0) {
        pending_group_command_ = PendingGroupCommand::None;
        viewport_->SetSelectionConfirmationMode(false);
        UpdateActiveToolUi("select");
        statusBar()->showMessage(
            "Project Curve:operation failed; check the selected geometry and parameters",
            2600);
        return;
    }
    RecordDocumentChange("Project curve to surface");
    pending_group_command_ = PendingGroupCommand::None;
    viewport_->SetSelectionConfirmationMode(false);
    viewport_->SetTool(ToolMode::Select);
    UpdateActiveToolUi("select");
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage(
        QString("Project Curve:operation completed").arg(created), 1800);
}

void MainWindow::ExtractSurfaceEdge() {
    const size_t created = document_.ExtractSelectedSurfaceEdges();
    if (created == 0) {
        pending_group_command_ = PendingGroupCommand::ExtractSurfaceEdge;
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionMode(SelectionMode::Edge);
        viewport_->SetSelectionConfirmationMode(true);
        UpdateActiveToolUi("ExtractSurfaceEdge");
        statusBar()->showMessage(
            "Extract Edge:select the required geometry and continue");
        return;
    }
    RecordDocumentChange("Extract surface edges");
    pending_group_command_ = PendingGroupCommand::None;
    viewport_->SetSelectionConfirmationMode(false);
    viewport_->SetTool(ToolMode::Select);
    UpdateActiveToolUi("select");
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage(
        QString("Extract Edge:operation completed").arg(created), 1800);
}

void MainWindow::CreateFourSplineSurface() {
    if (!document_.CreateFourSplineSurfaceFromSelection()) {
        pending_group_command_ = PendingGroupCommand::FourSplineSurface;
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionMode(SelectionMode::Object);
        viewport_->SetSelectionConfirmationMode(true);
        UpdateActiveToolUi("SurfaceFourSplines");
        statusBar()->showMessage(
            "Surface by 4 Splines:select the required geometry and continue");
        return;
    }
    RecordDocumentChange("Four-spline surface");
    pending_group_command_ = PendingGroupCommand::None;
    viewport_->SetSelectionConfirmationMode(false);
    viewport_->SetTool(ToolMode::Select);
    UpdateActiveToolUi("select");
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage("Surface by 4 Splines operation completed", 1600);
}

void MainWindow::CreateTwoRailSweepSolid() {
    if (!document_.CreateTwoRailSweepSolidFromSelection()) {
        pending_group_command_ = PendingGroupCommand::TwoRailSweepSolid;
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionMode(SelectionMode::Object);
        viewport_->SetSelectionConfirmationMode(true);
        UpdateActiveToolUi("SolidSweepTwoRails");
        statusBar()->showMessage(
            "Sweep Solid (2 Rails):select the required geometry and continue");
        return;
    }
    RecordDocumentChange("Two-rail sweep solid");
    pending_group_command_ = PendingGroupCommand::None;
    viewport_->SetSelectionConfirmationMode(false);
    viewport_->SetTool(ToolMode::Select);
    UpdateActiveToolUi("select");
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage(
        "Sweep Solid (2 Rails) operation completed",
        2200);
}

void MainWindow::CancelPendingGroupCommand(const QString& status_text) {
    pending_group_command_ = PendingGroupCommand::None;
    viewport_->SetSelectionConfirmationMode(false);
    viewport_->SetTool(ToolMode::Select);
    UpdateActiveToolUi("select");
    viewport_->setFocus();
    if (!status_text.isEmpty()) {
        statusBar()->showMessage(status_text, 1200);
    }
}

bool MainWindow::HasSelectedGroup() const {
    const auto& objects = document_.GetObjects();
    for (size_t index : document_.GetSelectedObjectIndices()) {
        if (index < objects.size() && dynamic_cast<const CGroup*>(objects[index].get())) {
            return true;
        }
    }
    return false;
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
        auto* rotate_90 = new QCheckBox("Rotate texture 90°", &dialog);
        rotate_90->setChecked(std::abs(
            std::fmod(std::fmod(static_cast<double>(current.texture_rotation_degrees), 360.0) + 360.0, 360.0)
            - 90.0) < 0.001);
        auto* fit_to_surface = new QCheckBox("Fit one image to the whole surface", &dialog);
        fit_to_surface->setChecked(current.texture_fit_to_surface);
        rotation->setSuffix(QString::fromUtf8("°"));

        form->addRow(new DragSpinBoxLabel("Offset U", offset_u, &dialog), offset_u);
        form->addRow(new DragSpinBoxLabel("Offset V", offset_v, &dialog), offset_v);
        form->addRow(new DragSpinBoxLabel("Scale U", scale_u, &dialog), scale_u);
        form->addRow(new DragSpinBoxLabel("Scale V", scale_v, &dialog), scale_v);
        form->addRow(new DragSpinBoxLabel("Rotate", rotation, &dialog), rotation);
        form->addRow(rotate_90);
        form->addRow(fit_to_surface);

        const auto apply_preview = [this, selected_meshes, offset_u, offset_v, scale_u, scale_v, rotation, fit_to_surface]() {
            for (CMesh3D* mesh : selected_meshes) {
                Material material = mesh->GetMaterial();
                material.texture_offset_u = static_cast<float>(offset_u->value());
                material.texture_offset_v = static_cast<float>(offset_v->value());
                material.texture_scale_u = static_cast<float>(scale_u->value());
                material.texture_scale_v = static_cast<float>(scale_v->value());
                material.texture_rotation_degrees = static_cast<float>(rotation->value());
                material.texture_fit_to_surface = fit_to_surface->isChecked();
                mesh->SetMaterial(std::move(material));
            }
            viewport_->update();
        };
        connect(offset_u, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dialog, [apply_preview](double) { apply_preview(); });
        connect(offset_v, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dialog, [apply_preview](double) { apply_preview(); });
        connect(scale_u, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dialog, [apply_preview](double) { apply_preview(); });
        connect(scale_v, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dialog, [apply_preview](double) { apply_preview(); });
        connect(rotation, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dialog, [apply_preview](double) { apply_preview(); });
        connect(rotate_90, &QCheckBox::toggled, &dialog, [rotation](bool checked) {
            rotation->setValue(checked ? 90.0 : 0.0);
        });
        connect(rotation, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dialog, [rotate_90](double angle) {
            const double normalized = std::fmod(std::fmod(angle, 360.0) + 360.0, 360.0);
            const QSignalBlocker blocker(rotate_90);
            rotate_90->setChecked(std::abs(normalized - 90.0) < 0.001);
        });
        connect(fit_to_surface, &QCheckBox::toggled, &dialog, [apply_preview](bool) { apply_preview(); });

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
    auto* rotate_90 = new QCheckBox("Rotate texture 90°", &dialog);
    rotate_90->setChecked(std::abs(
        std::fmod(std::fmod(static_cast<double>(current.rotation_degrees), 360.0) + 360.0, 360.0)
        - 90.0) < 0.001);
    auto* fit_to_surface = new QCheckBox("Fit one image to the whole surface", &dialog);
    fit_to_surface->setChecked(current.fit_to_surface);
    rotation->setSuffix(QString::fromUtf8("°"));

    form->addRow(new DragSpinBoxLabel("Offset U", offset_u, &dialog), offset_u);
    form->addRow(new DragSpinBoxLabel("Offset V", offset_v, &dialog), offset_v);
    form->addRow(new DragSpinBoxLabel("Scale U", scale_u, &dialog), scale_u);
    form->addRow(new DragSpinBoxLabel("Scale V", scale_v, &dialog), scale_v);
    form->addRow(new DragSpinBoxLabel("Rotate", rotation, &dialog), rotation);
    form->addRow(rotate_90);
    form->addRow(fit_to_surface);

    const auto apply_preview = [this, solid, selected_face_indices, offset_u, offset_v, scale_u, scale_v, rotation, fit_to_surface]() {
        SurfaceTextureTransform transform;
        transform.offset_u = static_cast<float>(offset_u->value());
        transform.offset_v = static_cast<float>(offset_v->value());
        transform.scale_u = static_cast<float>(scale_u->value());
        transform.scale_v = static_cast<float>(scale_v->value());
        transform.rotation_degrees = static_cast<float>(rotation->value());
        transform.fit_to_surface = fit_to_surface->isChecked();
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
    connect(rotate_90, &QCheckBox::toggled, &dialog, [rotation](bool checked) {
        rotation->setValue(checked ? 90.0 : 0.0);
    });
    connect(rotation, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dialog, [rotate_90](double angle) {
        const double normalized = std::fmod(std::fmod(angle, 360.0) + 360.0, 360.0);
        const QSignalBlocker blocker(rotate_90);
        rotate_90->setChecked(std::abs(normalized - 90.0) < 0.001);
    });
    connect(fit_to_surface, &QCheckBox::toggled, &dialog, [apply_preview](bool) { apply_preview(); });

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
        if (auto* solid = dynamic_cast<CSolid*>(object.get())) {
            for (int surface_index = 0; surface_index < solid->GetNumSurfaces(); ++surface_index) {
                CSurfaceFace* surface = solid->GetSurfaceFace(surface_index);
                if (surface && surface->MaterialOverride.enabled
                    && surface->MaterialOverride.material_id == saved.id) {
                    solid->SetSurfaceMaterial(surface_index, saved);
                }
                if (surface && surface->MaterialOverride.coating_enabled
                    && surface->MaterialOverride.coating_material_id == saved.id) {
                    solid->SetSurfaceCoating(surface_index, saved);
                }
            }
        }
    }
    RecordDocumentChange("Save material");
    viewport_->update();
    statusBar()->showMessage(QString("Material saved: %1").arg(QString::fromStdString(saved.name)), 1400);
}

void MainWindow::ApplyMaterialToSelection(const Material& material) {
    if (CSolid* solid = document_.GetSelectedFaceSolid();
        solid && !solid->GetSelectedFaceIndices().empty()) {
        const Material& document_material = document_.UpsertMaterial(material);
        struct FaceMaterialChange {
            int surface_index = -1;
            SurfaceMaterialOverride before;
            SurfaceMaterialOverride after;
        };
        const unsigned long solid_id = solid->m_id;
        std::vector<FaceMaterialChange> changes;
        changes.reserve(solid->GetSelectedFaceIndices().size());
        for (int surface_index : solid->GetSelectedFaceIndices()) {
            const CSurfaceFace* surface = solid->GetSurfaceFace(surface_index);
            if (surface) {
                changes.push_back({surface_index, surface->MaterialOverride, {}});
            }
        }
        solid->SetSelectedSurfaceMaterial(document_material);
        for (FaceMaterialChange& change : changes) {
            const CSurfaceFace* surface = solid->GetSurfaceFace(change.surface_index);
            if (surface) change.after = surface->MaterialOverride;
        }
        const auto apply_face_materials = [solid_id](
            CAlfaDoc& document,
            const std::vector<FaceMaterialChange>& values,
            bool use_after) {
            auto* target = dynamic_cast<CSolid*>(
                document.FindObjectById(solid_id));
            if (!target) return false;
            bool changed = false;
            for (const FaceMaterialChange& value : values) {
                CSurfaceFace* surface = target->GetSurfaceFace(
                    value.surface_index);
                if (!surface) continue;
                surface->MaterialOverride = use_after
                    ? value.after : value.before;
                changed = true;
            }
            return changed;
        };
        if (!changes.empty()) {
            undo_redo_.RecordCommand(
                "Apply material",
                [changes, apply_face_materials](CAlfaDoc& document) {
                    return apply_face_materials(document, changes, false);
                },
                [changes, apply_face_materials](CAlfaDoc& document) {
                    return apply_face_materials(document, changes, true);
                });
            UpdateUndoRedoActions();
        }
        viewport_->update();
        statusBar()->showMessage(QString("Material applied to %1 surface(s)")
            .arg(changes.size()), 1400);
        return;
    }
    const std::vector<size_t> selected_indices = document_.GetSelectedObjectIndices();
    if (selected_indices.empty()) {
        statusBar()->showMessage("Material: no selected object", 1400);
        return;
    }

    const Material& document_material = document_.UpsertMaterial(material);
    auto& objects = document_.GetObjects();
    struct ObjectMaterialChange {
        unsigned long object_id = 0;
        Material before;
        unsigned long before_id = 0;
        Material after;
        unsigned long after_id = 0;
    };
    std::vector<ObjectMaterialChange> changes;
    changes.reserve(selected_indices.size());
    for (size_t index : selected_indices) {
        if (index < objects.size() && objects[index]) {
            ObjectMaterialChange change;
            change.object_id = objects[index]->m_id;
            change.before = objects[index]->GetMaterial();
            change.before_id = objects[index]->GetMaterialId();
            objects[index]->SetMaterial(document_material);
            objects[index]->SetMaterialId(document_material.id);
            change.after = objects[index]->GetMaterial();
            change.after_id = objects[index]->GetMaterialId();
            changes.push_back(std::move(change));
        }
    }

    const auto apply_object_materials = [](
        CAlfaDoc& document,
        const std::vector<ObjectMaterialChange>& values,
        bool use_after) {
        bool changed = false;
        for (const ObjectMaterialChange& value : values) {
            CAlfaObject* object = document.FindObjectById(value.object_id);
            if (!object) continue;
            object->SetMaterial(use_after ? value.after : value.before);
            object->SetMaterialId(use_after
                ? value.after_id : value.before_id);
            changed = true;
        }
        return changed;
    };
    if (!changes.empty()) {
        undo_redo_.RecordCommand(
            "Apply material",
            [changes, apply_object_materials](CAlfaDoc& document) {
                return apply_object_materials(document, changes, false);
            },
            [changes, apply_object_materials](CAlfaDoc& document) {
                return apply_object_materials(document, changes, true);
            });
        UpdateUndoRedoActions();
    }
    viewport_->update();
    statusBar()->showMessage(QString("Material applied to %1 object(s)")
        .arg(changes.size()), 1400);
}

void MainWindow::ShowSurfaceFilmDialog(size_t object_index, int operation_index) {
    auto& objects = document_.GetObjects();
    CSolid* solid = nullptr;
    std::vector<int> surface_indices;

    if (object_index != static_cast<size_t>(-1)) {
        if (object_index < objects.size()) {
            solid = dynamic_cast<CSolid*>(objects[object_index].get());
        }
        const ParametricFunction* operation =
            solid && operation_index >= 0 ? solid->GetOperation(operation_index) : nullptr;
        if (!operation || operation->ToolId != "SurfaceFilmCoating") {
            statusBar()->showMessage("Film: saved operation is not available", 1600);
            return;
        }
        surface_indices = operation->CreatedSurfaceIndices;
    } else {
        solid = document_.GetSelectedFaceSolid();
        if (solid) {
            surface_indices = solid->GetSelectedFaceIndices();
            for (size_t index = 0; index < objects.size(); ++index) {
                if (objects[index].get() == solid) {
                    object_index = index;
                    break;
                }
            }
        }
    }

    surface_indices.erase(
        std::remove_if(surface_indices.begin(), surface_indices.end(),
                       [solid](int index) {
                           return !solid || index < 0 || index >= solid->GetNumSurfaces();
                       }),
        surface_indices.end());
    std::sort(surface_indices.begin(), surface_indices.end());
    surface_indices.erase(
        std::unique(surface_indices.begin(), surface_indices.end()),
        surface_indices.end());
    if (!solid || surface_indices.empty()
        || object_index == static_cast<size_t>(-1)) {
        statusBar()->showMessage("Apply Film: select one or more solid faces", 1800);
        return;
    }

    // Reusing Apply Film on the same face set edits the existing operation
    // instead of stacking a second coating and another history entry.
    if (operation_index < 0) {
        for (int index = solid->GetNumOperations() - 1; index >= 0; --index) {
            const ParametricFunction* operation = solid->GetOperation(index);
            if (!operation || operation->ToolId != "SurfaceFilmCoating") {
                continue;
            }
            std::vector<int> operation_surfaces = operation->CreatedSurfaceIndices;
            std::sort(operation_surfaces.begin(), operation_surfaces.end());
            operation_surfaces.erase(
                std::unique(operation_surfaces.begin(), operation_surfaces.end()),
                operation_surfaces.end());
            if (operation_surfaces == surface_indices) {
                operation_index = index;
                break;
            }
        }
    }

    MaterialLibrary library;
    library.Load(MaterialLibrary::DefaultLibraryPath());
    Material matte = Material::DefaultSurface();
    Material translucent = Material::DefaultGloss();
    matte.name = "Oracal Film Matte";
    matte.alpha = 1.0f;
    matte.roughness = 0.68f;
    matte.metallic = 0.0f;
    matte.coat_weight = 0.0f;
    translucent.name = "Oracal Film Translucent Glossy";
    translucent.alpha = 0.58f;
    translucent.specular = 0.85f;
    translucent.shininess = 180.0f;
    translucent.reflectivity = 0.18f;
    translucent.roughness = 0.08f;
    translucent.metallic = 0.0f;
    translucent.coat_weight = 1.0f;
    translucent.coat_roughness = 0.02f;

    struct RalChoice {
        int code = 0;
        Color color{};
    };
    std::vector<RalChoice> ral_choices;
    const QRegularExpression ral_expression(QStringLiteral("RAL\\s+(\\d{4})"));
    for (const MaterialLibrary::Entry& entry : library.Entries()) {
        const QString material_name = QString::fromStdString(entry.material.name);
        if (material_name.contains("Oracal Film Matte", Qt::CaseInsensitive)) {
            matte = entry.material;
        } else if (material_name.contains("Oracal Film Translucent", Qt::CaseInsensitive)) {
            translucent = entry.material;
        }
        if (!entry.category.contains("Powder Coating", Qt::CaseInsensitive)) {
            continue;
        }
        const QRegularExpressionMatch match = ral_expression.match(material_name);
        if (!match.hasMatch()) {
            continue;
        }
        const int code = match.captured(1).toInt();
        if (std::none_of(ral_choices.begin(), ral_choices.end(),
                         [code](const RalChoice& choice) { return choice.code == code; })) {
            ral_choices.push_back({code, entry.material.diffuse});
        }
    }
    if (ral_choices.empty()) {
        ral_choices.push_back({3020, {0.8f, 0.0235f, 0.0196f}});
    }
    std::sort(ral_choices.begin(), ral_choices.end(),
              [](const RalChoice& left, const RalChoice& right) {
                  return left.code < right.code;
              });

    int initial_ral = 3020;
    int initial_type = 0;
    unsigned long edited_material_id = 0;
    if (operation_index >= 0) {
        if (const ParametricFunction* operation = solid->GetOperation(operation_index)) {
            for (const ParametricParameterValue& parameter : operation->Parameters) {
                if (parameter.id == "ral") {
                    initial_ral = static_cast<int>(std::lround(parameter.value));
                } else if (parameter.id == "film.type") {
                    initial_type = parameter.value >= 0.5 ? 1 : 0;
                } else if (parameter.id == "material.id") {
                    edited_material_id = static_cast<unsigned long>(
                        std::max(0.0, parameter.value));
                }
            }
        }
    }

    std::vector<SurfaceMaterialOverride> original_overrides;
    original_overrides.reserve(surface_indices.size());
    for (int index : surface_indices) {
        original_overrides.push_back(solid->GetSurfaceFace(index)->MaterialOverride);
    }

    double area_mm2 = 0.0;
    for (int index : surface_indices) {
        try {
            const TopoDS_Face face = solid->GetTopoFace(index);
            if (!face.IsNull()) {
                GProp_GProps properties;
                BRepGProp::SurfaceProperties(face, properties);
                area_mm2 += properties.Mass();
            }
        } catch (...) {
            // A coating is still valid when an imported face cannot report area.
        }
    }

    QDialog dialog(this);
    dialog.setWindowTitle(operation_index >= 0 ? "Edit Oracal Film" : "Apply Oracal Film");
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();
    auto* ral_combo = new QComboBox(&dialog);
    for (const RalChoice& choice : ral_choices) {
        const QColor swatch = QColor::fromRgbF(choice.color.r, choice.color.g, choice.color.b);
        QPixmap icon(18, 18);
        icon.fill(swatch);
        ral_combo->addItem(QIcon(icon), QString("RAL %1").arg(choice.code), choice.code);
    }
    int ral_index = ral_combo->findData(initial_ral);
    ral_combo->setCurrentIndex(ral_index >= 0 ? ral_index : 0);
    auto* type_combo = new QComboBox(&dialog);
    type_combo->addItem("Matte", 0);
    type_combo->addItem("Translucent glossy", 1);
    type_combo->setCurrentIndex(std::clamp(initial_type, 0, 1));
    auto* area_label = new QLabel(
        area_mm2 > 0.0
            ? QString("%1 mm²").arg(area_mm2, 0, 'f', 1)
            : QString("Not available"),
        &dialog);
    form->addRow("RAL color", ral_combo);
    form->addRow("Film type", type_combo);
    form->addRow("Covered area", area_label);
    layout->addLayout(form);
    auto* note = new QLabel(
        "The film is applied as a thin surface layer over the existing material.",
        &dialog);
    note->setWordWrap(true);
    layout->addWidget(note);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    const auto selected_color = [&ral_choices, ral_combo]() {
        const int code = ral_combo->currentData().toInt();
        const auto found = std::find_if(
            ral_choices.begin(), ral_choices.end(),
            [code](const RalChoice& choice) { return choice.code == code; });
        return found != ral_choices.end() ? found->color : ral_choices.front().color;
    };
    const auto make_film = [&]() {
        const bool is_translucent = type_combo->currentData().toInt() == 1;
        Material film = is_translucent ? translucent : matte;
        const Color color = selected_color();
        film.id = edited_material_id;
        film.diffuse = color;
        film.ambient = {color.r * 0.22f, color.g * 0.22f, color.b * 0.22f};
        film.emission = {0.0f, 0.0f, 0.0f};
        film.color_texture_path.clear();
        film.light_texture_path.clear();
        film.bump_texture_path.clear();
        film.normal_texture_path.clear();
        film.roughness_texture_path.clear();
        film.metallic_texture_path.clear();
        film.displacement_texture_path.clear();
        const int ral = ral_combo->currentData().toInt();
        film.name = QString("Oracal Film %1 RAL %2")
            .arg(is_translucent ? "Translucent Glossy" : "Matte")
            .arg(ral).toStdString();
        return film;
    };
    const auto preview = [&, solid]() {
        const Material film = make_film();
        for (int index : surface_indices) {
            solid->SetSurfaceCoating(index, film);
        }
        viewport_->update();
    };

    if (!undo_redo_.BeginChange()) {
        statusBar()->showMessage("Apply Film: another edit is active", 1600);
        return;
    }
    connect(ral_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            &dialog, [preview](int) { preview(); });
    connect(type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            &dialog, [preview](int) { preview(); });
    preview();

    if (dialog.exec() != QDialog::Accepted) {
        for (size_t i = 0; i < surface_indices.size(); ++i) {
            if (CSurfaceFace* surface = solid->GetSurfaceFace(surface_indices[i])) {
                surface->MaterialOverride = original_overrides[i];
            }
        }
        undo_redo_.CancelChange();
        viewport_->update();
        return;
    }

    Material film = make_film();
    Material& saved_film = document_.UpsertMaterial(std::move(film));
    for (int index : surface_indices) {
        solid->SetSurfaceCoating(index, saved_film);
    }
    const int ral = ral_combo->currentData().toInt();
    const int film_type = type_combo->currentData().toInt();
    const size_t saved_operation_index = operation_index >= 0
        ? static_cast<size_t>(operation_index)
        : solid->GetOperationTree().size();
    solid->SetParametricOperation(
        saved_operation_index,
        "SurfaceFilmCoating",
        QString("Oracal Film — RAL %1").arg(ral).toStdString(),
        {{"material.id", static_cast<double>(saved_film.id)},
         {"ral", static_cast<double>(ral)},
         {"film.type", static_cast<double>(film_type)},
         {"area.mm2", area_mm2}},
        surface_indices);
    undo_redo_.CommitChange(operation_index >= 0 ? "Edit Oracal film" : "Apply Oracal film");
    UpdateUndoRedoActions();
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage(
        QString("Oracal film applied to %1 surface(s), area %2 mm²")
            .arg(surface_indices.size()).arg(area_mm2, 0, 'f', 1),
        2200);
}

void MainWindow::BeginTransformTool(TransformOperation operation) {
    ClearActiveProperties();
    viewport_->SetTransformOperation(operation);
    viewport_->setFocus();

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

void MainWindow::ShowViewportPopupMenu(const QPoint& global_position) {
    QMenu menu(this);

    QAction* stop = menu.addAction("Stop");
    stop->setEnabled(viewport_->CurrentTool() != ToolMode::Select
        || !active_parametric_object_.tool_id.empty());
    connect(stop, &QAction::triggered, this, [this]() {
        if (!active_parametric_object_.tool_id.empty()) {
            CancelActiveProperties();
        } else {
            SetTool(ToolMode::Select, "Select objects");
        }
    });

    QAction* rotation_pivot = menu.addAction("Pivot of Rotation");
    rotation_pivot->setCheckable(true);
    rotation_pivot->setChecked(viewport_->HasRotationPivot());
    connect(rotation_pivot, &QAction::triggered, this, [this](bool enabled) {
        if (!enabled) {
            pending_transform_point_pick_ = PendingTransformPointPick::None;
            viewport_->ClearRotationPivot();
            statusBar()->showMessage("Pivot of Rotation disabled", 1400);
            return;
        }
        pending_transform_point_pick_ = PendingTransformPointPick::RotationPivot;
        viewport_->BeginPick3DPoint(
            "Pivot of Rotation: pick a 3D point in the scene");
    });

    menu.addAction("Update Scene", this, [this]() {
        viewport_->RefreshSurfaceMeshQuality();
    });

    menu.addSeparator();
    menu.addAction("All Scene", this, [this]() {
        viewport_->FitToDocument();
        statusBar()->showMessage("All scene fitted", 1400);
    });
    menu.addAction("Zoom", this, [this]() {
        SetTool(ToolMode::ZoomRect, "Zoom By Rect: drag the area to enlarge");
    });
    menu.addAction("View to Clipboard", this, [this]() {
        QGuiApplication::clipboard()->setImage(viewport_->grabFramebuffer());
        statusBar()->showMessage("View copied to clipboard", 1400);
    });

    QMenu* polygon_view = menu.addMenu("Polygon View");
    auto* display_group = new QActionGroup(polygon_view);
    display_group->setExclusive(true);
    const auto add_display_mode = [this, polygon_view, display_group](
                                      const QString& label, SolidDisplayMode mode) {
        QAction* action = polygon_view->addAction(label);
        action->setCheckable(true);
        action->setChecked(CSolid::GetDisplayMode() == mode);
        display_group->addAction(action);
        connect(action, &QAction::triggered, this, [this, mode]() {
            SetSolidDisplayMode(mode);
        });
    };
    add_display_mode("Surfaces and Edges", SolidDisplayMode::SurfacesAndEdges);
    add_display_mode("Wireframe", SolidDisplayMode::Wireframe);
    add_display_mode("Mesh Only", SolidDisplayMode::MeshOnly);
    add_display_mode("Hidden Lines", SolidDisplayMode::HiddenLine);

    QAction* worktop = menu.addAction("Worktop / XY Plane");
    worktop->setCheckable(true);
    worktop->setChecked(viewport_->IsXYPlaneViewEnabled());
    connect(worktop, &QAction::toggled, this, [this](bool enabled) {
        SetXYPlaneViewEnabled(enabled);
    });

    menu.addSeparator();
    const bool selected_text =
        dynamic_cast<CDrawingText*>(document_.GetSelectedObject()) != nullptr;
    QAction* edit = menu.addAction(selected_text ? "Edit Text..." : "Edit");
    edit->setEnabled(document_.HasSelection());
    connect(edit, &QAction::triggered, this, [this]() {
        if (auto* text = dynamic_cast<CDrawingText*>(document_.GetSelectedObject())) {
            EditDrawingText(*text);
            return;
        }
        if (document_.GetSelectedSketch()) {
            int sketch_tab_index = -1;
            if (tool_tabs_) {
                for (int index = 0; index < tool_tabs_->count(); ++index) {
                    if (tool_tabs_->tabData(index).toString() == "Sketch") {
                        sketch_tab_index = index;
                        break;
                    }
                }
            }

            if (tool_tabs_ && sketch_tab_index >= 0
                && tool_tabs_->currentIndex() != sketch_tab_index) {
                // The currentChanged handler runs the same scenario as a
                // manual click on the Sketch tab.
                tool_tabs_->setCurrentIndex(sketch_tab_index);
                return;
            }

            if (viewport_->BeginEditSelectedSketch()) {
                ShowSketchPanel();
                UpdateActiveToolUi("NewSketch");
                viewport_->update();
                statusBar()->showMessage("Sketch edit panel opened", 1200);
            }
            return;
        }
        EditSelectedParametricObject();
    });

    QAction* edit_texture = menu.addAction(EditTextureIcon(), "Edit Texture");
    edit_texture->setEnabled(
        document_.HasSelectedSolidFace() || document_.GetSelectedMesh() != nullptr);
    connect(edit_texture, &QAction::triggered, this, [this]() {
        ShowSurfaceTextureEditor();
    });

    menu.addSeparator();
    QAction* add_to_catalog = menu.addAction("Add to Catalog...");
    add_to_catalog->setEnabled(document_.HasSelection());
    connect(add_to_catalog, &QAction::triggered,
            this, &MainWindow::AddSelectionToCatalog);

    menu.exec(global_position);
}

void MainWindow::ShowPreciseMoveDialog() {
    if (!document_.HasSelection()) {
        pending_precise_transform_ = PendingPreciseTransform::Move;
        viewport_->SetSelectionMode(SelectionMode::Object);
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi("move_dialog");
        statusBar()->showMessage("Move: pick an object in the scene", 0);
        return;
    }

    if (precise_move_dialog_) {
        precise_move_dialog_->show();
        precise_move_dialog_->raise();
        precise_move_dialog_->activateWindow();
        viewport_->BeginMovePointToPoint(true);
        viewport_->setFocus();
        return;
    }

    ClearActiveProperties();
    UpdateActiveToolUi("move_dialog");

    auto* command_dialog = new QDialog(this, Qt::Tool);
    precise_move_dialog_ = command_dialog;
    command_dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    command_dialog->setWindowTitle("Direction and Distance");
    command_dialog->setModal(false);
    command_dialog->setFixedWidth(240);
    auto* command_root = new QVBoxLayout(command_dialog);
    command_root->setContentsMargins(6, 5, 6, 6);
    command_root->setSpacing(4);

    QDoubleSpinBox* lengths[3]{};
    const char* names[] = {
        "Length Along an axis X",
        "Length Along an axis Y",
        "Length Along an axis Z"};
    const char* positive_names[] = {"+X", "+Y", "+Z"};
    const char* negative_names[] = {"−X", "−Y", "−Z"};
    const Vec3 directions[] = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}
    };

    const auto apply_axis_move = [this](Vec3 axis, double distance) {
        const Vec3 delta = axis * static_cast<float>(distance);
        if (viewport_->ApplyPreciseMove(delta)) {
            RefreshSceneTree();
            statusBar()->showMessage(
                QString("Moved: ΔX %1, ΔY %2, ΔZ %3 mm")
                    .arg(delta.x, 0, 'f', 3)
                    .arg(delta.y, 0, 'f', 3)
                    .arg(delta.z, 0, 'f', 3),
                1400);
        }
        viewport_->BeginMovePointToPoint(true);
    };

    for (int i = 0; i < 3; ++i) {
        auto* group = new QGroupBox(names[i], command_dialog);
        auto* row = new QHBoxLayout(group);
        row->setContentsMargins(6, 4, 6, 4);
        row->setSpacing(4);
        lengths[i] = new QDoubleSpinBox(group);
        lengths[i]->setRange(0.0, 1000000.0);
        lengths[i]->setDecimals(3);
        lengths[i]->setSingleStep(1.0);
        lengths[i]->setValue(100.0);
        lengths[i]->setKeyboardTracking(false);
        lengths[i]->setFixedWidth(116);
        row->addWidget(lengths[i]);
        auto* positive_button = new QPushButton(positive_names[i], group);
        auto* negative_button = new QPushButton(negative_names[i], group);
        positive_button->setFixedWidth(36);
        negative_button->setFixedWidth(36);
        row->addWidget(positive_button);
        row->addWidget(negative_button);
        connect(positive_button, &QPushButton::clicked, command_dialog,
                [apply_axis_move, directions, lengths, i]() {
            apply_axis_move(directions[i], lengths[i]->value());
        });
        connect(negative_button, &QPushButton::clicked, command_dialog,
                [apply_axis_move, directions, lengths, i]() {
            apply_axis_move(directions[i], -lengths[i]->value());
        });
        command_root->addWidget(group);
    }

    auto* two_points = new QPushButton("To enter 2 Points", command_dialog);
    two_points->setFixedHeight(22);
    command_root->addWidget(two_points);
    connect(two_points, &QPushButton::clicked, command_dialog, [this]() {
        BeginMoveTwoPointEntry();
    });

    auto* button_row = new QHBoxLayout();
    button_row->setContentsMargins(0, 2, 0, 0);
    button_row->setSpacing(10);
    auto* ok_button = new QPushButton("OK", command_dialog);
    auto* cancel_button = new QPushButton("Cancel", command_dialog);
    ok_button->setFixedSize(78, 24);
    cancel_button->setFixedSize(78, 24);
    ok_button->setDefault(true);
    ok_button->setAutoDefault(true);
    button_row->addStretch(1);
    button_row->addWidget(ok_button);
    button_row->addWidget(cancel_button);
    button_row->addStretch(1);
    command_root->addLayout(button_row);
    connect(ok_button, &QPushButton::clicked,
            command_dialog, &QDialog::close);
    connect(cancel_button, &QPushButton::clicked,
            command_dialog, &QDialog::close);

    const auto bind_enter_to_ok = [command_dialog, ok_button](int key) {
        auto* shortcut = new QShortcut(QKeySequence(key), command_dialog);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        QObject::connect(shortcut, &QShortcut::activated,
                         command_dialog, [ok_button]() {
            ok_button->click();
        });
    };
    bind_enter_to_ok(Qt::Key_Return);
    bind_enter_to_ok(Qt::Key_Enter);
    connect(command_dialog, &QObject::destroyed, this, [this]() {
        precise_move_dialog_ = nullptr;
        if (viewport_->CurrentTool() == ToolMode::MovePointToPoint) {
            viewport_->SetTool(ToolMode::Select);
        }
        UpdateActiveToolUi("select");
        statusBar()->showMessage("Move command completed", 1200);
    });

    PlaceDialogAtWorkspaceTopLeft(*command_dialog, *viewport_);
    command_dialog->show();
    command_dialog->raise();
    command_dialog->activateWindow();
    viewport_->BeginMovePointToPoint(true);
    viewport_->setFocus();
    statusBar()->showMessage("Move: pick two points in the scene or use an axis button", 0);
    return;

#if 0
    ClearActiveProperties();
    UpdateActiveToolUi("move_dialog");

    QDialog dlg(this);
    dlg.setWindowTitle("Move — Direction and Distance");
    dlg.setModal(true);
    auto* root = new QVBoxLayout(&dlg);
    root->setContentsMargins(12, 10, 12, 12);

    auto make_length = [&dlg](double value = 0.0) {
        auto* spin = new QDoubleSpinBox(&dlg);
        spin->setRange(-1000000.0, 1000000.0);
        spin->setDecimals(3);
        spin->setSingleStep(1.0);
        spin->setValue(value);
        spin->setKeyboardTracking(false);
        return spin;
    };

    auto* mode = new QComboBox(&dlg);
    mode->addItem("Axis and distance");
    mode->addItem("From point to point");
    root->addWidget(mode);

    auto* pages = new QStackedWidget(&dlg);
    root->addWidget(pages);

    auto* axis_page = new QWidget(pages);
    auto* axis_layout = new QGridLayout(axis_page);
    auto* axis = new QComboBox(axis_page);
    axis->addItems({"X", "Y", "Z"});
    auto* distance = make_length(100.0);
    distance->setParent(axis_page);
    auto* positive = new QPushButton("+", axis_page);
    auto* negative = new QPushButton("−", axis_page);
    axis_layout->addWidget(new QLabel("Axis", axis_page), 0, 0);
    axis_layout->addWidget(axis, 0, 1, 1, 2);
    axis_layout->addWidget(new QLabel("Distance", axis_page), 1, 0);
    axis_layout->addWidget(distance, 1, 1);
    axis_layout->addWidget(positive, 1, 2);
    axis_layout->addWidget(negative, 1, 3);
    pages->addWidget(axis_page);

    auto* points_page = new QWidget(pages);
    auto* points_layout = new QVBoxLayout(points_page);
    auto* point_hint = new QLabel(
        "After pressing OK, pick the source point and then the target point "
        "directly in the 3D scene.", points_page);
    point_hint->setWordWrap(true);
    points_layout->addWidget(point_hint);
    auto* point_mode_label = new QLabel("3D snaps and visible object vertices are used.", points_page);
    point_mode_label->setStyleSheet("QLabel { color: #707070; }");
    point_mode_label->setWordWrap(true);
    points_layout->addWidget(point_mode_label);
    points_layout->addStretch(1);
    pages->addWidget(points_page);

    connect(mode, qOverload<int>(&QComboBox::currentIndexChanged), pages, &QStackedWidget::setCurrentIndex);
    connect(positive, &QPushButton::clicked, &dlg, [distance]() {
        distance->setValue(std::fabs(distance->value()));
    });
    connect(negative, &QPushButton::clicked, &dlg, [distance]() {
        distance->setValue(-std::fabs(distance->value()));
    });

    Vec3 guide_center{};
    document_.GetTransformGizmoCenter(guide_center);
    const auto update_guide = [this, axis, guide_center]() {
        const TransformAxis selected_axis = axis->currentIndex() == 0
            ? TransformAxis::X
            : axis->currentIndex() == 1 ? TransformAxis::Y : TransformAxis::Z;
        viewport_->SetTransformDialogGuide(TransformOperation::Move, selected_axis, guide_center);
    };
    connect(axis, qOverload<int>(&QComboBox::currentIndexChanged), &dlg, [update_guide](int) { update_guide(); });
    update_guide();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    const bool accepted = dlg.exec() == QDialog::Accepted;
    viewport_->ClearTransformDialogGuide();
    if (!accepted) {
        return;
    }

    if (mode->currentIndex() == 1) {
        viewport_->BeginMovePointToPoint();
        viewport_->setFocus();
        UpdateActiveToolUi("MovePointToPoint");
        statusBar()->showMessage("Move: pick the source point in the 3D scene", 0);
        return;
    }

    Vec3 delta{};
    const float value = static_cast<float>(distance->value());
    if (axis->currentIndex() == 0) delta.x = value;
    else if (axis->currentIndex() == 1) delta.y = value;
    else delta.z = value;

    if (viewport_->ApplyPreciseMove(delta)) {
        RefreshSceneTree();
        statusBar()->showMessage(
            QString("Moved: ΔX %1 mm, ΔY %2 mm, ΔZ %3 mm")
                .arg(delta.x, 0, 'f', 3).arg(delta.y, 0, 'f', 3).arg(delta.z, 0, 'f', 3),
            2200);
    }
#endif
}

void MainWindow::BeginMoveTwoPointEntry() {
    if (!precise_move_dialog_ || !document_.HasSelection()) {
        return;
    }

    struct PointEntryState {
        Vec3 source{};
        int stage = 0;
        std::shared_ptr<std::function<void()>> show_next;
    };
    auto state = std::make_shared<PointEntryState>();
    state->show_next = std::make_shared<std::function<void()>>();

    *state->show_next = [this, state]() {
        if (!precise_move_dialog_) {
            *state->show_next = {};
            return;
        }

        const bool plane_xy = viewport_->IsXYPlaneViewEnabled();
        auto* dialog = new QDialog(precise_move_dialog_, Qt::Tool);
        dialog->setAttribute(Qt::WA_DeleteOnClose, true);
        dialog->setWindowTitle(plane_xy ? "2D Point modeling — On Plane XY"
                                        : "3D Point modeling");
        dialog->setFixedWidth(245);
        auto* root = new QVBoxLayout(dialog);
        root->setContentsMargins(12, 10, 12, 12);
        root->setSpacing(8);
        auto* point_title = new QLabel(
            state->stage == 0 ? "Source point" : "Target point", dialog);
        point_title->setStyleSheet("QLabel { font-weight: 600; }");
        root->addWidget(point_title);

        auto* coordinates = new QGridLayout();
        coordinates->setHorizontalSpacing(0);
        coordinates->setVerticalSpacing(3);
        const char* names[] = {"X", "Y", "Z"};
        const int count = plane_xy ? 2 : 3;
        for (int i = 0; i < count; ++i) {
            auto* label = new QLabel(names[i], dialog);
            label->setAlignment(Qt::AlignCenter);
            coordinates->addWidget(label, 0, i);
        }
        auto* coordinate_entry = new QLineEdit(dialog);
        coordinate_entry->setAlignment(Qt::AlignCenter);
        const QLocale number_locale = NumberInputLocale();
        const QString zero = number_locale.toString(0.0, 'f', 4);
        coordinate_entry->setText(
            plane_xy ? QString("%1   %1").arg(zero)
                     : QString("%1   %1   %1").arg(zero));
        coordinate_entry->setToolTip(
            plane_xy
                ? "Enter X and Y separated by spaces or semicolons"
                : "Enter X, Y, and Z separated by spaces or semicolons");
        coordinate_entry->setStyleSheet(
            "QLineEdit { font-family: Consolas, 'Courier New', monospace;"
            " padding: 3px 5px; }");
        coordinates->addWidget(coordinate_entry, 1, 0, 1, count);
        root->addLayout(coordinates);

        auto* error_label = new QLabel(dialog);
        error_label->setStyleSheet("QLabel { color: #c62828; }");
        error_label->setWordWrap(true);
        error_label->hide();
        root->addWidget(error_label);

        auto* ok_button = new QPushButton("OK", dialog);
        ok_button->setFixedWidth(82);
        root->addWidget(ok_button, 0, Qt::AlignHCenter);

        auto* mode = new QGroupBox("Mode", dialog);
        auto* mode_layout = new QVBoxLayout(mode);
        mode_layout->setContentsMargins(10, 7, 10, 8);
        auto* smart = new QRadioButton(
            plane_xy ? "Smart — On Plane XY" : "Smart 3D snapping", mode);
        smart->setChecked(true);
        mode_layout->addWidget(smart);
        root->addWidget(mode);

        auto* cancel_button = new QPushButton("Cancel", dialog);
        cancel_button->setFixedWidth(82);
        root->addWidget(cancel_button, 0, Qt::AlignHCenter);

        auto dialog_handled = std::make_shared<bool>(false);
        const auto complete_point = [this, dialog, state, dialog_handled](Vec3 point) {
            *dialog_handled = true;
            dialog->close();
            if (state->stage == 0) {
                state->source = point;
                state->stage = 1;
                QTimer::singleShot(0, this, [state]() {
                    if (*state->show_next) {
                        (*state->show_next)();
                    }
                });
                return;
            }

            const Vec3 delta = point - state->source;
            if (viewport_->ApplyPreciseMove(delta)) {
                RefreshSceneTree();
                statusBar()->showMessage(
                    QString("Moved by two points: ΔX %1, ΔY %2, ΔZ %3 mm")
                        .arg(delta.x, 0, 'f', 3)
                        .arg(delta.y, 0, 'f', 3)
                        .arg(delta.z, 0, 'f', 3),
                    1600);
            }
            *state->show_next = {};
            viewport_->BeginMovePointToPoint(true);
        };
        connect(viewport_, &OpenGLViewport::Point3DPicked, dialog,
                [complete_point, plane_xy](CPoint3d point) {
            complete_point({
                static_cast<float>(point.x),
                static_cast<float>(point.y),
                plane_xy ? 0.0f : static_cast<float>(point.z)});
        });
        connect(viewport_, &OpenGLViewport::Point3DPickCanceled,
                dialog, [dialog, state, dialog_handled]() {
            *dialog_handled = true;
            *state->show_next = {};
            dialog->close();
        });
        connect(cancel_button, &QPushButton::clicked, dialog,
                [this, dialog, state, dialog_handled]() {
            *dialog_handled = true;
            *state->show_next = {};
            dialog->close();
            viewport_->BeginMovePointToPoint(true);
        });
        connect(dialog, &QObject::destroyed, this,
                [this, state, dialog_handled]() {
            if (*dialog_handled) {
                return;
            }
            *state->show_next = {};
            viewport_->BeginMovePointToPoint(true);
        });
        const auto submit_coordinates = [coordinate_entry, error_label,
                                         plane_xy, complete_point,
                                         number_locale]() {
            QString normalized = coordinate_entry->text().trimmed();
            normalized.replace(';', ' ');
            const QStringList parts = normalized.split(' ', Qt::SkipEmptyParts);
            const int expected_count = plane_xy ? 2 : 3;
            if (parts.size() != expected_count) {
                error_label->setText(
                    plane_xy ? "Enter two coordinates: X Y"
                             : "Enter three coordinates: X Y Z");
                error_label->show();
                coordinate_entry->setFocus();
                coordinate_entry->selectAll();
                return;
            }
            double parsed[3]{};
            for (int i = 0; i < expected_count; ++i) {
                bool valid = false;
                parsed[i] = number_locale.toDouble(parts[i], &valid);
                if (!valid || !std::isfinite(parsed[i])) {
                    error_label->setText("Coordinates must be valid numbers");
                    error_label->show();
                    coordinate_entry->setFocus();
                    coordinate_entry->selectAll();
                    return;
                }
            }
            complete_point({
                static_cast<float>(parsed[0]),
                static_cast<float>(parsed[1]),
                plane_xy ? 0.0f : static_cast<float>(parsed[2])});
        };
        connect(ok_button, &QPushButton::clicked, dialog, submit_coordinates);
        connect(coordinate_entry, &QLineEdit::returnPressed,
                dialog, submit_coordinates);

        PlaceDialogAtWorkspaceTopLeft(*dialog, *viewport_);
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
        viewport_->BeginPick3DPoint(
            state->stage == 0
                ? (plane_xy ? "GetPoint2D: pick the source point on Plane XY"
                            : "GetPoint3D: pick the source point")
                : (plane_xy ? "GetPoint2D: pick the target point on Plane XY"
                            : "GetPoint3D: pick the target point"));
    };

    (*state->show_next)();
}

void MainWindow::ShowPreciseRotateDialog() {
    if (!document_.HasSelection()) {
        pending_precise_transform_ = PendingPreciseTransform::Rotate;
        viewport_->SetSelectionMode(SelectionMode::Object);
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi("rotate_dialog");
        statusBar()->showMessage("Rotate: pick an object in the scene", 0);
        return;
    }

    if (precise_rotate_dialog_) {
        precise_rotate_dialog_->show();
        precise_rotate_dialog_->raise();
        precise_rotate_dialog_->activateWindow();
        viewport_->setFocus();
        return;
    }

    if (!precise_rotate_axis_ready_) {
        viewport_->BeginPickRotationAxis();
        UpdateActiveToolUi("rotate_dialog");
        return;
    }

    ClearActiveProperties();
    UpdateActiveToolUi("rotate_dialog");
    const Vec3 center = precise_rotate_axis_start_;
    const Vec3 direction = normalize(
        precise_rotate_axis_end_ - precise_rotate_axis_start_);

    auto* dialog = new QDialog(this, Qt::Tool);
    precise_rotate_dialog_ = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setWindowTitle("Rotation of Object");
    dialog->setModal(false);
    auto* root = new QVBoxLayout(dialog);
    root->setContentsMargins(12, 10, 12, 12);

    auto* quick = new QHBoxLayout();
    auto* plus_90 = new QPushButton("+90°", dialog);
    auto* minus_90 = new QPushButton("−90°", dialog);
    auto* angle_180 = new QPushButton("180°", dialog);
    quick->addWidget(plus_90);
    quick->addWidget(minus_90);
    quick->addWidget(angle_180);
    root->addLayout(quick);

    auto* axis_label = new QLabel(
        QString("Axis:  (%1, %2, %3)  →  (%4, %5, %6)")
            .arg(precise_rotate_axis_start_.x, 0, 'f', 3)
            .arg(precise_rotate_axis_start_.y, 0, 'f', 3)
            .arg(precise_rotate_axis_start_.z, 0, 'f', 3)
            .arg(precise_rotate_axis_end_.x, 0, 'f', 3)
            .arg(precise_rotate_axis_end_.y, 0, 'f', 3)
            .arg(precise_rotate_axis_end_.z, 0, 'f', 3),
        dialog);
    axis_label->setWordWrap(true);
    root->addWidget(axis_label);

    auto* form = new QFormLayout();
    auto* angle = new QDoubleSpinBox(dialog);
    angle->setRange(-36000.0, 36000.0);
    angle->setDecimals(3);
    angle->setSingleStep(5.0);
    angle->setSuffix(QString::fromUtf8("°"));
    angle->setValue(90.0);
    angle->setKeyboardTracking(true);
    form->addRow("Angle", angle);
    root->addLayout(form);

    const auto update_guide = [this, angle, center, direction]() {
        viewport_->SetTransformDialogGuide(
            TransformOperation::Rotate,
            TransformAxis::None,
            center,
            static_cast<float>(angle->value()),
            direction);
    };
    connect(angle, qOverload<double>(&QDoubleSpinBox::valueChanged), dialog,
            [update_guide](double) { update_guide(); });
    connect(plus_90, &QPushButton::clicked, dialog,
            [angle]() { angle->setValue(90.0); });
    connect(minus_90, &QPushButton::clicked, dialog,
            [angle]() { angle->setValue(-90.0); });
    connect(angle_180, &QPushButton::clicked, dialog,
            [angle]() { angle->setValue(180.0); });
    update_guide();

    auto* hint = new QLabel(
        "The yellow arrow shows the positive rotation direction.", dialog);
    hint->setWordWrap(true);
    root->addWidget(hint);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, dialog,
            [this, dialog, angle, center, direction]() {
        const float degrees = static_cast<float>(angle->value());
        if (viewport_->ApplyPreciseRotate(
                center, direction, degrees * 3.14159265f / 180.0f)) {
            RefreshSceneTree();
            statusBar()->showMessage(
                QString("Rotated %1° around the selected axis")
                    .arg(degrees, 0, 'f', 3), 2200);
        }
        dialog->close();
    });
    connect(buttons, &QDialogButtonBox::rejected,
            dialog, &QDialog::close);
    connect(dialog, &QObject::destroyed, this, [this]() {
        precise_rotate_dialog_ = nullptr;
        precise_rotate_axis_ready_ = false;
        viewport_->ClearTransformDialogGuide();
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi("select");
    });

    CenterDialogOnCursor(*dialog);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    viewport_->setFocus();
}

void MainWindow::ShowPreciseScaleDialog() {
    if (!document_.HasSelection()) {
        pending_precise_transform_ = PendingPreciseTransform::Scale;
        viewport_->SetSelectionMode(SelectionMode::Object);
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi("scale_dialog");
        statusBar()->showMessage("Scale: pick an object in the scene", 0);
        return;
    }

    if (!precise_scale_base_point_ready_) {
        pending_transform_point_pick_ = PendingTransformPointPick::ScaleBasePoint;
        viewport_->BeginPick3DPoint("Scale: pick the base point in the 3D scene");
        UpdateActiveToolUi("scale_dialog");
        return;
    }

    ClearActiveProperties();
    UpdateActiveToolUi("scale_dialog");

    const Vec3 center = precise_scale_base_point_;
    Vec3 bounds_min{};
    Vec3 bounds_max{};
    document_.GetSelectionBounds(bounds_min, bounds_max);
    const Vec3 size = bounds_max - bounds_min;

    QDialog dlg(this);
    dlg.setWindowTitle("Scale Dialog");
    dlg.setModal(true);
    auto* root = new QVBoxLayout(&dlg);
    root->setContentsMargins(12, 10, 12, 12);

    const char* axis_names[] = {"X", "Y", "Z"};
    auto* base_point_label = new QLabel(
        QString("Base point: X %1 mm   Y %2 mm   Z %3 mm")
            .arg(center.x, 0, 'f', 3)
            .arg(center.y, 0, 'f', 3)
            .arg(center.z, 0, 'f', 3),
        &dlg);
    root->addWidget(base_point_label);

    auto* size_label = new QLabel(
        QString("Current size: X %1 mm   Y %2 mm   Z %3 mm")
            .arg(size.x, 0, 'f', 3).arg(size.y, 0, 'f', 3).arg(size.z, 0, 'f', 3),
        &dlg);
    root->addWidget(size_label);

    auto* mode = new QComboBox(&dlg);
    mode->addItem("Uniform coefficient");
    mode->addItem("Independent X / Y / Z coefficients");
    root->addWidget(mode);

    auto* pages = new QStackedWidget(&dlg);
    auto* uniform_page = new QWidget(pages);
    auto* uniform_form = new QFormLayout(uniform_page);
    auto* uniform_factor = new QDoubleSpinBox(uniform_page);
    uniform_factor->setRange(0.001, 1000.0);
    uniform_factor->setDecimals(4);
    uniform_factor->setSingleStep(0.1);
    uniform_factor->setValue(1.0);
    uniform_form->addRow("K", uniform_factor);
    pages->addWidget(uniform_page);

    auto* xyz_page = new QWidget(pages);
    auto* xyz_form = new QFormLayout(xyz_page);
    QDoubleSpinBox* factors[3]{};
    for (int i = 0; i < 3; ++i) {
        factors[i] = new QDoubleSpinBox(xyz_page);
        factors[i]->setRange(0.001, 1000.0);
        factors[i]->setDecimals(4);
        factors[i]->setSingleStep(0.1);
        factors[i]->setValue(1.0);
        xyz_form->addRow(QString("K%1").arg(axis_names[i]), factors[i]);
    }
    pages->addWidget(xyz_page);
    root->addWidget(pages);
    connect(mode, qOverload<int>(&QComboBox::currentIndexChanged), pages, &QStackedWidget::setCurrentIndex);

    const auto update_guide = [this, mode, center]() {
        viewport_->SetTransformDialogGuide(
            TransformOperation::Scale,
            mode->currentIndex() == 0 ? TransformAxis::UniformScale : TransformAxis::X,
            center);
    };
    connect(mode, qOverload<int>(&QComboBox::currentIndexChanged), &dlg, [update_guide](int) { update_guide(); });
    update_guide();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    const bool accepted = dlg.exec() == QDialog::Accepted;
    const bool uniform = mode->currentIndex() == 0;
    const float factor_x = static_cast<float>(uniform ? uniform_factor->value() : factors[0]->value());
    const float factor_y = static_cast<float>(uniform ? uniform_factor->value() : factors[1]->value());
    const float factor_z = static_cast<float>(uniform ? uniform_factor->value() : factors[2]->value());
    viewport_->ClearTransformDialogGuide();
    precise_scale_base_point_ready_ = false;
    if (!accepted) {
        return;
    }

    if (viewport_->ApplyPreciseScale(center, factor_x, factor_y, factor_z, uniform)) {
        RefreshSceneTree();
        statusBar()->showMessage(
            uniform
                ? QString("Uniform scale: K = %1").arg(factor_x, 0, 'f', 4)
                : QString("Scale: Kx %1, Ky %2, Kz %3")
                    .arg(factor_x, 0, 'f', 4).arg(factor_y, 0, 'f', 4).arg(factor_z, 0, 'f', 4),
            2200);
    }
}

void MainWindow::BeginSolidBox() {
    ClearActiveProperties();

    QDialog dlg(this);
    dlg.setWindowTitle("BOX");
    dlg.setModal(true);

    auto* layout = new QGridLayout(&dlg);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setHorizontalSpacing(12);
    layout->setVerticalSpacing(10);

    auto* coordinate_label = new QLabel("Placement", &dlg);
    auto* plane_combo = new QComboBox(&dlg);
    plane_combo->addItem(QString::fromUtf8("XY Plane"), static_cast<int>(OpenGLViewport::SketchPlane::XY));
    plane_combo->addItem(QString::fromUtf8("XZ Plane"), static_cast<int>(OpenGLViewport::SketchPlane::XZ));
    plane_combo->addItem(QString::fromUtf8("YZ Plane"), static_cast<int>(OpenGLViewport::SketchPlane::YZ));
    plane_combo->addItem(QString::fromUtf8("Solid Face"), static_cast<int>(OpenGLViewport::SketchPlane::XY));

    layout->addWidget(coordinate_label, 0, 0, 1, 2);
    layout->addWidget(plane_combo, 1, 0, 1, 2);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(buttons, 2, 0, 1, 2);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    CenterDialogOnCursor(dlg);
    if (dlg.exec() != QDialog::Accepted) {
        UpdateActiveToolUi("select");
        statusBar()->showMessage("BOX canceled", 800);
        return;
    }

    active_parametric_edit_existing_ = false;
    if (plane_combo->currentIndex() == 3) {
        viewport_->BeginSolidBoxFaceSelection();
    } else {
        const OpenGLViewport::SketchPlane plane =
            static_cast<OpenGLViewport::SketchPlane>(plane_combo->currentData().toInt());
        viewport_->BeginSolidBoxRectangle(plane);
    }
    UpdateActiveToolUi("SolidBox");
    statusBar()->showMessage(
        plane_combo->currentIndex() == 3
            ? "BOX: select a planar body face"
            : "BOX: click first rectangle corner");
}

void MainWindow::BeginSolidCylinder() {
    ClearActiveProperties();

    QDialog dlg(this);
    dlg.setWindowTitle("CYLINDER");
    dlg.setModal(true);
    auto* layout = new QGridLayout(&dlg);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setHorizontalSpacing(12);
    layout->setVerticalSpacing(10);

    auto* coordinate_label = new QLabel("Placement", &dlg);
    auto* plane_combo = new QComboBox(&dlg);
    plane_combo->addItem(QString::fromUtf8("XY Plane"), static_cast<int>(OpenGLViewport::SketchPlane::XY));
    plane_combo->addItem(QString::fromUtf8("XZ Plane"), static_cast<int>(OpenGLViewport::SketchPlane::XZ));
    plane_combo->addItem(QString::fromUtf8("YZ Plane"), static_cast<int>(OpenGLViewport::SketchPlane::YZ));
    plane_combo->addItem(QString::fromUtf8("Solid Face"), static_cast<int>(OpenGLViewport::SketchPlane::XY));
    layout->addWidget(coordinate_label, 0, 0, 1, 2);
    layout->addWidget(plane_combo, 1, 0, 1, 2);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(buttons, 2, 0, 1, 2);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    CenterDialogOnCursor(dlg);
    if (dlg.exec() != QDialog::Accepted) {
        UpdateActiveToolUi("select");
        statusBar()->showMessage("CYLINDER canceled", 800);
        return;
    }

    active_parametric_edit_existing_ = false;
    if (plane_combo->currentIndex() == 3) {
        viewport_->BeginSolidCylinderFaceSelection();
    } else {
        const OpenGLViewport::SketchPlane plane =
            static_cast<OpenGLViewport::SketchPlane>(plane_combo->currentData().toInt());
        viewport_->BeginSolidCylinderCircle(plane);
    }
    UpdateActiveToolUi("SolidCylinder");
    statusBar()->showMessage(
        plane_combo->currentIndex() == 3
            ? "CYLINDER: select a planar body face"
            : "CYLINDER: click circle center");
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
    plane_combo->addItem(QString::fromUtf8("XY Plane"), static_cast<int>(OpenGLViewport::SketchPlane::XY));
    plane_combo->addItem(QString::fromUtf8("XZ Plane"), static_cast<int>(OpenGLViewport::SketchPlane::XZ));
    plane_combo->addItem(QString::fromUtf8("YZ Plane"), static_cast<int>(OpenGLViewport::SketchPlane::YZ));
    plane_combo->addItem(QString::fromUtf8("Solid Face"), static_cast<int>(OpenGLViewport::SketchPlane::XY));
    plane_combo->addItem(QString::fromUtf8("Define with 3 Points"), static_cast<int>(OpenGLViewport::SketchPlane::XY));

    layout->addWidget(name_label, 0, 0);
    layout->addWidget(name_edit, 0, 1);
    layout->addWidget(coordinate_label, 1, 0, 1, 2);
    layout->addWidget(plane_combo, 2, 0, 1, 2);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(buttons, 3, 0, 1, 2);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    CenterDialogOnCursor(dlg);
    if (dlg.exec() != QDialog::Accepted) {
        UpdateActiveToolUi("select");
        statusBar()->showMessage("New Sketch canceled", 800);
        return;
    }

    const QString sketch_name = name_edit->text().trimmed().isEmpty()
        ? QString("Sketch-%1").arg(sketch_counter_)
        : name_edit->text().trimmed();
    const int coordinate_mode = plane_combo->currentIndex();
    if (coordinate_mode == 3) {
        const SolidDisplayMode previous_solid_mode = CSolid::GetDisplayMode();
        const MeshDisplayMode previous_mesh_mode = CMesh3D::GetDisplayMode();
        SetSolidDisplayMode(SolidDisplayMode::SurfacesAndEdges);
        SetMeshDisplayMode(MeshDisplayMode::SurfaceMaterial);
        connect(
            viewport_,
            &OpenGLViewport::SketchFaceSelectionFinished,
            this,
            [this,
             sketch_name,
             previous_solid_mode,
             previous_mesh_mode](bool selected) {
                SetSolidDisplayMode(previous_solid_mode);
                SetMeshDisplayMode(previous_mesh_mode);
                if (!selected) {
                    UpdateActiveToolUi("select");
                    statusBar()->showMessage(
                        QString::fromUtf8("Sketch creation canceled"), 1200);
                    return;
                }
                ++sketch_counter_;
                UpdateActiveToolUi("NewSketch");
                ShowSketchPanel();
                statusBar()->showMessage(
                    QString::fromUtf8("%1:operation completed")
                        .arg(sketch_name));
            },
            Qt::SingleShotConnection);
        viewport_->BeginSketchFaceSelection(sketch_name);
        UpdateActiveToolUi("NewSketch");
        statusBar()->showMessage(
            QString::fromUtf8("%1:select the required geometry and continue")
                .arg(sketch_name));
        return;
    } else if (coordinate_mode == 4) {
        QMessageBox::information(
            this,
            "New Sketch",
            QString::fromUtf8("Defining a plane by three points will be implemented separately."));
        UpdateActiveToolUi("select");
        return;
    } else {
        const OpenGLViewport::SketchPlane plane =
            static_cast<OpenGLViewport::SketchPlane>(plane_combo->currentData().toInt());
        ++sketch_counter_;
        viewport_->BeginSketch(sketch_name, plane);
    }
    UpdateActiveToolUi("NewSketch");
    ShowSketchPanel();
    statusBar()->showMessage(
        coordinate_mode == 3
            ? QString::fromUtf8("%1:operation completed").arg(sketch_name)
            : QString("%1: Rectangle tool").arg(sketch_name));
}

void MainWindow::BeginSketchFillet() {
    if (!sketch_fillet_dialog_) {
        sketch_fillet_dialog_ = new QDialog(this, Qt::Tool);
        sketch_fillet_dialog_->setWindowTitle("Fillets Box");
        sketch_fillet_dialog_->setModal(false);
        sketch_fillet_dialog_->setAttribute(Qt::WA_DeleteOnClose, false);

        auto* layout = new QGridLayout(sketch_fillet_dialog_);
        layout->setContentsMargins(12, 10, 12, 12);
        layout->setHorizontalSpacing(12);
        layout->setVerticalSpacing(10);

        auto* radius_label = new QLabel("Fillet", sketch_fillet_dialog_);
        sketch_fillet_radius_spin_ = new QDoubleSpinBox(sketch_fillet_dialog_);
        sketch_fillet_radius_spin_->setRange(0.001, 1000000.0);
        sketch_fillet_radius_spin_->setDecimals(3);
        sketch_fillet_radius_spin_->setValue(10.0);
        sketch_fillet_radius_spin_->setKeyboardTracking(true);
        layout->addWidget(radius_label, 0, 0);
        layout->addWidget(sketch_fillet_radius_spin_, 0, 1);

        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Cancel, sketch_fillet_dialog_);
        layout->addWidget(buttons, 1, 0, 1, 2);
        connect(buttons, &QDialogButtonBox::rejected,
                sketch_fillet_dialog_, &QDialog::reject);
        connect(sketch_fillet_dialog_, &QDialog::rejected, this, [this]() {
            if (viewport_->CurrentTool() == ToolMode::SketchFillet) {
                viewport_->SetTool(ToolMode::Select);
                statusBar()->showMessage("Sketch Fillet canceled", 800);
            }
        });
        connect(sketch_fillet_radius_spin_,
                qOverload<double>(&QDoubleSpinBox::valueChanged),
                viewport_, &OpenGLViewport::SetSketchFilletRadius);
    }

    const double radius = sketch_fillet_radius_spin_->value();
    viewport_->BeginSketchFillet(radius);
    sketch_fillet_dialog_->show();
    sketch_fillet_dialog_->raise();
    sketch_fillet_dialog_->activateWindow();
    sketch_fillet_radius_spin_->setFocus();
    sketch_fillet_radius_spin_->selectAll();
    UpdateActiveToolUi("CurveFillets");
    statusBar()->showMessage(QString("Fillets R=%1:select the required geometry and continue").arg(radius, 0, 'f', 2));
}

void MainWindow::BeginDrawSpline() {
    if (spatial_curve_kind_ != SpatialCurveKind::None) {
        CancelSpatialCurve();
    }
    ClearActiveProperties();
    viewport_->SetTool(ToolMode::DrawSpline);
    UpdateActiveToolUi("DrawSpline");
    ShowDrawSplineDialog();
    viewport_->setFocus(Qt::OtherFocusReason);
    statusBar()->showMessage(
        "Draw Spline:adjust the required parameters and continue");
}

void MainWindow::ShowDrawSplineDialog() {
    if (!draw_spline_dialog_) {
        draw_spline_dialog_ = new QDialog(this, Qt::Tool);
        draw_spline_dialog_->setWindowTitle("Draw Spline");
        draw_spline_dialog_->setModal(false);
        draw_spline_dialog_->setFixedWidth(330);

        auto* layout = new QHBoxLayout(draw_spline_dialog_);
        layout->setContentsMargins(10, 8, 10, 8);
        layout->setSpacing(8);
        layout->addWidget(new QLabel("Simplification", draw_spline_dialog_));

        draw_spline_simplification_slider_ = new QSlider(
            Qt::Horizontal, draw_spline_dialog_);
        draw_spline_simplification_slider_->setRange(0, 100);
        draw_spline_simplification_slider_->setSingleStep(1);
        draw_spline_simplification_slider_->setPageStep(10);
        draw_spline_simplification_slider_->setValue(
            QSettings().value("tools/drawSplineSimplification", 50).toInt());
        draw_spline_simplification_slider_->setToolTip(
            "0% keeps more captured points; 100% creates the simplest curve");
        layout->addWidget(draw_spline_simplification_slider_, 1);

        draw_spline_simplification_value_ = new QLabel(draw_spline_dialog_);
        draw_spline_simplification_value_->setMinimumWidth(38);
        draw_spline_simplification_value_->setAlignment(
            Qt::AlignRight | Qt::AlignVCenter);
        layout->addWidget(draw_spline_simplification_value_);

        connect(draw_spline_simplification_slider_, &QSlider::valueChanged,
                this, [this](int value) {
            draw_spline_simplification_value_->setText(
                QString("%1%").arg(value));
            QSettings().setValue("tools/drawSplineSimplification", value);
            viewport_->SetDrawSplineSimplification(value);
        });
        connect(draw_spline_dialog_, &QDialog::rejected, this, [this]() {
            if (viewport_->CurrentTool() == ToolMode::DrawSpline) {
                viewport_->SetTool(ToolMode::Select);
                UpdateActiveToolUi("select");
                statusBar()->showMessage("Draw Spline operation status", 1200);
            }
        });
    }

    const int value = draw_spline_simplification_slider_->value();
    draw_spline_simplification_value_->setText(QString("%1%").arg(value));
    viewport_->SetDrawSplineSimplification(value);
    const QPoint position = viewport_->mapToGlobal(QPoint(18, 18));
    draw_spline_dialog_->move(position);
    draw_spline_dialog_->show();
    draw_spline_dialog_->raise();
    draw_spline_dialog_->activateWindow();
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
        auto* sketch_tool_group = new QButtonGroup(panel);
        sketch_tool_group->setExclusive(true);

        auto* rectangle_button = new QPushButton(panel);
        rectangle_button->setObjectName("SketchRectangleButton");
        rectangle_button->setCheckable(true);
        rectangle_button->setChecked(true);
        rectangle_button->setIcon(SketchRectangleIcon());
        rectangle_button->setIconSize(QSize(44, 44));
        rectangle_button->setToolTip("Rectangle");
        rectangle_button->setFixedSize(58, 58);
        sketch_tool_group->addButton(rectangle_button);
        connect(rectangle_button, &QPushButton::clicked, this, [this]() {
            viewport_->SetSketchRectangleTool();
        });
        grid->addWidget(rectangle_button, 0, 0);

        auto* polyline_button = new QPushButton(panel);
        polyline_button->setObjectName("SketchPolylineButton");
        polyline_button->setCheckable(true);
        polyline_button->setIcon(SketchPolylineIcon());
        polyline_button->setIconSize(QSize(44, 44));
        polyline_button->setToolTip("Polyline");
        polyline_button->setFixedSize(58, 58);
        sketch_tool_group->addButton(polyline_button);
        connect(polyline_button, &QPushButton::clicked, this, [this]() {
            viewport_->SetSketchPolylineTool();
            viewport_->setFocus();
        });
        grid->addWidget(polyline_button, 0, 1);

        auto* bezier_button = new QPushButton(panel);
        bezier_button->setObjectName("SketchBezierButton");
        bezier_button->setCheckable(true);
        bezier_button->setIcon(SketchBezierIcon());
        bezier_button->setIconSize(QSize(44, 44));
        bezier_button->setToolTip("Bezier");
        bezier_button->setFixedSize(58, 58);
        sketch_tool_group->addButton(bezier_button);
        connect(bezier_button, &QPushButton::clicked, this, [this]() {
            viewport_->SetSketchBezierTool();
            viewport_->setFocus();
        });
        grid->addWidget(bezier_button, 2, 0);

        auto* convert_bezier_button = new QPushButton(panel);
        convert_bezier_button->setObjectName("SketchConvertBezierButton");
        convert_bezier_button->setIcon(SketchConvertBezierIcon());
        convert_bezier_button->setIconSize(QSize(44, 44));
        convert_bezier_button->setToolTip("Convert line to Bezier curve");
        convert_bezier_button->setFixedSize(58, 58);
        connect(convert_bezier_button, &QPushButton::clicked, this, [this]() {
            viewport_->BeginSketchConvertLineToBezier();
            viewport_->setFocus();
        });
        grid->addWidget(convert_bezier_button, 2, 1);

        const struct {
            const char* tooltip;
            int row;
            int column;
            QIcon icon;
        } sketch_placeholders[] = {
            {"Line to Arc", 1, 0, SketchArcIcon()},
            {"Fillet", 1, 1, SketchFilletIcon()}
        };
        for (const auto& placeholder : sketch_placeholders) {
            auto* button = new QPushButton(panel);
            button->setToolTip(placeholder.tooltip);
            button->setFixedSize(58, 58);
            button->setIcon(placeholder.icon);
            button->setIconSize(QSize(44, 44));
            if (QString::fromLatin1(placeholder.tooltip) == "Fillet") {
                connect(button, &QPushButton::clicked, this, [this]() {
                    BeginSketchFillet();
                });
            } else if (QString::fromLatin1(placeholder.tooltip) == "Line to Arc") {
                button->setObjectName("SketchConvertArcButton");
                connect(button, &QPushButton::clicked, this, [this]() {
                    viewport_->BeginSketchConvertLineToArc();
                    viewport_->setFocus();
                });
            }
            grid->addWidget(button, placeholder.row, placeholder.column);
        }

        auto* constraints = new QGroupBox("Geometry constraints", panel);
        auto* constraints_grid = new QGridLayout(constraints);
        constraints_grid->setContentsMargins(6, 8, 6, 6);
        constraints_grid->setSpacing(6);
        const struct {
            const char* tooltip;
            int kind;
            void (OpenGLViewport::*begin)();
        } constraint_tools[] = {
            {"Horizontal", 0, &OpenGLViewport::BeginSketchConstraintHorizontal},
            {"Vertical", 1, &OpenGLViewport::BeginSketchConstraintVertical},
            {"Tangent to previous segment", 2, &OpenGLViewport::BeginSketchConstraintTangentStart},
            {"Tangent to next segment", 3, &OpenGLViewport::BeginSketchConstraintTangentEnd}
        };
        for (int index = 0; index < 4; ++index) {
            const auto& constraint = constraint_tools[index];
            auto* button = new QPushButton(constraints);
            button->setObjectName(QString("SketchConstraintButton%1").arg(index));
            button->setToolTip(constraint.tooltip);
            button->setIcon(SketchGeometryConstraintIcon(constraint.kind));
            button->setIconSize(QSize(44, 44));
            button->setFixedSize(58, 58);
            connect(button, &QPushButton::clicked, this,
                    [this, begin = constraint.begin]() {
                        (viewport_->*begin)();
                        viewport_->setFocus();
                    });
            constraints_grid->addWidget(button, index / 2, index % 2);
        }
        root->addWidget(constraints);

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
        sketch_dock_->setFloating(true);
        sketch_dock_->resize(164, 430);
    }

    if (viewport_->CurrentTool() == ToolMode::Select) {
        if (auto* group = sketch_dock_->findChild<QButtonGroup*>()) {
            group->setExclusive(false);
            for (QAbstractButton* button : group->buttons()) {
                button->setChecked(false);
            }
            group->setExclusive(true);
        }
    } else if (auto* rectangle_button = sketch_dock_->findChild<QPushButton*>("SketchRectangleButton")) {
        rectangle_button->setChecked(true);
    }
    sketch_dock_->show();
    sketch_dock_->raise();
}

bool MainWindow::TryApplyPendingTrim() {
    if (pending_trim_tool_id_.empty()) {
        return false;
    }

    const std::string tool_id = pending_trim_tool_id_;
    ActiveParametricObject applied =
        tool_registry_.ApplyTrimToSelection(tool_id, document_);
    if (applied.tool_id.empty()) {
        const QString cutter = tool_id == "TrimByPlane"
            ? "Plane"
            : (tool_id == "TrimBySketch" ? "Sketch" : "Surface");
        viewport_->SetSelectionMode(SelectionMode::Object);
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionConfirmationMode(true);
        UpdateActiveToolUi(tool_id);
        statusBar()->showMessage(
            QString("%1:operation canceled")
                .arg(QString::fromStdString(
                    tool_registry_.LabelFor(tool_id)))
                .arg(cutter));
        return false;
    }

    pending_trim_tool_id_.clear();
    viewport_->SetSelectionConfirmationMode(false);
    active_parametric_object_ = std::move(applied);
    active_parametric_edit_existing_ = true;
    property_panel_->SetActiveObject(active_parametric_object_);
    ShowPropertyPanelAtCursor(
        QString::fromStdString(tool_registry_.LabelFor(tool_id)));
    viewport_->SetTool(ToolMode::Select);
    UpdateActiveToolUi(tool_id);
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage(
        QString("%1:select the required geometry and continue")
            .arg(QString::fromStdString(
                tool_registry_.LabelFor(tool_id))));
    return true;
}

void MainWindow::CancelPendingTrim(const QString& status_text) {
    pending_trim_tool_id_.clear();
    viewport_->SetSelectionConfirmationMode(false);
    viewport_->SetTool(ToolMode::Select);
    UpdateActiveToolUi("select");
    if (!status_text.isEmpty()) {
        statusBar()->showMessage(status_text, 1200);
    }
}

void MainWindow::ApplySheetBend() {
    ClearActiveProperties();
    viewport_->SetTool(ToolMode::Select);
    viewport_->SetSelectionMode(SelectionMode::Object);

    CSolid* solid = nullptr;
    CAlfaObject* line_object = nullptr;
    CPoint3d line_start;
    CPoint3d line_end;
    int solid_count = 0;
    int line_count = 0;

    const auto& objects = document_.GetObjects();
    for (size_t index : document_.GetSelectedObjectIndices()) {
        if (index >= objects.size() || !objects[index]) continue;
        CAlfaObject* object = objects[index].get();
        if (auto* candidate_solid = dynamic_cast<CSolid*>(object)) {
            solid = candidate_solid;
            ++solid_count;
            continue;
        }
        if (auto* polyline = dynamic_cast<CPolyline*>(object)) {
            if (polyline->GetPointCount() == 2) {
                line_object = object;
                line_start = polyline->GetPoints().front();
                line_end = polyline->GetPoints().back();
                ++line_count;
            }
            continue;
        }
        if (auto* sketch = dynamic_cast<CSmartLine*>(object)) {
            const CLinkLine* segment = sketch->GetNumLines() == 1
                ? sketch->GetLine(0) : nullptr;
            if (segment && segment->GetType() != LinkLineType::Bezier
                && segment->GetType() != LinkLineType::Arc) {
                line_object = object;
                line_start = sketch->LocalToWorld(segment->GetStart());
                line_end = sketch->LocalToWorld(segment->GetEnd());
                ++line_count;
            }
        }
    }

    if (solid_count != 1 || line_count != 1 || !solid || !line_object) {
        UpdateActiveToolUi("select");
        statusBar()->showMessage(
            "Sheet Bend: select one solid and one two-point directed line",
            3500);
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Sheet Bend");
    auto* layout = new QFormLayout(&dialog);
    auto* radius = new QDoubleSpinBox(&dialog);
    radius->setRange(0.001, 1000000.0);
    radius->setDecimals(3);
    radius->setSingleStep(0.5);
    radius->setValue(2.0);
    auto* angle = new QDoubleSpinBox(&dialog);
    angle->setRange(0.01, 178.99);
    angle->setDecimals(2);
    angle->setSingleStep(5.0);
    angle->setValue(90.0);
    angle->setSuffix(" deg");
    auto* direction = new QComboBox(&dialog);
    direction->addItem("Clockwise");
    direction->addItem("Counterclockwise");
    layout->addRow("Inner radius", radius);
    layout->addRow("Bend angle", angle);
    layout->addRow("Direction", direction);
    auto* hint = new QLabel(
        "Direction is viewed from the start of the line towards its end.",
        &dialog);
    hint->setWordWrap(true);
    layout->addRow(hint);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addRow(buttons);
    CenterDialogOnCursor(dialog);
    if (dialog.exec() != QDialog::Accepted) {
        UpdateActiveToolUi("select");
        statusBar()->showMessage("Sheet Bend canceled", 1000);
        return;
    }

    SheetBendParameters parameters;
    parameters.line_start_x = line_start.x;
    parameters.line_start_y = line_start.y;
    parameters.line_start_z = line_start.z;
    parameters.line_end_x = line_end.x;
    parameters.line_end_y = line_end.y;
    parameters.line_end_z = line_end.z;
    parameters.inner_radius = radius->value();
    parameters.angle_degrees = angle->value();
    parameters.clockwise = direction->currentIndex() == 0;

    TopoDS_Shape bent_shape;
    std::string error_message;
    if (!BuildSheetBendShape(
            solid->m_Shape, parameters, bent_shape, error_message)) {
        UpdateActiveToolUi("select");
        statusBar()->showMessage(
            QString("Sheet Bend: %1")
                .arg(QString::fromStdString(error_message)),
            5000);
        return;
    }

    if (!undo_redo_.BeginChange()) {
        UpdateActiveToolUi("select");
        statusBar()->showMessage("Sheet Bend: unable to start undo record", 3000);
        return;
    }
    TopoDS_Shape original_shape = solid->m_Shape;
    const bool needs_frozen_base = solid->GetNumOperations() == 0;
    solid->m_Shape = bent_shape;
    if (!solid->ReBuldMesh()) {
        solid->m_Shape = original_shape;
        solid->ReBuldMesh();
        undo_redo_.CancelChange();
        UpdateActiveToolUi("select");
        statusBar()->showMessage("Sheet Bend: mesh rebuild failed", 3500);
        return;
    }
    std::vector<ParametricParameterValue> saved_parameters{
        {"point1.x", parameters.line_start_x},
        {"point1.y", parameters.line_start_y},
        {"point1.z", parameters.line_start_z},
        {"point2.x", parameters.line_end_x},
        {"point2.y", parameters.line_end_y},
        {"point2.z", parameters.line_end_z},
        {"radius", parameters.inner_radius},
        {"angle", parameters.angle_degrees},
        {"direction", parameters.clockwise ? 0.0 : 1.0}
    };
    if (needs_frozen_base) {
        CSolid frozen_base(original_shape);
        const size_t base_tool_index = solid->AddBooleanToolCopy(frozen_base);
        saved_parameters.push_back(
            {"base.tool.index", static_cast<double>(base_tool_index)});
    }
    solid->SetParametricOperation(
        solid->GetOperationTree().size(),
        "SolidSheetBend",
        "Sheet Bend",
        std::move(saved_parameters));
    undo_redo_.CommitChangeLazy("Sheet Bend");

    RefreshSceneTree();
    UpdateActiveToolUi("select");
    viewport_->update();
    statusBar()->showMessage(
        QString("Sheet Bend: R%1, %2 deg, %3")
            .arg(parameters.inner_radius, 0, 'f', 3)
            .arg(parameters.angle_degrees, 0, 'f', 2)
            .arg(parameters.clockwise ? "clockwise" : "counterclockwise"),
        3000);
}

void MainWindow::CompleteArchitectureOpeningPlacement(
    unsigned long wall_id, CPoint3d point) {
    if (pending_architecture_opening_tool_id_.empty()) return;
    const std::string tool_id = pending_architecture_opening_tool_id_;
    active_parametric_object_ =
        tool_registry_.ActivateArchitectureOpening(
            tool_id, document_, wall_id, point);
    if (active_parametric_object_.tool_id.empty()) {
        viewport_->BeginPickArchitectureWall(
            "Window / Door: click a visible room wall");
        statusBar()->showMessage(
            "Window / Door: the clicked object is not a visible room wall");
        return;
    }

    pending_architecture_opening_tool_id_.clear();
    active_parametric_edit_existing_ = false;
    property_panel_->SetActiveObject(active_parametric_object_);
    ShowPropertyPanelAtCursor(
        QString::fromStdString(tool_registry_.LabelFor(tool_id)));
    document_.ClearSelection();
    viewport_->SetTool(ToolMode::Select);
    UpdateActiveToolUi(tool_id);
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage(
        QString("%1 placed on wall; adjust the exact distance with the slider")
            .arg(QString::fromStdString(tool_registry_.LabelFor(tool_id))),
        2600);
}

void MainWindow::CancelArchitectureOpeningPlacement() {
    if (pending_architecture_opening_tool_id_.empty()) return;
    pending_architecture_opening_tool_id_.clear();
    active_parametric_object_ = {};
    active_parametric_edit_existing_ = false;
    property_panel_->Clear();
    if (properties_dock_) properties_dock_->hide();
    viewport_->SetTool(ToolMode::Select);
    UpdateActiveToolUi("select");
    statusBar()->showMessage("Window / Door placement canceled", 1400);
}

void MainWindow::ActivateParametricTool(const std::string& tool_id) {
    const bool curve_edit_tool = tool_id == "CurveJoin"
        || tool_id == "CurveSplit"
        || tool_id == "CurveExtend"
        || tool_id == "CurveTrimByPlane"
        || tool_id == "CurveSimplifyByPoint"
        || tool_id == "CurveReverse";
    if (pending_curve_edit_command_ != CurveEditCommand::None
        && !curve_edit_tool) {
        CancelCurveEditCommand();
    }
    active_parametric_edit_existing_ = false;
    if (tool_id != "window" && tool_id != "door") {
        viewport_->CancelArchitectureWallPick();
        pending_architecture_opening_tool_id_.clear();
    }
    if (tool_id != "SolidLowPoly") {
        low_poly_pick_pending_ = false;
    }
    if (tool_id == "window" || tool_id == "door") {
        ClearActiveProperties();
        if (tool_registry_.HasVisibleArchitectureWalls(document_)) {
            pending_architecture_opening_tool_id_ = tool_id;
            document_.ClearSelection();
            viewport_->SetTool(ToolMode::Select);
            viewport_->SetSelectionMode(SelectionMode::Object);
            viewport_->BeginPickArchitectureWall(
                QString("%1: click the approximate position on a visible wall")
                    .arg(QString::fromStdString(
                        tool_registry_.LabelFor(tool_id))));
            UpdateActiveToolUi(tool_id);
            return;
        }
        pending_architecture_opening_tool_id_.clear();
    }
    if (tool_id == "PlaneTool") {
        ClearActiveProperties();
        std::array<double, 4> factors = LoadRememberedPlaneFactors();
        const int method = ShowPlaneDefinitionDialog(this, factors);
        if (method == QDialog::Rejected) {
            UpdateActiveToolUi("select");
            statusBar()->showMessage("Plane:operation canceled", 1200);
            return;
        }
        const ToolDefinition* definition = tool_registry_.Find("PlaneTool");
        if (!definition) return;
        std::vector<ToolParameter> parameters = definition->defaults;
        const auto set_value = [&parameters](const char* id, double value) {
            const auto found = std::find_if(parameters.begin(), parameters.end(),
                [id](const ToolParameter& parameter) { return parameter.id == id; });
            if (found != parameters.end()) found->value = value;
        };
        bool pick_three_points = false;
        if (method == 7) {
            pending_reference_plane_face_pick_ = true;
            viewport_->SetTool(ToolMode::Select);
            viewport_->SetSelectionMode(SelectionMode::Face);
            UpdateActiveToolUi(tool_id);
            statusBar()->showMessage(
                "Plane — Face of Solid:select the required geometry and continue");
            return;
        } else if (method == 1) {
            set_value("mode", 2.0);
            pick_three_points = true;
        } else if (method == 2) {
            set_value("mode", 3.0);
            set_value("offset", 0.0);
            factors = {0.0, 0.0, 1.0, 0.0};
        } else if (method == 3) {
            set_value("mode", 4.0);
            set_value("offset", 0.0);
            factors = {0.0, 1.0, 0.0, 0.0};
        } else if (method == 4) {
            set_value("mode", 5.0);
            set_value("offset", 0.0);
            factors = {1.0, 0.0, 0.0, 0.0};
        } else if (method == 5) {
            if (factors[0] * factors[0] + factors[1] * factors[1]
                    + factors[2] * factors[2] <= 1.0e-18) {
                statusBar()->showMessage("Plane:operation failed; check the selected geometry and parameters", 2600);
                return;
            }
            set_value("mode", 0.0);
            set_value("a", factors[0]); set_value("b", factors[1]);
            set_value("c", factors[2]); set_value("d", factors[3]);
        } else if (method == 6) {
            std::vector<double> values{0.0, 0.0, 0.0, 0.0, 0.0, 1.0};
            if (!ShowPlaneValuesDialog(this, "Point + Normal",
                    {"Point X", "Point Y", "Point Z", "Normal X", "Normal Y", "Normal Z"},
                    values)) {
                UpdateActiveToolUi("select");
                return;
            }
            if (values[3] * values[3] + values[4] * values[4]
                    + values[5] * values[5] <= 1.0e-18) {
                statusBar()->showMessage("Plane:operation failed; check the selected geometry and parameters", 2600);
                return;
            }
            set_value("mode", 1.0);
            set_value("plane.origin.x", values[0]);
            set_value("plane.origin.y", values[1]);
            set_value("plane.origin.z", values[2]);
            set_value("plane.normal.x", values[3]);
            set_value("plane.normal.y", values[4]);
            set_value("plane.normal.z", values[5]);
            factors = {values[3], values[4], values[5],
                       -(values[3] * values[0] + values[4] * values[1] + values[5] * values[2])};
        }
        if (!pick_three_points) SaveRememberedPlaneFactors(factors);
        active_parametric_object_ = tool_registry_.CreateParametricObject(
            "PlaneTool", document_, parameters);
        if (active_parametric_object_.tool_id.empty()) return;
        active_parametric_edit_existing_ = false;
        plane_three_point_method_selected_ = pick_three_points;
        property_panel_->SetActiveObject(active_parametric_object_);
        ShowPropertyPanelAtCursor("Plane");
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi(tool_id);
        RefreshSceneTree();
        viewport_->update();
        if (pick_three_points) BeginPlaneThreePointPick();
        else statusBar()->showMessage("Plane:operation completed");
        return;
    }
    if (tool_id == "SolidBox") {
        BeginSolidBox();
        return;
    }
    if (tool_id == "SolidCylinder") {
        BeginSolidCylinder();
        return;
    }
    if (tool_id == "SolidSheetBend") {
        ApplySheetBend();
        return;
    }
    if (tool_id == "SolidLowPoly") {
        ShowLowPolyTool();
        return;
    }
    if (tool_id == "MeshFillContour") {
        ShowMeshFillContourTool();
        return;
    }
    if (tool_id == "TrimMeshTest") {
        ShowTrimMeshTestTool();
        return;
    }
    if (tool_id == "ClassifyFaceCut") {
        ShowClassifyFaceCutTool();
        return;
    }
    if (tool_id == "SolidTwoSketches") {
        ClearActiveProperties();
        CreateBodyFromTwoSketches();
        return;
    }
    if (tool_id == "SolidSweepTwoRails") {
        ClearActiveProperties();
        CreateTwoRailSweepSolid();
        return;
    }
    if (tool_id == "SewingFaceTool") {
        ClearActiveProperties();
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionMode(SelectionMode::Object);

        QSettings settings;
        const double tolerance = std::clamp(
            settings.value("preferences/modeling/tolerance", 0.02).toDouble(),
            1.0e-6, 1000.0);
        double used_tolerance = tolerance;
        std::string error_message;
        if (!document_.SewSelectedSurfacesToSolid(
                tolerance, &error_message, &used_tolerance)) {
            UpdateActiveToolUi("select");
            statusBar()->showMessage(
                QString("Sewing Faces: %1")
                    .arg(QString::fromStdString(error_message)),
                3200);
            return;
        }

        RefreshSceneTree();
        UpdateActiveToolUi("select");
        viewport_->update();
        statusBar()->showMessage(
            QString("Sewing Faces: solid created (tolerance %1 mm)")
                .arg(used_tolerance, 0, 'g', 6),
            2200);
        return;
    }

    if (tool_id == "TrimByPlane"
        || tool_id == "TrimBySketch"
        || tool_id == "TrimBySurface") {
        ClearActiveProperties();
        pending_trim_tool_id_ = tool_id;
        viewport_->SetSelectionMode(SelectionMode::Object);
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionConfirmationMode(true);
        UpdateActiveToolUi(tool_id);
        TryApplyPendingTrim();
        return;
    }

    if (tool_id == "PolylineCurve") {
        BeginSpatialCurve(SpatialCurveKind::Polyline);
        return;
    }

    if (tool_id == "BSplineCurve") {
        BeginSpatialCurve(SpatialCurveKind::BSpline);
        return;
    }

    if (tool_id == "DrawSpline") {
        BeginDrawSpline();
        return;
    }

    if (tool_id == "BezierCurve3D") {
        BeginSpatialCurve(SpatialCurveKind::Bezier);
        return;
    }

    if (tool_id == "NurbsCurve3D") {
        BeginSpatialCurve(SpatialCurveKind::Nurbs);
        return;
    }

    if (tool_id == "PlaneIntersection") {
        CreatePlaneIntersection();
        return;
    }
    if (tool_id == "SurfaceIntersection") {
        CreateSurfaceIntersection();
        return;
    }
    if (tool_id == "ProjectCurveToSurface") {
        ProjectCurveToSurface();
        return;
    }
    if (tool_id == "ExtractSurfaceEdge") {
        ExtractSurfaceEdge();
        return;
    }

    if (tool_id == "NurbsParametersTool") {
        if (active_parametric_object_.tool_id != "NurbsParameters") {
            ClearActiveProperties();
        }
        if (UpdateNurbsParameterEditor()) {
            UpdateActiveToolUi(tool_id);
        } else {
            statusBar()->showMessage(
                "NURBS Parameters:select the required geometry and continue",
                3000);
            UpdateActiveToolUi("select");
        }
        return;
    }

    if (tool_id == "CurveJoin") {
        BeginCurveEditCommand(CurveEditCommand::Join);
        return;
    }
    if (tool_id == "CurveSplit") {
        BeginCurveEditCommand(CurveEditCommand::Split);
        return;
    }
    if (tool_id == "CurveExtend") {
        BeginCurveEditCommand(CurveEditCommand::Extend);
        return;
    }
    if (tool_id == "CurveTrimByPlane") {
        BeginCurveEditCommand(CurveEditCommand::TrimByPlane);
        return;
    }
    if (tool_id == "CurveSimplifyByPoint") {
        BeginCurveEditCommand(CurveEditCommand::SimplifyByPoint);
        return;
    }
    if (tool_id == "CurveReverse") {
        BeginCurveEditCommand(CurveEditCommand::Reverse);
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
        dlg.SetSelectedOperation(DialogOperationFromBoolean(last_boolean_operation_));
        CenterDialogOnCursor(dlg);
        if (dlg.exec() != QDialog::Accepted) {
            UpdateActiveToolUi("select");
            viewport_->SetTool(ToolMode::Select);
            statusBar()->showMessage("Boolean operation canceled", 800);
            return;
        }

        last_boolean_operation_ = BooleanOperationFromDialog(dlg.SelectedOperation());
        viewport_->BeginBooleanTool(last_boolean_operation_);
        UpdateActiveToolUi("boolean");
        return;
    }

    if (tool_id == "CurveFillets") {
        ClearActiveProperties();
        BeginSketchFillet();
        return;
    }

    if (tool_id == "SolidSketchFeature") {
        ClearActiveProperties();
        active_parametric_object_ =
            tool_registry_.ApplySketchFeatureToSelection(document_);
        if (active_parametric_object_.tool_id.empty()) {
            statusBar()->showMessage(
                "Boss / Pocket:select the required geometry and continue",
                3000);
            UpdateActiveToolUi("select");
            return;
        }
        active_parametric_edit_existing_ = true;
        property_panel_->SetActiveObject(active_parametric_object_);
        ShowPropertyPanelAtCursor("Boss / Pocket");
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi(tool_id);
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage(
            "Boss / Pocket:select the required geometry and continue");
        return;
    }

    if (tool_id == "SolidOffsetFace") {
        ClearActiveProperties();
        CSolid* selected_body = document_.GetSelectedFaceSolid();
        if (selected_body && selected_body->GetNumOperations() <= 0) {
            bool accepted = false;
            const double distance = QInputDialog::getDouble(
                this,
                "Offset Face",
                "Distance",
                1.0,
                -1000000.0,
                1000000.0,
                3,
                &accepted,
                Qt::WindowFlags(),
                0.1);
            if (!accepted) {
                UpdateActiveToolUi("select");
                statusBar()->showMessage("Offset Face canceled", 800);
                return;
            }
            if (!tool_registry_.ApplyOffsetFaceOnce(document_, distance)) {
                UpdateActiveToolUi("select");
                statusBar()->showMessage(
                    "Offset Face:operation failed; check the selected geometry and parameters",
                    2800);
                return;
            }
            RefreshSceneTree();
            viewport_->SetTool(ToolMode::Select);
            UpdateActiveToolUi("select");
            viewport_->update();
            statusBar()->showMessage(
                QString("Offset Face: Distance %1").arg(distance, 0, 'f', 3),
                1800);
            return;
        }
        active_parametric_object_ =
            tool_registry_.ApplyOffsetFaceToSelection(document_);
        if (active_parametric_object_.tool_id.empty()) {
            statusBar()->showMessage(selected_body
                ? "Offset Face:operation failed; check the selected geometry and parameters"
                : "Offset Face:select the required geometry and continue",
                2800);
            UpdateActiveToolUi("select");
            return;
        }
        active_parametric_edit_existing_ = true;
        property_panel_->SetActiveObject(active_parametric_object_);
        ShowPropertyPanelAtCursor("Offset Face");
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi(tool_id);
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage(
            "Offset Face:adjust the required parameters and continue");
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
            statusBar()->showMessage(QString("Extrude Face:operation status").arg(taper_angle, 0, 'f', 2));
        } else {
            statusBar()->showMessage(QString("Extrude Face:select the required geometry and continue").arg(taper_angle, 0, 'f', 2));
        }
        return;
    }

    if (tool_id == "SolidDraft") {
        ClearActiveProperties();
        viewport_->BeginDraftFaceTool();
        UpdateActiveToolUi(tool_id);
        if (document_.HasSelectedSolidFace()) {
            statusBar()->showMessage("Draft Face:select the required geometry and continue");
        } else {
            statusBar()->showMessage("Draft Face:select the required geometry and continue");
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
            statusBar()->showMessage("ThickSolid:adjust the required parameters and continue");
        } else {
            statusBar()->showMessage(document_.GetSelectedSolid() ? "ThickSolid:select the required geometry and continue" : "ThickSolid:select the required geometry and continue");
        }
        return;
    }

    if (tool_id == "SolidFrameTool") {
        ClearActiveProperties();
        CSmartLine* profile = document_.GetSelectedSketch();
        if (!profile || !profile->IsClosed()) {
            statusBar()->showMessage(
                "Frame:select the required geometry and continue",
                2200);
            UpdateActiveToolUi("select");
            return;
        }

        constexpr double default_width = 40.0;
        constexpr double default_height = 30.0;
        document_.EnsureObjectId(*profile);
        if (!document_.CreateFrameSolid(profile->m_id, default_width, default_height)) {
            statusBar()->showMessage(
                "Frame:operation failed; check the selected geometry and parameters",
                2200);
            UpdateActiveToolUi("select");
            return;
        }

        active_parametric_edit_existing_ = false;
        CAlfaObject* frame_solid = document_.GetSelectedObject();
        if (!frame_solid) {
            statusBar()->showMessage("Frame:operation failed; check the selected geometry and parameters", 1600);
            UpdateActiveToolUi("select");
            return;
        }
        active_parametric_object_ = tool_registry_.ActiveObjectFromDocument(
            document_.GetSelectedObjectIndex(), *frame_solid, 0, &document_);
        property_panel_->SetActiveObject(active_parametric_object_);
        ShowPropertyPanelAtCursor("Frame");
        RefreshSceneTree();
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi(tool_id);
        viewport_->update();
        statusBar()->showMessage("Frame:adjust the required parameters and continue");
        return;
    }

    if (tool_id == "SolidWireTool") {
        ClearActiveProperties();
        CAlfaObject* path = document_.GetSelectedObject();
        const bool supported = dynamic_cast<CPolyline*>(path)
            || dynamic_cast<CSmartLine*>(path)
            || dynamic_cast<CBSpline*>(path);
        const bool closed = (dynamic_cast<CSmartLine*>(path) && dynamic_cast<CSmartLine*>(path)->IsClosed())
            || (dynamic_cast<CBSpline*>(path) && dynamic_cast<CBSpline*>(path)->IsClosed());
        if (!path || !supported || closed) {
            statusBar()->showMessage("Wire:select the required geometry and continue", 2200);
            UpdateActiveToolUi("select");
            return;
        }
        constexpr double default_radius = 10.0;
        document_.EnsureObjectId(*path);
        if (!document_.CreateWireSolid(path->m_id, default_radius)) {
            statusBar()->showMessage("Wire:operation failed; check the selected geometry and parameters", 2200);
            UpdateActiveToolUi("select");
            return;
        }
        active_parametric_edit_existing_ = false;
        CAlfaObject* wire = document_.GetSelectedObject();
        if (!wire) return;
        active_parametric_object_ = tool_registry_.ActiveObjectFromDocument(
            document_.GetSelectedObjectIndex(), *wire, 0, &document_);
        property_panel_->SetActiveObject(active_parametric_object_);
        ShowPropertyPanelAtCursor("Wire");
        RefreshSceneTree();
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi(tool_id);
        viewport_->update();
        statusBar()->showMessage("Wire:adjust the required parameters and continue");
        return;
    }

    if (tool_id == "SolidPolyhedronTool") {
        ClearActiveProperties();
        CSmartLine* profile = document_.GetSelectedSketch();
        if (!profile || profile->GetNumLines() == 0) {
            statusBar()->showMessage(
                "Polyhedron:select the required geometry and continue",
                2200);
            UpdateActiveToolUi("select");
            return;
        }

        constexpr int default_turns = 8;
        constexpr int default_axis = 2;
        document_.EnsureObjectId(*profile);
        if (!document_.CreatePolyhedronSolid(
                profile->m_id, default_axis, default_turns)) {
            statusBar()->showMessage(
                "Polyhedron:operation failed; check the selected geometry and parameters",
                2200);
            UpdateActiveToolUi("select");
            return;
        }

        active_parametric_edit_existing_ = false;
        CAlfaObject* polyhedron = document_.GetSelectedObject();
        if (!polyhedron) {
            statusBar()->showMessage(
                "Polyhedron:operation failed; check the selected geometry and parameters", 1600);
            UpdateActiveToolUi("select");
            return;
        }
        active_parametric_object_ = tool_registry_.ActiveObjectFromDocument(
            document_.GetSelectedObjectIndex(), *polyhedron, 0, &document_);
        property_panel_->SetActiveObject(active_parametric_object_);
        ShowPropertyPanelAtCursor("Polyhedron");
        RefreshSceneTree();
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi(tool_id);
        viewport_->update();
        statusBar()->showMessage(
            "Polyhedron:adjust the required parameters and continue");
        return;
    }

    if (tool_id == "SolidSweptTool") {
        ClearActiveProperties();
        CSmartLine* section = nullptr;
        CAlfaObject* guide = nullptr;
        for (size_t index : document_.GetSelectedObjectIndices()) {
            auto& objects = document_.GetObjects();
            if (index >= objects.size() || !objects[index]) {
                continue;
            }
            CAlfaObject* object = objects[index].get();
            if (auto* sketch = dynamic_cast<CSmartLine*>(object)) {
                if (sketch->IsClosed()) {
                    if (section) {
                        section = nullptr;
                        break;
                    }
                    section = sketch;
                } else {
                    if (guide) {
                        guide = nullptr;
                        break;
                    }
                    guide = sketch;
                }
            } else if (auto* spline = dynamic_cast<CBSpline*>(object)) {
                if (!spline->IsClosed()) {
                    if (guide) {
                        guide = nullptr;
                        break;
                    }
                    guide = spline;
                }
            }
        }

        if (!section || !guide) {
            statusBar()->showMessage(
                "Swept:select the required geometry and continue",
                2200);
            UpdateActiveToolUi("select");
            return;
        }

        document_.EnsureObjectId(*section);
        document_.EnsureObjectId(*guide);
        if (!document_.CreateSweptSolid(section->m_id, guide->m_id, 1, 0.0, 0.0, 0.0)) {
            statusBar()->showMessage(
                "Swept:operation failed; check the selected geometry and parameters",
                2200);
            UpdateActiveToolUi("select");
            return;
        }

        active_parametric_edit_existing_ = false;
        CAlfaObject* swept_solid = document_.GetSelectedObject();
        if (!swept_solid) {
            statusBar()->showMessage("Swept:operation failed; check the selected geometry and parameters", 1600);
            UpdateActiveToolUi("select");
            return;
        }
        active_parametric_object_ = tool_registry_.ActiveObjectFromDocument(
            document_.GetSelectedObjectIndex(), *swept_solid, 0, &document_);
        property_panel_->SetActiveObject(active_parametric_object_);
        ShowPropertyPanelAtCursor("Orientation Profile");
        RefreshSceneTree();
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi(tool_id);
        viewport_->update();
        statusBar()->showMessage("Swept:adjust the required parameters and continue");
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

        if ((document_.GetSelectedPolyline() || document_.GetSelectedSketch())
            && !TryStartLivePolylineExtrudeFromSelection()) {
            document_.CancelLiveExtrudeSelectedPolyline();
            active_parametric_object_ = {};
            UpdateActiveToolUi("select");
            viewport_->SetTool(ToolMode::Select);
            statusBar()->showMessage("Extrude:operation failed; check the selected geometry and parameters");
            return;
        }
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage(document_.HasLivePolylineExtrude()
            ? "Extrude:adjust the required parameters and continue"
            : "Extrude:select the required geometry and continue");
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

        if ((document_.GetSelectedPolyline() || document_.GetSelectedSketch())
            && !TryStartLivePolylineRevolveFromSelection()) {
            document_.CancelLiveRevolveSelectedPolyline();
            active_parametric_object_ = {};
            UpdateActiveToolUi("select");
            viewport_->SetTool(ToolMode::Select);
            statusBar()->showMessage("Revolve:operation failed; check the selected geometry and parameters");
            return;
        }
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage(document_.HasLivePolylineRevolve()
            ? "Revolve:adjust the required parameters and continue"
            : "Revolve:select the required geometry and continue");
        return;
    }

    if (tool_id == "SurfaceFourSplines") {
        ClearActiveProperties();
        CreateFourSplineSurface();
        return;
    }

    if (tool_id == "SurfaceLoft") {
        ClearActiveProperties();
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionMode(SelectionMode::Object);
        UpdateActiveToolUi(tool_id);
        if (!document_.CreateLoftSurfaceFromSelectedBSplines()) {
            UpdateActiveToolUi("select");
            statusBar()->showMessage("Loft Surface:select the required geometry and continue", 1800);
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
            statusBar()->showMessage("Reverse Normals:select the required geometry and continue", 1800);
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
            {{"distance", "Distance", 2.0, 0.01, 100.0, 0.1}}
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
            statusBar()->showMessage("Chamfer:operation failed; check the selected geometry and parameters");
            return;
        }
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage(document_.HasLiveChamfer()
            ? "Chamfer:adjust the required parameters and continue"
            : "Chamfer:select the required geometry and continue");
        return;
    }

    if (tool_id == "fillet_edge" || tool_id == "fillet_all_edges") {
        ClearActiveProperties();
        const bool selected_edge = document_.HasSelectedSolidEdge();
        const bool selected_body = document_.GetSelectedSolid() != nullptr;
        const bool all_edges = tool_id == "fillet_all_edges"
            || (tool_id == "fillet_edge" && selected_body && !selected_edge);
        if (!selected_edge && !selected_body) {
            UpdateActiveToolUi("select");
            viewport_->SetTool(ToolMode::Select);
            statusBar()->showMessage("Fillet Solid: select an edge or a body");
            return;
        }
        const std::string operation_tool_id = all_edges
            ? "fillet_all_edges" : "fillet_edge";
        const ToolDefinition* definition = tool_registry_.Find("fillet_edge");
        active_parametric_object_ = {
            operation_tool_id,
            document_.GetSelectedObjectIndex(),
            0,
            definition ? definition->defaults : std::vector<ToolParameter>{}
        };
        property_panel_->SetActiveObject(active_parametric_object_);
        ShowPropertyPanelAtCursor("Fillet Solid");
        viewport_->SetTool(ToolMode::SolidFillet);
        if (!all_edges) {
            viewport_->SetSelectionMode(SelectionMode::Edge);
        }
        UpdateActiveToolUi("fillet_edge");

        const bool should_start_now = all_edges || document_.HasSelectedSolidEdge();
        if (all_edges) {
            undo_redo_.BeginChange();
        }
        const bool started = all_edges
            ? document_.BeginLiveFilletSelectedEdges(all_edges)
            : TryStartLiveEdgeToolFromSelection();
        if (should_start_now && (!started || !document_.HasLiveFillet())) {
            document_.CancelLiveFillet();
            undo_redo_.CancelChange();
            active_parametric_object_ = {};
            UpdateActiveToolUi("select");
            viewport_->SetTool(ToolMode::Select);
            statusBar()->showMessage("Fillet:operation failed; check the selected geometry and parameters");
            return;
        }
        if (all_edges) {
            ScheduleLiveFilletRebuild(true);
        }
        if (document_.HasLiveFillet()) {
            if (CSolid* solid = document_.GetSelectedSolid()) {
                solid->ClearSelectedEdge();
                solid->ClearSelectedFace();
            }
            document_.SetObjectSelectionHighlightHidden(
                document_.GetSelectedObjectIndex(), true);
            viewport_->SetSolidDimensionEdit(
                active_parametric_object_,
                FilletPrimaryParameter(active_parametric_object_.parameters));
        }
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage(document_.HasLiveFillet()
            ? (all_edges ? "Fillet Solid: all edges; adjust the radius"
                         : "Fillet Solid: adjust the radius")
            : "Fillet:select the required geometry and continue");
        return;
    }

    if (IsCabinetTool(tool_id)) {
        const ToolDefinition* cabinet_tool = tool_registry_.Find(tool_id);
        std::vector<ToolParameter> parameters = cabinet_tool
            ? cabinet_tool->defaults
            : std::vector<ToolParameter>{};
        QSettings settings;
        settings.beginGroup(
            QStringLiteral("tools/%1/parameters")
                .arg(QString::fromStdString(tool_id)));
        for (ToolParameter& parameter : parameters) {
            const QString key = QString::fromStdString(parameter.id);
            if (settings.contains(key)) {
                parameter.value = std::clamp(
                    settings.value(key).toDouble(),
                    parameter.minimum,
                    parameter.maximum);
            }
        }
        settings.endGroup();

        if (tool_id == "cabinet_advanced_slx") {
            std::vector<CSmartLine*> sketches;
            for (size_t index : document_.GetSelectedObjectIndices()) {
                if (index >= document_.GetObjects().size()) {
                    continue;
                }
                if (auto* sketch = dynamic_cast<CSmartLine*>(
                        document_.GetObjects()[index].get())) {
                    sketches.push_back(sketch);
                }
            }
            const auto set_sketch_id = [&](const std::string& id, unsigned long value) {
                const auto parameter = std::find_if(
                    parameters.begin(), parameters.end(),
                    [&](const ToolParameter& candidate) { return candidate.id == id; });
                if (parameter != parameters.end()) {
                    parameter->value = static_cast<double>(value);
                }
            };
            const auto facade_style = std::find_if(
                parameters.begin(), parameters.end(),
                [](const ToolParameter& parameter) {
                    return parameter.id == "facade_style";
                });
            // Never inherit object IDs from an earlier SLX run.
            set_sketch_id("slx.profile.id", 0);
            set_sketch_id("slx.panel.id", 0);
            set_sketch_id("slx.milling.profile.id", 0);
            set_sketch_id("slx.milling.guide.id", 0);
            set_sketch_id("slx.handle.id", 0);
            for (size_t contour = 0; contour < 8; ++contour) {
                set_sketch_id(
                    "slx.contour." + std::to_string(contour + 1) + ".id", 0);
            }

            if (!sketches.empty()) {
                for (CSmartLine* sketch : sketches) {
                    document_.EnsureObjectId(*sketch);
                }
            }
            if (sketches.size() == 2) {
                if (!sketches[0]->IsClosed() || !sketches[1]->IsClosed()) {
                    statusBar()->showMessage(
                        "SLX Frame:operation failed; check the selected geometry and parameters",
                        5000);
                    return;
                }
                // Two sketches can be used either as Frame Profile + Panel
                // Profile or as two Milled contours.  Populate both sets;
                // the facade style selected in the dialog decides which set
                // the builder consumes.
                set_sketch_id("slx.profile.id", sketches[0]->m_id);
                set_sketch_id("slx.panel.id", sketches[1]->m_id);
                set_sketch_id("slx.contour.1.id", sketches[0]->m_id);
                set_sketch_id("slx.contour.2.id", sketches[1]->m_id);
                if (facade_style != parameters.end()) {
                    facade_style->value = 1.0;
                }
            } else if (!sketches.empty()) {
                if (facade_style != parameters.end()) {
                    facade_style->value = 3.0;
                }
                const size_t contour_count = std::min<size_t>(8, sketches.size());
                for (size_t contour = 0; contour < contour_count; ++contour) {
                    if (!sketches[contour]->IsClosed()) {
                        statusBar()->showMessage(
                            "SLX Milled:operation failed; check the selected geometry and parameters",
                            4500);
                        return;
                    }
                    set_sketch_id(
                        "slx.contour." + std::to_string(contour + 1) + ".id",
                        sketches[contour]->m_id);
                }
            }
        }
        ToolParameter* body_type = nullptr;
        ToolParameter* facade_type = nullptr;
        for (ToolParameter& parameter : parameters) {
            if (parameter.id == "body_type") {
                body_type = &parameter;
            } else if (parameter.id == "facade_type") {
                facade_type = &parameter;
            }
        }
        if (body_type && facade_type
            && static_cast<int>(body_type->value) == 1
            && static_cast<int>(facade_type->value) == 1) {
            facade_type->value = 2.0;
        }
        active_parametric_object_ = tool_registry_.PrepareParametricObject(
            tool_id, document_, parameters);
        active_cabinet_parameters_dirty_ = true;
    } else if (tool_id != "SurfaceRuled" && tool_id != "SolidShell") {
        if (IsFurnitureAssemblyTool(tool_id)) {
            undo_redo_.BeginChange();
        }
        active_parametric_object_ = tool_registry_.Activate(tool_id, document_);
        if (IsFurnitureAssemblyTool(tool_id)
            && active_parametric_object_.tool_id.empty()) {
            undo_redo_.CancelChange();
        }
    }

    if (tool_id == "SurfaceSweepTwoRails") {
        ClearActiveProperties();
        CreateTwoRailSweepSurface();
        return;
    }

    if (tool_id == "SurfaceJoin") {
        ClearActiveProperties();
        JoinSelectedSurfaces();
        return;
    }

    if (tool_id == "SurfaceRuled") {
        ClearActiveProperties();
        auto* spline = document_.GetSelectedBSpline();
        if (!spline || spline->GetPointCount() < 2) {
            UpdateActiveToolUi("select");
            statusBar()->showMessage(
                "Ruled Surface:select the required geometry and continue", 2200);
            return;
        }
        document_.EnsureObjectId(*spline);
        const ToolDefinition* definition = tool_registry_.Find(tool_id);
        std::vector<ToolParameter> parameters = definition
            ? definition->defaults : std::vector<ToolParameter>{};
        for (ToolParameter& parameter : parameters) {
            if (parameter.id == "profile.id") {
                parameter.value = static_cast<double>(spline->m_id);
            }
        }
        active_parametric_object_ = tool_registry_.CreateParametricObject(
            tool_id, document_, parameters);
        if (active_parametric_object_.tool_id.empty()) {
            UpdateActiveToolUi("select");
            statusBar()->showMessage("Ruled Surface:operation failed; check the selected geometry and parameters", 2000);
            return;
        }
        property_panel_->SetActiveObject(active_parametric_object_);
        ShowPropertyPanelAtCursor("Ruled Surface");
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionMode(SelectionMode::Object);
        UpdateActiveToolUi(tool_id);
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage(
            "Ruled Surface:adjust the required parameters and continue");
        return;
    }
    if (tool_id == "SolidShell") {
        ClearActiveProperties();
        CSolid* source = document_.HasSelectedSolidFace()
            ? document_.GetSelectedFaceSolid()
            : document_.GetSelectedSolid();
        int face_index = source && source->HasSelectedFace()
            ? source->GetSelectedFaceIndex() : -1;
        if (source && face_index < 0 && source->GetNumSurfaces() == 1) {
            face_index = 0;
        }
        if (!source || face_index < 0 || !source->GetSurfaceFace(face_index)) {
            UpdateActiveToolUi("select");
            statusBar()->showMessage(
                "Shell:select one surface face and continue", 2400);
            return;
        }

        document_.EnsureObjectId(*source);
        const ToolDefinition* definition = tool_registry_.Find(tool_id);
        std::vector<ToolParameter> parameters = definition
            ? definition->defaults : std::vector<ToolParameter>{};
        for (ToolParameter& parameter : parameters) {
            if (parameter.id == "surface.id") {
                parameter.value = static_cast<double>(source->m_id);
            } else if (parameter.id == "face.index") {
                parameter.value = static_cast<double>(face_index);
            }
        }
        active_parametric_object_ = tool_registry_.CreateParametricObject(
            tool_id, document_, parameters);
        if (active_parametric_object_.tool_id.empty()) {
            UpdateActiveToolUi("select");
            statusBar()->showMessage(
                "Shell:operation failed; check the surface and distance", 2600);
            return;
        }
        property_panel_->SetActiveObject(active_parametric_object_);
        ShowPropertyPanelAtCursor("Shell");
        viewport_->SetTool(ToolMode::Select);
        viewport_->SetSelectionMode(SelectionMode::Object);
        UpdateActiveToolUi(tool_id);
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage(
            "Shell:adjust the distance and continue");
        return;
    }
    if (!active_parametric_object_.tool_id.empty()) {
        property_panel_->SetActiveObject(active_parametric_object_);
        ShowPropertyPanelAtCursor(QString::fromStdString(tool_registry_.LabelFor(tool_id)));
        if (ActiveParametricObjectIsAssembly() || IsCabinetTool(tool_id)
            || tool_id == "SolidBeamTool" || tool_id == "SolidBox" || tool_id == "SolidCylinder" || tool_id == "SolidSphereTool"
            || tool_id == "SolidTorusTool" || tool_id == "SolidPrismTool") {
            document_.ClearSelection();
        }
        if (tool_id == "SolidPrismTool") {
            viewport_->SetSolidDimensionEdit(
                active_parametric_object_, QStringLiteral("height"));
        } else if (IsCabinetTool(tool_id)) {
            viewport_->SetSolidDimensionEdit(active_parametric_object_);
            viewport_->SetCabinetPreviewVisible(true);
        }
    } else {
        ClearActiveProperties();
    }
    viewport_->SetTool(
        IsFurnitureAssemblyTool(active_parametric_object_.tool_id)
            ? ToolMode::Orbit
            : ToolMode::Select);
    if ((tool_id == "room" || tool_id == "window" || tool_id == "door"
         || tool_id == "chair" || tool_id == "chair_simple" || tool_id == "table" || tool_id == "desk" || tool_id == "drawer_box" || tool_id == "single_drawer" || tool_id == "single_facade" || tool_id == "kitchen_nika_260" || tool_id == "kitchen_corner")
        && !active_parametric_object_.tool_id.empty()) {
        viewport_->FitToDocument();
    }
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
        statusBar()->showMessage("Low Poly:select the required geometry and continue");
        return;
    }

    const SolidDisplayMode previous_display_mode = CSolid::GetDisplayMode();
    SetSolidDisplayMode(SolidDisplayMode::SurfacesAndRaisedMesh);

    low_poly_pick_pending_ = false;
    ClearActiveProperties();
    viewport_->SetTool(ToolMode::Select);
    viewport_->SetSelectionMode(SelectionMode::Object);
    UpdateActiveToolUi("SolidLowPoly");
    connect(dialog, &QDialog::finished, this,
            [this, previous_display_mode]() {
        SetSolidDisplayMode(previous_display_mode);
        low_poly_pick_pending_ = false;
        ClearActiveProperties();
        viewport_->SetTool(ToolMode::Orbit);
        UpdateActiveToolUi("orbit");
        viewport_->update();
        statusBar()->showMessage("Orbit camera", 900);
    });
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    statusBar()->showMessage("Low Poly:adjust the required parameters and continue", 1600);
}

void MainWindow::ShowMeshFillContourTool() {
    ClearActiveProperties();
    viewport_->SetTool(ToolMode::Select);
    viewport_->SetSelectionMode(SelectionMode::Object);
    UpdateActiveToolUi("MeshFillContour");

    auto* dialog = new MeshFillContourDialog(
        document_,
        [this]() {
            RefreshSceneTree();
            viewport_->update();
        },
        [this](const QString& text) {
            statusBar()->showMessage(text, 1800);
        },
        this);

    if (!dialog->HasContours()) {
        dialog->deleteLater();
        statusBar()->showMessage("Fiill Contour:operation status", 2000);
        return;
    }

    connect(dialog, &QDialog::finished, this, [this]() {
        UpdateActiveToolUi("select");
        viewport_->SetTool(ToolMode::Select);
        viewport_->update();
        statusBar()->showMessage("Select objects", 900);
    });
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    statusBar()->showMessage("Fiill Contour:select the required geometry and continue", 1800);
}

void MainWindow::ShowTrimMeshTestTool() {
    ClearActiveProperties();
    viewport_->SetTool(ToolMode::Select);
    viewport_->SetSelectionMode(SelectionMode::Object);
    UpdateActiveToolUi("TrimMeshTest");

    auto* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setWindowTitle("Trim Mesh Test");

    auto mesh_id = std::make_shared<unsigned long>(0);
    auto line_id = std::make_shared<unsigned long>(0);

    auto* root = new QVBoxLayout(dialog);
    auto* selection_form = new QFormLayout();
    auto* mesh_label = new QLabel("none", dialog);
    auto* line_label = new QLabel("none", dialog);
    selection_form->addRow("Mesh", mesh_label);
    selection_form->addRow("Cut line", line_label);
    root->addLayout(selection_form);

    auto* select_mesh_button = new QPushButton("Use Selected Mesh", dialog);
    auto* select_line_button = new QPushButton("Use Selected Line", dialog);
    root->addWidget(select_mesh_button);
    root->addWidget(select_line_button);

    auto* pc_box = new QGroupBox("Pc", dialog);
    auto* pc_form = new QFormLayout(pc_box);
    const auto make_spin = [dialog]() {
        auto* spin = new QDoubleSpinBox(dialog);
        spin->setRange(-1000000.0, 1000000.0);
        spin->setDecimals(6);
        spin->setSingleStep(0.1);
        spin->setKeyboardTracking(true);
        return spin;
    };
    auto* pc_x = make_spin();
    auto* pc_y = make_spin();
    auto* pc_z = make_spin();
    pc_form->addRow("X", pc_x);
    pc_form->addRow("Y", pc_y);
    pc_form->addRow("Z", pc_z);
    root->addWidget(pc_box);

    auto* buttons = new QHBoxLayout();
    auto* pick_pc_button = new QPushButton("Pick Pc", dialog);
    auto* run_button = new QPushButton("Run Trim Test", dialog);
    auto* close_button = new QPushButton("Close", dialog);
    buttons->addWidget(pick_pc_button);
    buttons->addWidget(run_button);
    buttons->addStretch();
    buttons->addWidget(close_button);
    root->addLayout(buttons);

    const auto describe_object = [](const CAlfaObject* object) {
        if (!object) {
            return QString("none");
        }
        const QString name = QString::fromStdString(object->GetName());
        return name.isEmpty() ? QString("ID %1").arg(object->m_id) : QString("%1 (ID %2)").arg(name).arg(object->m_id);
    };

    connect(select_mesh_button, &QPushButton::clicked, dialog, [this, mesh_id, mesh_label, describe_object]() {
        CMesh3D* mesh = document_.GetSelectedMesh();
        if (!mesh) {
            statusBar()->showMessage("Trim Mesh Test:select the required geometry and continue", 1400);
            return;
        }
        document_.EnsureObjectId(*mesh);
        *mesh_id = mesh->m_id;
        mesh_label->setText(describe_object(mesh));
        statusBar()->showMessage("Trim Mesh Test: mesh select the required geometry and continue", 1600);
    });

    connect(select_line_button, &QPushButton::clicked, dialog, [this, line_id, line_label, describe_object]() {
        CPolyline* line = document_.GetSelectedPolyline();
        if (!line) {
            statusBar()->showMessage("Trim Mesh Test:select the required geometry and continue", 1400);
            return;
        }
        document_.EnsureObjectId(*line);
        *line_id = line->m_id;
        line_label->setText(describe_object(line));
        statusBar()->showMessage("Trim Mesh Test: line adjust the required parameters and continue", 1600);
    });

    connect(pick_pc_button, &QPushButton::clicked, dialog, [this]() {
        viewport_->BeginPickXYPoint();
        viewport_->setFocus();
    });
    connect(viewport_, &OpenGLViewport::XYPointPicked, dialog, [pc_x, pc_y, pc_z](CPoint3d point) {
        pc_x->setValue(point.x);
        pc_y->setValue(point.y);
        pc_z->setValue(point.z);
    });

    connect(run_button, &QPushButton::clicked, dialog, [this, mesh_id, line_id, pc_x, pc_y, pc_z]() {
        auto* mesh = dynamic_cast<CMesh3D*>(document_.FindObjectById(*mesh_id));
        auto* line = dynamic_cast<CPolyline*>(document_.FindObjectById(*line_id));
        if (!mesh || !line) {
            statusBar()->showMessage("Trim Mesh Test:select the required geometry and continue", 1600);
            return;
        }

        CPoint3d pc(pc_x->value(), pc_y->value(), pc_z->value());
        const bool changed = mesh->TrimByPlineTest(line, pc);
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage(changed ? "Trim Mesh Test:operation completed" : "Trim Mesh Test:operation failed; check the selected geometry and parameters", 1800);
    });
    connect(close_button, &QPushButton::clicked, dialog, &QDialog::accept);

    CenterDialogOnCursor(*dialog);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    statusBar()->showMessage("Trim Mesh Test:select the required geometry and continue", 1800);
}

void MainWindow::ShowClassifyFaceCutTool() {
    ClearActiveProperties();
    viewport_->SetTool(ToolMode::Select);
    viewport_->SetSelectionMode(SelectionMode::Object);
    UpdateActiveToolUi("ClassifyFaceCut");

    auto* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setWindowTitle("Classify Face Cut");

    auto face_id = std::make_shared<unsigned long>(0);
    auto cut_id = std::make_shared<unsigned long>(0);

    auto* root = new QVBoxLayout(dialog);
    auto* selection_form = new QFormLayout();
    auto* face_label = new QLabel("none", dialog);
    auto* cut_label = new QLabel("none", dialog);
    selection_form->addRow("Plface", face_label);
    selection_form->addRow("PlCut", cut_label);
    root->addLayout(selection_form);

    auto* select_face_button = new QPushButton("Use Selected Plface", dialog);
    auto* select_cut_button = new QPushButton("Use Selected PlCut", dialog);
    root->addWidget(select_face_button);
    root->addWidget(select_cut_button);

    auto* buttons = new QHBoxLayout();
    auto* test_button = new QPushButton("DoTest", dialog);
    auto* close_button = new QPushButton("Close", dialog);
    buttons->addWidget(test_button);
    buttons->addStretch();
    buttons->addWidget(close_button);
    root->addLayout(buttons);

    const auto describe_object = [](const CAlfaObject* object) {
        if (!object) {
            return QString("none");
        }
        const QString name = QString::fromStdString(object->GetName());
        return name.isEmpty() ? QString("ID %1").arg(object->m_id) : QString("%1 (ID %2)").arg(name).arg(object->m_id);
    };

    connect(select_face_button, &QPushButton::clicked, dialog, [this, face_id, face_label, describe_object]() {
        CPolyline* face = document_.GetSelectedPolyline();
        if (!face) {
            statusBar()->showMessage("Classify Face Cut:select the required geometry and continue", 1500);
            return;
        }
        document_.EnsureObjectId(*face);
        *face_id = face->m_id;
        face_label->setText(describe_object(face));
        statusBar()->showMessage("Classify Face Cut: Plface select the required geometry and continue", 1600);
    });

    connect(select_cut_button, &QPushButton::clicked, dialog, [this, cut_id, cut_label, describe_object]() {
        CPolyline* cut = document_.GetSelectedPolyline();
        if (!cut) {
            statusBar()->showMessage("Classify Face Cut:select the required geometry and continue", 1500);
            return;
        }
        document_.EnsureObjectId(*cut);
        *cut_id = cut->m_id;
        cut_label->setText(describe_object(cut));
        statusBar()->showMessage("Classify Face Cut: PlCut operation status", 1600);
    });

    connect(test_button, &QPushButton::clicked, dialog, [this, face_id, cut_id]() {
        auto* face = dynamic_cast<CPolyline*>(document_.FindObjectById(*face_id));
        auto* cut = dynamic_cast<CPolyline*>(document_.FindObjectById(*cut_id));
        if (!face || !cut) {
            statusBar()->showMessage("Classify Face Cut:select the required geometry and continue", 1800);
            return;
        }
        if (face == cut) {
            statusBar()->showMessage("Classify Face Cut: Plface operation failed; check the selected geometry and parameters", 1800);
            return;
        }
        if (face->np() < 3 || cut->np() < 2) {
            statusBar()->showMessage("Classify Face Cut: Plface operation failed; check the selected geometry and parameters", 2000);
            return;
        }

        DoTest(face, cut);
        viewport_->update();
        statusBar()->showMessage("Classify Face Cut: DoTest operation completed", 1800);
    });
    connect(close_button, &QPushButton::clicked, dialog, &QDialog::accept);

    CenterDialogOnCursor(*dialog);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    statusBar()->showMessage("Classify Face Cut:select the required geometry and continue", 1900);
}

void MainWindow::UpdateSolidBodyDimensions() {
    if (!solid_body_edit_mode_
        || solid_body_edit_object_index_ >= document_.GetObjects().size()) {
        viewport_->ClearSolidDimensionEdit();
        return;
    }
    auto* solid = dynamic_cast<CSolid*>(
        document_.GetObjects()[solid_body_edit_object_index_].get());
    if (!solid) {
        viewport_->ClearSolidDimensionEdit();
        return;
    }

    const auto supports_dimensions = [](const std::string& tool_id) {
        return tool_id == "SolidBox"
            || tool_id == "SolidCylinder"
            || tool_id == "SolidPrismTool"
            || tool_id == "fillet_edge"
            || tool_id == "fillet_all_edges";
    };
    std::vector<ActiveParametricObject> dimension_objects;
    for (int operation_index = 0;
         operation_index < solid->GetNumOperations();
         ++operation_index) {
        if (!solid_body_all_dimensions_
            && operation_index != solid_body_selected_operation_index_) {
            continue;
        }
        ActiveParametricObject active_object =
            tool_registry_.ActiveObjectFromDocument(
                solid_body_edit_object_index_,
                *solid,
                static_cast<size_t>(operation_index),
                &document_);
        if (supports_dimensions(active_object.tool_id)) {
            dimension_objects.push_back(std::move(active_object));
        }
    }
    if (dimension_objects.empty()) {
        viewport_->ClearSolidDimensionEdit();
        return;
    }
    viewport_->SetSolidDimensionEdits(
        dimension_objects,
        static_cast<size_t>(solid_body_selected_operation_index_));
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
                    && operation->ToolId != "SolidSketchFeature"
                    && operation->ToolId != "SolidExtrudeFace"
                    && operation->ToolId != "SolidOffsetFace"
                    && operation->ToolId != "SolidDraft"
                    && operation->ToolId != "SolidSheetBend"
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
            const ActiveParametricObject initial_body_dimension_object =
                tool_registry_.ActiveObjectFromDocument(
                    edited_object_index, *solid, 0, &document_);
            std::vector<ActiveParametricObject> initial_dimension_objects;
            for (int index = 0; index < solid->GetNumOperations(); ++index) {
                ActiveParametricObject initial_object =
                    tool_registry_.ActiveObjectFromDocument(
                        edited_object_index, *solid,
                        static_cast<size_t>(index), &document_);
                if (initial_object.tool_id == "SolidBox"
                    || initial_object.tool_id == "SolidCylinder"
                    || initial_object.tool_id == "SolidPrismTool"
                    || initial_object.tool_id == "fillet_edge"
                    || initial_object.tool_id == "fillet_all_edges") {
                    initial_dimension_objects.push_back(std::move(initial_object));
                }
            }
            solid_body_dimension_object_ = initial_body_dimension_object;
            solid_body_edit_object_index_ = edited_object_index;
            solid_body_selected_operation_index_ = 0;
            solid_body_all_dimensions_ = false;
            solid_body_edit_mode_ =
                solid_body_dimension_object_.tool_id == "SolidBox"
                || solid_body_dimension_object_.tool_id == "SolidCylinder"
                || solid_body_dimension_object_.tool_id == "SolidPrismTool";
            solid_body_dimensions_modified_ = false;
            if (solid_body_edit_mode_) {
                UpdateSolidBodyDimensions();
                statusBar()->showMessage(
                    "Body edit: click a dimension label to change the size");
            }
            document_.SetObjectSelectionHighlightHidden(edited_object_index, true);
            viewport_->update();
            undo_redo_.BeginChange();
            const SolidOperationsDialogResult operation_action = ShowSolidOperationsDialog(
                this,
                document_,
                edited_object_index,
                tool_registry_,
                [this]() {
                    RefreshSceneTree();
                    viewport_->update();
                },
                [this](int selected_operation_index, bool all_dimensions) {
                    solid_body_selected_operation_index_ = selected_operation_index;
                    solid_body_all_dimensions_ = all_dimensions;
                    UpdateSolidBodyDimensions();
                });
            const bool body_dimensions_were_active = solid_body_edit_mode_;
            const bool body_dimensions_were_modified = solid_body_dimensions_modified_;
            solid_body_edit_mode_ = false;
            solid_body_dimensions_modified_ = false;
            solid_body_dimension_object_ = {};
            solid_body_edit_object_index_ = static_cast<size_t>(-1);
            solid_body_selected_operation_index_ = 0;
            solid_body_all_dimensions_ = false;
            viewport_->ClearSolidDimensionEdit();
            object = edited_object_index < document_.GetObjects().size()
                ? document_.GetObjects()[edited_object_index].get()
                : nullptr;
            solid = dynamic_cast<CSolid*>(object);
            document_.SetObjectSelectionHighlightHidden(edited_object_index, false);
            viewport_->update();
            if (!solid) {
                return;
            }
            const auto finish_solid_edit = [this]() {
                document_.ClearSelection();
                viewport_->ClearSolidDimensionEdit();
                RefreshSceneTree();
                viewport_->update();
            };
            if (operation_action.action == SolidOperationsDialogAction::None) {
                if (body_dimensions_were_active && body_dimensions_were_modified) {
                    for (const ActiveParametricObject& initial_object
                         : initial_dimension_objects) {
                        tool_registry_.Rebuild(initial_object, document_);
                    }
                }
                undo_redo_.CancelChange();
                finish_solid_edit();
                return;
            }
            if (!operation_action.object_name.empty()) {
                solid->SetName(operation_action.object_name);
            }
            if (operation_action.action == SolidOperationsDialogAction::Accept) {
                undo_redo_.CommitChange("Edit solid");
                UpdateUndoRedoActions();
                finish_solid_edit();
                statusBar()->showMessage("Solid changes accepted", 1200);
                return;
            }
            if (operation_action.action == SolidOperationsDialogAction::Delete) {
                const size_t object_index = document_.GetSelectedObjectIndex();
                const ParametricFunction* selected_operation =
                    solid->GetOperation(operation_action.operation_index);
                if (selected_operation
                    && selected_operation->ToolId == "SurfaceFilmCoating") {
                    const std::vector<int> coated_surfaces =
                        selected_operation->CreatedSurfaceIndices;
                    for (int surface_index : coated_surfaces) {
                        solid->ClearSurfaceCoating(surface_index);
                    }
                    if (!solid->RemoveParametricOperation(
                            static_cast<size_t>(operation_action.operation_index))) {
                        undo_redo_.CancelChange();
                        statusBar()->showMessage("Film operation delete failed", 1400);
                        return;
                    }
                    undo_redo_.CommitChange("Delete Oracal film");
                    UpdateUndoRedoActions();
                    finish_solid_edit();
                    statusBar()->showMessage("Oracal film removed", 1200);
                    return;
                }
                if (!solid->RemoveParametricOperation(static_cast<size_t>(operation_action.operation_index))
                    || !tool_registry_.ReplayOperations(object_index, document_)) {
                    undo_redo_.CancelChange();
                    statusBar()->showMessage("Operation delete failed", 1400);
                    return;
                }
                undo_redo_.CommitChange("Delete solid operation");
                UpdateUndoRedoActions();
                finish_solid_edit();
                statusBar()->showMessage("Operation deleted", 1200);
                return;
            }
            const ParametricFunction* selected_operation =
                solid->GetOperation(operation_action.operation_index);
            if (selected_operation
                && selected_operation->ToolId == "SurfaceFilmCoating") {
                undo_redo_.CancelChange();
                finish_solid_edit();
                ShowSurfaceFilmDialog(
                    edited_object_index, operation_action.operation_index);
                return;
            }
            undo_redo_.CancelChange();
            operation_index = static_cast<size_t>(operation_action.operation_index);
            reopen_solid_editor_after_properties_ = true;
        }
    }

    active_parametric_object_ = tool_registry_.ActiveObjectFromDocument(
        document_.GetSelectedObjectIndex(), *object, operation_index, &document_);
    if (active_parametric_object_.tool_id.empty()) {
        reopen_solid_editor_after_properties_ = false;
        statusBar()->showMessage("Saved parametric tool is not available", 1400);
        return;
    }

    active_parametric_edit_existing_ = true;
    active_cabinet_parameters_dirty_ = false;
    property_panel_->SetActiveObject(active_parametric_object_);
    viewport_->SetSolidDimensionEdit(active_parametric_object_);
    viewport_->SetCabinetPreviewVisible(false);
    if (ActiveParametricObjectIsAssembly()) {
        document_.ClearSelection();
    }
    const QString tool_label = QString::fromStdString(tool_registry_.LabelFor(active_parametric_object_.tool_id));
    ShowPropertyPanelAtCursor(tool_label);
    if (ActiveParametricObjectIsAssembly()) {
        viewport_->SetTool(ToolMode::Orbit);
    } else if (viewport_->CurrentTool() != ToolMode::Orbit) {
        const bool editing_fillet = active_parametric_object_.tool_id == "fillet_edge"
            || active_parametric_object_.tool_id == "fillet_all_edges";
        viewport_->SetTool(editing_fillet ? ToolMode::SolidFillet : ToolMode::Select);
        if (editing_fillet) {
            if (CSolid* solid = document_.GetSelectedSolid()) {
                solid->ClearSelectedEdge();
                solid->ClearSelectedFace();
            }
            document_.SetObjectSelectionHighlightHidden(
                document_.GetSelectedObjectIndex(), true);
        }
    }
    UpdateActiveToolUi(active_parametric_object_.tool_id);
    statusBar()->showMessage(QString("%1 parameters").arg(tool_label), 1200);
}

bool MainWindow::ActiveParametricObjectIsAssembly() const {
    const size_t object_index = active_parametric_object_.object_index;
    const auto& objects = document_.GetObjects();
    return object_index < objects.size()
        && dynamic_cast<const CAssembled*>(objects[object_index].get());
}

bool MainWindow::TryStartLiveEdgeToolFromSelection() {
    if (active_parametric_object_.tool_id == "fillet_edge") {
        if (document_.HasLiveFillet()) {
            return true;
        }
        if (!document_.HasSelectedSolidEdge()) {
            statusBar()->showMessage("Fillet:select the required geometry and continue");
            return false;
        }
        undo_redo_.BeginChange();
        if (!document_.BeginLiveFilletSelectedEdges(false)) {
            document_.CancelLiveFillet();
            undo_redo_.CancelChange();
            statusBar()->showMessage("Fillet:operation failed; check the selected geometry and parameters");
            return false;
        }
        ScheduleLiveFilletRebuild(true);
        viewport_->SetSolidDimensionEdit(
            active_parametric_object_,
            FilletPrimaryParameter(active_parametric_object_.parameters));
        if (CSolid* solid = document_.GetSelectedSolid()) {
            solid->ClearSelectedEdge();
            solid->ClearSelectedFace();
        }
        document_.SetObjectSelectionHighlightHidden(
            document_.GetSelectedObjectIndex(), true);
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage("Fillet:adjust the required parameters and continue");
        return true;
    }

    if (active_parametric_object_.tool_id == "ChamferSolid") {
        if (document_.HasLiveChamfer()) {
            return true;
        }
        if (!document_.HasSelectedSolidEdge()) {
            statusBar()->showMessage("Chamfer:select the required geometry and continue");
            return false;
        }
        const double distance = active_parametric_object_.parameters.empty() ? 2.0 : active_parametric_object_.parameters[0].value;
        undo_redo_.BeginChange();
        if (!document_.BeginLiveChamferSelectedEdges() || !document_.UpdateLiveChamfer(distance)) {
            document_.CancelLiveChamfer();
            undo_redo_.CancelChange();
            statusBar()->showMessage("Chamfer:operation failed; check the selected geometry and parameters");
            return false;
        }
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage("Chamfer:adjust the required parameters and continue");
        return true;
    }

    return false;
}

void MainWindow::ScheduleLiveFilletRebuild(bool immediate) {
    if (!document_.HasLiveFillet()) {
        return;
    }
    ++live_fillet_build_generation_;

    const FilletRadiusValues radii =
        FilletRadii(active_parametric_object_.parameters);
    if (radii.mode == 0) {
        // Constant radius is the overwhelmingly common interactive case and
        // was real-time before radius laws were introduced.  Do not put it
        // through the 280 ms debounce/deep-copy/background pipeline.
        live_fillet_build_pending_ = false;
        if (live_fillet_update_timer_) {
            live_fillet_update_timer_->stop();
        }
        if (document_.UpdateLiveFillet(radii.start)) {
            viewport_->SetSolidDimensionEdit(
                active_parametric_object_,
                FilletPrimaryParameter(active_parametric_object_.parameters));
            RefreshSceneTree();
            viewport_->update();
        } else {
            statusBar()->showMessage(
                "Fillet: operation failed; check radius and geometry", 1800);
        }
        return;
    }

    live_fillet_build_pending_ = true;
    if (!live_fillet_update_timer_) {
        return;
    }
    live_fillet_update_timer_->start(immediate ? 0 : 280);
}

void MainWindow::StartPendingLiveFilletRebuild() {
    if (!live_fillet_build_pending_ || !document_.HasLiveFillet()) {
        return;
    }
    if (live_fillet_build_running_) {
        return;
    }

    CAlfaDoc::LiveFilletBuildRequest request;
    if (!document_.CreateLiveFilletBuildRequest(request)) {
        live_fillet_build_pending_ = false;
        return;
    }
    const std::vector<double> radius_law =
        FilletLaw(active_parametric_object_.parameters);
    const unsigned long long generation = live_fillet_build_generation_;
    live_fillet_build_pending_ = false;
    live_fillet_build_running_ = true;
    statusBar()->showMessage("Fillet: calculating preview in background...");

    QPointer<MainWindow> window(this);
    QThreadPool::globalInstance()->start(
        [window, request, radius_law, generation]() mutable {
            TopoDS_Shape result_shape;
            std::vector<int> created_surface_indices;
            const bool success = CAlfaDoc::BuildLiveFilletShape(
                request, radius_law, result_shape, created_surface_indices);
            if (!window) {
                return;
            }
            QMetaObject::invokeMethod(
                window,
                [window, request, generation, success,
                 result_shape = std::move(result_shape),
                 created_surface_indices = std::move(created_surface_indices)]() mutable {
                    if (!window) {
                        return;
                    }
                    window->live_fillet_build_running_ = false;
                    const bool current = generation
                        == window->live_fillet_build_generation_;
                    bool applied = false;
                    if (current && success && window->document_.HasLiveFillet()) {
                        applied = window->document_.ApplyLiveFilletShape(
                            request, result_shape,
                            std::move(created_surface_indices));
                    }
                    if (applied) {
                        window->viewport_->SetSolidDimensionEdit(
                            window->active_parametric_object_,
                            FilletPrimaryParameter(
                                window->active_parametric_object_.parameters));
                        window->RefreshSceneTree();
                        window->viewport_->update();
                        window->statusBar()->showMessage(
                            "Fillet: preview ready", 1200);
                    } else if (current && !success) {
                        window->statusBar()->showMessage(
                            "Fillet: operation failed; check radii and geometry",
                            2200);
                    }
                    if (window->live_fillet_build_pending_
                        && window->document_.HasLiveFillet()) {
                        window->live_fillet_update_timer_->start(0);
                    }
                },
                Qt::QueuedConnection);
        });
}

void MainWindow::InvalidateLiveFilletRebuild() {
    ++live_fillet_build_generation_;
    live_fillet_build_pending_ = false;
    if (live_fillet_update_timer_) {
        live_fillet_update_timer_->stop();
    }
}

bool MainWindow::TryStartLivePolylineExtrudeFromSelection() {
    if (active_parametric_object_.tool_id != "SolidExtrudeTool") {
        return false;
    }
    if (document_.HasLivePolylineExtrude()) {
        return true;
    }
    if (!document_.GetSelectedPolyline() && !document_.GetSelectedSketch()) {
        statusBar()->showMessage("Extrude:select the required geometry and continue");
        return false;
    }

    const auto& parameters = active_parametric_object_.parameters;
    const double distance = parameters.size() > 0 ? parameters[0].value : 1.0;
    const bool reverse = parameters.size() > 1 && parameters[1].value >= 0.5;
    const double taper_angle = parameters.size() > 2 ? parameters[2].value : 0.0;
    if (!document_.BeginLiveExtrudeSelectedPolyline(distance, reverse, taper_angle)) {
        document_.CancelLiveExtrudeSelectedPolyline();
        statusBar()->showMessage("Extrude:operation failed; check the selected geometry and parameters");
        return false;
    }

    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage("Extrude:adjust the required parameters and continue");
    return true;
}

bool MainWindow::TryStartLivePolylineRevolveFromSelection() {
    if (active_parametric_object_.tool_id != "SurfaceOfRevolution") {
        return false;
    }
    if (document_.HasLivePolylineRevolve()) {
        return true;
    }
    if (!document_.GetSelectedPolyline() && !document_.GetSelectedSketch()) {
        statusBar()->showMessage(
            "Revolve:select the required geometry and continue");
        return false;
    }

    const auto& parameters = active_parametric_object_.parameters;
    const double angle = parameters.size() > 0 ? parameters[0].value : 360.0;
    const int axis_index = parameters.size() > 1 ? static_cast<int>(parameters[1].value) : 2;
    if (!document_.BeginLiveRevolveSelectedPolyline(angle, axis_index)) {
        document_.CancelLiveRevolveSelectedPolyline();
        statusBar()->showMessage(
            "Revolve:operation failed; check the selected geometry and parameters");
        return false;
    }

    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage("Revolve:adjust the required parameters and continue");
    return true;
}

void MainWindow::ClearActiveProperties() {
    if (spatial_curve_kind_ != SpatialCurveKind::None) {
        CancelSpatialCurve();
    }
    if (active_parametric_object_.tool_id == "PlaneTool") {
        CancelPlaneThreePointPick();
        plane_three_point_method_selected_ = false;
        pending_reference_plane_face_pick_ = false;
    }
    if (active_parametric_object_.tool_id == "NurbsParameters") {
        CancelNurbsParameterChanges();
        return;
    }
    const bool fillet_properties = active_parametric_object_.tool_id == "fillet_edge"
        || active_parametric_object_.tool_id == "fillet_all_edges";
    if (fillet_properties) {
        if (!active_parametric_edit_existing_) {
            InvalidateLiveFilletRebuild();
            document_.CancelLiveFillet();
            undo_redo_.CancelChange();
        }
        document_.SetObjectSelectionHighlightHidden(
            document_.GetSelectedObjectIndex(), false);
    } else if (active_parametric_object_.tool_id == "ChamferSolid") {
        document_.CancelLiveChamfer();
        undo_redo_.CancelChange();
    } else if (active_parametric_object_.tool_id == "ThickSolidTool") {
        document_.CancelLiveThickSolid();
    } else if (active_parametric_object_.tool_id == "SolidExtrudeTool"
               && !active_parametric_edit_existing_) {
        document_.CancelLiveExtrudeSelectedPolyline();
    } else if (active_parametric_object_.tool_id == "SurfaceOfRevolution") {
        document_.CancelLiveRevolveSelectedPolyline();
    }
    active_parametric_object_ = {};
    active_parametric_edit_existing_ = false;
    active_cabinet_parameters_dirty_ = false;
    viewport_->ClearSolidDimensionEdit();
    property_panel_->Clear();
    if (properties_dock_) {
        properties_dock_->hide();
    }
}

bool MainWindow::ApplyActiveCabinetProperties() {
    if (!IsCabinetTool(active_parametric_object_.tool_id)) {
        return false;
    }
    if (!active_cabinet_parameters_dirty_) {
        statusBar()->showMessage("Cabinet has no pending changes", 1600);
        return true;
    }

    QProgressDialog progress(
        "Building cabinet geometry...", QString(), 0, 0, this);
    progress.setWindowTitle("Cabinet");
    progress.setWindowModality(Qt::WindowModal);
    progress.setCancelButton(nullptr);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.show();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    QElapsedTimer elapsed;
    elapsed.start();
    const bool create = !active_parametric_edit_existing_
        && active_parametric_object_.object_index
               >= document_.GetObjects().size();
    undo_redo_.BeginChange();
    if (create) {
        ActiveParametricObject created =
            tool_registry_.CreateParametricObject(
                active_parametric_object_.tool_id,
                document_, active_parametric_object_.parameters);
        if (created.tool_id.empty()) {
            undo_redo_.CancelChange();
            progress.close();
            statusBar()->showMessage(
                "Cabinet: operation failed; check the parameters", 2500);
            return false;
        }
        active_parametric_object_ = std::move(created);
        active_parametric_edit_existing_ = true;
        undo_redo_.CommitChangeLazy(
            "Create " + tool_registry_.LabelFor(
                active_parametric_object_.tool_id));
    } else {
        tool_registry_.Rebuild(active_parametric_object_, document_);
        undo_redo_.CommitChange("Edit cabinet");
    }
    UpdateUndoRedoActions();
    active_cabinet_parameters_dirty_ = false;

    progress.close();
    document_.ClearSelection();
    property_panel_->SetActiveObject(active_parametric_object_);
    viewport_->SetSolidDimensionEdit(active_parametric_object_);
    viewport_->SetCabinetPreviewVisible(false);
    RefreshSceneTree();
    viewport_->SetTool(ToolMode::Orbit);
    viewport_->update();
    statusBar()->showMessage(
        QString("Cabinet built in %1 s")
            .arg(elapsed.elapsed() / 1000.0, 0, 'f', 1),
        3000);
    return true;
}

void MainWindow::AcceptActiveProperties() {
    if (active_parametric_object_.tool_id == "NurbsParameters") {
        AcceptNurbsParameterChanges();
        return;
    }
    if (IsCabinetTool(active_parametric_object_.tool_id)) {
        active_parametric_object_ = property_panel_->ActiveObject();
        const bool was_new = !active_parametric_edit_existing_;
        if (!ApplyActiveCabinetProperties()) {
            return;
        }
        QSettings settings;
        settings.beginGroup(
            QStringLiteral("tools/%1/parameters")
                .arg(QString::fromStdString(
                    active_parametric_object_.tool_id)));
        for (const ToolParameter& parameter
             : active_parametric_object_.parameters) {
            settings.setValue(
                QString::fromStdString(parameter.id), parameter.value);
        }
        settings.endGroup();
        ClearActiveProperties();
        document_.ClearSelection();
        RefreshSceneTree();
        viewport_->SetTool(ToolMode::Orbit);
        UpdateActiveToolUi("orbit");
        viewport_->update();
        statusBar()->showMessage(
            was_new ? "Cabinet created" : "Cabinet changes accepted",
            1600);
        return;
    }
    bool created_cabinet = false;
    const bool created_ruled_surface =
        !active_parametric_edit_existing_
        && active_parametric_object_.tool_id == "SurfaceRuled";
    const bool created_shell =
        !active_parametric_edit_existing_
        && active_parametric_object_.tool_id == "SolidShell";
    if (!active_parametric_edit_existing_
        && IsCabinetTool(active_parametric_object_.tool_id)
        && active_parametric_object_.object_index
               >= document_.GetObjects().size()) {
        statusBar()->showMessage("Cabinet:operation completed");
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        undo_redo_.BeginChange();
        ActiveParametricObject created =
            tool_registry_.CreateParametricObject(
                active_parametric_object_.tool_id,
                document_,
                active_parametric_object_.parameters);
        if (created.tool_id.empty()) {
            undo_redo_.CancelChange();
            statusBar()->showMessage(
                "Cabinet:operation failed; check the selected geometry and parameters", 2500);
            return;
        }
        active_parametric_object_ = std::move(created);
        created_cabinet = true;
        document_.ClearSelection();
        RefreshSceneTree();
        viewport_->update();
    }

    if (!active_parametric_edit_existing_
        && (active_parametric_object_.tool_id == "SolidBox"
            || active_parametric_object_.tool_id == "SolidCylinder")) {
        const QString primitive_name = active_parametric_object_.tool_id == "SolidBox"
            ? QStringLiteral("BOX")
            : QStringLiteral("CYLINDER");
        const auto parameter_value = [this](const char* id, double fallback) {
            const auto parameter = std::find_if(
                active_parametric_object_.parameters.begin(),
                active_parametric_object_.parameters.end(),
                [id](const ToolParameter& candidate) {
                    return candidate.id == id;
                });
            return parameter == active_parametric_object_.parameters.end()
                ? fallback
                : parameter->value;
        };
        const unsigned long body_id = static_cast<unsigned long>(
            std::max(0.0, parameter_value("boolean.body_id", 0.0)));
        if (body_id != 0) {
            const double depth = parameter_value(
                active_parametric_object_.tool_id == "SolidBox"
                    ? "depth"
                    : "height",
                0.0);
            const size_t body_index = document_.FindObjectIndexById(body_id);
            const size_t tool_index = active_parametric_object_.object_index;
            const BooleanOperation operation =
                depth >= 0.0 ? BooleanOperation::Union : BooleanOperation::Cut;
            if (std::fabs(depth) <= 0.001
                || body_index >= document_.GetObjects().size()
                || tool_index >= document_.GetObjects().size()
                || !document_.ApplyBooleanToSolids(body_index, tool_index, operation)) {
                statusBar()->showMessage(
                    depth >= 0.0
                        ? QString("%1: Union failed; check that the primitive touches the body").arg(primitive_name)
                        : QString("%1: Cut failed; check that the primitive intersects the body").arg(primitive_name),
                    2200);
                viewport_->update();
                return;
            }

            ClearActiveProperties();
            RefreshSceneTree();
            viewport_->SetTool(ToolMode::Select);
            UpdateActiveToolUi("select");
            viewport_->update();
            statusBar()->showMessage(
                operation == BooleanOperation::Union
                    ? QString("%1: body joined (Union)").arg(primitive_name)
                    : QString("%1: body cut (Cut)").arg(primitive_name),
                1600);
            return;
        }
    }

    if (active_parametric_edit_existing_
        && (active_parametric_object_.tool_id == "fillet_edge"
            || active_parametric_object_.tool_id == "fillet_all_edges"
            || active_parametric_object_.tool_id == "ChamferSolid"
            || active_parametric_object_.tool_id == "SolidSketchFeature"
            || active_parametric_object_.tool_id == "SolidExtrudeFace"
            || active_parametric_object_.tool_id == "SolidOffsetFace"
            || active_parametric_object_.tool_id == "SolidDraft"
            || active_parametric_object_.tool_id == "SolidSheetBend"
            || active_parametric_object_.tool_id == "ThickSolidTool"
            || active_parametric_object_.tool_id == "SolidExtrudeTool"
            || active_parametric_object_.tool_id == "SurfaceOfRevolution"
            || active_parametric_object_.tool_id == "SolidSweptTool"
            || active_parametric_object_.tool_id == "SolidFrameTool"
            || active_parametric_object_.tool_id == "SolidWireTool"
            || active_parametric_object_.tool_id == "SolidPolyhedronTool"
            || active_parametric_object_.tool_id == "TrimByPlane"
            || active_parametric_object_.tool_id == "TrimBySketch"
            || active_parametric_object_.tool_id == "TrimBySurface")) {
        const double value = active_parametric_object_.parameters.empty() ? 0.0 : active_parametric_object_.parameters[0].value;
        const QString label = QString::fromStdString(tool_registry_.LabelFor(active_parametric_object_.tool_id));
        tool_registry_.AcceptTransientTrim(
            active_parametric_object_, document_);
        ClearActiveProperties();
        RefreshSceneTree();
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi("select");
        viewport_->update();
        statusBar()->showMessage(QString("%1: %2").arg(label).arg(value, 0, 'f', 2));
        return;
    }

    if (active_parametric_object_.tool_id == "fillet_edge" || active_parametric_object_.tool_id == "fillet_all_edges") {
        const FilletRadiusValues radii = FilletRadii(active_parametric_object_.parameters);
        const bool all_edges = active_parametric_object_.tool_id == "fillet_all_edges";
        const std::vector<std::pair<int, int>> edge_refs = document_.GetLiveFilletEdgeRefs();
        if (!document_.HasLiveFillet()) {
            statusBar()->showMessage(all_edges ? "Fillet All:select the required geometry and continue" : "Fillet:select the required geometry and continue", 1600);
            return;
        }
        if (live_fillet_build_running_ || live_fillet_build_pending_
            || (live_fillet_update_timer_ && live_fillet_update_timer_->isActive())) {
            if (!live_fillet_build_running_) {
                live_fillet_update_timer_->stop();
                StartPendingLiveFilletRebuild();
            }
            statusBar()->showMessage(
                "Fillet: wait for the current calculation to finish", 1800);
            return;
        }
        const std::vector<int> created_surface_indices = document_.GetLiveFilletCreatedSurfaceIndices();
        InvalidateLiveFilletRebuild();
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
        undo_redo_.CommitChange(all_edges ? "Fillet all edges" : "Fillet edge");
        UpdateUndoRedoActions();
        const bool restore_face_selection =
            edge_tool_started_from_face_quick_menu_;
        edge_tool_started_from_face_quick_menu_ = false;
        ClearActiveProperties();
        if (restore_face_selection) {
            viewport_->SetSelectionMode(SelectionMode::Face);
        }
        RefreshSceneTree();
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi("select");
        viewport_->update();
        statusBar()->showMessage(radii.mode == 2
            ? QString("Fillet: radius law with %1 points").arg(radii.law.size())
            : (radii.mode == 1
                ? QString("Fillet: radius %1 → %2").arg(radii.start, 0, 'f', 2).arg(radii.end, 0, 'f', 2)
                : QString("Fillet: radius %1").arg(radii.start, 0, 'f', 2)));
        return;
    }

    if (active_parametric_object_.tool_id == "ChamferSolid") {
        const double distance = active_parametric_object_.parameters.empty() ? 2.0 : active_parametric_object_.parameters[0].value;
        const std::vector<std::pair<int, int>> edge_refs = document_.GetLiveChamferEdgeRefs();
        if (!document_.HasLiveChamfer()) {
            statusBar()->showMessage("Chamfer:select the required geometry and continue", 1600);
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
        undo_redo_.CommitChange("Chamfer");
        UpdateUndoRedoActions();
        const bool restore_face_selection =
            edge_tool_started_from_face_quick_menu_;
        edge_tool_started_from_face_quick_menu_ = false;
        ClearActiveProperties();
        if (restore_face_selection) {
            viewport_->SetSelectionMode(SelectionMode::Face);
        }
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
            statusBar()->showMessage("ThickSolid:select the required geometry and continue", 1600);
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
            statusBar()->showMessage("Extrude:select the required geometry and continue", 1600);
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
            statusBar()->showMessage(
                "Revolve:select the required geometry and continue",
                1600);
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

    if (!active_parametric_edit_existing_
        && IsCabinetTool(active_parametric_object_.tool_id)) {
        QSettings settings;
        settings.beginGroup(
            QStringLiteral("tools/%1/parameters")
                .arg(QString::fromStdString(active_parametric_object_.tool_id)));
        for (const ToolParameter& parameter : active_parametric_object_.parameters) {
            settings.setValue(
                QString::fromStdString(parameter.id), parameter.value);
        }
        settings.endGroup();
    }

    if (!active_parametric_edit_existing_
        && IsFurnitureAssemblyTool(active_parametric_object_.tool_id)) {
        undo_redo_.CommitChangeLazy(
            "Create " + tool_registry_.LabelFor(active_parametric_object_.tool_id));
        UpdateUndoRedoActions();
    }

    if (created_ruled_surface) {
        RecordDocumentChange("Create ruled surface");
    }
    if (created_shell) {
        RecordDocumentChange("Create shell");
    }

    active_parametric_object_ = {};
    active_parametric_edit_existing_ = false;
    document_.ClearSelection();
    viewport_->ClearSolidDimensionEdit();
    RefreshSceneTree();
    if (properties_dock_) {
        properties_dock_->hide();
    }
    viewport_->SetTool(ToolMode::Orbit);
    UpdateActiveToolUi("orbit");
    statusBar()->showMessage(
        created_cabinet
            ? "Cabinet created"
            : "Object parameters accepted",
        1200);
}

void MainWindow::CancelActiveProperties() {
    if (active_parametric_object_.tool_id == "NurbsParameters") {
        CancelNurbsParameterChanges();
        return;
    }
    const bool canceling_new_furniture = !active_parametric_edit_existing_
        && IsFurnitureAssemblyTool(active_parametric_object_.tool_id);
    if (active_parametric_object_.transient
        && (active_parametric_object_.tool_id == "TrimByPlane"
            || active_parametric_object_.tool_id == "TrimBySketch"
            || active_parametric_object_.tool_id == "TrimBySurface")) {
        tool_registry_.CancelTransientTrim(
            active_parametric_object_, document_);
        ClearActiveProperties();
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi("select");
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage("Trim canceled", 1200);
        return;
    }

    if (active_parametric_object_.tool_id == "fillet_edge" || active_parametric_object_.tool_id == "fillet_all_edges") {
        InvalidateLiveFilletRebuild();
        document_.CancelLiveFillet();
        undo_redo_.CancelChange();
        const bool restore_face_selection =
            edge_tool_started_from_face_quick_menu_;
        edge_tool_started_from_face_quick_menu_ = false;
        ClearActiveProperties();
        if (restore_face_selection) {
            viewport_->SetSelectionMode(SelectionMode::Face);
        }
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi("select");
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage("Fillet canceled", 1200);
        return;
    }

    if (active_parametric_object_.tool_id == "ChamferSolid") {
        document_.CancelLiveChamfer();
        undo_redo_.CancelChange();
        const bool restore_face_selection =
            edge_tool_started_from_face_quick_menu_;
        edge_tool_started_from_face_quick_menu_ = false;
        ClearActiveProperties();
        if (restore_face_selection) {
            viewport_->SetSelectionMode(SelectionMode::Face);
        }
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
        if (!active_parametric_edit_existing_) {
            document_.CancelLiveExtrudeSelectedPolyline();
        }
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
            std::vector<size_t> indices;
            std::set<unsigned long> visited_ids;
            std::function<void(size_t)> collect_object;
            collect_object = [&](size_t index) {
                if (index >= objects.size() || !objects[index]
                    || !visited_ids.insert(objects[index]->m_id).second) {
                    return;
                }
                indices.push_back(index);
                if (const auto* group = dynamic_cast<const CGroup*>(objects[index].get())) {
                    for (unsigned long id : group->GetElementIds()) {
                        collect_object(document_.FindObjectIndexById(id));
                    }
                }
            };
            collect_object(object_index);
            std::sort(indices.begin(), indices.end(), std::greater<size_t>());
            for (size_t index : indices) {
                if (index < objects.size()) {
                    objects.erase(
                        objects.begin()
                        + static_cast<CAlfaDoc::ObjectList::difference_type>(index));
                }
            }
            document_.ClearSelection();
        }
        tool_registry_.RebuildArchitectureRooms(document_);
    }

    if (canceling_new_furniture) {
        undo_redo_.CancelChange();
        UpdateUndoRedoActions();
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

    properties_dock_->setWindowTitle(
        active_parametric_object_.tool_id == "SurfaceRuled"
            ? title
            : title + " Parameters");
    properties_dock_->setFloating(true);
    const bool plane_tool = active_parametric_object_.tool_id == "PlaneTool";
    const bool plane_three_points = plane_tool
        && static_cast<int>(PlaneParameterValue(
            active_parametric_object_.parameters, "mode", 1.0)) == 2;
    properties_dock_->resize(
        plane_tool ? 330 : 300,
        plane_three_points ? 250 : (plane_tool ? 420 : 180));
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

void MainWindow::RegisterToolButton(QAbstractButton* button, const std::string& key) {
    if (!button) {
        return;
    }

    button->setCheckable(true);
    button->setProperty("toolKey", QString::fromStdString(key));
    if (button->icon().isNull()) {
        button->setIcon(ToolIcon(key));
    }
    button->setIconSize(QSize(22, 22));
    tool_buttons_.push_back(button);
}

void MainWindow::UpdateActiveToolUi(const std::string& key) {
    active_tool_key_ = key;
    const QString active_key = QString::fromStdString(key);

    for (QAction* action : tool_actions_) {
        action->setChecked(action->property("toolKey").toString() == active_key);
    }

    for (QAbstractButton* button : tool_buttons_) {
        bool checked = button->property("toolKey").toString() == active_key;
        const QVariant group_keys = button->property("toolGroupKeys");
        if (!checked && group_keys.isValid()) {
            checked = group_keys.toStringList().contains(active_key);
        }
        button->setChecked(checked);
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

    for (QAbstractButton* button : tool_buttons_) {
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
    } else if (icon_key == "PolylineCurve" || icon_key == "BSplineCurve"
               || icon_key == "DrawSpline"
               || icon_key == "BezierCurve3D" || icon_key == "NurbsCurve3D") {
        icon_key = "curve";
    } else if (icon_key == "EditPoint") {
        icon_key = "select";
    } else if (icon_key == "CurveFillets") {
        return SketchFilletIcon();
    } else if (icon_key == "NewSketch") {
        return NewSketchIcon();
    } else if (icon_key == "SurfaceLoft") {
        return LoftSurfaceIcon();
    } else if (icon_key == "SurfaceRuled") {
        return LoftSurfaceIcon();
    } else if (icon_key == "SolidShell") {
        return QIcon(":/icons/ThickSolidTool.png");
    } else if (icon_key == "SurfaceSweepTwoRails") {
        return QIcon(":/icons/SurfaceSweepTwoRails.png");
    } else if (icon_key == "SurfaceFourSplines") {
        return QIcon(":/icons/SurfaceFourSplines.svg");
    } else if (icon_key == "SurfaceJoin") {
        return LoftSurfaceIcon();
    } else if (icon_key == "SolidSweepTwoRails") {
        icon_key = "SolidSweptTool";
    } else if (icon_key == "SurfaceReverseNormals") {
        return ReverseNormalsIcon();
    } else if (icon_key == "SolidFrameTool") {
        return FrameSolidIcon();
    } else if (icon_key == "SolidWireTool") {
        icon_key = "SolidSweptTool";
    } else if (icon_key == "SolidTwoSketches") {
        return BodyByTwoSketchesIcon();
    } else if (icon_key == "chair" || icon_key == "chair_simple") {
        return ChairFurnitureIcon();
    } else if (icon_key == "table" || icon_key == "desk") {
        return TableFurnitureIcon();
    } else if (icon_key == "drawer_box"
               || icon_key == "single_drawer"
               || icon_key == "single_facade"
               || icon_key == "cabinet_advanced"
               || icon_key == "cabinet_advanced_slx"
               || icon_key == "cabinet_showcase"
               || icon_key == "kitchen_nika_260"
               || icon_key == "kitchen_corner") {
        icon_key = "cabinet";
    } else if (icon_key == "PlaneTool") {
        icon_key = "SurfaceOfRevolution";
    } else if (icon_key == "room") {
        icon_key = "stair";
    } else if (icon_key == "SolidOffsetFace") {
        icon_key = "SolidExtrudeFace";
    }
    return QIcon(QString(":/icons/%1.png").arg(icon_key));
}

void MainWindow::NewProject() {
    document_.Clear();
    undo_redo_.Reset();
    UpdateUndoRedoActions();
    project_path_.clear();
    ClearActiveProperties();
    RefreshSceneTree();
    viewport_->update();
    UpdateWindowTitle();
    statusBar()->showMessage("New project");
}

void MainWindow::OpenProject() {
    const QString path = SelectProjectToOpen();
    if (path.isEmpty()) {
        return;
    }

    OpenProjectFromPath(path);
}

QString MainWindow::SelectProjectToOpen() {
    QFileDialog dialog(this, "Open Dom3D Project", LastDialogDir());
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters({"Dom3D Project (*.dom3d)", "Legacy Dom3D Project (*.d3dm *.wrk)", "All files (*.*)"});
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);

    auto* preview = new QLabel(&dialog);
    preview->setFixedSize(250, 150);
    preview->setAlignment(Qt::AlignCenter);
    preview->setFrameShape(QFrame::StyledPanel);
    preview->setText("No thumbnail");
    preview->setScaledContents(false);

    if (auto* grid = qobject_cast<QGridLayout*>(dialog.layout())) {
        grid->addWidget(preview, 0, grid->columnCount(), grid->rowCount(), 1, Qt::AlignTop);
    }

    const auto update_preview = [this, preview](const QString& path) {
        QImage thumbnail;
        QString error;
        if (path.toLower().endsWith(".dom3d") && dom3d_serializer_.LoadThumbnail(path, thumbnail, error) && !thumbnail.isNull()) {
            preview->setPixmap(QPixmap::fromImage(thumbnail).scaled(preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            preview->setPixmap(QPixmap());
            preview->setText("No thumbnail");
        }
    };
    connect(&dialog, &QFileDialog::currentChanged, this, update_preview);
    connect(&dialog, &QFileDialog::fileSelected, this, update_preview);

    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
        return {};
    }
    return dialog.selectedFiles().first();
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

    const QString lower_path = path.toLower();
    const bool legacy_project = lower_path.endsWith(".d3dm") || lower_path.endsWith(".wrk");
    QProgressDialog loading(
        QString("Opening %1...").arg(QFileInfo(path).fileName()),
        QString(), 0, 100, this);
    loading.setWindowTitle("Loading Project");
    loading.setCancelButton(nullptr);
    loading.setWindowModality(Qt::WindowModal);
    loading.setMinimumDuration(350);
    loading.setAutoClose(false);
    loading.setAutoReset(false);
    loading.setMinimumWidth(420);
    loading.setValue(0);
    const auto update_loading = [&loading](int value, const QString& text) {
        loading.setLabelText(text);
        loading.setValue(std::clamp(value, 0, 100));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    };
    bool restored_camera = false;
    if (legacy_project) {
        std::string error;
        if (!project_io_.Load(
                path.toStdString(), document_, error,
                [&update_loading](int value, const std::string& text) {
                    update_loading(value, QString::fromStdString(text));
                })) {
            loading.close();
            QMessageBox::critical(this, "Dom3D Pro", QString::fromStdString(error));
            return;
        }
    } else {
        QString error;
        QString active_room;
        ProjectViewState view_state;
        if (!dom3d_serializer_.Load(
                path, document_, active_room, view_state, error,
                update_loading)) {
            loading.close();
            QMessageBox::critical(this, "Dom3D Pro", error);
            return;
        }
        if (tool_tabs_ && !active_room.isEmpty()) {
            for (int i = 0; i < tool_tabs_->count(); ++i) {
                if (tool_tabs_->tabData(i).toString() == active_room) {
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
    update_loading(100, "Project opened");
    loading.close();

    project_path_ = path.toStdString();
    undo_redo_.Reset();
    UpdateUndoRedoActions();
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

void MainWindow::SaveProject(bool save_as) {
    QString path = QString::fromStdString(project_path_);
    const QString lower_path = path.toLower();
    if (save_as || path.isEmpty() || lower_path.endsWith(".d3dm") || lower_path.endsWith(".wrk")) {
        path = QFileDialog::getSaveFileName(this, "Save Dom3D Project", LastDialogDir(), "Dom3D Project (*.dom3d);;All files (*.*)");
    }
    if (path.isEmpty()) {
        return;
    }
    if (QFileInfo(path).suffix().isEmpty()) {
        path += ".dom3d";
    }

    QString error;
    const QString active_room = tool_tabs_
        ? tool_tabs_->tabData(tool_tabs_->currentIndex()).toString()
        : QString("Architecture");
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
    if (!dom3d_serializer_.Save(path, document_, active_room, view_state, CaptureProjectThumbnail(), error)) {
        QMessageBox::critical(this, "Dom3D Pro", error);
        return;
    }

    project_path_ = path.toStdString();
    UpdateWindowTitle();
    RememberLastDialogDir(path);
    AddRecentProjectFile(path);
    statusBar()->showMessage("Project saved", 1400);
}

void MainWindow::SaveProjectAs() {
    SaveProject(true);
}

QImage MainWindow::CaptureProjectThumbnail() const {
    if (!viewport_) {
        return {};
    }

    QImage thumbnail = viewport_->grabFramebuffer();
    if (thumbnail.isNull()) {
        return {};
    }
    return thumbnail.scaled(750, 450, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QImage MainWindow::CaptureSelectionThumbnail(
        const std::vector<size_t>& selected_indices) {
    if (!viewport_ || selected_indices.empty()) {
        return CaptureProjectThumbnail();
    }

    auto& objects = document_.GetObjects();
    std::vector<bool> visibility;
    visibility.reserve(objects.size());
    for (auto& object : objects) {
        visibility.push_back(object && object->IsVisible());
        if (object) {
            object->CAlfaObject::SetVisible(false);
        }
    }
    for (size_t index : selected_indices) {
        if (index < objects.size() && objects[index]) {
            objects[index]->SetVisible(true);
        }
    }

    const Camera camera = viewport_->GetCamera();
    viewport_->FitToDocument();
    viewport_->repaint();
    const QImage thumbnail = CaptureProjectThumbnail();

    for (size_t index = 0; index < objects.size(); ++index) {
        if (objects[index]) {
            objects[index]->CAlfaObject::SetVisible(visibility[index]);
        }
    }
    viewport_->SetCamera(camera);
    viewport_->update();
    return thumbnail;
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
    connect(&dialog, &PreferencesDialog::SettingsApplied, this, [this]() {
        viewport_->ReloadModelingPreferences();
        if (!active_parametric_object_.tool_id.empty()) {
            property_panel_->SetActiveObject(active_parametric_object_);
        }
    });
    if (dialog.exec() == QDialog::Accepted) {
        statusBar()->showMessage("Preferences applied", 1400);
    }
}

void MainWindow::ShowLightingDialog() {
    if (!lighting_dialog_) {
        lighting_dialog_ = new LightingDialog(this);
        connect(lighting_dialog_, &LightingDialog::LightingChanged,
                viewport_, qOverload<>(&OpenGLViewport::update));
    }
    lighting_dialog_->show();
    lighting_dialog_->raise();
    lighting_dialog_->activateWindow();
}

void MainWindow::ShowBlenderCyclesDialog() {
    RenderScene scene = BuildRenderScene(
        document_, viewport_->GetCamera(),
        viewport_->IsOrthographicProjection(),
        viewport_->GetBackgroundColor());
    auto* dialog = new BlenderCyclesDialog(
        std::move(scene), viewport_->size(), this);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void MainWindow::ShowNativeRaytraceDialog() {
    RenderScene scene = BuildRenderScene(
        document_, viewport_->GetCamera(),
        viewport_->IsOrthographicProjection(),
        viewport_->GetBackgroundColor());
    auto* dialog = new NativeRaytraceDialog(
        std::move(scene), viewport_->size(), this);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void MainWindow::ViewLastRenderResult() {
    const QString path = QSettings("Dom3D", "Dom3D_Pro")
        .value("render/lastOutputPath").toString();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        QMessageBox::information(
            this, "Render Result", "There is no completed render to view yet.");
        return;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
        QMessageBox::warning(
            this, "Render Result", QString("Could not open:\n%1").arg(path));
    }
}

void MainWindow::AddReferenceImage(ReferenceImageAxis axis) {
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Add Reference Image",
        LastDialogDir(),
        "Raster images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp);;All files (*.*)");
    if (path.isEmpty()) {
        return;
    }

    const QImage bitmap(path);
    if (bitmap.isNull() || bitmap.width() <= 0 || bitmap.height() <= 0) {
        QMessageBox::warning(
            this, "Reference Image",
            QString("Cannot read image:\n%1").arg(path));
        return;
    }

    constexpr float kInitialLongSide = 1000.0f;
    const float aspect = static_cast<float>(bitmap.width())
        / static_cast<float>(bitmap.height());
    const float width = aspect >= 1.0f
        ? kInitialLongSide : kInitialLongSide * aspect;
    const float height = aspect >= 1.0f
        ? kInitialLongSide / aspect : kInitialLongSide;
    std::unique_ptr<CReferenceImage> image = CReferenceImage::Create(
        QFileInfo(path).absoluteFilePath().toStdString(), axis, width, height);
    if (!image) {
        QMessageBox::warning(
            this, "Reference Image", "Could not create the image plane.");
        return;
    }

    const QString axis_name = axis == ReferenceImageAxis::X
        ? "X" : axis == ReferenceImageAxis::Y ? "Y" : "Z";
    image->SetName(QString("Ref %1 [%2]")
        .arg(QFileInfo(path).completeBaseName(), axis_name).toStdString());
    Material material = image->GetMaterial();
    material.id = 0;
    material.name = QString("Reference: %1")
        .arg(QFileInfo(path).fileName()).toStdString();
    image->SetMaterial(material);
    image->SetMaterialId(0);
    document_.AddObject(std::move(image));
    RecordDocumentChange("Add reference image");

    RememberLastDialogDir(path);
    ClearActiveProperties();
    RefreshSceneTree();
    viewport_->FitToDocument();
    BeginTransformTool(TransformOperation::Move);
    SetMeshSurfaceOpacity(1.0f);
    statusBar()->showMessage(
        QString("Reference image added for %1-axis; use Gizmo to transform it")
            .arg(axis_name),
        2600);
}

void MainWindow::ImportFile() {
    const QString filter = "Dom3D Project (*.dom3d);;Wavefront OBJ (*.obj);;3D Studio (*.3ds);;STEP (*.step *.stp);;IGES (*.iges *.igs);;AutoCAD DXF (*.dxf);;Encapsulated PostScript (*.eps);;HPGL Plotter (*.hpgl *.hpg *.plt);;STL Mesh (*.stl);;TEXT (*.txt);;All files (*.*)";
    QString selected_filter;
    const QString path = QFileDialog::getOpenFileName(this, "Import", LastDialogDir(), filter, &selected_filter);
    if (path.isEmpty()) {
        return;
    }

    ImportFileFromPath(path);
}

bool MainWindow::ImportFileFromPath(const QString& path, bool catalog_sketch) {
    if (path.isEmpty()) {
        return false;
    }

    std::string error;
    std::map<std::string, unsigned long> imported_material_ids;
    bool preserve_import_selection = false;
    const auto register_imported_material = [this, &imported_material_ids](CMesh3D& mesh) {
        Material material = mesh.GetMaterial();
        if (material.id != 0 || material.name.empty() || material.name == "Imported Mesh") {
            return;
        }
        const std::string key = material.name + "\n"
            + material.color_texture_path + "\n"
            + material.light_texture_path + "\n"
            + material.bump_texture_path;
        const std::string pbr_key = key + "\n"
            + material.normal_texture_path + "\n"
            + material.roughness_texture_path + "\n"
            + material.metallic_texture_path + "\n"
            + material.displacement_texture_path;
        const auto existing = imported_material_ids.find(pbr_key);
        if (existing != imported_material_ids.end()) {
            if (const Material* saved = document_.FindMaterial(existing->second)) {
                mesh.SetMaterial(*saved);
            }
            return;
        }
        material.id = 0;
        const Material& saved = document_.UpsertMaterial(std::move(material));
        imported_material_ids.emplace(pbr_key, saved.id);
        mesh.SetMaterial(saved);
    };
    const QString lower_path = path.toLower();
    if (lower_path.endsWith(".dom3d")) {
        QDialog dialog(this);
        dialog.setWindowTitle("Import Dom3D Part");
        auto* layout = new QVBoxLayout(&dialog);
        auto* form = new QFormLayout;
        auto* name_edit = new QLineEdit(QFileInfo(path).completeBaseName(), &dialog);
        form->addRow("Part name", name_edit);

        const auto coordinate_row = [&dialog](double initial) {
            auto* value = new QDoubleSpinBox(&dialog);
            value->setRange(-1000000000.0, 1000000000.0);
            value->setDecimals(4);
            value->setValue(initial);
            return value;
        };
        auto* px = coordinate_row(0.0);
        auto* py = coordinate_row(0.0);
        auto* pz = coordinate_row(0.0);
        auto* position_widget = new QWidget(&dialog);
        auto* position_layout = new QHBoxLayout(position_widget);
        position_layout->setContentsMargins(0, 0, 0, 0);
        position_layout->addWidget(new QLabel("X", position_widget));
        position_layout->addWidget(px);
        position_layout->addWidget(new QLabel("Y", position_widget));
        position_layout->addWidget(py);
        position_layout->addWidget(new QLabel("Z", position_widget));
        position_layout->addWidget(pz);
        form->addRow("Insertion point P0", position_widget);

        const auto scale_value = [&dialog]() {
            auto* value = new QDoubleSpinBox(&dialog);
            value->setRange(0.0001, 10000.0);
            value->setDecimals(4);
            value->setValue(1.0);
            return value;
        };
        auto* sx = scale_value();
        auto* sy = scale_value();
        auto* sz = scale_value();
        auto* scale_widget = new QWidget(&dialog);
        auto* scale_layout = new QHBoxLayout(scale_widget);
        scale_layout->setContentsMargins(0, 0, 0, 0);
        scale_layout->addWidget(new QLabel("X", scale_widget));
        scale_layout->addWidget(sx);
        scale_layout->addWidget(new QLabel("Y", scale_widget));
        scale_layout->addWidget(sy);
        scale_layout->addWidget(new QLabel("Z", scale_widget));
        scale_layout->addWidget(sz);
        form->addRow("Scale", scale_widget);

        auto* linked = new QCheckBox("File linked", &dialog);
        linked->setToolTip("Keep the source path for a future Reload from File command");
        form->addRow(QString(), linked);
        layout->addLayout(form);
        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }

        QString import_error;
        if (!dom3d_serializer_.ImportPart(
                path,
                document_,
                name_edit->text(),
                {static_cast<float>(px->value()),
                 static_cast<float>(py->value()),
                 static_cast<float>(pz->value())},
                {static_cast<float>(sx->value()),
                 static_cast<float>(sy->value()),
                 static_cast<float>(sz->value())},
                linked->isChecked(),
                !catalog_sketch,
                import_error)) {
            QMessageBox::critical(this, "Dom3D Import", import_error);
            return false;
        }
        preserve_import_selection = true;
        RecordDocumentChange("Import Dom3D Part");
    } else if (lower_path.endsWith(".3ds")) {
        std::vector<std::unique_ptr<CMesh3D>> meshes;
        if (!three_ds_io_.Import(path.toStdString(), meshes, error)) {
            QMessageBox::critical(this, "3DS Import", QString::fromStdString(error));
            return false;
        }
        const std::string group_name = meshes.size() > 1
            ? (QFileInfo(path).completeBaseName() + " (3DS)").toStdString()
            : std::string{};
        for (auto& mesh : meshes) {
            mesh->SetGroupName(group_name);
            register_imported_material(*mesh);
            document_.AddMesh(std::move(mesh));
        }
    } else if (lower_path.endsWith(".step") || lower_path.endsWith(".stp")) {
        std::vector<std::unique_ptr<CSolid>> solids;
        if (!step_io_.Import(path.toStdString(), solids, error)) {
            QMessageBox::critical(this, "STEP Import", QString::fromStdString(error));
            return false;
        }
        for (auto& solid : solids) {
            document_.AddObject(std::move(solid));
        }
    } else if (lower_path.endsWith(".iges") || lower_path.endsWith(".igs")) {
        std::vector<std::unique_ptr<CAlfaObject>> objects;
        if (!iges_io_.Import(path.toStdString(), objects, error)) {
            QMessageBox::critical(this, "IGES Import", QString::fromStdString(error));
            return false;
        }
        for (auto& object : objects) {
            document_.AddObject(std::move(object));
        }
    } else if (lower_path.endsWith(".dxf")) {
        std::vector<std::unique_ptr<CAlfaObject>> objects;
        if (!dxf_io_.Import(path.toStdString(), objects, error)) {
            QMessageBox::critical(this, "DXF Import", QString::fromStdString(error));
            return false;
        }
        for (auto& object : objects) {
            document_.AddObject(std::move(object));
        }
    } else if (lower_path.endsWith(".eps")) {
        std::vector<std::unique_ptr<CAlfaObject>> objects;
        if (!eps_io_.Import(path.toStdString(), objects, error)) {
            QMessageBox::critical(this, "EPS Import", QString::fromStdString(error));
            return false;
        }
        for (auto& object : objects) {
            document_.AddObject(std::move(object));
        }
    } else if (lower_path.endsWith(".hpgl") || lower_path.endsWith(".hpg")
               || lower_path.endsWith(".plt")) {
        std::vector<std::unique_ptr<CAlfaObject>> objects;
        if (!hpgl_io_.Import(path.toStdString(), objects, error)) {
            QMessageBox::critical(this, "HPGL Import", QString::fromStdString(error));
            return false;
        }
        for (auto& object : objects) {
            document_.AddObject(std::move(object));
        }
    } else if (lower_path.endsWith(".stl")) {
        std::vector<std::unique_ptr<CMesh3D>> meshes;
        if (!stl_io_.Import(path.toStdString(), meshes, error)) {
            QMessageBox::critical(this, "STL Import", QString::fromStdString(error));
            return false;
        }
        for (auto& mesh : meshes) {
            register_imported_material(*mesh);
            document_.AddMesh(std::move(mesh));
        }
    } else if (lower_path.endsWith(".obj")) {
        std::vector<std::unique_ptr<CMesh3D>> meshes;
        if (!obj_io_.Import(path.toStdString(), meshes, error)) {
            QMessageBox::critical(this, "OBJ Import", QString::fromStdString(error));
            return false;
        }
        for (auto& mesh : meshes) {
            register_imported_material(*mesh);
            document_.AddMesh(std::move(mesh));
        }
    } else if (lower_path.endsWith(".txt")) {
        std::ifstream stream(path.toStdString());
        if (!stream) {
            QMessageBox::critical(this, "TEXT Import", QString("Cannot open file:\n%1").arg(path));
            return false;
        }

        std::vector<std::unique_ptr<CPolyline>> polylines;
        if (!CPolyline::LoadTextPolylines(stream, polylines, error)) {
            QMessageBox::critical(this, "TEXT Import", QString::fromStdString(error));
            return false;
        }

        const std::string group_name = polylines.size() > 1
            ? (QFileInfo(path).completeBaseName() + " (TEXT)").toStdString()
            : std::string{};
        for (auto& polyline : polylines) {
            polyline->SetGroupName(group_name);
            document_.AddObject(std::move(polyline));
        }
    } else {
        QMessageBox::warning(this, "Dom3D Pro", QString("Unsupported dropped file:\n%1").arg(path));
        return false;
    }

    if (!preserve_import_selection) {
        document_.ClearSelection();
    }
    RememberLastDialogDir(path);
    ClearActiveProperties();
    RefreshSceneTree();
    viewport_->FitToDocument();
    viewport_->update();
    statusBar()->showMessage(QString("File imported: %1").arg(QFileInfo(path).fileName()), 1400);
    return true;
}

void MainWindow::AddSelectionToCatalog() {
    std::vector<size_t> selected;
    selected.reserve(document_.GetSelectedObjectCount());
    for (size_t index : document_.GetSelectedObjectIndices()) {
        const size_t catalog_index = document_.ResolveGroupSelectionIndex(index);
        if (catalog_index < document_.GetObjects().size()
            && std::find(selected.begin(), selected.end(), catalog_index)
                == selected.end()) {
            selected.push_back(catalog_index);
        }
    }
    if (selected.empty()) {
        statusBar()->showMessage("Catalog: select an object first", 1600);
        return;
    }

    const auto& objects = document_.GetObjects();
    std::map<unsigned long, const CAlfaObject*> objects_by_id;
    for (const auto& object : objects) {
        if (object) {
            objects_by_id[object->m_id] = object.get();
        }
    }
    std::set<unsigned long> checked_ids;
    std::function<bool(const CAlfaObject*)> contains_only_sketches =
        [&](const CAlfaObject* object) -> bool {
            if (!object) {
                return false;
            }
            if (dynamic_cast<const CSmartLine*>(object)) {
                return true;
            }
            const auto* group = dynamic_cast<const CGroup*>(object);
            if (!group || group->GetElementIds().empty()
                || !checked_ids.insert(object->m_id).second) {
                return false;
            }
            for (unsigned long id : group->GetElementIds()) {
                const auto child = objects_by_id.find(id);
                if (child == objects_by_id.end()
                    || !contains_only_sketches(child->second)) {
                    return false;
                }
            }
            return true;
        };
    const bool sketch_item = std::all_of(
        selected.begin(), selected.end(), [&](size_t index) {
            return index < objects.size()
                && contains_only_sketches(objects[index].get());
        });
    const QString catalog_root =
        QDir(QCoreApplication::applicationDirPath()).filePath("Catalog");
    QDir().mkpath(QDir(catalog_root).filePath("Sketches"));
    QDir().mkpath(QDir(catalog_root).filePath("Products"));
    const QString initial_directory = QDir(catalog_root).filePath(
        sketch_item ? "Sketches" : "Products");
    const QString suggested_name = selected.front() < objects.size()
        && objects[selected.front()]
        ? QString::fromStdString(objects[selected.front()]->GetName())
        : QString("Catalog item");
    QString path = QFileDialog::getSaveFileName(
        this,
        "Add to Catalog",
        QDir(initial_directory).filePath(suggested_name + ".dom3d"),
        "Dom3D Catalog Item (*.dom3d)");
    if (path.isEmpty()) {
        return;
    }
    if (QFileInfo(path).suffix().isEmpty()) {
        path += ".dom3d";
    }

    ProjectViewState view_state;
    view_state.camera = viewport_->GetCamera();
    view_state.has_camera = true;
    view_state.orthographic_projection = viewport_->IsOrthographicProjection();
    view_state.has_orthographic_projection = true;
    QString error;
    const QString room = tool_tabs_
        ? tool_tabs_->tabData(tool_tabs_->currentIndex()).toString()
        : QString("Catalog");
    if (!dom3d_serializer_.SaveSelection(
            path,
            document_,
            selected,
            room,
            view_state,
            CaptureSelectionThumbnail(selected),
            error)) {
        QMessageBox::critical(this, "Add to Catalog", error);
        return;
    }
    statusBar()->showMessage(
        QString("Added to catalog: %1").arg(QFileInfo(path).fileName()),
        1800);
}

void MainWindow::ShowCatalogDialog() {
    const QString catalog_root =
        QDir(QCoreApplication::applicationDirPath()).filePath("Catalog");
    QDir().mkpath(QDir(catalog_root).filePath("Sketches"));
    QDir().mkpath(QDir(catalog_root).filePath("Products"));

    QDialog dialog(this);
    dialog.setWindowTitle("Dom3D Catalog");
    dialog.resize(760, 500);
    auto* root_layout = new QVBoxLayout(&dialog);
    auto* content = new QHBoxLayout;
    auto* tree = new QTreeWidget(&dialog);
    tree->setHeaderLabel("Catalog");
    tree->setMinimumWidth(390);
    auto* preview = new QLabel("Select a catalog item", &dialog);
    preview->setAlignment(Qt::AlignCenter);
    preview->setFrameShape(QFrame::StyledPanel);
    preview->setMinimumSize(300, 260);
    preview->setWordWrap(true);
    content->addWidget(tree, 3);
    content->addWidget(preview, 2);
    root_layout->addLayout(content);

    const auto add_directory = [&](auto&& self,
                                   QTreeWidgetItem* parent,
                                   const QString& directory_path) -> void {
        QDir directory(directory_path);
        const QFileInfoList entries = directory.entryInfoList(
            QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
            QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo& entry : entries) {
            if (entry.isFile() && entry.suffix().compare(
                    "dom3d", Qt::CaseInsensitive) != 0) {
                continue;
            }
            auto* item = new QTreeWidgetItem(parent);
            item->setText(0, entry.completeBaseName());
            item->setData(0, Qt::UserRole, entry.absoluteFilePath());
            item->setData(0, Qt::UserRole + 1, entry.isFile());
            if (entry.isDir()) {
                item->setText(0, entry.fileName());
                self(self, item, entry.absoluteFilePath());
            }
        }
    };
    auto* root_item = new QTreeWidgetItem(tree);
    root_item->setText(0, "Catalog");
    root_item->setData(0, Qt::UserRole, catalog_root);
    root_item->setData(0, Qt::UserRole + 1, false);
    add_directory(add_directory, root_item, catalog_root);
    root_item->setExpanded(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto* insert = buttons->addButton("Insert", QDialogButtonBox::AcceptRole);
    insert->setEnabled(false);
    root_layout->addWidget(buttons);
    QString selected_path;
    connect(tree, &QTreeWidget::currentItemChanged, &dialog,
            [this, preview, insert, &selected_path](QTreeWidgetItem* current) {
        selected_path.clear();
        insert->setEnabled(false);
        preview->setPixmap(QPixmap());
        preview->setText("Select a catalog item");
        if (!current || !current->data(0, Qt::UserRole + 1).toBool()) {
            return;
        }
        selected_path = current->data(0, Qt::UserRole).toString();
        QImage thumbnail;
        QString error;
        if (dom3d_serializer_.LoadThumbnail(
                selected_path, thumbnail, error) && !thumbnail.isNull()) {
            preview->setPixmap(QPixmap::fromImage(thumbnail).scaled(
                preview->size() - QSize(12, 12),
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation));
        } else {
            preview->setText(QFileInfo(selected_path).completeBaseName());
        }
        insert->setEnabled(true);
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(insert, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(tree, &QTreeWidget::itemDoubleClicked, &dialog,
            [&dialog](QTreeWidgetItem* item) {
        if (item && item->data(0, Qt::UserRole + 1).toBool()) {
            dialog.accept();
        }
    });

    if (dialog.exec() != QDialog::Accepted || selected_path.isEmpty()) {
        return;
    }
    const QString normalized = QDir::fromNativeSeparators(selected_path);
    const bool sketch_item = normalized.contains(
        "/Sketches/", Qt::CaseInsensitive);
    ImportFileFromPath(selected_path, sketch_item);
}

void MainWindow::HandleDroppedFiles(const QStringList& paths) {
    if (paths.isEmpty()) {
        return;
    }

    for (const QString& path : paths) {
        const QString lower_path = path.toLower();
        if (lower_path.endsWith(".dom3d") || lower_path.endsWith(".d3dm") || lower_path.endsWith(".wrk")) {
            OpenProjectFromPath(path);
            return;
        }
    }

    int imported_count = 0;
    for (const QString& path : paths) {
        if (ImportFileFromPath(path)) {
            ++imported_count;
        }
    }
    if (imported_count > 1) {
        statusBar()->showMessage(QString("Imported %1 files").arg(imported_count), 1400);
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

void MainWindow::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void MainWindow::dropEvent(QDropEvent* event) {
    QStringList paths;
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            paths.push_back(url.toLocalFile());
        }
    }
    if (paths.isEmpty()) {
        event->ignore();
        return;
    }

    event->acceptProposedAction();
    HandleDroppedFiles(paths);
}

void MainWindow::ExportFile() {
    const QString filter = "Wavefront OBJ (*.obj);;STEP (*.step *.stp);;IGES (*.iges *.igs);;AutoCAD DXF (*.dxf);;Encapsulated PostScript (*.eps);;HPGL Plotter (*.hpgl);;STL Mesh (*.stl);;All files (*.*)";
    QFileDialog file_dialog(this, "Export", LastDialogDir(), filter);
    file_dialog.setAcceptMode(QFileDialog::AcceptSave);
    file_dialog.setFileMode(QFileDialog::AnyFile);
    file_dialog.setOption(QFileDialog::DontUseNativeDialog, true);

    auto* unit_row = new QWidget(&file_dialog);
    auto* unit_layout = new QFormLayout(unit_row);
    unit_layout->setContentsMargins(0, 4, 0, 0);
    auto* export_unit = new QComboBox(unit_row);
    export_unit->addItem("Meters (m)", static_cast<int>(ObjLengthUnit::Meters));
    export_unit->addItem("Decimeters (dm)", static_cast<int>(ObjLengthUnit::Decimeters));
    export_unit->addItem("Centimeters (cm)", static_cast<int>(ObjLengthUnit::Centimeters));
    export_unit->addItem("Millimeters (mm)", static_cast<int>(ObjLengthUnit::Millimeters));
    export_unit->addItem("Feet (ft)", static_cast<int>(ObjLengthUnit::Feet));
    export_unit->addItem("Inches (in)", static_cast<int>(ObjLengthUnit::Inches));
    QSettings export_settings("Dom3D", "Dom3D_Pro");
    const int saved_unit = export_settings.value(
        "export/objLengthUnit",
        static_cast<int>(ObjLengthUnit::Millimeters)).toInt();
    const int saved_unit_index = export_unit->findData(saved_unit);
    export_unit->setCurrentIndex(saved_unit_index >= 0 ? saved_unit_index : 3);
    unit_layout->addRow("File export units (OBJ)", export_unit);
    file_dialog.layout()->addWidget(unit_row);

    if (file_dialog.exec() != QDialog::Accepted
        || file_dialog.selectedFiles().isEmpty()) {
        return;
    }
    QString path = file_dialog.selectedFiles().constFirst();
    const QString selected_filter = file_dialog.selectedNameFilter();

    std::string error;
    const QString lower_path = path.toLower();
    const bool export_step = selected_filter.startsWith("STEP") || lower_path.endsWith(".step") || lower_path.endsWith(".stp");
    const bool export_iges = selected_filter.startsWith("IGES") || lower_path.endsWith(".iges") || lower_path.endsWith(".igs");
    const bool export_dxf = selected_filter.startsWith("AutoCAD DXF") || lower_path.endsWith(".dxf");
    const bool export_eps = selected_filter.startsWith("Encapsulated PostScript") || lower_path.endsWith(".eps");
    const bool export_hpgl = selected_filter.startsWith("HPGL") || lower_path.endsWith(".hpgl")
        || lower_path.endsWith(".hpg") || lower_path.endsWith(".plt");
    const bool export_stl = selected_filter.startsWith("STL") || lower_path.endsWith(".stl");
    if (QFileInfo(path).suffix().isEmpty()) {
        path += export_step ? ".step"
            : export_iges ? ".iges"
            : export_dxf ? ".dxf"
            : export_eps ? ".eps"
            : export_hpgl ? ".hpgl"
            : export_stl ? ".stl"
            : ".obj";
    }

    bool exported = false;
    if (export_step) {
        exported = step_io_.Export(path.toStdString(), document_, error);
    } else if (export_iges) {
        exported = iges_io_.Export(path.toStdString(), document_, error);
    } else if (export_dxf) {
        exported = dxf_io_.Export(path.toStdString(), document_, error);
    } else if (export_eps) {
        exported = eps_io_.Export(path.toStdString(), document_, error);
    } else if (export_hpgl) {
        exported = hpgl_io_.Export(path.toStdString(), document_, error);
    } else if (export_stl) {
        exported = stl_io_.Export(path.toStdString(), document_, error);
    } else {
        const ObjLengthUnit unit = static_cast<ObjLengthUnit>(
            export_unit->currentData().toInt());
        exported = obj_io_.Export(
            path.toStdString(), document_, error, unit);
        if (exported) {
            export_settings.setValue(
                "export/objLengthUnit", static_cast<int>(unit));
        }
    }

    if (!exported) {
        const QString title = export_step ? "STEP Export"
            : export_iges ? "IGES Export"
            : export_dxf ? "DXF Export"
            : export_eps ? "EPS Export"
            : export_hpgl ? "HPGL Export"
            : export_stl ? "STL Export"
            : "OBJ Export";
        QMessageBox::critical(this, title, QString::fromStdString(error));
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

    RecordDocumentChange("Duplicate object");

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
    RecordDocumentChange("Mirror object");
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
        && solid_display_mode <= static_cast<int>(SolidDisplayMode::HiddenLineHatch)) {
        CSolid::SetDisplayMode(static_cast<SolidDisplayMode>(solid_display_mode));
    }
    const int mesh_display_mode = settings.value("view/meshDisplayMode", static_cast<int>(MeshDisplayMode::SurfaceGray)).toInt();
    if (mesh_display_mode >= static_cast<int>(MeshDisplayMode::SurfaceGray)
        && mesh_display_mode <= static_cast<int>(MeshDisplayMode::SurfaceMaterial)) {
        const MeshDisplayMode saved_mode =
            static_cast<MeshDisplayMode>(mesh_display_mode);
        CMesh3D::SetDisplayMode(
            saved_mode == MeshDisplayMode::Wire
                ? MeshDisplayMode::SurfaceMaterial
                : saved_mode);
    }
    const QVariant legacy_mesh_opacity = settings.value("view/meshWireOpacity", 1.0);
    CMesh3D::SetSurfaceOpacity(
        settings.value("view/meshSurfaceOpacity", legacy_mesh_opacity).toFloat());
    CSolid::SetEdgeDrawingEnabled(settings.value("view/drawSolidEdges", true).toBool());
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

void MainWindow::RestoreUserInterfaceSettings() {
    QSettings settings;
    const QByteArray geometry = settings.value("ui/mainWindowGeometry").toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    const QByteArray state = settings.value("ui/mainWindowState").toByteArray();
    if (!state.isEmpty()) {
        restoreState(state, 1);
    }

    const QString active_room = settings.value("ui/activeRoom", "Architecture").toString();
    if (tool_tabs_) {
        for (int index = 0; index < tool_tabs_->count(); ++index) {
            if (tool_tabs_->tabData(index).toString() == active_room) {
                tool_tabs_->setCurrentIndex(index);
                break;
            }
        }
    }
}

void MainWindow::SaveUserInterfaceSettings() {
    QSettings settings;
    settings.setValue("ui/mainWindowGeometry", saveGeometry());
    settings.setValue("ui/mainWindowState", saveState(1));
    if (tool_tabs_ && tool_tabs_->currentIndex() >= 0) {
        settings.setValue(
            "ui/activeRoom",
            tool_tabs_->tabData(tool_tabs_->currentIndex()).toString());
    }
    settings.sync();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    SaveUserInterfaceSettings();
    QMainWindow::closeEvent(event);
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
    if (active_parametric_object_.tool_id == "NurbsParameters") {
        AcceptNurbsParameterChanges();
    }
    undo_redo_.BeginChange();
    if (document_.DeleteSelectedPoint() || document_.DeleteSelectedObject()) {
        tool_registry_.RebuildArchitectureRooms(document_);
        undo_redo_.CommitChange("Delete selected");
        UpdateUndoRedoActions();
        ClearActiveProperties();
        RefreshSceneTree();
        viewport_->update();
    } else {
        undo_redo_.CancelChange();
    }
}

void MainWindow::RecordDocumentChange(const std::string& command_name) {
    undo_redo_.RecordChange(command_name);
    UpdateUndoRedoActions();
}

void MainWindow::UndoDocumentChange() {
    if (active_parametric_object_.tool_id == "NurbsParameters") {
        CancelNurbsParameterChanges();
    }
    const std::string name = undo_redo_.UndoName();
    if (!undo_redo_.Undo()) {
        return;
    }
    ClearActiveProperties();
    RefreshSceneTree();
    viewport_->update();
    UpdateUndoRedoActions();
    statusBar()->showMessage(
        name.empty() ? "Undo" : QString("Undo: %1").arg(QString::fromStdString(name)),
        1400);
}

void MainWindow::RedoDocumentChange() {
    if (active_parametric_object_.tool_id == "NurbsParameters") {
        CancelNurbsParameterChanges();
    }
    const std::string name = undo_redo_.RedoName();
    if (!undo_redo_.Redo()) {
        return;
    }
    ClearActiveProperties();
    RefreshSceneTree();
    viewport_->update();
    UpdateUndoRedoActions();
    statusBar()->showMessage(
        name.empty() ? "Redo" : QString("Redo: %1").arg(QString::fromStdString(name)),
        1400);
}

void MainWindow::UpdateUndoRedoActions() {
    if (undo_action_) {
        const std::string name = undo_redo_.UndoName();
        undo_action_->setText(name.empty()
            ? "&Undo"
            : QString("&Undo %1").arg(QString::fromStdString(name)));
        undo_action_->setEnabled(undo_redo_.CanUndo());
    }
    if (redo_action_) {
        const std::string name = undo_redo_.RedoName();
        redo_action_->setText(name.empty()
            ? "&Redo"
            : QString("&Redo %1").arg(QString::fromStdString(name)));
        redo_action_->setEnabled(undo_redo_.CanRedo());
    }
}

void MainWindow::PopulateToolsPanelForTab(int tab_index) {
    if (!tools_dock_ || !tools_layout_ || !tool_tabs_) {
        return;
    }

    viewport_->EndDirectCurveEdit();
    ClearActiveProperties();
    tool_buttons_.erase(
        std::remove_if(tool_buttons_.begin(), tool_buttons_.end(), [](QAbstractButton* button) {
            return !button || !button->property("persistentToolButton").toBool();
        }),
        tool_buttons_.end());
    while (QLayoutItem* item = tools_layout_->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    const QString tab = tool_tabs_->tabData(tab_index).toString();
    tools_dock_->setWindowTitle(tool_tabs_->tabText(tab_index));
    tools_dock_->setVisible(tab == "Architecture" || tab == "Furniture" || tab == "Surfaces" || tab == "Solid" || tab == "Curves" || tab == "Mesh 3D");

    if (tab == "Curves") {
        const auto add_curve_section =
            [this](const QString& title,
                   const QString& settings_key,
                   const std::vector<std::string>& ids,
                   int row) {
                auto* section = new QWidget(tools_panel_);
                auto* section_layout = new QVBoxLayout(section);
                section_layout->setContentsMargins(0, 0, 0, 2);
                section_layout->setSpacing(3);

                QSettings settings;
                const bool expanded = settings.value(settings_key, true).toBool();
                auto* header = new QToolButton(section);
                header->setText(title);
                header->setCheckable(true);
                header->setChecked(expanded);
                header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
                header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
                header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                header->setStyleSheet(
                    "QToolButton { text-align: left; font-weight: 600; padding: 3px; "
                    "border: 1px solid #9b9b9b; background: #e9e9e9; }"
                    "QToolButton:hover { background: #dceaff; }");
                section_layout->addWidget(header);

                auto* content = new QWidget(section);
                auto* grid = new QGridLayout(content);
                grid->setContentsMargins(5, 3, 0, 4);
                grid->setHorizontalSpacing(5);
                grid->setVerticalSpacing(5);
                for (size_t index = 0; index < ids.size(); ++index) {
                    AddToolButton(grid, content, ids[index],
                                  static_cast<int>(index / 2),
                                  static_cast<int>(index % 2));
                }
                content->setVisible(expanded);
                section_layout->addWidget(content);
                connect(header, &QToolButton::toggled, section,
                        [header, content, settings_key](bool checked) {
                            header->setArrowType(
                                checked ? Qt::DownArrow : Qt::RightArrow);
                            content->setVisible(checked);
                            QSettings settings;
                            settings.setValue(settings_key, checked);
                        });
                tools_layout_->addWidget(section, row, 0, 1, 2);
            };

        add_curve_section(
            "Create Curves", "tools/curves/createExpanded",
            {"PolylineCurve", "BSplineCurve", "DrawSpline", "BezierCurve3D", "NurbsCurve3D",
             "PlaneIntersection", "SurfaceIntersection", "ProjectCurveToSurface",
             "ExtractSurfaceEdge"}, 0);
        add_curve_section(
            "Edit Curves", "tools/curves/editExpanded",
            {"EditPoint", "NurbsParametersTool", "CurveFillets", "CurveJoin", "CurveSplit",
             "CurveExtend", "CurveTrimByPlane", "CurveSimplifyByPoint",
             "CurveReverse"}, 1);
        tools_layout_->setRowStretch(2, 1);
        UpdateActiveToolUi(active_tool_key_);
        return;
    }

    std::vector<std::string> tool_ids;
    if (tab == "Architecture") {
        tool_ids = {"room", "window", "door"};
    } else if (tab == "Furniture") {
        tool_ids = {"chair_simple", "chair", "cabinet", "cabinet_advanced",
                    "cabinet_showcase", "cabinet_advanced_slx",
                    "table", "desk", "drawer_box", "single_drawer",
                    "single_facade", "kitchen_nika_260",
                    "kitchen_corner"};
    } else if (tab == "Mesh 3D") {
        tool_ids = {"MeshFillContour", "SolidLowPoly", "TrimMeshTest", "ClassifyFaceCut"};
    } else if (tab == "Surfaces") {
        tool_ids = {"PlaneTool", "SurfaceRuled", "SurfaceLoft", "SurfaceSweepTwoRails", "SurfaceFourSplines", "SurfaceJoin", "SurfaceReverseNormals", "SurfaceOfRevolution"};
    } else if (tab == "Solid") {
        tool_ids = {"SolidBeamTool", "SolidBox", "SolidCylinder", "SolidSphereTool", "SolidTorusTool", "SolidPrismTool", "SolidExtrudeTool", "SolidTwoSketches", "SolidSketchFeature", "SolidSweptTool", "SolidSweepTwoRails", "SolidFrameTool", "SolidWireTool", "SolidPolyhedronTool", "TrimByPlane", "TrimBySketch", "TrimBySurface", "SurfaceOfRevolution", "boolean", "fillet_edge", "ChamferSolid", "SolidExtrudeFace", "SolidOffsetFace", "SolidDraft", "SolidSheetBend", "ThickSolidTool", "SolidShell"};
    }

    int index = 0;
    for (const std::string& tool_id : tool_ids) {
        AddToolButton(tools_layout_, tools_panel_, tool_id, index / 2, index % 2);
        ++index;
    }

    if (tab == "Solid") {
        const std::vector<std::pair<QString, QString>> placeholders = {
            {"DeleteFaceOrEdge", "Delete Face or Edge"},
            {"ExtractFaceTool", "Extract Face"},
            {"SewingFaceTool", "Sew Faces"},
            {"SplitRings", "Split Rings"},
            {"SolidInSet", "Inset"},
            {"SolidTransform", "Solid Transform"}
        };
        for (const auto& placeholder : placeholders) {
            if (placeholder.first == "SewingFaceTool") {
                AddToolButton(
                    tools_layout_, tools_panel_,
                    placeholder.first.toStdString(), index / 2, index % 2);
            } else {
                AddPlaceholderButton(
                    tools_layout_, tools_panel_, placeholder.first,
                    placeholder.second, index / 2, index % 2);
            }
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
