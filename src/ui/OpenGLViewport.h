#pragma once

#include "QtSceneRenderer.h"
#include "ToolRegistry.h"

#include "../Material.h"
#include "../Dimens.h"
#include "../Point3d.h"

#include <QOpenGLWidget>
#include <QColor>
#include <QRectF>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <vector>

class QKeyEvent;
class QLabel;

class OpenGLViewport : public QOpenGLWidget {
    Q_OBJECT

public:
    enum class SketchPlane {
        XY,
        XZ,
        YZ
    };

    explicit OpenGLViewport(QWidget* parent = nullptr);

    void SetDocument(CAlfaDoc* document);
    void SetTool(ToolMode tool);
    void SetDrawSplineSimplification(int percent);
    void SetTransformOperation(TransformOperation operation);
    bool ApplyPreciseMove(Vec3 delta);
    bool ApplyPreciseRotate(Vec3 center, Vec3 axis, float angle_radians);
    bool ApplyPreciseScale(Vec3 center, float factor_x, float factor_y, float factor_z, bool uniform);
    void SetTransformDialogGuide(TransformOperation operation,
                                 TransformAxis axis,
                                 Vec3 center,
                                 float rotation_angle_degrees = 0.0f,
                                 Vec3 custom_rotation_axis = {});
    void ClearTransformDialogGuide();
    void BeginFaceExtrudeTool(double taper_angle_degrees = 0.0);
    void BeginDraftFaceTool();
    void BeginThickSolidTool(double thickness);
    void SetThickSolidThickness(double thickness);
    void BeginBooleanTool(BooleanOperation operation);
    SelectionMode GetSelectionMode() const;
    void SetSelectionMode(SelectionMode mode);
    void SetSelectionConfirmationMode(bool enabled);
    void FitToDocument();
    ToolMode CurrentTool() const;
    bool IsOrthographicProjection() const;
    void SetOrthographicProjection(bool enabled);
    Camera GetCamera() const;
    void SetCamera(const Camera& camera);
    float GetVerticalFovDegrees() const;
    void SetVerticalFovDegrees(float degrees);
    void SetRotationPivot(CPoint3d point);
    void ClearRotationPivot();
    bool HasRotationPivot() const;
    void SetXYView();
    OrbitMode GetOrbitMode() const;
    void SetOrbitMode(OrbitMode mode);
    bool IsXYPlaneViewEnabled() const;
    void SetXYPlaneViewEnabled(bool enabled);
    bool IsCoordinateAxesVisible() const;
    void SetCoordinateAxesVisible(bool visible);
    bool IsFloorGridVisible() const;
    void SetFloorGridVisible(bool visible);
    void SetBackgroundColor(const QColor& color);
    Vec3 GetBackgroundColor() const;
    void ReloadModelingPreferences();
    void RefreshSurfaceMeshQuality();
    void BeginMaterialPaint(const Material& material);
    void BeginMaterialPick();
    void CancelMaterialInteraction();
    void BeginSketch(const QString& name, SketchPlane plane);
    void BeginSketchFaceSelection(const QString& name);
    void BeginSketchOnFace(const QString& name,
                           Vec3 origin,
                           Vec3 x_axis,
                           Vec3 y_axis,
                           Vec3 normal,
                           unsigned long body_id,
                           int face_index);
    bool BeginEditSelectedSketch();
    void EndDirectCurveEdit();
    void SetSketchRectangleTool();
    void SetSketchPolylineTool();
    void SetSketchBezierTool();
    void BeginSketchConvertLineToBezier();
    void BeginSketchConvertLineToArc();
    void BeginSketchFillet(double radius);
    void BeginSketchConstraintHorizontal();
    void BeginSketchConstraintVertical();
    void BeginSketchConstraintTangentStart();
    void BeginSketchConstraintTangentEnd();
    void SetSketchFilletRadius(double radius);
    void BeginSolidBoxRectangle(SketchPlane plane);
    void BeginSolidBoxFaceSelection();
    void BeginSolidCylinderCircle(SketchPlane plane);
    void BeginSolidCylinderFaceSelection();
    void SetSolidDimensionEdit(
        const ActiveParametricObject& active_object,
        const QString& primary_parameter = {});
    void SetSolidDimensionEdits(
        const std::vector<ActiveParametricObject>& active_objects,
        size_t primary_operation_index);
    void SetCabinetPreviewVisible(bool visible);
    void ClearSolidDimensionEdit();
    void BeginPickXYPoint(const QString& prompt = {});
    void BeginPick3DPoint(const QString& prompt = {});
    void BeginPick3DPointOnObject(unsigned long object_id, const QString& prompt = {});
    void BeginPickArchitectureWall(const QString& prompt = {});
    void CancelArchitectureWallPick();
    void SetPointPickMarkers(const std::vector<CPoint3d>& points);
    void ClearPointPickMarkers();
    void BeginPickRotationAxis();
    void BeginMovePointToPoint(bool repeat = false);
    void BeginMeasurePointToPoint();
    void EndSketch();
    void DrawFPS();
    void UpdateFPS();
    bool TakeCurvePointDragChange(
        unsigned long& object_id,
        std::vector<CPoint3d>& before,
        std::vector<CPoint3d>& after);
    bool TakeObjectMoveChange(
        std::vector<unsigned long>& object_ids,
        Vec3& delta);
    bool TakeMaterialDropChange(
        unsigned long& object_id,
        Material& before_material,
        unsigned long& before_material_id,
        Material& after_material,
        unsigned long& after_material_id);

signals:
    void FirstFrameRendered();
    void DocumentChanged();
    void SelectionChanged();
    void SelectionConfirmed();
    void SelectionCommandCanceled();
    void StatusTextChanged(const QString& text);
    void CursorWorldPositionChanged(double x, double y, double z, bool valid);
    void BooleanFinished();
    void MaterialPicked(const Material& material);
    void ToolModeChanged(ToolMode tool);
    void CameraFieldOfViewChanged(float degrees);
    void XYPointPicked(CPoint3d point);
    void XYPointPickCanceled();
    void Point3DPicked(CPoint3d point);
    void Point3DPickFinished();
    void Point3DPickCloseRequested();
    void Point3DPickCanceled();
    void ArchitectureWallPicked(unsigned long wall_id, CPoint3d point);
    void ArchitectureWallPickCanceled();
    void RotationAxisPicked(CPoint3d start, CPoint3d end);
    void RotationAxisPickCanceled();
    void SolidBoxRectangleFinished(std::vector<ToolParameter> parameters);
    void SolidCylinderCircleFinished(std::vector<ToolParameter> parameters);
    void SketchFaceSelectionFinished(bool selected);
    void SolidDimensionEditRequested(int operation_index, QString parameter_id, double current_value);
    void SolidDimensionGripChanged(int operation_index, QString parameter_id, double value, bool finished);
    void EdgeQuickMenuRequested(QPoint global_position);
    void FaceQuickMenuRequested(QPoint global_position);
    void ObjectQuickMenuRequested(QPoint global_position);
    void ViewportPopupMenuRequested(QPoint global_position);
    void ObjectDoubleClicked();
    void FurnitureInteractionRequested(unsigned long object_id);
    void PointToPointMeasurementFinished(double distance_mm);
    void FilesDropped(const QStringList& paths);

protected:
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    enum class SketchHandleKind {
        None,
        Node,
        Fillet,
        BezierControl,
        ArcControl
    };

