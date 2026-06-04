#include "MainWindow.h"

#include "../CMesh3D.h"
#include "../CPolyline.h"
#include "PreferencesDialog.h"

#include <QAction>
#include <QCursor>
#include <QDockWidget>
#include <QFileDialog>
#include <QIcon>
#include <QKeySequence>
#include <QMessageBox>
#include <QMenuBar>
#include <QPushButton>
#include <QSize>
#include <QStatusBar>
#include <QTabBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QVariant>

#include <memory>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      viewport_(new OpenGLViewport(this)),
      property_panel_(new PropertyPanel(this)) {
    setWindowTitle("Dom3D Pro");
    resize(1280, 760);

    viewport_->SetDocument(&document_);
    setCentralWidget(viewport_);

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
}

void MainWindow::CreateActions() {
    auto* file_menu = menuBar()->addMenu("&File");
    auto* tools_menu = menuBar()->addMenu("&Tools");
    auto* edit_menu = menuBar()->addMenu("&Edit");

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
    tool_tabs_->addTab("Body");
    tool_tabs_->addTab("Lines");
    tool_tabs_->addTab("Sketch");
    tool_tabs_->addTab("Assemblies");
    tool_tabs_->setCurrentIndex(0);
    tab_toolbar_->addWidget(tool_tabs_);
    connect(tool_tabs_, &QTabBar::currentChanged, this, [this](int index) {
        if (!tool_tabs_ || index < 0) {
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
    file_menu->addAction(add_action("&Save...", QKeySequence::Save, [this]() { SaveProject(); }));
    file_menu->addSeparator();
    file_menu->addAction(add_action("&Preferences...", {}, [this]() { ShowPreferences(); }));
    file_menu->addSeparator();
    file_menu->addAction(add_action("Import &OBJ...", {}, [this]() { ImportObj(); }));
    file_menu->addAction(add_action("Export O&BJ...", {}, [this]() { ExportObj(); }));
    file_menu->addSeparator();
    file_menu->addAction(add_action("E&xit", QKeySequence::Quit, [this]() { close(); }));

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

    main_toolbar_->addAction(orbit_action);
    main_toolbar_->addAction(select_action);
    main_toolbar_->addAction(curve_action);
    main_toolbar_->addAction(transform_action);
    main_toolbar_->addSeparator();
    main_toolbar_->addAction(move_action);
    main_toolbar_->addAction(rotate_action);
    main_toolbar_->addAction(scale_action);
    main_toolbar_->addSeparator();
    for (const ToolDefinition& tool : tool_registry_.Tools()) {
        auto* action = main_toolbar_->addAction(QString::fromStdString(tool.label), this, [this, id = tool.id]() {
            ActivateParametricTool(id);
        });
        RegisterToolAction(action, tool.id);
    }
    main_toolbar_->addSeparator();
    main_toolbar_->addAction(ToolIcon("save"), "Save", this, [this]() { SaveProject(); });
    main_toolbar_->addAction(ToolIcon("open"), "Open", this, [this]() { OpenProject(); });
    UpdateActiveToolUi(active_tool_key_);
}

void MainWindow::CreateDocks() {
    auto* tools_dock = new QDockWidget("Tools", this);
    CreateToolsPanel(tools_dock);
    addDockWidget(Qt::LeftDockWidgetArea, tools_dock);

    auto* tree_dock = new QDockWidget("Scene Tree", this);
    scene_tree_ = new QTreeWidget(tree_dock);
    scene_tree_->setHeaderLabels({"Object", "Type"});
    tree_dock->setWidget(scene_tree_);
    addDockWidget(Qt::RightDockWidgetArea, tree_dock);

    properties_dock_ = new QDockWidget("Property Panel", this);
    properties_dock_->setWidget(property_panel_);
    properties_dock_->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, properties_dock_);
    properties_dock_->hide();
}

void MainWindow::CreateToolsPanel(QDockWidget* dock) {
    auto* panel = new QWidget(dock);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto* orbit = new QPushButton("Orbit", panel);
    RegisterToolButton(orbit, "orbit");
    connect(orbit, &QPushButton::clicked, this, [this]() { SetTool(ToolMode::Orbit, "Orbit camera"); });
    layout->addWidget(orbit);

    auto* select = new QPushButton("Select", panel);
    RegisterToolButton(select, "select");
    connect(select, &QPushButton::clicked, this, [this]() { SetTool(ToolMode::Select, "Select objects"); });
    layout->addWidget(select);

    auto* curve = new QPushButton("Curve", panel);
    RegisterToolButton(curve, "curve");
    connect(curve, &QPushButton::clicked, this, [this]() {
        document_.CreatePolyline();
        SetTool(ToolMode::DrawCurve, "Curve tool active: click in viewport");
        RefreshSceneTree();
    });
    layout->addWidget(curve);

    auto* transform = new QPushButton("Transform", panel);
    RegisterToolButton(transform, "transform");
    connect(transform, &QPushButton::clicked, this, [this]() { BeginTransformTool(TransformOperation::Move); });
    layout->addWidget(transform);

    auto* move = new QPushButton("Move", panel);
    RegisterToolButton(move, "move");
    connect(move, &QPushButton::clicked, this, [this]() { BeginTransformTool(TransformOperation::Move); });
    layout->addWidget(move);

    auto* rotate = new QPushButton("Rotate", panel);
    RegisterToolButton(rotate, "rotate");
    connect(rotate, &QPushButton::clicked, this, [this]() { BeginTransformTool(TransformOperation::Rotate); });
    layout->addWidget(rotate);

    auto* scale = new QPushButton("Scale", panel);
    RegisterToolButton(scale, "scale");
    connect(scale, &QPushButton::clicked, this, [this]() { BeginTransformTool(TransformOperation::Scale); });
    layout->addWidget(scale);

    for (const ToolDefinition& tool : tool_registry_.Tools()) {
        auto* button = new QPushButton(QString::fromStdString(tool.label), panel);
        RegisterToolButton(button, tool.id);
        connect(button, &QPushButton::clicked, this, [this, id = tool.id]() { ActivateParametricTool(id); });
        layout->addWidget(button);
    }

    layout->addStretch();
    panel->setStyleSheet(
        "QPushButton:checked {"
        "  background-color: #2f80ed;"
        "  color: white;"
        "  border: 1px solid #1f5fbf;"
        "  font-weight: 600;"
        "}"
    );
    dock->setWidget(panel);
    UpdateActiveToolUi(active_tool_key_);
}

void MainWindow::RefreshSceneTree() {
    scene_tree_->clear();
    const auto& objects = document_.GetObjects();
    for (size_t i = 0; i < objects.size(); ++i) {
        const CAlfaObject* object = objects[i].get();
        QString type = "Object";
        if (dynamic_cast<const CPolyline*>(object)) {
            type = "Curve";
        } else if (dynamic_cast<const CMesh3D*>(object)) {
            type = "Mesh";
        }

        auto* item = new QTreeWidgetItem(scene_tree_);
        item->setText(0, QString::fromStdString(object->GetName()));
        item->setText(1, type);
        if (document_.IsObjectSelected(i)) {
            item->setSelected(true);
        }
    }
    scene_tree_->expandAll();
    UpdateToolAvailability();
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
    return QIcon(QString(":/icons/%1.png").arg(QString::fromStdString(key)));
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
    const QString path = QFileDialog::getOpenFileName(this, "Open Dom3D Project", QString(), "Dom3D Pro Project (*.d3dm);;All files (*.*)");
    if (path.isEmpty()) {
        return;
    }

    std::string error;
    if (!project_io_.Load(path.toStdString(), document_, error)) {
        QMessageBox::critical(this, "Dom3D Pro", QString::fromStdString(error));
        return;
    }

    project_path_ = path.toStdString();
    ClearActiveProperties();
    RefreshSceneTree();
    viewport_->update();
    statusBar()->showMessage("Project opened");
}

void MainWindow::SaveProject() {
    QString path = QString::fromStdString(project_path_);
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, "Save Dom3D Project", QString(), "Dom3D Pro Project (*.d3dm);;All files (*.*)");
    }
    if (path.isEmpty()) {
        return;
    }

    std::string error;
    if (!project_io_.Save(path.toStdString(), document_, error)) {
        QMessageBox::critical(this, "Dom3D Pro", QString::fromStdString(error));
        return;
    }

    project_path_ = path.toStdString();
    statusBar()->showMessage("Project saved", 1400);
}

