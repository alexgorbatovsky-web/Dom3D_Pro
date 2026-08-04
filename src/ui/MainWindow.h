#pragma once

// Make sure the file is saved in UTF-8 without BOM or in ANSI (Windows-1251) if the project uses Russian characters in comments or strings.
// Open the file in the editor, select "Save As..." and specify the desired encoding (e.g., UTF-8 without BOM).
// If using Visual Studio: File -> Advanced -> Encoding -> Save with Encoding -> UTF-8 without signature.
#include "OpenGLViewport.h"
#include "PropertyPanel.h"
#include "ToolRegistry.h"

#include "../ObjIO.h"
#include "../ThreeDSIO.h"
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
class QAbstractButton;
class QCheckBox;
class QCloseEvent;
class QDockWidget;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QGridLayout;
class QLabel;
class QMenu;
class QPushButton;
class QSlider;
class QTabBar;
class QTreeWidget;
class QTreeWidgetItem;
class QToolBar;
class QToolButton;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void CreateActions();
    void CreateDocks();
    void CreateVerticalToolBar();
    void CreateMaterialLibraryDock();
    void CreateToolsPanel(QDockWidget* dock);
    void PopulateToolsPanelForTab(int tab_index);
    void AddToolButton(QGridLayout* layout, QWidget* parent, const std::string& key, int row, int column);
    void AddPlaceholderButton(QGridLayout* layout, QWidget* parent, const QString& icon_key, const QString& title, int row, int column);
    void RefreshSceneTree();
    void OnSceneTreeItemClicked(QTreeWidgetItem* item, int column);
    void OnSceneTreeItemDoubleClicked(QTreeWidgetItem* item, int column);
    void SetTool(ToolMode tool, const QString& status_text);
    void SetSolidDisplayMode(SolidDisplayMode mode);
    void SetMeshDisplayMode(MeshDisplayMode mode);
    void SetMeshWireOpacity(float opacity);
    void ToggleWireShadedDisplay();
    void SetOrthographicProjection(bool enabled);
    void SetOrbitMode(OrbitMode mode);
    void SetXYPlaneViewEnabled(bool enabled);
    void SetCoordinateAxesVisible(bool visible);
    void SetFloorGridVisible(bool visible);
    void UpdateProjectionStatus();
    void ShowMaterialEditor(const Material* initial_material = nullptr, const QString& material_file_path = {});
    void ShowSurfaceTextureEditor();
    void RequestObjectColor();
    void EditSelectedObjectColor();
    void ShowLayerProperties();
    void ChangeSelectedObjectLayer();
    void CreateSelectedGroup();
    void UngroupSelectedGroup();
    void CreateSelectedAssembly();
    void CreateBodyFromTwoSketches();
    void CreateAssociativeClone();
    void CancelPendingGroupCommand(const QString& status_text = {});
    bool HasSelectedGroup() const;
    void SaveMaterialToDocument(const Material& material);
    void ApplyMaterialToSelection(const Material& material);
    void BeginTransformTool(TransformOperation operation);
    void BeginNewSketch();
    void BeginSolidBox();
    void BeginSolidCylinder();
    void BeginSketchFillet();
    void ShowSketchPanel();
    void ActivateParametricTool(const std::string& tool_id);
    bool TryApplyPendingTrim();
    void CancelPendingTrim(const QString& status_text = {});
    void ShowLowPolyTool();
    void ShowMeshFillContourTool();
    void ShowTrimMeshTestTool();
    void ShowClassifyFaceCutTool();
    void EditSelectedParametricObject();
    bool ActiveParametricObjectIsAssembly() const;
    bool TryStartLiveEdgeToolFromSelection();
    bool TryStartLivePolylineExtrudeFromSelection();
    bool TryStartLivePolylineRevolveFromSelection();
    void ClearActiveProperties();
    void AcceptActiveProperties();
    void CancelActiveProperties();
    void ShowPropertyPanelAtCursor(const QString& title);
    void RegisterToolAction(QAction* action, const std::string& key);
    void RegisterToolButton(QAbstractButton* button, const std::string& key);
    void UpdateActiveToolUi(const std::string& key);
    void UpdateToolAvailability();
    QIcon ToolIcon(const std::string& key) const;
    void NewProject();
    void OpenProject();
    void OpenProjectFromPath(const QString& path);
    void SaveProject(bool save_as = false);
    void SaveProjectAs();
    QString SelectProjectToOpen();
    QImage CaptureProjectThumbnail() const;
    void UpdateWindowTitle();
    void ShowPreferences();
    void ImportFile();
    bool ImportFileFromPath(const QString& path);
    void HandleDroppedFiles(const QStringList& paths);
    void ExportFile();
    void DuplicateSelectedObject();
    void MirrorSelectedObject();
    void DeleteSelected();
    void LoadUserSettings();
    void RestoreUserInterfaceSettings();
    void SaveUserInterfaceSettings();
    void RememberLastDialogDir(const QString& path);
    QString LastDialogDir() const;
    void AddRecentProjectFile(const QString& path);
    void UpdateRecentFilesMenu();
    void ClearRecentProjectFiles();
    void ShowGreetingDialog(bool force = false);

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    enum class PendingGroupCommand {
        None,
        Create,
        Ungroup,
        CreateAssembly,
        TwoSketchBody,
        AssociativeClone
    };

    OpenGLViewport* viewport_ = nullptr;
    QTreeWidget* scene_tree_ = nullptr;
    PropertyPanel* property_panel_ = nullptr;
    QDockWidget* tools_dock_ = nullptr;
    QDockWidget* vertical_tools_dock_ = nullptr;
    QDockWidget* scene_tree_dock_ = nullptr;
    QDockWidget* material_library_dock_ = nullptr;
    QDockWidget* sketch_dock_ = nullptr;
    QWidget* tools_panel_ = nullptr;
    QGridLayout* tools_layout_ = nullptr;
    QDockWidget* properties_dock_ = nullptr;
    QTabBar* tool_tabs_ = nullptr;
    QToolBar* tab_toolbar_ = nullptr;
    QToolBar* main_toolbar_ = nullptr;
    QToolBar* vertical_toolbar_ = nullptr;
    QMenu* recent_files_menu_ = nullptr;
    QAction* orthographic_projection_action_ = nullptr;
    QAction* cad_orbit_action_ = nullptr;
    QAction* architectural_orbit_action_ = nullptr;
    QAction* surfaces_edges_action_ = nullptr;
    QAction* mesh_only_action_ = nullptr;
    QAction* surfaces_wire_action_ = nullptr;
    QAction* solid_wireframe_action_ = nullptr;
    QAction* solid_hidden_line_action_ = nullptr;
    QCheckBox* coordinate_axes_check_box_ = nullptr;
    QCheckBox* floor_grid_check_box_ = nullptr;
    QCheckBox* xy_plane_view_check_box_ = nullptr;
    QAbstractButton* edit_texture_button_ = nullptr;
    QSlider* mesh_opacity_slider_ = nullptr;
    QLabel* mesh_opacity_value_label_ = nullptr;
    QLabel* projection_status_label_ = nullptr;
    Material selected_library_material_;
    bool has_selected_library_material_ = false;
    std::function<void()> refresh_material_library_;
    std::vector<QAction*> tool_actions_;
    std::vector<QAbstractButton*> tool_buttons_;
    QStringList recent_project_files_;
    QString last_file_dialog_dir_;
    std::string active_tool_key_ = "orbit";
    int sketch_counter_ = 3;

    CAlfaDoc document_;
    Dom3DProjectSerializer dom3d_serializer_;
    ProjectIO project_io_;
    ObjIO obj_io_;
    ThreeDSIO three_ds_io_;
    IgesIO iges_io_;
    StepIO step_io_;
    ToolRegistry tool_registry_;
    ActiveParametricObject active_parametric_object_;
    ActiveParametricObject solid_body_dimension_object_;
    bool active_parametric_edit_existing_ = false;
    bool solid_body_edit_mode_ = false;
    bool solid_body_dimensions_modified_ = false;
    bool reopen_solid_editor_after_properties_ = false;
    bool object_color_pick_pending_ = false;
    bool low_poly_pick_pending_ = false;
    bool edge_tool_started_from_face_quick_menu_ = false;
    std::string pending_trim_tool_id_;
    PendingGroupCommand pending_group_command_ = PendingGroupCommand::None;
    BooleanOperation last_boolean_operation_ = BooleanOperation::Union;
    std::string project_path_;
};
