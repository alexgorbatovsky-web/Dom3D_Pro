#pragma once

#include "OpenGLViewport.h"
#include "PropertyPanel.h"
#include "ToolRegistry.h"

#include "../ObjIO.h"
#include "../ProjectIO.h"
#include "../Dom3DProjectSerializer.h"
#include "../IgesIO.h"
#include "../StepIO.h"

#include <QIcon>
#include <QMainWindow>
#include <QStringList>

#include <functional>
#include <string>
#include <vector>

class QAction;
class QCheckBox;
class QDockWidget;
class QGridLayout;
class QLabel;
class QMenu;
class QPushButton;
class QSlider;
class QTabBar;
class QTreeWidget;
class QTreeWidgetItem;
class QToolBar;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void CreateActions();
    void CreateDocks();
    void CreateMaterialLibraryDock();
    void CreateToolsPanel(QDockWidget* dock);
    void PopulateToolsPanelForTab(int tab_index);
    void AddToolButton(QGridLayout* layout, QWidget* parent, const std::string& key, int row, int column);
    void AddPlaceholderButton(QGridLayout* layout, QWidget* parent, const QString& icon_key, const QString& title, int row, int column);
    void RefreshSceneTree();
    void OnSceneTreeItemClicked(QTreeWidgetItem* item, int column);
    void SetTool(ToolMode tool, const QString& status_text);
    void SetSolidDisplayMode(SolidDisplayMode mode);
    void SetMeshDisplayMode(MeshDisplayMode mode);
    void SetMeshWireOpacity(float opacity);
    void SetOrthographicProjection(bool enabled);
    void SetOrbitMode(OrbitMode mode);
    void SetXYPlaneViewEnabled(bool enabled);
    void SetCoordinateAxesVisible(bool visible);
    void UpdateProjectionStatus();
    void ShowMaterialEditor(const Material* initial_material = nullptr, const QString& material_file_path = {});
    void SaveMaterialToDocument(const Material& material);
    void ApplyMaterialToSelection(const Material& material);
    void BeginTransformTool(TransformOperation operation);
    void BeginNewSketch();
    void BeginSketchFillet();
    void ShowSketchPanel();
    void ActivateParametricTool(const std::string& tool_id);
    bool TryStartLiveEdgeToolFromSelection();
    bool TryStartLivePolylineExtrudeFromSelection();
    bool TryStartLivePolylineRevolveFromSelection();
    void ClearActiveProperties();
    void AcceptActiveProperties();
    void CancelActiveProperties();
    void ShowPropertyPanelAtCursor(const QString& title);
    void RegisterToolAction(QAction* action, const std::string& key);
    void RegisterToolButton(QPushButton* button, const std::string& key);
    void UpdateActiveToolUi(const std::string& key);
    void UpdateToolAvailability();
    QIcon ToolIcon(const std::string& key) const;
    void NewProject();
    void OpenProject();
    void OpenProjectFromPath(const QString& path);
    void SaveProject();
    void ShowPreferences();
    void ImportFile();
    void ExportFile();
    void DuplicateSelectedObject();
    void DeleteSelected();
    void LoadUserSettings();
    void RememberLastDialogDir(const QString& path);
    QString LastDialogDir() const;
    void AddRecentProjectFile(const QString& path);
    void UpdateRecentFilesMenu();
    void ClearRecentProjectFiles();
    void ShowGreetingDialog(bool force = false);

    OpenGLViewport* viewport_ = nullptr;
    QTreeWidget* scene_tree_ = nullptr;
    PropertyPanel* property_panel_ = nullptr;
    QDockWidget* tools_dock_ = nullptr;
    QDockWidget* scene_tree_dock_ = nullptr;
    QDockWidget* material_library_dock_ = nullptr;
    QDockWidget* sketch_dock_ = nullptr;
    QWidget* tools_panel_ = nullptr;
    QGridLayout* tools_layout_ = nullptr;
    QDockWidget* properties_dock_ = nullptr;
    QTabBar* tool_tabs_ = nullptr;
    QToolBar* tab_toolbar_ = nullptr;
    QToolBar* main_toolbar_ = nullptr;
    QMenu* recent_files_menu_ = nullptr;
    QAction* orthographic_projection_action_ = nullptr;
    QAction* cad_orbit_action_ = nullptr;
    QAction* architectural_orbit_action_ = nullptr;
    QAction* surfaces_edges_action_ = nullptr;
    QAction* mesh_only_action_ = nullptr;
    QAction* surfaces_wire_action_ = nullptr;
    QCheckBox* coordinate_axes_check_box_ = nullptr;
    QCheckBox* xy_plane_view_check_box_ = nullptr;
    QPushButton* mesh_display_button_ = nullptr;
    QMenu* mesh_display_menu_ = nullptr;
    QSlider* mesh_opacity_slider_ = nullptr;
    QLabel* mesh_opacity_value_label_ = nullptr;
    QLabel* projection_status_label_ = nullptr;
    Material selected_library_material_;
    bool has_selected_library_material_ = false;
    std::function<void()> refresh_material_library_;
    std::vector<QAction*> tool_actions_;
    std::vector<QPushButton*> tool_buttons_;
    QStringList recent_project_files_;
    QString last_file_dialog_dir_;
    std::string active_tool_key_ = "orbit";
    int sketch_counter_ = 3;

    CAlfaDoc document_;
    Dom3DProjectSerializer dom3d_serializer_;
    ProjectIO project_io_;
    ObjIO obj_io_;
    IgesIO iges_io_;
    StepIO step_io_;
    ToolRegistry tool_registry_;
    ActiveParametricObject active_parametric_object_;
    std::string project_path_;
};
