#include "MainWindow.h"

#include "../CMesh3D.h"
#include "../CPolyline.h"
#include "../solid/Solid.h"
#include "PreferencesDialog.h"

#include <QAction>
#include <QActionGroup>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCursor>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLayoutItem>
#include <QMenu>
#include <QMessageBox>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
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
#include <vector>

namespace {
constexpr int kMaxRecentProjectFiles = 18;
constexpr int kSceneTreeObjectIndexRole = Qt::UserRole + 1;
constexpr int kSceneTreeGroupRole = Qt::UserRole + 2;

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
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      viewport_(new OpenGLViewport(this)),
      property_panel_(new PropertyPanel(this)) {
    setWindowTitle("Dom3D Pro");
    resize(1280, 760);

    viewport_->SetDocument(&document_);
    setCentralWidget(viewport_);

    LoadUserSettings();
    CreateActions();
    CreateDocks();
    statusBar()->showMessage("Ready");

    connect(viewport_, &OpenGLViewport::DocumentChanged, this, [this]() {
        RefreshSceneTree();
    });
    connect(viewport_, &OpenGLViewport::SelectionChanged, this, [this]() {
        ClearActiveProperties();
        UpdateToolAvailability();
    });
    connect(viewport_, &OpenGLViewport::StatusTextChanged, this, [this](const QString& text) {
        statusBar()->showMessage(text);
    });
    connect(viewport_, &OpenGLViewport::BooleanFinished, this, [this]() {
        UpdateActiveToolUi("select");
    });
    connect(property_panel_, &PropertyPanel::ParametersChanged, this, [this]() {
        active_parametric_object_ = property_panel_->ActiveObject();
        if (active_parametric_object_.tool_id == "fillet_edge" || active_parametric_object_.tool_id == "fillet_all_edges") {
            statusBar()->showMessage("Fillet: нажми OK чтобы применить Radius");
            return;
        }
        tool_registry_.Rebuild(active_parametric_object_, document_);
        RefreshSceneTree();
        viewport_->update();
        statusBar()->showMessage("Object rebuilt", 1200);
    });
    connect(property_panel_, &PropertyPanel::Accepted, this, [this]() {
        AcceptActiveProperties();
    });
    connect(property_panel_, &PropertyPanel::Canceled, this, [this]() {
        CancelActiveProperties();
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
    tool_tabs_->addTab("Lines");
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
        document_.CreatePolyline();
        SetTool(ToolMode::DrawCurve, "Curve tool active: click in viewport");
        RefreshSceneTree();
    });
    auto* transform_action = add_action("Transform", {}, [this]() { BeginTransformTool(TransformOperation::Move); });
    auto* move_action = add_action("Move", {}, [this]() { BeginTransformTool(TransformOperation::Move); });
    auto* rotate_action = add_action("Rotate", {}, [this]() { BeginTransformTool(TransformOperation::Rotate); });
    auto* scale_action = add_action("Scale", {}, [this]() { BeginTransformTool(TransformOperation::Scale); });
    auto* all_scene_action = add_action("All Scene", {}, [this]() {
        viewport_->FitToDocument();
        statusBar()->showMessage("All scene fitted", 1400);
    });
    RegisterToolAction(orbit_action, "orbit");
    RegisterToolAction(select_action, "select");
    RegisterToolAction(curve_action, "curve");
    RegisterToolAction(transform_action, "transform");
    RegisterToolAction(move_action, "move");
    RegisterToolAction(rotate_action, "rotate");
    RegisterToolAction(scale_action, "scale");

    tools_menu->addAction(orbit_action);
    tools_menu->addAction(select_action);
    tools_menu->addAction(curve_action);
    tools_menu->addAction(transform_action);
    tools_menu->addSeparator();
    tools_menu->addAction(move_action);
    tools_menu->addAction(rotate_action);
    tools_menu->addAction(scale_action);
    tools_menu->addSeparator();
    for (const ToolDefinition& tool : tool_registry_.Tools()) {
        auto* action = tools_menu->addAction(QString::fromStdString(tool.label), this, [this, id = tool.id]() {
            ActivateParametricTool(id);
        });
        RegisterToolAction(action, tool.id);
    }

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
    main_toolbar_->addSeparator();
    main_toolbar_->addAction(move_action);
    main_toolbar_->addAction(rotate_action);
    main_toolbar_->addAction(scale_action);
    main_toolbar_->addSeparator();
    coordinate_axes_check_box_ = new QCheckBox("Axis", main_toolbar_);
    coordinate_axes_check_box_->setToolTip("Show coordinate axes");
    coordinate_axes_check_box_->setChecked(viewport_->IsCoordinateAxesVisible());
    connect(coordinate_axes_check_box_, &QCheckBox::toggled, this, [this](bool checked) {
        SetCoordinateAxesVisible(checked);
    });
    main_toolbar_->addWidget(coordinate_axes_check_box_);
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
    main_toolbar_->addAction(all_scene_action);
    UpdateActiveToolUi(active_tool_key_);
    UpdateRecentFilesMenu();
}

void MainWindow::CreateDocks() {
    tools_dock_ = new QDockWidget("Architecture", this);
    tools_dock_->setMinimumWidth(104);
    CreateToolsPanel(tools_dock_);
    addDockWidget(Qt::LeftDockWidgetArea, tools_dock_);

    auto* tree_dock = new QDockWidget("Scene Tree", this);
    scene_tree_ = new QTreeWidget(tree_dock);
    scene_tree_->setHeaderLabels({"Visible", "Object", "Type"});
    scene_tree_->setColumnWidth(0, 58);
    tree_dock->setWidget(scene_tree_);
    addDockWidget(Qt::RightDockWidgetArea, tree_dock);
    connect(scene_tree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int column) {
        OnSceneTreeItemClicked(item, column);
    });

