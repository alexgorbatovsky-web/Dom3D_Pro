#pragma once

// Make sure the file is saved in UTF-8 without BOM or in ANSI (Windows-1251) if the project uses Russian characters in comments or strings.
// Open the file in the editor, select "Save As..." and specify the desired encoding (e.g., UTF-8 without BOM).
// If using Visual Studio: File -> Advanced -> Encoding -> Save with Encoding -> UTF-8 without signature.
#include "OpenGLViewport.h"
#include "PropertyPanel.h"
#include "ToolRegistry.h"
#include "../UndoRedo.h"

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
class QDialog;
class QDoubleSpinBox;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QGridLayout;
class QLabel;
class QMenu;
class QPushButton;
class QSlider;
class QTabBar;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
class QToolBar;
class QToolButton;
enum class ReferenceImageAxis;

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
    void SetMeshSurfaceOpacity(float opacity);
    void ToggleWireShadedDisplay();
    void SetOrthographicProjection(bool enabled);
    void SetOrbitMode(OrbitMode mode);
    void SetXYPlaneViewEnabled(bool enabled);
    void SetCoordinateAxesVisible(bool visible);
    void SetFloorGridVisible(bool visible);
    void UpdateProjectionStatus();
    void ShowMaterialEditor(const Material* initial_material = nullptr, const QString& material_file_path = {});
    void ShowSurfaceTextureEditor();
    void ShowViewportPopupMenu(const QPoint& global_position);
    void RequestObjectColor();
    void EditSelectedObjectColor();
    void ShowLayerProperties();
    void ChangeSelectedObjectLayer();
    void CreateSelectedGroup();
    void UngroupSelectedGroup();
    void CreateSelectedAssembly();
    void CreateBodyFromTwoSketches();
    void CreateAssociativeClone();
    void JoinSelectedSurfaces();
    void CreatePlaneIntersection();
    void CreateSurfaceIntersection();
    void ProjectCurveToSurface();
    void ExtractSurfaceEdge();
    void CreateFourSplineSurface();
    void CreateTwoRailSweepSurface();
    void CreateTwoRailSweepSolid();
    void CancelPendingGroupCommand(const QString& status_text = {});
    bool HasSelectedGroup() const;
    void SaveMaterialToDocument(const Material& material);
    void ApplyMaterialToSelection(const Material& material);
    void BeginTransformTool(TransformOperation operation);
    void ShowPreciseMoveDialog();
    void BeginMoveTwoPointEntry();
    void ShowPreciseRotateDialog();
    void ShowPreciseScaleDialog();
    void BeginNewSketch();
    enum class SpatialCurveKind {
        None,
        Polyline,
        BSpline,
        Bezier,
        Nurbs
    };
    void BeginSpatialCurve(SpatialCurveKind kind);
    void AppendSpatialCurvePoint(CPoint3d point);
    void FinishSpatialCurve();
    void CloseSpatialCurve();
    void CancelSpatialCurve();
    QString SpatialCurvePrompt() const;
    bool UpdateNurbsParameterEditor();
    void ApplyNurbsParameterChanges();
    void AcceptNurbsParameterChanges();
    void CancelNurbsParameterChanges();
    void BeginPlaneThreePointPick();
    void AppendPlaneThreePointPick(CPoint3d point);
    void CancelPlaneThreePointPick(const QString& message = {});
    bool CompletePendingPlaneFacePick();
    enum class CurveEditCommand {
        None,
        Join,
        Split,
        Extend,
        TrimByPlane,
        SimplifyByPoint,
        Reverse
    };
    void BeginCurveEditCommand(CurveEditCommand command);
    bool PrepareCurveEditCommandSelection();
    void CompleteCurveEditPoint(CPoint3d point);
    void CancelCurveEditCommand(const QString& message = {});
    bool JoinSelectedCurves();
    bool SplitSelectedCurves(CPoint3d near_point);
    bool ExtendSelectedCurve(CPoint3d endpoint_hint);
    bool TrimSelectedCurveByPlane(CPoint3d keep_point);
    bool SimplifySelectedCurveByPoint(CPoint3d split_point);
    bool ReverseSelectedCurves();
    void BeginSolidBox();
    void BeginSolidCylinder();
    void BeginSketchFillet();
    void BeginDrawSpline();
    void ShowDrawSplineDialog();
    void ShowSketchPanel();
    void ActivateParametricTool(const std::string& tool_id);
    bool TryApplyPendingTrim();
    void CancelPendingTrim(const QString& status_text = {});
    void ShowLowPolyTool();
    void ShowMeshFillContourTool();
    void ShowTrimMeshTestTool();
    void ShowClassifyFaceCutTool();
    void EditSelectedParametricObject();
    void UpdateSolidBodyDimensions();
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
    void AddReferenceImage(ReferenceImageAxis axis);
    bool ImportFileFromPath(const QString& path);
    void HandleDroppedFiles(const QStringList& paths);
    void ExportFile();
    void DuplicateSelectedObject();
    void MirrorSelectedObject();
    void DeleteSelected();
    void RecordDocumentChange(const std::string& command_name);
    void UndoDocumentChange();
    void RedoDocumentChange();
    void UpdateUndoRedoActions();
    void LoadUserSettings();
    void RestoreUserInterfaceSettings();
    void SaveUserInterfaceSettings();
    void RememberLastDialogDir(const QString& path);
    QString LastDialogDir() const;
    void AddRecentProjectFile(const QString& path);
    void UpdateRecentFilesMenu();
    void ClearRecentProjectFiles();
    void ShowGreetingDialog(bool force = false);
    void StartFurnitureInteraction(unsigned long object_id);
    void AdvanceFurnitureAnimation();
    void RestoreFurnitureAnimationSelection();

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
        AssociativeClone,
        JoinSurfaces,
        PlaneIntersection,
        SurfaceIntersection,
        ProjectCurveToSurface,
        ExtractSurfaceEdge,
        FourSplineSurface,
        TwoRailSweepSurface,
        TwoRailSweepSolid
    };

    enum class PendingPreciseTransform {
        None,
        Move,
        Rotate,
        Scale
    };

    enum class PendingTransformPointPick {
        None,
        ScaleBasePoint,
        RotationPivot
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
    QAction* undo_action_ = nullptr;
    QAction* redo_action_ = nullptr;
    QAction* cad_orbit_action_ = nullptr;
    QAction* architectural_orbit_action_ = nullptr;
    QAction* surfaces_edges_action_ = nullptr;
    QAction* mesh_only_action_ = nullptr;
    QAction* surfaces_wire_action_ = nullptr;
    QAction* solid_wireframe_action_ = nullptr;
    QAction* solid_hidden_line_action_ = nullptr;
    QAction* solid_hidden_line_hatch_action_ = nullptr;
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
    QDialog* sketch_fillet_dialog_ = nullptr;
    QDialog* draw_spline_dialog_ = nullptr;
    QSlider* draw_spline_simplification_slider_ = nullptr;
    QLabel* draw_spline_simplification_value_ = nullptr;
    QDialog* precise_move_dialog_ = nullptr;
    QDialog* precise_rotate_dialog_ = nullptr;
    QDoubleSpinBox* sketch_fillet_radius_spin_ = nullptr;

    CAlfaDoc document_;
    CUndoRedo undo_redo_;
    Dom3DProjectSerializer dom3d_serializer_;
    ProjectIO project_io_;
    ObjIO obj_io_;
    ThreeDSIO three_ds_io_;
    IgesIO iges_io_;
    StepIO step_io_;
    ToolRegistry tool_registry_;
    ActiveParametricObject active_parametric_object_;
    ActiveParametricObject solid_body_dimension_object_;
    size_t solid_body_edit_object_index_ = static_cast<size_t>(-1);
    int solid_body_selected_operation_index_ = 0;
    bool solid_body_all_dimensions_ = false;
    bool active_parametric_edit_existing_ = false;
    bool solid_body_edit_mode_ = false;
    bool solid_body_dimensions_modified_ = false;
    bool reopen_solid_editor_after_properties_ = false;
    SpatialCurveKind spatial_curve_kind_ = SpatialCurveKind::None;
    unsigned long spatial_curve_object_id_ = 0;
    size_t spatial_curve_point_count_ = 0;
    std::vector<CPoint3d> spatial_curve_interpolation_points_;
    unsigned long nurbs_parameter_object_id_ = 0;
    int nurbs_parameter_original_degree_ = 3;
    std::vector<double> nurbs_parameter_original_weights_;
    std::vector<double> nurbs_parameter_original_knots_;
    double nurbs_parameter_displayed_weight_ = 1.0;
    bool nurbs_parameters_modified_ = false;
    CurveEditCommand pending_curve_edit_command_ = CurveEditCommand::None;
    std::vector<CPoint3d> pending_curve_trim_plane_points_;
    bool plane_three_point_pick_active_ = false;
    bool plane_three_point_method_selected_ = false;
    std::vector<CPoint3d> plane_three_point_picks_;
    bool pending_reference_plane_face_pick_ = false;
    bool pending_trim_plane_face_pick_ = false;
    unsigned long pending_trim_plane_curve_id_ = 0;
    bool object_color_pick_pending_ = false;
    bool low_poly_pick_pending_ = false;
    bool edge_tool_started_from_face_quick_menu_ = false;
    QTimer* furniture_animation_timer_ = nullptr;
    ActiveParametricObject furniture_animation_object_;
    int furniture_animation_frame_ = 0;
    double furniture_animation_start_value_ = 0.0;
    double furniture_animation_end_value_ = 0.0;
    double furniture_animation_final_drawer_ = -1.0;
    double furniture_animation_saved_distance_ = 0.0;
    double furniture_animation_preview_value_ = 0.0;
    Vec3 furniture_animation_preview_center_{0.0f, 0.0f, 0.0f};
    Vec3 furniture_animation_preview_axis_{0.0f, 0.0f, 1.0f};
    float furniture_animation_preview_rotation_sign_ = 0.0f;
    std::vector<unsigned long> furniture_animation_preview_ids_;
    std::vector<unsigned long> furniture_animation_selection_ids_;
    std::string furniture_animation_parameter_id_;
    std::string pending_trim_tool_id_;
    PendingGroupCommand pending_group_command_ = PendingGroupCommand::None;
    PendingPreciseTransform pending_precise_transform_ = PendingPreciseTransform::None;
    PendingTransformPointPick pending_transform_point_pick_ = PendingTransformPointPick::None;
    Vec3 precise_rotate_axis_start_{};
    Vec3 precise_rotate_axis_end_{};
    bool precise_rotate_axis_ready_ = false;
    Vec3 precise_scale_base_point_{};
    bool precise_scale_base_point_ready_ = false;
    BooleanOperation last_boolean_operation_ = BooleanOperation::Union;
    std::string project_path_;
};
