#pragma once

#include "QtSceneRenderer.h"

#include <QOpenGLWidget>

#include <cstddef>

class OpenGLViewport : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit OpenGLViewport(QWidget* parent = nullptr);

    void SetDocument(CAlfaDoc* document);
    void SetTool(ToolMode tool);
    void SetTransformOperation(TransformOperation operation);
    void BeginBooleanTool(BooleanOperation operation);
    void FitToDocument();
    ToolMode CurrentTool() const;
    bool IsOrthographicProjection() const;
    void SetOrthographicProjection(bool enabled);
    OrbitMode GetOrbitMode() const;
    void SetOrbitMode(OrbitMode mode);
    bool IsCoordinateAxesVisible() const;
    void SetCoordinateAxesVisible(bool visible);

signals:
    void DocumentChanged();
    void SelectionChanged();
    void StatusTextChanged(const QString& text);
    void BooleanFinished();

protected:
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void SelectAt(const QPoint& point, SelectionAction action);
    void DrawCurveAt(const QPoint& point);
    void HandleBooleanClick(const QPoint& point);
    void HandleTransformClick(const QPoint& point, bool add_to_selection);
    void HandleTransformDrag(const QPoint& point);
    TransformAxis HitTestTransformGizmo(const QPoint& point) const;
    Vec3 AxisVector(TransformAxis axis) const;
    float DistanceToScreenSegment(DomPoint point, DomPoint start, DomPoint end) const;
    void DrawCoordinateAxisLabels();

    CAlfaDoc* document_ = nullptr;
    QtSceneRenderer renderer_;
    Camera camera_;
    ToolMode tool_ = ToolMode::Orbit;
    TransformOperation transform_operation_ = TransformOperation::Move;
    BooleanOperation boolean_operation_ = BooleanOperation::Union;
    TransformAxis highlighted_transform_axis_ = TransformAxis::None;
    TransformAxis active_transform_axis_ = TransformAxis::None;
    QPoint last_mouse_;
    bool orbiting_ = false;
    bool alt_orbiting_ = false;
    bool panning_ = false;
    bool dragging_transform_ = false;
    bool has_boolean_body_ = false;
    bool orthographic_projection_ = false;
    bool show_coordinate_axes_ = true;
    OrbitMode orbit_mode_ = OrbitMode::CAD;
    size_t boolean_body_index_ = 0;
};