    properties_dock_ = new QDockWidget("Property Panel", this);
    properties_dock_->setWidget(property_panel_);
    properties_dock_->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, properties_dock_);
    properties_dock_->hide();
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
    tools_dock_->setVisible(tab == "Architecture" || tab == "Furniture" || tab == "Solid");

    std::vector<std::string> tool_ids;
    if (tab == "Architecture") {
        tool_ids = {"stair", "window", "door"};
    } else if (tab == "Furniture") {
        tool_ids = {"cabinet"};
    } else if (tab == "Solid") {
        tool_ids = {"SolidBox", "boolean_union", "boolean_cut", "boolean_common", "fillet_edge", "fillet_all_edges"};
    }

    int index = 0;
    for (const std::string& tool_id : tool_ids) {
        AddToolButton(tools_layout_, tools_panel_, tool_id, index / 2, index % 2);
        ++index;
    }

    if (tab == "Solid") {
        const std::vector<std::pair<QString, QString>> placeholders = {
            {"SolidCylinder", "Cylinder"},
            {"SolidSphereTool", "Sphere"},
            {"SolidPrismTool", "Prism"},
            {"SolidExtrudeTool", "Extrude"},
            {"SolidExtrudeFace", "Extrude Face"},
            {"SolidTorusTool", "Torus"},
            {"SolidSweptTool", "Swept Solid"},
            {"SurfaceOfRevolution", "Revolution"},
            {"SolidDraft", "Draft"},
            {"ChamferSolid", "Chamfer"},
            {"DeleteFaceOrEdge", "Delete Face or Edge"},
            {"ExtractFaceTool", "Extract Face"},
            {"ThickSolidTool", "Thicken"},
            {"SewingFaceTool", "Sew Faces"},
            {"SplitRings", "Split Rings"},
            {"SolidInSet", "Inset"},
            {"SolidTransform", "Solid Transform"}
        };
        for (const auto& placeholder : placeholders) {
            AddPlaceholderButton(tools_layout_, tools_panel_, placeholder.first, placeholder.second, index / 2, index % 2);
            ++index;
        }
    }

    tools_layout_->setRowStretch((index + 1) / 2, 1);
    UpdateActiveToolUi(active_tool_key_);
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
    scene_tree_->clear();
    const auto& objects = document_.GetObjects();
    std::map<QString, QTreeWidgetItem*> group_items;

    for (size_t i = 0; i < objects.size(); ++i) {
        const CAlfaObject* object = objects[i].get();
        if (!object) {
            continue;
        }

        QString type = "Object";
        if (dynamic_cast<const CPolyline*>(object)) {
            type = "Curve";
        } else if (dynamic_cast<const CMesh3D*>(object)) {
            type = "Mesh";
        } else if (dynamic_cast<const CSolid*>(object)) {
            type = "Solid";
        }

        QTreeWidgetItem* parent = nullptr;
        const QString group_name = QString::fromStdString(object->GetGroupName());
        if (!group_name.isEmpty()) {
            auto existing_group = group_items.find(group_name);
            if (existing_group == group_items.end()) {
                auto* group_item = new QTreeWidgetItem(scene_tree_);
                group_item->setText(0, "👁");
                group_item->setText(1, group_name);
                group_item->setText(2, "Group");
                group_item->setData(0, kSceneTreeGroupRole, group_name);
                existing_group = group_items.emplace(group_name, group_item).first;
            }
            parent = existing_group->second;
        }

        auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(scene_tree_);
        item->setText(0, object->IsVisible() ? "👁" : "");
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
        bool any_hidden = false;
        for (int i = 0; i < group_item->childCount(); ++i) {
            const bool child_visible = !group_item->child(i)->text(0).isEmpty();
            any_visible = any_visible || child_visible;
            any_hidden = any_hidden || !child_visible;
        }
        group_item->setText(0, any_visible && any_hidden ? "◐" : any_visible ? "👁" : "");
    }

    scene_tree_->expandAll();
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
    ClearActiveProperties();
    viewport_->SetTool(tool);
    if (tool == ToolMode::Orbit) {
        UpdateActiveToolUi("orbit");
    } else if (tool == ToolMode::Select) {
        UpdateActiveToolUi("select");
    } else if (tool == ToolMode::DrawCurve) {
        UpdateActiveToolUi("curve");
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
    QSettings settings;
    settings.setValue("view/solidDisplayMode", static_cast<int>(mode));

    QString message = "Solid display: surfaces and edges";
    if (mode == SolidDisplayMode::MeshOnly) {
        message = "Solid display: mesh only";
    } else if (mode == SolidDisplayMode::SurfacesAndRaisedMesh) {
        message = "Solid display: surfaces and raised mesh";
    }
    viewport_->update();
    statusBar()->showMessage(message, 1400);
}

