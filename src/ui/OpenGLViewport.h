#pragma once

#include "QtSceneRenderer.h"

#include "../Material.h"
#include "../Point3d.h"

#include <QOpenGLWidget>
#include <QString>

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
    void FitToDocument();
    ToolMode CurrentTool() const;
    bool IsOrthographicProjection() const;
    void SetOrthographicProjection(bool enabled);
    OrbitMode GetOrbitMode() const;
    void SetOrbitMode(OrbitMode mode);
    bool IsXYPlaneViewEnabled() const;
    void SetXYPlaneViewEnabled(bool enabled);
    bool IsCoordinateAxesVisible() const;
    void SetCoordinateAxesVisible(bool visible);
    void BeginMaterialPaint(const Material& material);
    void BeginMaterialPick();
    void CancelMaterialInteraction();
    void BeginSketch(const QString& name, SketchPlane plane);
    void SetSketchRectangleTool();
    void BeginSketchFillet(double radius);
    void EndSketch();

signals:
    void DocumentChanged();
    void SelectionChanged();
    void StatusTextChanged(const QString& text);
    void BooleanFinished();
    void MaterialPicked(const Material& material);
    void ToolModeChanged(ToolMode tool);

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
    void SelectAt(const QPoint& point, SelectionAction action);
    void DrawCurveAt(const QPoint& point);
    void DrawBSplineAt(const QPoint& point);
    void HandleSketchRectangleClick(const QPoint& point);
    void HandleSketchFilletClick(const QPoint& point);
    bool ScreenToSketchPlane(const QPoint& point, CPoint3d& result) const;
    std::vector<CPoint3d> SketchRectanglePoints(const CPoint3d& first, const CPoint3d& second) const;
    void DrawSketchRectanglePreview();
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
    void HandleTransformDrag(const QPoint& point);
    void CommitTransformDrag();
    TransformAxis HitTestTransformGizmo(const QPoint& point) const;
    bool HitTestSelectedPolylineHandle(const QPoint& point, size_t* point_index = nullptr) const;
    void BeginCurvePointDrag(const CPoint3d& point);
    bool CurrentSelectedCurvePlane(Vec3& plane_point, Vec3& plane_normal) const;
    bool ScreenToWorldPlane(const QPoint& point, Vec3 plane_point, Vec3 plane_normal, CPoint3d& result) const;
    bool ScreenToCurvePlane(const QPoint& point, CPoint3d& result);
    bool ScreenToPlaneY(const QPoint& point, double y, CPoint3d& result) const;
    bool ScreenToViewPlane(const QPoint& point, Vec3 plane_point, CPoint3d& result) const;
    void DrawCurveRubberBand();
    void DrawSelectedCurvePointHandles();
    void DrawEditPointSelectionRect();
    Vec3 AxisVector(TransformAxis axis) const;
    float DistanceToScreenSegment(DomPoint point, DomPoint start, DomPoint end) const;
    void DrawCoordinateAxisLabels();
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
    bool dragging_polyline_point_ = false;
    bool curve_preview_valid_ = false;
    CPoint3d curve_preview_point_{};
    double polyline_drag_plane_y_ = 0.0;
    Vec3 curve_point_drag_anchor_{};
    Vec3 curve_point_drag_plane_point_{};
    Vec3 curve_point_drag_plane_normal_{};
    CPoint3d curve_point_drag_last_{};
    bool curve_point_drag_has_plane_ = false;
    bool selecting_edit_points_ = false;
    SelectionAction edit_point_selection_action_ = SelectionAction::Replace;
    QPoint edit_point_selection_start_;
    QPoint edit_point_selection_current_;
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
    float transform_drag_rotation_angle_ = 0.0f;
    float transform_drag_scale_factor_ = 1.0f;
    bool has_boolean_body_ = false;
    bool orthographic_projection_ = false;
    bool show_coordinate_axes_ = true;
    bool material_drag_active_ = false;
    bool sketch_active_ = false;
    bool sketch_rectangle_has_first_point_ = false;
    bool sketch_rectangle_preview_valid_ = false;
    bool highlighted_sketch_fillet_point_ = false;
    QPoint material_drag_pos_;
    QString sketch_name_;
    Vec3 sketch_origin_{};
    Vec3 sketch_u_{1.0f, 0.0f, 0.0f};
    Vec3 sketch_v_{0.0f, 1.0f, 0.0f};
    Vec3 sketch_normal_{0.0f, 0.0f, 1.0f};
    CPoint3d sketch_rectangle_first_point_{};
    CPoint3d sketch_rectangle_preview_point_{};
    Material material_drag_preview_;
    MaterialInteractionMode material_interaction_mode_ = MaterialInteractionMode::None;
    Material active_paint_material_;
    OrbitMode orbit_mode_ = OrbitMode::CAD;
    size_t boolean_body_index_ = 0;
    // raw (unwrapped) angles to avoid flip oscillation when crossing +/-90°
    float raw_yaw_ = 0.0f;
    float raw_pitch_ = 0.0f;
};