void MainWindow::ShowPreferences() {
    PreferencesDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        statusBar()->showMessage("Preferences applied", 1400);
    }
}

void MainWindow::ImportObj() {
    const QString path = QFileDialog::getOpenFileName(this, "Import OBJ", QString(), "Wavefront OBJ (*.obj);;All files (*.*)");
    if (path.isEmpty()) {
        return;
    }

    std::unique_ptr<CMesh3D> mesh;
    std::string error;
    if (!obj_io_.Import(path.toStdString(), mesh, error)) {
        QMessageBox::critical(this, "OBJ Import", QString::fromStdString(error));
        return;
    }

    document_.AddMesh(std::move(mesh));
    ClearActiveProperties();
    RefreshSceneTree();
    viewport_->update();
}

void MainWindow::ExportObj() {
    const QString path = QFileDialog::getSaveFileName(this, "Export OBJ", QString(), "Wavefront OBJ (*.obj);;All files (*.*)");
    if (path.isEmpty()) {
        return;
    }

    std::string error;
    if (!obj_io_.Export(path.toStdString(), document_, error)) {
        QMessageBox::critical(this, "OBJ Export", QString::fromStdString(error));
    }
}

void MainWindow::DeleteSelected() {
    if (document_.DeleteSelectedPoint() || document_.DeleteSelectedObject()) {
        ClearActiveProperties();
        RefreshSceneTree();
        viewport_->update();
    }
}