void MainWindow::SetMeshDisplayMode(MeshDisplayMode mode) {
    CMesh3D::SetDisplayMode(mode);
    if (mesh_display_button_) {
        QString button_text = "Surface Gray v";
        if (mode == MeshDisplayMode::SurfaceColored) {
            button_text = "Surface Colored v";
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
    if (orthographic_projection_action_ && orthographic_projection_action_->isChecked() != enabled) {
        orthographic_projection_action_->setChecked(enabled);
    }

    QSettings settings;
    settings.setValue("view/orthographicProjection", enabled);
    UpdateProjectionStatus();
    statusBar()->showMessage(enabled ? "Orthographic projection" : "Perspective projection", 1400);
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

void MainWindow::SetCoordinateAxesVisible(bool visible) {
    viewport_->SetCoordinateAxesVisible(visible);
    if (coordinate_axes_check_box_ && coordinate_axes_check_box_->isChecked() != visible) {
        coordinate_axes_check_box_->setChecked(visible);
    }

    QSettings settings;
    settings.setValue("view/showCoordinateAxes", visible);
    statusBar()->showMessage(visible ? "Coordinate axes shown" : "Coordinate axes hidden", 1400);
}

void MainWindow::UpdateProjectionStatus() {
    if (!projection_status_label_) {
        return;
    }
    projection_status_label_->setText(viewport_->IsOrthographicProjection() ? "[ORTHO]" : "[PERSPECTIVE]");
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

void MainWindow::ActivateParametricTool(const std::string& tool_id) {
    if (tool_id == "boolean_union" || tool_id == "boolean_cut" || tool_id == "boolean_common") {
        ClearActiveProperties();

        BooleanOperation operation = BooleanOperation::Union;
        if (tool_id == "boolean_cut") {
            operation = BooleanOperation::Cut;
        } else if (tool_id == "boolean_common") {
            operation = BooleanOperation::Common;
        }

        viewport_->BeginBooleanTool(operation);
        UpdateActiveToolUi(tool_id);
        return;
    }

    if (tool_id == "fillet_edge" || tool_id == "fillet_all_edges") {
        ClearActiveProperties();
        if (tool_id == "fillet_edge" && !document_.HasSelectedSolidEdge()) {
            UpdateActiveToolUi("select");
            viewport_->SetTool(ToolMode::Select);
            statusBar()->showMessage("Fillet: выбери кромку тела");
            return;
        }
        if (tool_id == "fillet_all_edges" && !document_.GetSelectedSolid()) {
            UpdateActiveToolUi("select");
            viewport_->SetTool(ToolMode::Select);
            statusBar()->showMessage("Fillet All: выбери тело");
            return;
        }

        active_parametric_object_ = {
            tool_id,
            document_.GetSelectedObjectIndex(),
            {{"radius", "Radius", 0.20, 0.01, 10.0, 0.01}}
        };
        property_panel_->SetActiveObject(active_parametric_object_);
        ShowPropertyPanelAtCursor(tool_id == "fillet_edge" ? "Fillet Edge" : "Fillet All Edges");
        UpdateActiveToolUi(tool_id);
        statusBar()->showMessage(tool_id == "fillet_edge" ? "Fillet: задай Radius и нажми OK" : "Fillet All: задай Radius и нажми OK");
        return;
    }

    active_parametric_object_ = tool_registry_.Activate(tool_id, document_);
    if (!active_parametric_object_.tool_id.empty()) {
        property_panel_->SetActiveObject(active_parametric_object_);
        ShowPropertyPanelAtCursor(QString::fromStdString(tool_id));
    } else {
        ClearActiveProperties();
    }
    viewport_->SetTool(ToolMode::Select);
    UpdateActiveToolUi(tool_id);
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage(QString("%1 applied").arg(QString::fromStdString(tool_id)));
}

void MainWindow::ClearActiveProperties() {
    active_parametric_object_ = {};
    property_panel_->Clear();
    if (properties_dock_) {
        properties_dock_->hide();
    }
}

void MainWindow::AcceptActiveProperties() {
    if (active_parametric_object_.tool_id == "fillet_edge" || active_parametric_object_.tool_id == "fillet_all_edges") {
        const double radius = active_parametric_object_.parameters.empty() ? 0.20 : active_parametric_object_.parameters[0].value;
        const bool all_edges = active_parametric_object_.tool_id == "fillet_all_edges";
        const bool applied = all_edges ? document_.ApplyFilletToAllSelectedSolidEdges(radius) : document_.ApplyFilletToSelectedEdge(radius);
        ClearActiveProperties();
        RefreshSceneTree();
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi("select");
        viewport_->update();
        statusBar()->showMessage(applied ? QString("%1: radius %2").arg(all_edges ? "Fillet All" : "Fillet").arg(radius, 0, 'f', 2) : "Fillet: операция не выполнена");
        return;
    }

    active_parametric_object_ = {};
    if (properties_dock_) {
        properties_dock_->hide();
    }
    viewport_->SetTool(ToolMode::Orbit);
    UpdateActiveToolUi("orbit");
    statusBar()->showMessage("Object parameters accepted", 1200);
}

void MainWindow::CancelActiveProperties() {
    if (active_parametric_object_.tool_id == "fillet_edge" || active_parametric_object_.tool_id == "fillet_all_edges") {
        ClearActiveProperties();
        viewport_->SetTool(ToolMode::Select);
        UpdateActiveToolUi("select");
        statusBar()->showMessage("Fillet canceled", 1200);
        return;
    }

    const size_t object_index = active_parametric_object_.object_index;
    auto& objects = document_.GetObjects();
    if (object_index < objects.size()) {
        objects.erase(objects.begin() + static_cast<CAlfaDoc::ObjectList::difference_type>(object_index));
        document_.ClearSelection();
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
    const bool has_selected_edge = document_.HasSelectedSolidEdge();

    const auto is_enabled = [has_selected_solid, has_selected_edge](const QString& key) {
        if (key == "fillet_edge") {
            return has_selected_edge;
        }
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
    if (icon_key == "boolean_union" || icon_key == "boolean_cut" || icon_key == "boolean_common") {
        icon_key = "BooleanSolid";
    } else if (icon_key == "fillet_edge" || icon_key == "fillet_all_edges") {
        icon_key = "FilletSolid";
    }
    return QIcon(QString(":/icons/%1.png").arg(icon_key));
}

void MainWindow::NewProject() {
    document_.Clear();
    project_path_.clear();
    ClearActiveProperties();
    RefreshSceneTree();
    viewport_->update();
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
    if (legacy_project) {
        std::string error;
        if (!project_io_.Load(path.toStdString(), document_, error)) {
            QMessageBox::critical(this, "Dom3D Pro", QString::fromStdString(error));
            return;
        }
    } else {
        QString error;
        QString active_room;
        if (!dom3d_serializer_.Load(path, document_, active_room, error)) {
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
    }

    project_path_ = path.toStdString();
    RememberLastDialogDir(path);
    AddRecentProjectFile(path);
    ClearActiveProperties();
    RefreshSceneTree();
    viewport_->FitToDocument();
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
    if (!dom3d_serializer_.Save(path, document_, active_room, error)) {
        QMessageBox::critical(this, "Dom3D Pro", error);
        return;
    }

    project_path_ = path.toStdString();
    RememberLastDialogDir(path);
    AddRecentProjectFile(path);
    statusBar()->showMessage("Project saved", 1400);
}

void MainWindow::ShowPreferences() {
    PreferencesDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        statusBar()->showMessage("Preferences applied", 1400);
    }
}

void MainWindow::ImportFile() {
    const QString filter = "Wavefront OBJ (*.obj);;STEP (*.step *.stp);;All files (*.*)";
    QString selected_filter;
    const QString path = QFileDialog::getOpenFileName(this, "Import", LastDialogDir(), filter, &selected_filter);
    if (path.isEmpty()) {
        return;
    }

    std::string error;
    const QString lower_path = path.toLower();
    if (selected_filter.startsWith("STEP") || lower_path.endsWith(".step") || lower_path.endsWith(".stp")) {
        std::vector<std::unique_ptr<CSolid>> solids;
        if (!step_io_.Import(path.toStdString(), solids, error)) {
            QMessageBox::critical(this, "STEP Import", QString::fromStdString(error));
            return;
        }
        for (auto& solid : solids) {
            document_.AddObject(std::move(solid));
        }
    } else {
        std::unique_ptr<CMesh3D> mesh;
        if (!obj_io_.Import(path.toStdString(), mesh, error)) {
            QMessageBox::critical(this, "OBJ Import", QString::fromStdString(error));
            return;
        }
        document_.AddMesh(std::move(mesh));
    }

    RememberLastDialogDir(path);
    ClearActiveProperties();
    RefreshSceneTree();
    viewport_->FitToDocument();
    viewport_->update();
    statusBar()->showMessage("File imported", 1400);
}

void MainWindow::ExportFile() {
    const QString filter = "Wavefront OBJ (*.obj);;STEP (*.step *.stp);;All files (*.*)";
    QString selected_filter;
    QString path = QFileDialog::getSaveFileName(this, "Export", LastDialogDir(), filter, &selected_filter);
    if (path.isEmpty()) {
        return;
    }

    std::string error;
    const QString lower_path = path.toLower();
    const bool export_step = selected_filter.startsWith("STEP") || lower_path.endsWith(".step") || lower_path.endsWith(".stp");
    if (QFileInfo(path).suffix().isEmpty()) {
        path += export_step ? ".step" : ".obj";
    }

    const bool exported = export_step
        ? step_io_.Export(path.toStdString(), document_, error)
        : obj_io_.Export(path.toStdString(), document_, error);

    if (!exported) {
        QMessageBox::critical(this, export_step ? "STEP Export" : "OBJ Export", QString::fromStdString(error));
        return;
    }

    RememberLastDialogDir(path);
    statusBar()->showMessage("File exported", 1400);
}

void MainWindow::LoadUserSettings() {
    QSettings settings;
    last_file_dialog_dir_ = settings.value("files/lastDir", QDir::homePath()).toString();
    recent_project_files_ = settings.value("files/recentProjects").toStringList();
    const int solid_display_mode = settings.value("view/solidDisplayMode", static_cast<int>(SolidDisplayMode::SurfacesAndEdges)).toInt();
    if (solid_display_mode >= static_cast<int>(SolidDisplayMode::SurfacesAndEdges)
        && solid_display_mode <= static_cast<int>(SolidDisplayMode::SurfacesAndRaisedMesh)) {
        CSolid::SetDisplayMode(static_cast<SolidDisplayMode>(solid_display_mode));
    }
    const int mesh_display_mode = settings.value("view/meshDisplayMode", static_cast<int>(MeshDisplayMode::SurfaceGray)).toInt();
    if (mesh_display_mode >= static_cast<int>(MeshDisplayMode::SurfaceGray)
        && mesh_display_mode <= static_cast<int>(MeshDisplayMode::Wire)) {
        CMesh3D::SetDisplayMode(static_cast<MeshDisplayMode>(mesh_display_mode));
    }
    CMesh3D::SetWireOpacity(settings.value("view/meshWireOpacity", 0.76).toFloat());
    CSolid::SetSurfaceTransparencyEnabled(settings.value("view/solidSurfaceTransparency", false).toBool());
    viewport_->SetOrthographicProjection(settings.value("view/orthographicProjection", false).toBool());
    const QString orbit_mode = settings.value("view/orbitMode", "cad").toString();
    viewport_->SetOrbitMode(orbit_mode == "architectural" ? OrbitMode::Architectural : OrbitMode::CAD);
    viewport_->SetCoordinateAxesVisible(settings.value("view/showCoordinateAxes", true).toBool());
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