    void SelectAt(const QPoint& point, SelectionAction action);
    void DrawCurveAt(const QPoint& point);
    void DrawBSplineAt(const QPoint& point);
    void BeginDrawSplineStroke(const QPoint& point);
    void AppendDrawSplineStroke(const QPoint& point);
    void UpdateDrawSplinePreview();
    void FinishDrawSplineStroke();
    void CancelDrawSplineStroke();
    void HandleSketchRectangleClick(const QPoint& point);
    void HandleSketchPolylineClick(const QPoint& point);
    void HandleSketchBezierClick(const QPoint& point);
    void HandleSketchConvertLineToBezierClick(const QPoint& point);
    void HandleSketchConvertLineToArcClick(const QPoint& point);
    void HandleSolidBoxRectangleClick(const QPoint& point);
    void HandleSolidCylinderCircleClick(const QPoint& point);
    void HandleSketchFilletClick(const QPoint& point);
    void HandleSketchConstraintClick(const QPoint& point);
    bool ScreenToSketchPlane(const QPoint& point, CPoint3d& result) const;
    bool SnapCreationPoint(const QPoint& point,
                           CPoint3d& result,
                           bool require_sketch_plane) const;
    bool PickModelingPoint(const QPoint& point, CPoint3d& result) const;
    bool HitTestRotationAxisLine(
        const QPoint& point, Vec3& start, Vec3& end) const;
    void DrawRotationAxisPickPreview();
    bool SnapSketchGridPoint(const QPoint& point,
                             Vec3 origin,
                             Vec3 u_axis,
                             Vec3 v_axis,
                             CPoint3d& result,
                             float& best_distance) const;
    void RestoreDefaultToolCursor();
    void SetCreationSnapCursor(bool snapped);
    std::vector<CPoint3d> SketchRectanglePoints(const CPoint3d& first, const CPoint3d& second) const;
    CPoint3d AlignSketchPolylinePoint(const CPoint3d& point) const;
    bool IsNearSketchPolylineFirstPoint(const QPoint& point) const;
    bool IsNearSelectedSketchFirstPoint(const QPoint& point) const;
    bool CommitSketchPolyline(bool closed);
    bool CommitSketchBezier();
    void ApplyPendingSketchAttachment();
    std::vector<ToolParameter> SolidBoxParametersFromRectangle(const CPoint3d& first, const CPoint3d& second) const;
    std::vector<ToolParameter> SolidCylinderParametersFromCircle(const CPoint3d& center, const CPoint3d& radius_point) const;
    void DrawSketchRectanglePreview();
    void DrawSolidCylinderCirclePreview();
    void DrawSketchFilletRadiusPreview();
    void DrawSketchPolylinePreview();
    void DrawSketchBezierPreview();
    void HandleBooleanClick(const QPoint& point);
    void HandleFaceExtrudeClick(const QPoint& point);
    void HandleFaceExtrudeDrag(const QPoint& point);
    void CommitFaceExtrudeDrag();
    bool HitTestFaceExtrudeGizmo(const QPoint& point) const;
    void HandleDraftFaceClick(const QPoint& point);
    void HandleDraftFaceDrag(const QPoint& point);
    void CommitDraftFaceDrag();
    bool HitTestDraftFaceGizmo(const QPoint& point) const;
    void HandleThickSolidClick(const QPoint& point);
    void HandleTransformClick(const QPoint& point, bool add_to_selection);
    void HandleMovePointToPointClick(const QPoint& point);
    void HandleMeasurePointToPointClick(const QPoint& point);
    void HandleTransformDrag(const QPoint& point, Qt::KeyboardModifiers modifiers);
    void CommitTransformDrag();
    TransformAxis HitTestTransformGizmo(const QPoint& point) const;
    bool HitTestSelectedPolylineHandle(const QPoint& point, size_t* point_index = nullptr) const;
    bool HitTestSelectedSketchHandle(const QPoint& point, SketchHandleKind& kind, size_t& index) const;
    void DrawSketchEditHandles();
    void BeginCurvePointDrag(const CPoint3d& point);
    bool CurrentSelectedCurvePlane(Vec3& plane_point, Vec3& plane_normal) const;
    bool ScreenToWorldPlane(const QPoint& point, Vec3 plane_point, Vec3 plane_normal, CPoint3d& result) const;
    bool ScreenToCurvePlane(const QPoint& point, CPoint3d& result);
    bool ScreenToPlaneY(const QPoint& point, double y, CPoint3d& result) const;
    bool ScreenToViewPlane(const QPoint& point, Vec3 plane_point, CPoint3d& result) const;
    void DrawCurveRubberBand();
    void DrawSplinePreview();
    void DrawSelectedCurvePointHandles();
    void UpdateHoveredSolidEdge(const QPoint& point);
    void ClearHoveredSolidEdge();
    void DrawHoveredSolidEdge();
    void DrawEditPointSelectionRect();
    void DrawSelectRubberBandRect();
    void DrawZoomRubberBandRect();
    bool ApplyZoomRect();
    Vec3 AxisVector(TransformAxis axis) const;
    float DistanceToScreenSegment(DomPoint point, DomPoint start, DomPoint end) const;
    void DrawCoordinateAxisLabels();
    void DrawCabinetPreview();
    void DrawSolidDimensions();
    void DrawPointToPointMeasurement();
    void DrawPointPickMarkers();
    struct WalkRoomFootprint {
        Vec3 minimum{};
        Vec3 maximum{};
    };
    std::vector<WalkRoomFootprint> WalkRoomFootprints() const;
    QRectF WalkMiniMapRect() const;
    bool WalkMiniMapTransform(QRectF& content_rect,
                              Vec3& world_minimum,
                              Vec3& world_maximum,
                              std::vector<WalkRoomFootprint>& footprints) const;
    void DrawWalkMiniMap();
    bool PlaceWalkCameraFromMiniMap(const QPoint& point);
    void MoveWalkCamera(int key, Qt::KeyboardModifiers modifiers);
    bool ApplyMaterialDrop(const QPoint& point, const Material& material);
    CAlfaObject* FindObjectForMaterialAt(const QPoint& point);
    void CaptureCurvePointChangeBefore();
    void FinalizeCurvePointChange();

