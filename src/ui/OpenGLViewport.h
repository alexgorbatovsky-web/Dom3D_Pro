#pragma once

#include "QtSceneRenderer.h"
#include "ToolRegistry.h"

#include "../Material.h"
#include "../Dimens.h"
#include "../Point3d.h"

#include <QOpenGLWidget>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <vector>

class QKeyEvent;

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
    void SetTransformOperation(TransformOperation operation);
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
    void SetXYView();
    OrbitMode GetOrbitMode() const;
    void SetOrbitMode(OrbitMode mode);
    bool IsXYPlaneViewEnabled() const;
    void SetXYPlaneViewEnabled(bool enabled);
    bool IsCoordinateAxesVisible() const;
    void SetCoordinateAxesVisible(bool visible);
    bool IsFloorGridVisible() const;
    void SetFloorGridVisible(bool visible);
    void ReloadModelingPreferences();
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
    void SetSketchRectangleTool();
    void SetSketchPolylineTool();
    void SetSketchBezierTool();
    void BeginSketchConvertLineToBezier();
    void BeginSketchConvertLineToArc();
    void BeginSketchFillet(double radius);
    void BeginSolidBoxRectangle(SketchPlane plane);
    void BeginSolidBoxFaceSelection();
    void BeginSolidCylinderCircle(SketchPlane plane);
    void BeginSolidCylinderFaceSelection();
    void SetSolidDimensionEdit(
        const ActiveParametricObject& active_object,
        const QString& primary_parameter = {});
    void ClearSolidDimensionEdit();
    void BeginPickXYPoint();
    void BeginMovePointToPoint();
    void EndSketch();
    void DrawFPS();
    void UpdateFPS();

signals:
    void DocumentChanged();
    void SelectionChanged();
    void SelectionConfirmed();
    void SelectionCommandCanceled();
    void StatusTextChanged(const QString& text);
    void BooleanFinished();
    void MaterialPicked(const Material& material);
    void ToolModeChanged(ToolMode tool);
    void XYPointPicked(CPoint3d point);
    void SolidBoxRectangleFinished(std::vector<ToolParameter> parameters);
    void SolidCylinderCircleFinished(std::vector<ToolParameter> parameters);
    void SketchFaceSelectionFinished(bool selected);
    void SolidDimensionEditRequested(QString parameter_id, double current_value);
    void EdgeQuickMenuRequested(QPoint global_position);
    void FaceQuickMenuRequested(QPoint global_position);
    void ObjectQuickMenuRequested(QPoint global_position);
    void ObjectDoubleClicked();
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
    void HandleSketchRectangleClick(const QPoint& point);
    void HandleSketchPolylineClick(const QPoint& point);
    void HandleSketchBezierClick(const QPoint& point);
    void HandleSketchConvertLineToBezierClick(const QPoint& point);
    void HandleSketchConvertLineToArcClick(const QPoint& point);
    void HandleSolidBoxRectangleClick(const QPoint& point);
    void HandleSolidCylinderCircleClick(const QPoint& point);
    void HandleSketchFilletClick(const QPoint& point);
    bool ScreenToSketchPlane(const QPoint& point, CPoint3d& result) const;
    bool SnapCreationPoint(const QPoint& point,
                           CPoint3d& result,
                           bool require_sketch_plane) const;
    bool SnapSketchGridPoint(const QPoint& point,
                             Vec3 origin,
                             Vec3 u_axis,
                             Vec3 v_axis,
                             CPoint3d& result,
                             float& best_distance) const;
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
    void DrawSelectedCurvePointHandles();
    void DrawEditPointSelectionRect();
    void DrawSelectRubberBandRect();
    void DrawZoomRubberBandRect();
    bool ApplyZoomRect();
    Vec3 AxisVector(TransformAxis axis) const;
    float DistanceToScreenSegment(DomPoint point, DomPoint start, DomPoint end) const;
    void DrawCoordinateAxisLabels();
    void DrawSolidDimensions();
    bool ApplyMaterialDrop(const QPoint& point, const Material& material);
    CAlfaObject* FindObjectForMaterialAt(const QPoint& point);

    enum class MaterialInteractionMode {
        None,
        Paint,
        Pick
    };

    CAlfaDoc* document_ = nullptr;
    QtSceneRenderer renderer_;
    Camera camera_;
    ToolMode tool_ = ToolMode::Orbit;
    TransformOperation transform_operation_ = TransformOperation::Move;
    BooleanOperation boolean_operation_ = BooleanOperation::Union;
    SelectionMode selection_mode_ = SelectionMode::Object;
    TransformAxis highlighted_transform_axis_ = TransformAxis::None;
    TransformAxis active_transform_axis_ = TransformAxis::None;
    QPoint last_mouse_;
    bool orbiting_ = false;
    bool alt_orbiting_ = false;
    bool panning_ = false;
    bool zooming_ = false;
    bool xy_plane_view_enabled_ = false;
    bool dragging_transform_ = false;
    bool dragging_face_extrude_ = false;
    bool dragging_draft_face_ = false;
    bool editing_polyline_ = false;
    bool editing_sketch_ = false;
    bool dragging_polyline_point_ = false;
    bool dragging_sketch_handle_ = false;
    SketchHandleKind active_sketch_handle_kind_ = SketchHandleKind::None;
    size_t active_sketch_handle_index_ = 0;
    SketchHandleKind highlighted_sketch_handle_kind_ = SketchHandleKind::None;
    size_t highlighted_sketch_handle_index_ = 0;
    bool curve_preview_valid_ = false;
    CPoint3d curve_preview_point_{};
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
    enum class MovePointStage { SelectObjects, PickSource, PickTarget };
    MovePointStage move_point_stage_ = MovePointStage::SelectObjects;
    CPoint3d move_point_source_{};
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
    bool transform_drag_has_preview_ = false;
    Vec3 transform_drag_center_{};
    Vec3 transform_drag_axis_{};
    Vec3 transform_drag_move_delta_{};
    float transform_drag_rotation_input_angle_ = 0.0f;
    float transform_drag_rotation_angle_ = 0.0f;
    float transform_drag_scale_factor_ = 1.0f;
    bool has_boolean_body_ = false;
    bool orthographic_projection_ = false;
    bool show_coordinate_axes_ = true;
    bool show_floor_grid_ = true;
    bool material_drag_active_ = false;
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
        DomPoint line_start{};
        DomPoint line_end{};
    };
    ActiveParametricObject solid_dimension_object_;
    std::vector<CDimens3D> solid_dimensions_;
    std::vector<SolidDimensionHit> solid_dimension_hits_;
    QString solid_dimension_primary_parameter_;
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
};