    enum class MaterialInteractionMode {
        None,
        Paint,
        Pick
    };

    CAlfaDoc* document_ = nullptr;
    QLabel* walk_mini_map_overlay_ = nullptr;
    QtSceneRenderer renderer_;
    Camera camera_;
    Vec3 rotation_pivot_previous_target_{};
    bool rotation_pivot_enabled_ = false;
    ToolMode tool_ = ToolMode::Orbit;
    TransformOperation transform_operation_ = TransformOperation::Move;
    BooleanOperation boolean_operation_ = BooleanOperation::Union;
    SelectionMode selection_mode_ = SelectionMode::Object;
    TransformAxis highlighted_transform_axis_ = TransformAxis::None;
    TransformAxis active_transform_axis_ = TransformAxis::None;
    float transform_dialog_rotation_angle_degrees_ = 0.0f;
    Vec3 transform_dialog_rotation_axis_{};
    QPoint last_mouse_;
    bool orbiting_ = false;
    bool alt_orbiting_ = false;
    bool panning_ = false;
    bool alt_navigation_modifier_down_ = false;
    bool pan_navigation_modifier_down_ = false;
    bool zooming_ = false;
    QPoint right_button_press_{};
    bool right_button_dragged_ = false;
    bool xy_plane_view_enabled_ = false;
    bool dragging_transform_ = false;
    bool dragging_face_extrude_ = false;
    bool dragging_draft_face_ = false;
    bool editing_polyline_ = false;
    bool editing_sketch_ = false;
    bool dragging_polyline_point_ = false;
    bool curve_point_drag_changed_ = false;
    bool curve_point_drag_change_pending_ = false;
    unsigned long curve_point_drag_object_id_ = 0;
    std::vector<CPoint3d> curve_point_drag_before_points_;
    std::vector<CPoint3d> curve_point_drag_after_points_;
    unsigned long direct_curve_edit_object_id_ = 0;
    bool dragging_sketch_handle_ = false;
    bool sketch_drag_changed_ = false;
    SketchHandleKind active_sketch_handle_kind_ = SketchHandleKind::None;
    size_t active_sketch_handle_index_ = 0;
    SketchHandleKind highlighted_sketch_handle_kind_ = SketchHandleKind::None;
    size_t highlighted_sketch_handle_index_ = 0;
    bool curve_preview_valid_ = false;
    CPoint3d curve_preview_point_{};
    bool drawing_spline_stroke_ = false;
    int draw_spline_simplification_ = 50;
    std::vector<QPoint> draw_spline_screen_points_;
    std::vector<CPoint3d> draw_spline_raw_points_;
    std::vector<CPoint3d> draw_spline_preview_points_;
    double polyline_drag_plane_y_ = 0.0;
    Vec3 curve_point_drag_anchor_{};
    Vec3 curve_point_drag_plane_point_{};
    Vec3 curve_point_drag_plane_normal_{};
    CPoint3d curve_point_drag_last_{};
    bool curve_point_drag_has_plane_ = false;
    bool selecting_edit_points_ = false;
    bool selecting_with_rect_ = false;
    bool zoom_rect_active_ = false;
    bool selection_confirmation_mode_ = false;
    bool picking_xy_point_ = false;
    bool picking_3d_point_ = false;
    bool picking_architecture_wall_ = false;
    unsigned long point_pick_object_id_ = 0;
    bool picking_rotation_axis_ = false;
    bool rotation_axis_hover_valid_ = false;
    Vec3 rotation_axis_hover_start_{};
    Vec3 rotation_axis_hover_end_{};
    enum class MovePointStage { SelectObjects, PickSource, PickTarget };
    MovePointStage move_point_stage_ = MovePointStage::SelectObjects;
    CPoint3d move_point_source_{};
    bool move_point_repeat_ = false;
    SelectionAction edit_point_selection_action_ = SelectionAction::Replace;
    SelectionAction rect_selection_action_ = SelectionAction::Replace;
    QPoint edit_point_selection_start_;
    QPoint edit_point_selection_current_;
    QPoint rect_selection_start_;
    QPoint rect_selection_current_;
    QPoint zoom_rect_start_;
    QPoint zoom_rect_current_;
    bool highlighted_draft_face_gizmo_ = false;
    bool highlighted_polyline_handle_ = false;
    bool hovering_furniture_handle_ = false;
    size_t hovered_edge_object_index_ = static_cast<size_t>(-1);
    int hovered_edge_surface_index_ = -1;
    int hovered_edge_index_ = -1;
    Vec3 face_extrude_center_{};
    Vec3 face_extrude_normal_{};
    float face_extrude_distance_ = 0.0f;
    double face_extrude_taper_angle_degrees_ = 0.0;
    Vec3 draft_face_axis_center_{};
    Vec3 draft_face_axis_dir_{};
    double draft_face_angle_degrees_ = 0.0;
    double draft_face_start_mouse_angle_ = 0.0;
    double thick_solid_thickness_ = 1.0;
    double sketch_fillet_radius_ = 1.0;
    bool sketch_fillet_preview_valid_ = false;
    CPoint3d sketch_fillet_preview_center_{};
    bool transform_drag_has_preview_ = false;
    Vec3 transform_drag_center_{};
    Vec3 transform_drag_axis_{};
    Vec3 transform_drag_move_delta_{};
    float transform_drag_rotation_input_angle_ = 0.0f;
    float transform_drag_rotation_angle_ = 0.0f;
    float transform_drag_scale_factor_ = 1.0f;
    bool object_move_change_pending_ = false;
    std::vector<unsigned long> object_move_change_ids_;
    Vec3 object_move_change_delta_{};
    bool has_boolean_body_ = false;
    bool orthographic_projection_ = false;
    bool show_coordinate_axes_ = true;
    bool show_floor_grid_ = true;
    float grid_size_ = kDefaultSceneSize;
    float grid_step_ = 100.0f;
    int grid_subdivisions_ = 4;
    int grid_division_count_ = 10;
    bool material_drag_active_ = false;
    bool material_drop_change_pending_ = false;
    unsigned long material_drop_object_id_ = 0;
    Material material_drop_before_;
    unsigned long material_drop_before_id_ = 0;
    Material material_drop_after_;
    unsigned long material_drop_after_id_ = 0;
    bool sketch_active_ = false;
    bool sketch_waiting_for_face_ = false;
    bool solid_box_waiting_for_face_ = false;
    unsigned long solid_box_target_body_id_ = 0;
    bool sketch_rectangle_has_first_point_ = false;
    bool sketch_rectangle_preview_valid_ = false;
    bool sketch_polyline_preview_valid_ = false;
    bool sketch_bezier_preview_valid_ = false;
    bool sketch_arc_has_line_ = false;
    std::size_t sketch_arc_line_index_ = 0;
    bool highlighted_sketch_fillet_point_ = false;
    double sketch_alignment_angle_degrees_ = 7.0;
    bool snapping_enabled_ = true;
    int capture_distance_pixels_ = 6;
    bool creation_snap_active_ = false;
    bool measurement_waiting_for_second_point_ = false;
    bool measurement_visible_ = false;
    bool measurement_preview_valid_ = false;
    CPoint3d measurement_start_{};
    CPoint3d measurement_end_{};
    CPoint3d measurement_preview_{};
    std::vector<CPoint3d> point_pick_markers_;
    QPoint material_drag_pos_;
    QString sketch_name_;
    Vec3 sketch_origin_{};
    Vec3 sketch_u_{1.0f, 0.0f, 0.0f};
    Vec3 sketch_v_{0.0f, 1.0f, 0.0f};
    Vec3 sketch_normal_{0.0f, 0.0f, 1.0f};
    unsigned long sketch_attachment_body_id_ = 0;
    int sketch_attachment_face_index_ = -1;
    CPoint3d sketch_rectangle_first_point_{};
    CPoint3d sketch_rectangle_preview_point_{};
    CPoint3d sketch_polyline_preview_point_{};
    std::vector<CPoint3d> sketch_polyline_points_;
    CPoint3d sketch_bezier_preview_point_{};
    std::vector<CPoint3d> sketch_bezier_points_;
    struct SolidDimensionHit {
        QRect rect;
        QString parameter_id;
        double value = 0.0;
        bool is_label = false;
        bool is_grip = false;
        DomPoint line_start{};
        DomPoint line_end{};
        size_t source_index = 0;
    };
    ActiveParametricObject solid_dimension_object_;
    std::vector<ActiveParametricObject> solid_dimension_objects_;
    std::vector<CDimens3D> solid_dimensions_;
    std::vector<SolidDimensionHit> solid_dimension_hits_;
    QString solid_dimension_primary_parameter_;
    bool cabinet_preview_visible_ = false;
    QString highlighted_solid_dimension_grip_;
    int highlighted_solid_dimension_operation_index_ = -1;
    QString active_solid_dimension_grip_;
    int active_solid_dimension_operation_index_ = -1;
    bool dragging_solid_dimension_grip_ = false;
    QPoint solid_dimension_drag_start_mouse_;
    double solid_dimension_drag_start_value_ = 0.0;
    double solid_dimension_drag_minimum_ = 0.0;
    double solid_dimension_drag_maximum_ = 0.0;
    double solid_dimension_drag_step_ = 0.1;
    QPointF solid_dimension_drag_screen_direction_;
    double solid_dimension_drag_screen_length_ = 1.0;
    double solid_dimension_drag_current_value_ = 0.0;
    QPoint edge_quick_menu_anchor_;
    unsigned int edge_quick_menu_generation_ = 0;
    Material material_drag_preview_;
    MaterialInteractionMode material_interaction_mode_ = MaterialInteractionMode::None;
    Material active_paint_material_;
    OrbitMode orbit_mode_ = OrbitMode::CAD;
    size_t boolean_body_index_ = 0;
    int    m_frameCounter = 0;
    float  m_fps = 0.0f;
	int m_lastFpsTime = 0;
    bool first_frame_rendered_ = false;
};
