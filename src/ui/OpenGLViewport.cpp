#include "solid/SubdivideFace.h"
#include "ExtrudeShapeBuilder.h"
#include "../Diagnostics.h"
#include "../solid/SolidBoxTool.h"
#include "../solid/SolidCylinderTool.h"
#include <windows.h>
#include "OpenGLViewport.h"
#include "CurveCutGeometry.h"
#include "LanguageManager.h"
#include "TransformGizmoGeometry.h"

#include "../CBSpline.h"
#include "../BottleSection.h"
#include "../Body1Builder.h"
#include "../BezierSpline.h"
#include "../CPolyline.h"
#include "../CPart.h"
#include "../CAssembled.h"
#include "../KitchenLayout.h"
#include "../DrawingText.h"
#include "../SmartLine.h"
#include "../Sketch.h"
#include "../SketchArcLine.h"
#include "../solid/Solid.h"
#include "../SurfaceFilletBuilder.h"
#include "MaterialDrag.h"
#include "MeasurementUnits.h"

#include <QDialog>
#include <QComboBox>
#include <QSignalBlocker>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCursor>
#include <QCoreApplication>
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QImage>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QSettings>
#include <QSurfaceFormat>
#include <QTimer>
#include <QVariantAnimation>
#include <QQuaternion>
#include <QUrl>
#include <QWheelEvent>

#include <BRep_Tool.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <IntCurvesFace_ShapeIntersector.hxx>
#include <gp_Lin.hxx>
#include <Standard_Failure.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepTools.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Vertex.hxx>

#include <algorithm>
#include <cmath>
#include <limits>


namespace {
size_t primitive_grip_count(const CSmartLine& sketch) {
    return sketch.GetPrimitive().kind==3 ? 2 : sketch.GetPrimitive().IsFullConic() ? 4 : 6;
}
CPoint3d primitive_grip(const CSmartLine& sketch, size_t index) {
    const auto& p=sketch.GetPrimitive();
    const double t=p.start_angle+(index==5 ? p.sweep_angle : 0);
    const double r=p.kind==1 ? p.radius : p.minor_radius;
    const double x=index>=4 ? p.radius*std::cos(t) : (index==1 || index==3) ? p.radius : 0;
    const double y=index>=4 ? r*std::sin(t) : (index==2 || index==3) ? r : 0;
    return sketch.LocalToWorld({p.u+x*std::cos(p.angle)-y*std::sin(p.angle),
        p.v+x*std::sin(p.angle)+y*std::cos(p.angle),0});
}
void draw_primitive_grips(const CSmartLine& sketch, int highlighted=-1) {
    glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glLineWidth(1);
    const auto vertex=[](CPoint3d p) { glVertex3d(p.x,p.y,p.z); };
    glColor3f(.15f,.65f,.85f);
    glBegin(GL_LINES);
    vertex(primitive_grip(sketch,0)); vertex(primitive_grip(sketch,1));
    if(sketch.GetPrimitive().kind!=3) { vertex(primitive_grip(sketch,0)); vertex(primitive_grip(sketch,2)); }
    glEnd();
    if(sketch.GetPrimitive().kind!=3) {
        const auto& p=sketch.GetPrimitive();
        const double r=p.kind==1 ? p.radius : p.minor_radius;
        glBegin(GL_LINE_LOOP);
        for(const auto& sign : {std::pair<int,int>{-1,-1},{1,-1},{1,1},{-1,1}}) {
            const double x=sign.first*p.radius,y=sign.second*r;
            vertex(sketch.LocalToWorld({p.u+x*std::cos(p.angle)-y*std::sin(p.angle),
                p.v+x*std::sin(p.angle)+y*std::cos(p.angle),0}));
        }
        glEnd();
    }
    for(size_t i=0;i<primitive_grip_count(sketch);++i) {
        glPointSize(int(i)==highlighted ? 14.f : 10.f);
        if(int(i)==highlighted) glColor3f(1,.9f,0);
        else if(i==0) glColor3f(0,1,0);
        else if(i>=4) glColor3f(1,.4f,.1f);
        else glColor3f(.15f,.75f,1);
        glBegin(GL_POINTS); vertex(primitive_grip(sketch,i)); glEnd();
    }
    glPointSize(1); glEnable(GL_DEPTH_TEST);
}

constexpr double kCurvePlaneY = 0.08;

bool endpoint_tangent(const CBSpline& spline, bool at_start, Vec3& origin, Vec3& direction) {
    if (spline.IsClosed() || spline.GetPointCount()<2) return false;
    try {
        CBSpline normalized=spline;
        if (normalized.GetKnots().empty())
            normalized.SetDegree(std::min(normalized.GetDegree(),int(normalized.GetPointCount())-1));
        const auto curve=curve_cut::Spline(normalized);
        if (curve.IsNull()) return false;
        gp_Pnt end; gp_Vec tangent;
        curve->D1(at_start?curve->FirstParameter():curve->LastParameter(),end,tangent);
        if (tangent.SquareMagnitude()<1.e-20) return false;
        if (at_start) tangent.Reverse();
        tangent.Normalize();
        origin={float(end.X()),float(end.Y()),float(end.Z())};
        direction={float(tangent.X()),float(tangent.Y()),float(tangent.Z())};
        return true;
    } catch (const Standard_Failure&) { return false; }
}

double point_segment_distance_squared(const QPoint& point,
                                      const QPoint& start,
                                      const QPoint& end) {
    const double dx = static_cast<double>(end.x() - start.x());
    const double dy = static_cast<double>(end.y() - start.y());
    const double length_squared = dx * dx + dy * dy;
    if (length_squared <= 1.0e-12) {
        const double px = static_cast<double>(point.x() - start.x());
        const double py = static_cast<double>(point.y() - start.y());
        return px * px + py * py;
    }
    const double t = std::clamp(
        (static_cast<double>(point.x() - start.x()) * dx
         + static_cast<double>(point.y() - start.y()) * dy) / length_squared,
        0.0, 1.0);
    const double px = static_cast<double>(point.x())
        - (static_cast<double>(start.x()) + dx * t);
    const double py = static_cast<double>(point.y())
        - (static_cast<double>(start.y()) + dy * t);
    return px * px + py * py;
}

void simplify_screen_stroke(const std::vector<QPoint>& points,
                            size_t first,
                            size_t last,
                            double tolerance_squared,
                            std::vector<bool>& keep) {
    if (last <= first + 1) {
        return;
    }
    size_t farthest = first;
    double farthest_distance = 0.0;
    for (size_t index = first + 1; index < last; ++index) {
        const double distance = point_segment_distance_squared(
            points[index], points[first], points[last]);
        if (distance > farthest_distance) {
            farthest_distance = distance;
            farthest = index;
        }
    }
    if (farthest_distance <= tolerance_squared) {
        return;
    }
    keep[farthest] = true;
    simplify_screen_stroke(points, first, farthest, tolerance_squared, keep);
    simplify_screen_stroke(points, farthest, last, tolerance_squared, keep);
}

void viewport_camera_basis(const Camera& camera, Vec3& forward, Vec3& right, Vec3& up) {
    forward = normalize(rotate(camera.orientation, {0.0f, 0.0f, -1.0f}));
    right = normalize(rotate(camera.orientation, {1.0f, 0.0f, 0.0f}));
    up = normalize(rotate(camera.orientation, {0.0f, 1.0f, 0.0f}));
}

void rotation_arc_basis(Vec3 axis, Vec3 camera_forward, Vec3& tangent, Vec3& bitangent) {
    tangent = normalize(cross(axis, camera_forward));
    if (std::fabs(tangent.x) <= 0.00001f && std::fabs(tangent.y) <= 0.00001f && std::fabs(tangent.z) <= 0.00001f) {
        tangent = normalize(cross(axis, {0.0f, 1.0f, 0.0f}));
    }
    if (std::fabs(tangent.x) <= 0.00001f && std::fabs(tangent.y) <= 0.00001f && std::fabs(tangent.z) <= 0.00001f) {
        tangent = normalize(cross(axis, {0.0f, 0.0f, 1.0f}));
    }
    bitangent = normalize(cross(axis, tangent));
}

Vec3 point_to_vec3(const CPoint3d& point) {
    return {static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z)};
}

bool plane_from_points(const std::vector<CPoint3d>& points, Vec3& plane_point, Vec3& plane_normal) {
    if (points.size() < 3) {
        return false;
    }

    Vec3 normal{};
    for (size_t i = 0; i < points.size(); ++i) {
        const CPoint3d& current = points[i];
        const CPoint3d& next = points[(i + 1) % points.size()];
        normal.x += static_cast<float>((current.y - next.y) * (current.z + next.z));
        normal.y += static_cast<float>((current.z - next.z) * (current.x + next.x));
        normal.z += static_cast<float>((current.x - next.x) * (current.y + next.y));
    }

    normal = normalize(normal);
    if (std::fabs(normal.x) <= 0.00001f && std::fabs(normal.y) <= 0.00001f && std::fabs(normal.z) <= 0.00001f) {
        return false;
    }

    plane_point = point_to_vec3(points.front());
    plane_normal = normal;
    return true;
}

Quaternion quaternion_from_basis(Vec3 right, Vec3 up, Vec3 back) {
    const float m00 = right.x;
    const float m01 = up.x;
    const float m02 = back.x;
    const float m10 = right.y;
    const float m11 = up.y;
    const float m12 = back.y;
    const float m20 = right.z;
    const float m21 = up.z;
    const float m22 = back.z;
    const float trace = m00 + m11 + m22;

    Quaternion q{};
    if (trace > 0.0f) {
        const float s = std::sqrt(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m21 - m12) / s;
        q.y = (m02 - m20) / s;
        q.z = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        q.w = (m21 - m12) / s;
        q.x = 0.25f * s;
        q.y = (m01 + m10) / s;
        q.z = (m02 + m20) / s;
    } else if (m11 > m22) {
        const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        q.w = (m02 - m20) / s;
        q.x = (m01 + m10) / s;
        q.y = 0.25f * s;
        q.z = (m12 + m21) / s;
    } else {
        const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        q.w = (m10 - m01) / s;
        q.x = (m02 + m20) / s;
        q.y = (m12 + m21) / s;
        q.z = 0.25f * s;
    }
    return normalize_quaternion(q);
}

Quaternion z_up_orientation_from_forward(Vec3 forward, Vec3 fallback_right) {
    constexpr Vec3 kWorldUp{0.0f, 0.0f, 1.0f};
    forward = normalize(forward);
    const Vec3 back = forward * -1.0f;
    Vec3 right = normalize(cross(kWorldUp, back));
    if (dot(right, right) <= 0.00001f) {
        right = normalize(fallback_right - back * dot(fallback_right, back));
    }
    if (dot(right, right) <= 0.00001f) {
        right = {1.0f, 0.0f, 0.0f};
    }
    const Vec3 up = normalize(cross(back, right));
    return quaternion_from_basis(right, up, back);
}

Quaternion orientation_from_forward_up(Vec3 forward, Vec3 desired_up) {
    forward = normalize(forward);
    const Vec3 back = forward * -1.0f;
    Vec3 up = normalize(desired_up - back * dot(desired_up, back));
    if (dot(up, up) <= 0.00001f) {
        up = {0.0f, 1.0f, 0.0f};
    }
    Vec3 right = normalize(cross(up, back));
    if (dot(right, right) <= 0.00001f) {
        right = {1.0f, 0.0f, 0.0f};
    }
    up = normalize(cross(back, right));
    return quaternion_from_basis(right, up, back);
}

bool exact_radius_dimension_points(const TopoDS_Face& face,
                                   CPoint3d& center,
                                   CPoint3d& arrow_tip) {
    if (face.IsNull()) return false;
    try {
        BRepAdaptor_Surface surface(face, true);
        double u_min = 0.0;
        double u_max = 0.0;
        double v_min = 0.0;
        double v_max = 0.0;
        BRepTools::UVBounds(face, u_min, u_max, v_min, v_max);
        if (!std::isfinite(u_min) || !std::isfinite(u_max)
            || !std::isfinite(v_min) || !std::isfinite(v_max)) {
            return false;
        }
        const gp_Pnt point = surface.Value(
            (u_min + u_max) * 0.5, (v_min + v_max) * 0.5);
        gp_Pnt radius_center;
        if (surface.GetType() == GeomAbs_Cylinder) {
            const gp_Ax1 axis = surface.Cylinder().Axis();
            const gp_Vec from_axis(axis.Location(), point);
            radius_center = axis.Location().Translated(
                gp_Vec(axis.Direction()) * from_axis.Dot(gp_Vec(axis.Direction())));
        } else if (surface.GetType() == GeomAbs_Torus) {
            const gp_Torus torus = surface.Torus();
            const gp_Ax1 axis = torus.Axis();
            const gp_Vec from_axis(axis.Location(), point);
            const gp_Vec axis_vector(axis.Direction());
            const gp_Vec axial = axis_vector * from_axis.Dot(axis_vector);
            gp_Vec radial = from_axis - axial;
            if (radial.SquareMagnitude() <= 1.0e-18) return false;
            radial.Normalize();
            radius_center = axis.Location().Translated(
                axial + radial * torus.MajorRadius());
        } else if (surface.GetType() == GeomAbs_Sphere) {
            radius_center = surface.Sphere().Location();
        } else {
            return false;
        }
        center = CPoint3d(radius_center.X(), radius_center.Y(), radius_center.Z());
        arrow_tip = CPoint3d(point.X(), point.Y(), point.Z());
        return true;
    } catch (const Standard_Failure&) {
        return false;
    }
}

double parameter_value(const std::vector<ToolParameter>& parameters,
                       const char* id,
                       double fallback) {
    const auto parameter = std::find_if(
        parameters.begin(),
        parameters.end(),
        [id](const ToolParameter& candidate) {
            return candidate.id == id;
        });
    return parameter == parameters.end() ? fallback : parameter->value;
}

Vec3 hole_center_for_dimensions(
    const std::vector<ToolParameter>& parameters,
    std::array<Vec3, 2>* edge_starts = nullptr,
    std::array<Vec3, 2>* edge_ends = nullptr) {
    const Vec3 saved_center{
        static_cast<float>(parameter_value(parameters, "hole.center.x", 0.0)),
        static_cast<float>(parameter_value(parameters, "hole.center.y", 0.0)),
        static_cast<float>(parameter_value(parameters, "hole.center.z", 0.0))};
    if (parameter_value(parameters, "hole.refs.valid", 0.0) < 0.5)
        return saved_center;

    std::array<Vec3, 2> starts{};
    std::array<Vec3, 2> ends{};
    std::array<double, 2> sides{};
    std::array<double, 2> distances{};
    for (int index = 0; index < 2; ++index) {
        const std::string prefix = "hole.edge" + std::to_string(index + 1);
        starts[static_cast<size_t>(index)] = {
            static_cast<float>(parameter_value(parameters,
                (prefix + ".start.x").c_str(), 0.0)),
            static_cast<float>(parameter_value(parameters,
                (prefix + ".start.y").c_str(), 0.0)),
            static_cast<float>(parameter_value(parameters,
                (prefix + ".start.z").c_str(), 0.0))};
        ends[static_cast<size_t>(index)] = {
            static_cast<float>(parameter_value(parameters,
                (prefix + ".end.x").c_str(), 0.0)),
            static_cast<float>(parameter_value(parameters,
                (prefix + ".end.y").c_str(), 0.0)),
            static_cast<float>(parameter_value(parameters,
                (prefix + ".end.z").c_str(), 0.0))};
        sides[static_cast<size_t>(index)] = parameter_value(parameters,
            (prefix + ".side").c_str(), 1.0) < 0.0 ? -1.0 : 1.0;
        distances[static_cast<size_t>(index)] = parameter_value(parameters,
            index == 0 ? "hole.distance1" : "hole.distance2", 0.0);
    }
    if (edge_starts) *edge_starts = starts;
    if (edge_ends) *edge_ends = ends;

    Vec3 face_normal{
        static_cast<float>(parameter_value(parameters, "hole.normal.x", 0.0)),
        static_cast<float>(parameter_value(parameters, "hole.normal.y", 0.0)),
        static_cast<float>(parameter_value(parameters, "hole.normal.z", 1.0))};
    face_normal = normalize(face_normal);
    const Vec3 normal1 = normalize(cross(face_normal, normalize(ends[0] - starts[0])));
    const Vec3 normal2 = normalize(cross(face_normal, normalize(ends[1] - starts[1])));
    const double determinant = dot(normal1, cross(normal2, face_normal));
    if (std::fabs(determinant) <= 1.0e-8)
        return saved_center;
    const double rhs1 = dot(normal1, starts[0]) + sides[0] * distances[0];
    const double rhs2 = dot(normal2, starts[1]) + sides[1] * distances[1];
    const double rhs3 = dot(face_normal, saved_center);
    return (cross(normal2, face_normal) * static_cast<float>(rhs1)
        + cross(face_normal, normal1) * static_cast<float>(rhs2)
        + cross(normal1, normal2) * static_cast<float>(rhs3))
        * static_cast<float>(1.0 / determinant);
}

double saved_parameter_value(const std::vector<ParametricParameterValue>& parameters,
                             const char* id,
                             double fallback) {
    const auto parameter = std::find_if(
        parameters.begin(),
        parameters.end(),
        [id](const ParametricParameterValue& candidate) {
            return candidate.id == id;
        });
    return parameter == parameters.end() ? fallback : parameter->value;
}

CPoint3d add_dimension_point(const CPoint3d& first, const CPoint3d& second) {
    return {
        first.x + second.x,
        first.y + second.y,
        first.z + second.z
    };
}

CPoint3d subtract_dimension_point(const CPoint3d& first, const CPoint3d& second) {
    return {
        first.x - second.x,
        first.y - second.y,
        first.z - second.z
    };
}

QCursor captured_point_cursor() {
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(0, 170, 255), 2.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPointF(12.0, 12.0), 6.0, 6.0);
    painter.drawPoint(QPointF(12.0, 12.0));
    return QCursor(pixmap, 12, 12);
}

QCursor zoom_rect_cursor() {
    constexpr int size = 32;
    constexpr qreal center = 11.5;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(Qt::NoBrush);

    const auto draw_symbol = [&painter](const QColor& color,
                                        qreal lens_width,
                                        qreal handle_width) {
        painter.setPen(QPen(
            color, lens_width, Qt::SolidLine,
            Qt::RoundCap, Qt::RoundJoin));
        painter.drawEllipse(QPointF(center, center), 8.5, 8.5);
        painter.drawLine(QPointF(8.5, 8.5), QPointF(14.5, 14.5));
        painter.drawLine(QPointF(14.5, 8.5), QPointF(8.5, 14.5));

        painter.setPen(QPen(
            color, handle_width, Qt::SolidLine,
            Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(18.0, 18.0), QPointF(27.5, 27.5));
    };

    // A dark outline keeps the white symbol visible over light furniture,
    // while the white inner stroke stays clear over the black viewport.
    draw_symbol(QColor(0, 0, 0), 3.5, 5.0);
    draw_symbol(QColor(255, 255, 255), 1.25, 1.75);
    return QCursor(pixmap, 12, 12);
}

QCursor orbit_cursor() {
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(Qt::NoBrush);

    QPainterPath arrows;
    arrows.moveTo(5.0, 14.0);
    arrows.cubicTo(8.0, 5.0, 22.0, 5.0, 27.0, 14.0);
    arrows.moveTo(27.0, 18.0);
    arrows.cubicTo(23.0, 27.0, 9.0, 27.0, 5.0, 18.0);
    painter.setPen(QPen(
        Qt::black, 4.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(arrows);
    painter.setPen(QPen(
        Qt::white, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(arrows);

    const QPolygonF upper_head{
        QPointF(27.0, 14.0), QPointF(21.0, 13.0), QPointF(25.0, 8.5)};
    const QPolygonF lower_head{
        QPointF(5.0, 18.0), QPointF(11.0, 19.0), QPointF(7.0, 23.5)};
    painter.setPen(QPen(Qt::black, 1.5, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::white);
    painter.drawPolygon(upper_head);
    painter.drawPolygon(lower_head);
    return QCursor(pixmap, 16, 16);
}

QCursor pan_scene_cursor() {
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QLineF axes[] = {
        {QPointF(16.0, 5.0), QPointF(16.0, 27.0)},
        {QPointF(5.0, 16.0), QPointF(27.0, 16.0)}
    };
    painter.setPen(QPen(
        Qt::black, 4.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for (const QLineF& axis : axes) painter.drawLine(axis);
    painter.setPen(QPen(
        Qt::white, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for (const QLineF& axis : axes) painter.drawLine(axis);

    const QPolygonF heads[] = {
        {{16.0, 2.0}, {11.5, 8.0}, {20.5, 8.0}},
        {{16.0, 30.0}, {11.5, 24.0}, {20.5, 24.0}},
        {{2.0, 16.0}, {8.0, 11.5}, {8.0, 20.5}},
        {{30.0, 16.0}, {24.0, 11.5}, {24.0, 20.5}}
    };
    painter.setPen(QPen(
        Qt::black, 1.5, Qt::SolidLine, Qt::SquareCap, Qt::RoundJoin));
    painter.setBrush(Qt::white);
    for (const QPolygonF& head : heads) painter.drawPolygon(head);
    return QCursor(pixmap, 16, 16);
}

CPoint3d scale_dimension_point(const CPoint3d& point, double factor) {
    return {point.x * factor, point.y * factor, point.z * factor};
}

double dot_dimension_point(const CPoint3d& first, const CPoint3d& second) {
    return first.x * second.x + first.y * second.y + first.z * second.z;
}

CPoint3d cross_dimension_point(const CPoint3d& first, const CPoint3d& second) {
    return {
        first.y * second.z - first.z * second.y,
        first.z * second.x - first.x * second.z,
        first.x * second.y - first.y * second.x
    };
}

CPoint3d normalized_dimension_point(const CPoint3d& point) {
    const double length = std::sqrt(dot_dimension_point(point, point));
    return length <= 1.0e-12
        ? CPoint3d{}
        : scale_dimension_point(point, 1.0 / length);
}

CPoint3d transform_dimension_point(const CPoint3d& point,
                                   const ParametricFunction& operation,
                                   bool direction) {
    if (operation.ToolId != "SolidTransform") {
        return point;
    }

    const int type = std::clamp(
        static_cast<int>(saved_parameter_value(operation.Parameters, "type", 0.0)),
        0,
        2);
    if (type == 0) {
        if (direction) {
            return point;
        }
        return {
            point.x + saved_parameter_value(operation.Parameters, "dx", 0.0),
            point.y + saved_parameter_value(operation.Parameters, "dy", 0.0),
            point.z + saved_parameter_value(operation.Parameters, "dz", 0.0)
        };
    }

    const CPoint3d center(
        saved_parameter_value(operation.Parameters, "center.x", 0.0),
        saved_parameter_value(operation.Parameters, "center.y", 0.0),
        saved_parameter_value(operation.Parameters, "center.z", 0.0));
    const CPoint3d axis = normalized_dimension_point(CPoint3d(
        saved_parameter_value(operation.Parameters, "axis.x", 0.0),
        saved_parameter_value(operation.Parameters, "axis.y", 0.0),
        saved_parameter_value(operation.Parameters, "axis.z", 1.0)));

    if (type == 1) {
        if (dot_dimension_point(axis, axis) <= 1.0e-12) {
            return point;
        }
        const double angle = saved_parameter_value(operation.Parameters, "angle", 0.0);
        const CPoint3d relative = direction ? point : subtract_dimension_point(point, center);
        const CPoint3d rotated = add_dimension_point(
            add_dimension_point(
                scale_dimension_point(relative, std::cos(angle)),
                scale_dimension_point(cross_dimension_point(axis, relative), std::sin(angle))),
            scale_dimension_point(
                axis,
                dot_dimension_point(axis, relative) * (1.0 - std::cos(angle))));
        return direction ? rotated : add_dimension_point(center, rotated);
    }

    const double factor = saved_parameter_value(operation.Parameters, "factor", 1.0);
    const CPoint3d relative = direction ? point : subtract_dimension_point(point, center);
    CPoint3d scaled;
    if (dot_dimension_point(axis, axis) <= 1.0e-12) {
        scaled = scale_dimension_point(relative, factor);
    } else {
        scaled = add_dimension_point(
            relative,
            scale_dimension_point(
                axis,
                (factor - 1.0) * dot_dimension_point(axis, relative)));
    }
    return direction ? scaled : add_dimension_point(center, scaled);
}

Quaternion orientation_from_forward_right(Vec3 forward, Vec3 desired_right) {
    forward = normalize(forward);
    const Vec3 back = forward * -1.0f;
    Vec3 right = normalize(desired_right - back * dot(desired_right, back));
    if (dot(right, right) <= 0.00001f) {
        right = {1.0f, 0.0f, 0.0f};
    }
    Vec3 up = normalize(cross(back, right));
    if (dot(up, up) <= 0.00001f) {
        up = {0.0f, 1.0f, 0.0f};
    }
    right = normalize(cross(up, back));
    return quaternion_from_basis(right, up, back);
}

void cad_orbit_camera(Camera& camera, float yaw_delta_degrees, float pitch_delta_degrees) {
    const Vec3 right = normalize(rotate(camera.orientation, {1.0f, 0.0f, 0.0f}));
    const Vec3 up = normalize(rotate(camera.orientation, {0.0f, 1.0f, 0.0f}));
    const Quaternion yaw_delta = quaternion_from_axis_angle(up, deg_to_rad(yaw_delta_degrees));
    const Quaternion pitch_delta = quaternion_from_axis_angle(right, deg_to_rad(-pitch_delta_degrees));
    camera.orientation = normalize_quaternion(pitch_delta * yaw_delta * camera.orientation);
}

void architectural_orbit_camera(Camera& camera, float yaw_delta_degrees, float pitch_delta_degrees) {
    constexpr Vec3 kWorldUp{0.0f, 0.0f, 1.0f};
    Vec3 forward = normalize(rotate(camera.orientation, {0.0f, 0.0f, -1.0f}));
    const Vec3 current_right = normalize(rotate(camera.orientation, {1.0f, 0.0f, 0.0f}));

    forward = normalize(rotate_around_axis(forward, kWorldUp, deg_to_rad(yaw_delta_degrees)));
    const float horizontal = std::sqrt(forward.x * forward.x + forward.y * forward.y);
    const float current_pitch = std::atan2(-forward.z, horizontal) * 180.0f / kPi;
    const float target_pitch = std::clamp(current_pitch + pitch_delta_degrees, -10.0f, 89.0f);
    Vec3 horizontal_forward{forward.x, forward.y, 0.0f};
    if (dot(horizontal_forward, horizontal_forward) <= 0.00001f) {
        horizontal_forward = normalize(cross(kWorldUp, current_right));
    } else {
        horizontal_forward = normalize(horizontal_forward);
    }
    if (dot(horizontal_forward, horizontal_forward) <= 0.00001f) {
        horizontal_forward = {0.0f, -1.0f, 0.0f};
    }

    const float pitch = deg_to_rad(target_pitch);
    forward = normalize(horizontal_forward * std::cos(pitch) + Vec3{0.0f, 0.0f, -1.0f} * std::sin(pitch));
    camera.orientation = z_up_orientation_from_forward(forward, current_right);
}

void set_view_by_camera_ray(Camera& camera) {
    const Vec3 forward = normalize(rotate(camera.orientation, {0.0f, 0.0f, -1.0f}));
    const Vec3 current_right = normalize(rotate(camera.orientation, {1.0f, 0.0f, 0.0f}));
    const Vec3 axes[] = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {-1.0f, 0.0f, 0.0f},
        {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, -1.0f}
    };

    Vec3 best_axis = axes[0];
    float best_dot = dot(forward, axes[0]);
    for (size_t i = 1; i < std::size(axes); ++i) {
        const float value = dot(forward, axes[i]);
        if (value > best_dot) {
            best_dot = value;
            best_axis = axes[i];
        }
    }

    const Vec3 base_axes[] = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}
    };
    Vec3 horizontal_axis{};
    float best_horizontal = -1.0f;
    for (const Vec3 axis : base_axes) {
        if (std::fabs(dot(axis, best_axis)) > 0.5f) {
            continue;
        }
        const float alignment = std::fabs(dot(axis, current_right));
        if (alignment > best_horizontal) {
            best_horizontal = alignment;
            horizontal_axis = dot(axis, current_right) < 0.0f ? axis * -1.0f : axis;
        }
    }

    if (dot(horizontal_axis, horizontal_axis) <= 0.00001f) {
        const Vec3 desired_up = std::fabs(best_axis.z) > 0.5f
            ? Vec3{0.0f, 1.0f, 0.0f}
            : Vec3{0.0f, 0.0f, 1.0f};
        camera.orientation = orientation_from_forward_up(best_axis, desired_up);
    } else {
        camera.orientation = orientation_from_forward_right(best_axis, horizontal_axis);
    }
}

bool is_interactive_furniture_handle_name(const std::string& name) {
    const bool furniture_handle =
        name.find("Desk Drawer Handle") != std::string::npos
        || (name.rfind("Drawer ", 0) == 0
            && name.find(" Handle") != std::string::npos);
    const bool cabinet_handle = name.find("Cabinet") != std::string::npos
        && name.find("Facade") != std::string::npos
        && name.find("Handle") != std::string::npos;
    const bool nika_handle = name.rfind("Nika ", 0) == 0
        && name.find("Handle") != std::string::npos;
    const bool corner_kitchen_handle = name.rfind("Corner ", 0) == 0
        && name.find("Handle") != std::string::npos;
    const bool single_facade_handle = name == "Single Facade Handle";
    return furniture_handle || cabinet_handle || nika_handle
        || corner_kitchen_handle || single_facade_handle;
}
}

OpenGLViewport::OpenGLViewport(QWidget* parent)
    : QOpenGLWidget(parent) {
    QSurfaceFormat viewport_format = QSurfaceFormat::defaultFormat();
    viewport_format.setSamples(8);
    setFormat(viewport_format);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(true);
    setMinimumSize(640, 420);
    walk_mini_map_overlay_ = new QLabel(this);
    walk_mini_map_overlay_->setAttribute(Qt::WA_TransparentForMouseEvents);
    walk_mini_map_overlay_->setScaledContents(false);
    walk_mini_map_overlay_->hide();
    QSettings settings;
    QColor background_color(
        settings.value("view/backgroundColor", QStringLiteral("#0e1114")).toString());
    if (!background_color.isValid()) {
        background_color = QColor(QStringLiteral("#0e1114"));
    }
    SetBackgroundColor(background_color);
    ReloadModelingPreferences();
    qApp->installEventFilter(this);
}

bool OpenGLViewport::eventFilter(QObject* watched, QEvent* event) {
    // A selection dwell belongs to one uninterrupted interaction. Menus and
    // dialogs run nested event loops, so their opening must retire the timer,
    // even if they close before the one-second delay expires.
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    case QEvent::KeyPress:
    case QEvent::Shortcut:
    case QEvent::Wheel:
    case QEvent::ApplicationDeactivate:
    case QEvent::WindowDeactivate:
        ++edge_quick_menu_generation_;
        break;
    case QEvent::Show:
        if (auto* widget = qobject_cast<QWidget*>(watched); widget && widget->isWindow())
            ++edge_quick_menu_generation_;
        break;
    case QEvent::FocusOut:
    case QEvent::Hide:
        if (watched == this) ++edge_quick_menu_generation_;
        break;
    default:
        break;
    }
    return QOpenGLWidget::eventFilter(watched, event);
}

void OpenGLViewport::RemapObjectIndices(const std::vector<unsigned long>& previous_ids) {
    const auto remap = [&](size_t index) {
        return document_ && index < previous_ids.size()
            ? document_->FindObjectIndexById(previous_ids[index]) : static_cast<size_t>(-1);
    };
    hovered_edge_object_index_ = remap(hovered_edge_object_index_);
    boolean_body_index_ = remap(boolean_body_index_);
    if (!document_ || boolean_body_index_ >= document_->GetObjects().size()) has_boolean_body_ = false;
    solid_dimension_object_.object_index = remap(solid_dimension_object_.object_index);
    bool removed_dimension = !document_ || solid_dimension_object_.object_index >= document_->GetObjects().size();
    for (auto& object : solid_dimension_objects_) {
        object.object_index = remap(object.object_index);
        removed_dimension |= !document_ || object.object_index >= document_->GetObjects().size();
    }
    if (removed_dimension) ClearSolidDimensionEdit();
    update();
}

void OpenGLViewport::SetDocument(CAlfaDoc* document) {
    StopCameraAnimation(false);
    reference_hover_body_ = 0;
    if (document_ && dragging_sketch_handle_) {
        if(auto* sketch=EditableSketch()) sketch->CancelEdit();
        dragging_sketch_handle_=false; sketch_drag_changed_=false;
        sketch_before_.reset(); sketch_after_.reset();
    }
    document_ = document;
    if (document_) {
        QSettings settings("Dom3D", "Dom3D_Pro");
        document_->SetGroupInteractionEnabled(settings.value(
            "preferences/modeling/changeGroup", true).toBool());
    }
    update();
}

void OpenGLViewport::SetTool(ToolMode tool) {
    if (tool_!=tool) {
        active_tangent_id_=pending_tangent_id_=0; ++tangent_hover_generation_;
    }
    StopCameraAnimation(true);
    reference_hover_body_ = 0;
    reference_plane_pending_ = false;
    hovered_reference_plane_ = -1;
    if(tool_==ToolMode::FaceExtrude&&tool!=tool_&&document_&&document_->IsLiveExtrudeSelectedSolidFaceActive())
        document_->CancelLiveExtrudeSelectedSolidFace();
    if (tool_==ToolMode::DraftFace && tool!=tool_ && document_ && document_->IsLiveDraftFaceActive()) {
        document_->CancelLiveDraftFace();
        emit DraftFaceEditFinished(false);
    }
    const bool changed = tool_ != tool;
    if (tool_ == ToolMode::DrawSpline && tool != ToolMode::DrawSpline) {
        CancelDrawSplineStroke();
    }
    const bool cancel_sketch_face_selection =
        sketch_waiting_for_face_ && tool != ToolMode::SketchRectangle;
    tool_ = tool;
    sketch_boolean_first_ = 0;
    sketch_boolean_parent_ = 0;
    if (work_plane_assigned_ && (tool == ToolMode::DrawCurve
        || tool == ToolMode::DrawBSpline || tool == ToolMode::DrawSpline)) {
        ApplyWorkPlaneFrame();
    }
    if (walk_mini_map_overlay_) {
        walk_mini_map_overlay_->setVisible(tool_ == ToolMode::Walk);
        if (tool_ == ToolMode::Walk) walk_mini_map_overlay_->raise();
    }
    primitive_height_active_ = false;
    primitive_preview_solid_.reset();
    primitive_base_pressed_ = false;
    ClearHoveredSolidEdge();
    if (tool_ != ToolMode::Boolean) {
        has_boolean_body_ = false;
        boolean_body_index_ = 0;
    }
    orbiting_ = false;
    alt_orbiting_ = false;
    panning_ = false;
    pan_navigation_modifier_down_ = false;
    right_navigation_active_ = false;
    right_button_dragged_ = false;
    dragging_transform_ = false;
    dragging_face_extrude_ = false;
    dragging_draft_face_ = false;
    dragging_polyline_point_ = false;
    curve_point_drag_changed_ = false;
    dragging_sketch_handle_ = false;
    sketch_drag_changed_ = false;
    hovering_furniture_handle_ = false;
    hovered_furniture_handle_id_ = 0;
    furniture_handle_capture_anchor_ = {};
    selecting_with_rect_ = false;
    zoom_rect_active_ = false;
    active_sketch_handle_kind_ = SketchHandleKind::None;
    curve_point_drag_has_plane_ = false;
    curve_preview_valid_ = false;
    creation_snap_active_ = false;
    if (tool_ != ToolMode::Select) {
        editing_polyline_ = false;
        direct_curve_edit_object_id_ = 0;
        editing_sketch_ = false;
        highlighted_polyline_handle_ = false;
        highlighted_sketch_handle_kind_ = SketchHandleKind::None;
    }
    if (tool_ != ToolMode::SketchRectangle
        && tool_ != ToolMode::SolidBoxRectangle
        && tool_ != ToolMode::SolidCylinderCircle) {
        sketch_rectangle_has_first_point_ = false;
        sketch_rectangle_preview_valid_ = false;
    }
    if (tool_ != ToolMode::SketchRectangle) {
        sketch_waiting_for_face_ = false;
    }
    if (tool_ != ToolMode::SolidBoxRectangle
        && tool_ != ToolMode::SolidCylinderCircle) {
        solid_box_waiting_for_face_ = false;
        solid_box_target_body_id_ = 0;
    }
    if (tool_ != ToolMode::SketchPolyline) {
        sketch_polyline_preview_valid_ = false;
    }
    if (tool_ != ToolMode::SketchBezier) {
        sketch_bezier_preview_valid_ = false;
    }
    if (tool_ != ToolMode::SketchConvertArc) {
        sketch_arc_has_line_ = false;
    }
    if (tool_ != ToolMode::SketchFillet) {
        highlighted_sketch_fillet_point_ = false;
        sketch_fillet_preview_valid_ = false;
    }
    if (tool_ != ToolMode::MeasurePointToPoint) {
        measurement_visible_ = false;
        measurement_waiting_for_second_point_ = false;
        measurement_preview_valid_ = false;
    }
    highlighted_draft_face_gizmo_ = false;
    face_extrude_distance_ = 0.0f;
    draft_face_angle_degrees_ = 0.0;
    active_transform_axis_ = TransformAxis::None;
    highlighted_transform_axis_ = TransformAxis::None;
    RestoreDefaultToolCursor();
    if (changed) {
        emit ToolModeChanged(tool_);
    }
    if (cancel_sketch_face_selection) {
        emit SketchFaceSelectionFinished(false);
    }
    update();
}

void OpenGLViewport::RestoreDefaultToolCursor() {
    if (material_interaction_mode_ != MaterialInteractionMode::None) {
        return;
    }

    if (panning_ || pan_navigation_modifier_down_) {
        setCursor(pan_scene_cursor());
    } else if (orbiting_ || alt_orbiting_
               || alt_navigation_modifier_down_
               || (tool_ == ToolMode::Orbit && !picking_3d_point_)) {
        setCursor(orbit_cursor());
    } else if (tool_ == ToolMode::Walk) {
        setCursor(Qt::OpenHandCursor);
    } else if (tool_ == ToolMode::ZoomRect) {
        setCursor(zoom_rect_cursor());
    } else if (picking_3d_point_ || picking_xy_point_
               || picking_architecture_wall_) {
        setCursor(Qt::CrossCursor);
    } else if (tool_ == ToolMode::Select
        || tool_ == ToolMode::Transform
        || tool_ == ToolMode::Boolean
        || tool_ == ToolMode::FaceExtrude
        || tool_ == ToolMode::DraftFace
        || tool_ == ToolMode::ThickSolid
        || tool_ == ToolMode::DrawCurve
        || tool_ == ToolMode::DrawBSpline
        || tool_ == ToolMode::DrawSpline
        || tool_ == ToolMode::EditPoint
        || tool_ == ToolMode::SketchRectangle
        || tool_ == ToolMode::SketchPolyline
        || tool_ == ToolMode::SketchBezier
        || tool_ == ToolMode::SketchConvertBezier
        || tool_ == ToolMode::SketchConvertArc
        || tool_ == ToolMode::SketchFillet
        || tool_ == ToolMode::SketchConstraintHorizontal
        || tool_ == ToolMode::SketchConstraintVertical
        || tool_ == ToolMode::SketchSmoothJoint
        || tool_ == ToolMode::SketchSharpJoint
        || tool_ == ToolMode::SketchBoolean
        || tool_ == ToolMode::SolidFillet
        || tool_ == ToolMode::SolidBoxRectangle
        || tool_ == ToolMode::SolidCylinderCircle
        || tool_ == ToolMode::MeasurePointToPoint) {
        setCursor(Qt::CrossCursor);
    } else {
        unsetCursor();
    }
}

OpenGLViewport::NavigationDrag OpenGLViewport::NavigationDragFor(
    const QMouseEvent& event) const {
    const Qt::MouseButton button = event.button();
    const Qt::KeyboardModifiers modifiers = event.modifiers()
        & (Qt::ShiftModifier | Qt::ControlModifier
           | Qt::AltModifier | Qt::MetaModifier);
    const auto exact = [modifiers](Qt::KeyboardModifiers expected) {
        return modifiers == expected;
    };

    // Shift + RMB has a dedicated gizmo-origin command in Transform mode.
    if (tool_ == ToolMode::Transform
        && button == Qt::RightButton
        && modifiers.testFlag(Qt::ShiftModifier)) {
        return NavigationDrag::None;
    }

    if (navigation_preset_ == "dom3d") {
        // Alt temporarily invokes orbit without changing the active modeling
        // tool. In particular, Select keeps Ctrl/Shift for selection changes,
        // while Alt + LMB remains available for scene navigation.
        if (button == Qt::LeftButton && exact(Qt::AltModifier)) {
            return NavigationDrag::Orbit;
        }
        if (button == Qt::LeftButton && tool_ == ToolMode::Orbit) {
            return modifiers.testFlag(Qt::ControlModifier)
                ? NavigationDrag::Pan : NavigationDrag::Orbit;
        }
        if (button == Qt::MiddleButton) return NavigationDrag::Pan;
        if (button == Qt::RightButton) return NavigationDrag::Zoom;
    } else if (navigation_preset_ == "3dcoat") {
        if (button == Qt::LeftButton
            && (exact(Qt::AltModifier) || tool_ == ToolMode::Orbit)) {
            return NavigationDrag::Orbit;
        }
        if (button == Qt::MiddleButton
            && (exact(Qt::NoModifier) || exact(Qt::AltModifier))) {
            return NavigationDrag::Pan;
        }
        if (button == Qt::RightButton
            && (exact(Qt::NoModifier) || exact(Qt::AltModifier))) {
            return NavigationDrag::Zoom;
        }
    } else if (navigation_preset_ == "3dsmax") {
        if (button == Qt::MiddleButton && exact(Qt::AltModifier)) {
            return NavigationDrag::Orbit;
        }
        if (button == Qt::MiddleButton && exact(Qt::NoModifier)) {
            return NavigationDrag::Pan;
        }
        if (button == Qt::MiddleButton
            && exact(Qt::ControlModifier | Qt::AltModifier)) {
            return NavigationDrag::Zoom;
        }
    } else if (navigation_preset_ == "maya"
               || navigation_preset_ == "houdini") {
        if (exact(Qt::AltModifier)) {
            if (button == Qt::LeftButton) return NavigationDrag::Orbit;
            if (button == Qt::MiddleButton) return NavigationDrag::Pan;
            if (button == Qt::RightButton) return NavigationDrag::Zoom;
        }
    } else if (navigation_preset_ == "fusion360") {
        if (button == Qt::MiddleButton && exact(Qt::ShiftModifier)) {
            return NavigationDrag::Orbit;
        }
        if (button == Qt::MiddleButton && exact(Qt::NoModifier)) {
            return NavigationDrag::Pan;
        }
    } else if (navigation_preset_ == "blender"
               || navigation_preset_ == "plasticity") {
        if (button == Qt::MiddleButton && exact(Qt::NoModifier)) {
            return NavigationDrag::Orbit;
        }
        if (button == Qt::MiddleButton && exact(Qt::ShiftModifier)) {
            return NavigationDrag::Pan;
        }
    } else if (navigation_preset_ == "zbrush") {
        if (button == Qt::RightButton && exact(Qt::NoModifier)) {
            return NavigationDrag::Orbit;
        }
        if (button == Qt::RightButton && exact(Qt::AltModifier)) {
            return NavigationDrag::Pan;
        }
        if (button == Qt::RightButton && exact(Qt::ControlModifier)) {
            return NavigationDrag::Zoom;
        }
    } else if (navigation_preset_ == "shapr3d") {
        if (button == Qt::RightButton && exact(Qt::NoModifier)) {
            return NavigationDrag::Orbit;
        }
        if ((button == Qt::MiddleButton && exact(Qt::NoModifier))
            || (button == Qt::RightButton && exact(Qt::ShiftModifier))) {
            return NavigationDrag::Pan;
        }
    }
    return NavigationDrag::None;
}

bool OpenGLViewport::UsesAltNavigationModifier() const {
    return navigation_preset_ == "dom3d"
        || navigation_preset_ == "3dcoat"
        || navigation_preset_ == "3dsmax"
        || navigation_preset_ == "maya"
        || navigation_preset_ == "houdini";
}

void OpenGLViewport::SetTransformOperation(TransformOperation operation) {
    // A sketch can leave point-edit mode active without document point
    // selection. In that case Move/Rotate/Scale act on the selected object.
    if (document_ && selection_mode_==SelectionMode::Point && !document_->HasSelectedPoint())
        selection_mode_=SelectionMode::Object; // Preserve the selected sketch.
    universal_transform_ = operation == TransformOperation::Universal;
    transform_operation_ = universal_transform_ ? TransformOperation::Move : operation;
    transform_dialog_rotation_angle_degrees_ = 0.0f;
    transform_dialog_rotation_axis_ = {};
    SetTool(ToolMode::Transform);
}

bool OpenGLViewport::ApplyPreciseMove(Vec3 delta) {
    if (!document_ || !document_->HasSelection()) {
        return false;
    }
    const bool point_mode = selection_mode_ == SelectionMode::Point
        && document_->HasSelectedPoint();
    const bool changed = point_mode
        ? document_->MoveSelectedCurvePoints(delta, xy_plane_view_enabled_)
        : document_->PreviewMoveSelectedObjects(delta);
    if (!changed) {
        return false;
    }
    if (!point_mode) {
        document_->CommitMoveSelectedSolids(delta);
    }
    NotifyDocumentChanged();
    update();
    return true;
}

bool OpenGLViewport::ApplyPreciseRotate(Vec3 center, Vec3 axis, float angle_radians) {
    if (!document_ || !document_->HasSelection()
        || std::fabs(angle_radians) <= 0.000001f
        || dot(axis, axis) <= 0.000001f) {
        return false;
    }
    axis = normalize(axis);
    const bool point_mode = selection_mode_ == SelectionMode::Point
        && document_->HasSelectedPoint();
    const bool changed = point_mode
        ? document_->RotateSelectedCurvePoints(center, axis, angle_radians)
        : document_->PreviewRotateSelectedObjects(center, axis, angle_radians);
    if (!changed) {
        return false;
    }
    if (!point_mode) {
        document_->CommitRotateSelectedSolids(center, axis, angle_radians);
    }
    NotifyDocumentChanged();
    update();
    return true;
}

bool OpenGLViewport::ApplyPreciseScale(Vec3 center,
                                       float factor_x,
                                       float factor_y,
                                       float factor_z,
                                       bool uniform) {
    if (!document_ || !document_->HasSelection()) {
        return false;
    }

    const bool point_mode = selection_mode_ == SelectionMode::Point
        && document_->HasSelectedPoint();
    bool changed = false;
    const auto apply_factor = [&](Vec3 axis, float factor) {
        if (factor <= 0.0001f || std::fabs(factor - 1.0f) <= 0.000001f) {
            return;
        }
        const bool step_changed = point_mode
            ? document_->ScaleSelectedCurvePoints(center, axis, factor)
            : (axis.x == 0.0f && axis.y == 0.0f && axis.z == 0.0f
                ? document_->PreviewUniformScaleSelectedObjects(center, factor)
                : document_->PreviewScaleSelectedObjects(center, axis, factor));
        if (!step_changed) {
            return;
        }
        changed = true;
        if (!point_mode) {
            if (axis.x == 0.0f && axis.y == 0.0f && axis.z == 0.0f) {
                document_->CommitUniformScaleSelectedSolids(center, factor);
            } else {
                document_->CommitScaleSelectedSolids(center, axis, factor);
            }
        }
    };

    if (uniform) {
        apply_factor({}, factor_x);
    } else {
        apply_factor({1.0f, 0.0f, 0.0f}, factor_x);
        apply_factor({0.0f, 1.0f, 0.0f}, factor_y);
        apply_factor({0.0f, 0.0f, 1.0f}, factor_z);
    }
    if (changed) {
        NotifyDocumentChanged();
        update();
    }
    return changed;
}

void OpenGLViewport::SetTransformDialogGuide(TransformOperation operation,
                                             TransformAxis axis,
                                             Vec3 center,
                                             float rotation_angle_degrees,
                                             Vec3 custom_rotation_axis) {
    universal_transform_ = false;
    transform_operation_ = operation;
    highlighted_transform_axis_ = axis;
    transform_dialog_rotation_angle_degrees_ = rotation_angle_degrees;
    transform_dialog_rotation_axis_ = custom_rotation_axis;
    if (document_) {
        document_->SetTransformGizmoOrigin(center);
    }
    SetTool(ToolMode::Transform);
    update();
}

void OpenGLViewport::ClearTransformDialogGuide() {
    transform_dialog_rotation_angle_degrees_ = 0.0f;
    transform_dialog_rotation_axis_ = {};
    highlighted_transform_axis_ = TransformAxis::None;
    if (document_) {
        document_->ClearTransformGizmoOrigin();
    }
    update();
}

void OpenGLViewport::SetSelectionConfirmationMode(bool enabled) {
    selection_confirmation_mode_ = enabled;
    if (enabled) {
        setFocus();
    }
}

void OpenGLViewport::BeginFaceExtrudeTool(double taper_angle_degrees) {
    face_extrude_taper_angle_degrees_ = taper_angle_degrees;
    SetTool(ToolMode::FaceExtrude);
    if (document_ && document_->GetSelectedSolidFaceCenterAndNormal(face_extrude_center_, face_extrude_normal_)) {
        emit StatusTextChanged("Extrude Face: drag the orange normal gizmo");
    } else {
        emit StatusTextChanged("Extrude Face: select a solid face");
    }
}

void OpenGLViewport::BeginDraftFaceTool() {
    SetTool(ToolMode::DraftFace);
    if (document_ && document_->BeginDraftFaceFromSelectedFace()) {
        emit StatusTextChanged("Draft Face: choose a straight edge axis");
    } else {
        emit StatusTextChanged("Draft Face: select a planar face");
    }
}

void OpenGLViewport::BeginThickSolidTool(double thickness) {
    thick_solid_thickness_ = thickness;
    SetTool(ToolMode::ThickSolid);
    if (document_ && document_->BeginLiveThickSolidFromSelectedFaces(thick_solid_thickness_)) {
        emit StatusTextChanged("ThickSolid:adjust the required parameters and continue");
    } else if (document_ && document_->GetSelectedSolid()) {
        document_->BeginLiveThickSolidFromSelectedSolid(thick_solid_thickness_);
        emit StatusTextChanged("ThickSolid: select a solid and the faces to remove; set wall thickness and confirm");
    } else {
        emit StatusTextChanged("ThickSolid: select a solid and the faces to remove; set wall thickness and confirm");
    }
}

void OpenGLViewport::SetThickSolidThickness(double thickness) {
    thick_solid_thickness_ = thickness;
    if (tool_ == ToolMode::ThickSolid && document_ && document_->HasLiveThickSolid()) {
        document_->UpdateLiveThickSolid(thickness);
    }
}

void OpenGLViewport::BeginBooleanTool(BooleanOperation operation) {
    has_boolean_body_ = false;
    boolean_body_index_ = 0;
    SetTool(ToolMode::Boolean);
    SetBooleanOperation(operation);
}

void OpenGLViewport::SetBooleanOperation(BooleanOperation operation) {
    // Changing Type must retain the first body while waiting for the second.
    boolean_operation_ = operation;
    if (tool_ != ToolMode::Boolean) return;
    const QString type = operation == BooleanOperation::Cut ? "Subtract"
        : operation == BooleanOperation::Common ? "Intersect" : "Add";
    emit StatusTextChanged(QString("Boolean (%1): select %2 body")
        .arg(type, has_boolean_body_ ? "second" : "first"));
}

SelectionMode OpenGLViewport::GetSelectionMode() const {
    return selection_mode_;
}

void OpenGLViewport::SetSelectionMode(SelectionMode mode) {
    if (selection_mode_ == mode) {
        return;
    }
    const bool converted_faces_to_edges =
        document_
        && selection_mode_ == SelectionMode::Face
        && mode == SelectionMode::Edge
        && document_->SelectEdgesOfSelectedFaces();
    selection_mode_ = mode;
    selected_origin_plane_ = -1;
    ClearHoveredSolidEdge();
    if (document_) {
        if (!converted_faces_to_edges) {
            document_->ClearSelection();
        }
        emit SelectionChanged();
    }
    update();
}

void OpenGLViewport::FitToDocument() {
    StopCameraAnimation(false);
    if (!document_) {
        return;
    }

    Vec3 min_point{};
    Vec3 max_point{};
    bool has_bounds = false;
    for (const auto& object : document_->GetObjects()) {
        if (!object || !document_->IsObjectVisible(*object)) {
            continue;
        }

        Vec3 object_min{};
        Vec3 object_max{};
        if (!object->GetBounds(object_min, object_max)) {
            continue;
        }

        if (!has_bounds) {
            min_point = object_min;
            max_point = object_max;
            has_bounds = true;
        } else {
            min_point.x = std::min(min_point.x, object_min.x);
            min_point.y = std::min(min_point.y, object_min.y);
            min_point.z = std::min(min_point.z, object_min.z);
            max_point.x = std::max(max_point.x, object_max.x);
            max_point.y = std::max(max_point.y, object_max.y);
            max_point.z = std::max(max_point.z, object_max.z);
        }
    }

    if (!has_bounds) {
        return;
    }

    const Vec3 center = (min_point + max_point) * 0.5f;
    Vec3 forward{};
    Vec3 right{};
    Vec3 up{};
    viewport_camera_basis(camera_, forward, right, up);

    constexpr float kFitPadding = 1.12f;
    const float aspect = static_cast<float>(std::max(1, width()))
        / static_cast<float>(std::max(1, height()));
    const float tan_half_fov = std::tan(
        deg_to_rad(camera_.vertical_fov_degrees) * 0.5f);
    float fitted_distance = kMinimumCameraDistance;

    const Vec3 corners[] = {
        {min_point.x, min_point.y, min_point.z},
        {max_point.x, min_point.y, min_point.z},
        {min_point.x, max_point.y, min_point.z},
        {max_point.x, max_point.y, min_point.z},
        {min_point.x, min_point.y, max_point.z},
        {max_point.x, min_point.y, max_point.z},
        {min_point.x, max_point.y, max_point.z},
        {max_point.x, max_point.y, max_point.z},
    };

    if (orthographic_projection_) {
        float required_half_height = 0.0f;
        for (const Vec3& corner : corners) {
            const Vec3 offset = corner - center;
            required_half_height = std::max(
                required_half_height,
                std::max(std::fabs(dot(offset, up)),
                         std::fabs(dot(offset, right)) / aspect));
        }
        fitted_distance = required_half_height * kFitPadding / 0.42f;
    } else {
        for (const Vec3& corner : corners) {
            const Vec3 offset = corner - center;
            const float depth_offset = dot(offset, forward);
            const float horizontal_distance =
                std::fabs(dot(offset, right)) * kFitPadding / (tan_half_fov * aspect)
                - depth_offset;
            const float vertical_distance =
                std::fabs(dot(offset, up)) * kFitPadding / tan_half_fov
                - depth_offset;
            fitted_distance = std::max(
                fitted_distance,
                std::max(horizontal_distance, vertical_distance));
        }
    }

    camera_.target = center;
    camera_.distance = std::clamp(fitted_distance, kMinimumCameraDistance, 100000.0f);
    update();
}

namespace {
const char* snap_target_keys[] = {"grid", "auxLine", "knot", "line", "auxLine45", "workPlane", "surface"};
}

bool OpenGLViewport::IsSnapTargetEnabled(SnapTarget target) const {
    const int index = static_cast<int>(target);
    return snapping_enabled_ && index >= 0 && index < static_cast<int>(SnapTarget::Count)
        && snap_targets_[index];
}

void OpenGLViewport::SetSnapTargetEnabled(SnapTarget target, bool enabled) {
    const int index = static_cast<int>(target);
    if (index < 0 || index >= static_cast<int>(SnapTarget::Count)) return;
    QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "Dom3D", "Dom3D_Pro");
    settings.setValue(QString("preferences/modeling/snapTargets/%1").arg(snap_target_keys[index]), enabled);
    if (enabled) settings.setValue("preferences/modeling/snappingEnabled", true);
    ReloadModelingPreferences();
    curve_preview_valid_ = false;
    SetCreationSnapCursor(false);
}

void OpenGLViewport::ReloadModelingPreferences() {
    CMesh3D::ReloadLightingSettings();
    QSettings settings(QSettings::defaultFormat(), QSettings::UserScope, "Dom3D", "Dom3D_Pro");
    const QString grid_mode = settings.value(
        "view/gridDensityMode", QStringLiteral("medium")).toString();
    grid_step_ = 100.0f;
    grid_division_count_ = 10;
    if (grid_mode == "small") {
        grid_subdivisions_ = 2;
    } else if (grid_mode == "large") {
        grid_subdivisions_ = 10;
    } else if (grid_mode == "custom") {
        grid_step_ = std::clamp(
            settings.value("view/customGridStep", 100.0).toFloat(),
            0.001f, 1000000.0f);
        grid_subdivisions_ = std::clamp(
            settings.value("view/customGridSubdivisions", 2).toInt(),
            1, 100);
        grid_division_count_ = std::clamp(
            settings.value("view/customGridDivisionCount", 10).toInt(),
            1, 1000);
    } else {
        grid_subdivisions_ = 4;
    }
    grid_size_ = std::clamp(
        grid_step_ * static_cast<float>(grid_division_count_),
        1.0f, 1000000.0f);
    snapping_enabled_ =
        settings.value("preferences/modeling/snappingEnabled", true).toBool();
    for (int index = 0; index < static_cast<int>(SnapTarget::Count); ++index) {
        snap_targets_[index] = settings.value(
            QString("preferences/modeling/snapTargets/%1").arg(snap_target_keys[index]),
            index < 4).toBool();
    }
    capture_distance_pixels_ = std::clamp(
        settings.value("preferences/modeling/captureDistance", 6).toInt(),
        1,
        50);
    navigation_preset_ = settings.value(
        "preferences/picture/navigationPreset", "dom3d").toString();
    if (document_) {
        document_->SetGroupInteractionEnabled(settings.value(
            "preferences/modeling/changeGroup", true).toBool());
    }
    if (!snapping_enabled_) {
        SetCreationSnapCursor(false);
    }
    update();
}

void OpenGLViewport::RefreshSurfaceMeshQuality(float pixel_scale) {
    if (!document_ || width() <= 0 || height() <= 0) {
        return;
    }

    const float viewport_height = static_cast<float>(std::max(1, height()));
    float world_per_pixel = 0.0f;
    if (orthographic_projection_) {
        const float half_height = std::max(
            kMinimumOrthographicHalfHeight, camera_.distance * 0.42f);
        world_per_pixel = (2.0f * half_height) / viewport_height;
    } else {
        world_per_pixel = (2.0f * camera_.distance
            * std::tan(deg_to_rad(camera_.vertical_fov_degrees) * 0.5f))
            / viewport_height;
    }

    QSettings settings("Dom3D", "Dom3D_Pro");
    const float modeling_tolerance = std::clamp(
        settings.value("preferences/modeling/tolerance", 0.02).toFloat(),
        0.001f,
        10.0f);
    // CSolid::BuldMesh converts this public deflection to OCC units by
    // dividing it by ten. Compensate here so the resulting tessellation is
    // approximately one screen pixel at the current camera scale.
    const float mesh_deflection =
        std::max(modeling_tolerance, world_per_pixel * std::clamp(pixel_scale,.1f,4.f)) * 10.0f;
    if (!std::isfinite(mesh_deflection) || mesh_deflection <= 0.0f) {
        return;
    }

    const int rebuilt = document_->RebuildVisibleObjectMeshes(mesh_deflection);
    if (rebuilt > 0) {
        update();
    }
    emit StatusTextChanged(
        rebuilt > 0
            ? QString("Update Scene: rebuilt mesh for %1 visible object(s)")
                  .arg(rebuilt)
            : QString("Update Scene: no visible BRep objects to rebuild"));
}

ToolMode OpenGLViewport::CurrentTool() const {
    return tool_;
}

bool OpenGLViewport::IsOrthographicProjection() const {
    return orthographic_projection_;
}

Camera OpenGLViewport::GetCamera() const {
    return camera_;
}

void OpenGLViewport::SetCamera(const Camera& camera) {
    StopCameraAnimation(false);
    const float previous_fov = camera_.vertical_fov_degrees;
    camera_ = camera;
    camera_.vertical_fov_degrees = std::clamp(
        camera_.vertical_fov_degrees, 20.0f, 100.0f);
    rotation_pivot_enabled_ = false;
    if (std::fabs(previous_fov - camera_.vertical_fov_degrees) > 0.001f) {
        emit CameraFieldOfViewChanged(camera_.vertical_fov_degrees);
    }
    update();
}

float OpenGLViewport::GetVerticalFovDegrees() const {
    return camera_.vertical_fov_degrees;
}

void OpenGLViewport::SetVerticalFovDegrees(float degrees) {
    const float value = std::clamp(degrees, 20.0f, 100.0f);
    if (std::fabs(camera_.vertical_fov_degrees - value) <= 0.001f) return;
    camera_.vertical_fov_degrees = value;
    emit CameraFieldOfViewChanged(value);
    update();
}

void OpenGLViewport::SetRotationPivot(CPoint3d point) {
    if (!rotation_pivot_enabled_) {
        rotation_pivot_previous_target_ = camera_.target;
    }
    camera_.target = {
        static_cast<float>(point.x),
        static_cast<float>(point.y),
        static_cast<float>(point.z)};
    rotation_pivot_enabled_ = true;
    rotation_pivot_point_ = camera_.target;
    update();
}

void OpenGLViewport::ClearRotationPivot() {
    if (!rotation_pivot_enabled_) {
        return;
    }
    camera_.target = rotation_pivot_previous_target_;
    rotation_pivot_enabled_ = false;
    update();
}

bool OpenGLViewport::HasRotationPivot() const {
    return rotation_pivot_enabled_;
}

void OpenGLViewport::SetXYView() {
    StopCameraAnimation(false);
    camera_.orientation = camera_orientation_from_yaw_pitch(0.0f, 0.0f);
    camera_.target = {grid_size_ * 0.5f, grid_size_ * 0.5f, 0.0f};
    camera_.distance = grid_size_ / 0.84f;
    update();
}

void OpenGLViewport::SetOrthographicProjection(bool enabled) {
    StopCameraAnimation(false);
    if (xy_plane_view_enabled_ && !enabled) {
        enabled = true;
    }
    if (orthographic_projection_ == enabled) {
        return;
    }
    orthographic_projection_ = enabled;
    update();
}

OrbitMode OpenGLViewport::GetOrbitMode() const {
    return orbit_mode_;
}

void OpenGLViewport::SetOrbitMode(OrbitMode mode) {
    orbit_mode_ = mode;
    update();
}

bool OpenGLViewport::IsXYPlaneViewEnabled() const {
    return xy_plane_view_enabled_;
}

void OpenGLViewport::SetXYPlaneViewEnabled(bool enabled) {
    StopCameraAnimation(false);
    if (xy_plane_view_enabled_ == enabled) {
        return;
    }
    xy_plane_view_enabled_ = enabled;
    orbiting_ = false;
    alt_orbiting_ = false;
    zooming_ = false;
    if (enabled) {
        orthographic_projection_ = true;
        camera_.orientation = camera_orientation_from_yaw_pitch(0.0f, 0.0f);
        camera_.target = {grid_size_ * 0.5f, grid_size_ * 0.5f, 0.0f};
        camera_.distance = grid_size_ / 0.84f;
    }
    update();
}

bool OpenGLViewport::IsCoordinateAxesVisible() const {
    return show_coordinate_axes_;
}

void OpenGLViewport::SetCoordinateAxesVisible(bool visible) {
    if (show_coordinate_axes_ == visible) {
        return;
    }
    show_coordinate_axes_ = visible;
    update();
}

bool OpenGLViewport::IsFloorGridVisible() const {
    return show_floor_grid_;
}

void OpenGLViewport::SetFloorGridVisible(bool visible) {
    if (show_floor_grid_ == visible) {
        return;
    }
    show_floor_grid_ = visible;
    update();
}

bool OpenGLViewport::AreCurvePointsVisible() const {
    return show_curve_points_;
}

void OpenGLViewport::SetCurvePointsVisible(bool visible) {
    if (show_curve_points_ == visible) return;
    show_curve_points_ = visible;
    update();
}

void OpenGLViewport::SetBackgroundColor(const QColor& color) {
    if (!color.isValid()) return;
    const Color background{
        static_cast<float>(color.redF()),
        static_cast<float>(color.greenF()),
        static_cast<float>(color.blueF())};
    renderer_.SetBackgroundColor({background.r, background.g, background.b});
    CSolid::SetHiddenLineBackgroundColor(background);
    CAlfaObject::UpdateSelectedColorFromBackground(background);
    update();
}

Vec3 OpenGLViewport::GetBackgroundColor() const {
    return renderer_.GetBackgroundColor();
}

void OpenGLViewport::BeginMaterialPaint(const Material& material) {
    active_paint_material_ = material;
    material_interaction_mode_ = MaterialInteractionMode::Paint;
    orbiting_ = false;
    alt_orbiting_ = false;
    panning_ = false;
    zooming_ = false;
    dragging_transform_ = false;
    setCursor(Qt::CrossCursor);
    emit StatusTextChanged(QString("Material brush: click a surface to paint with %1").arg(QString::fromStdString(material.name)));
}

void OpenGLViewport::BeginMaterialPick() {
    material_interaction_mode_ = MaterialInteractionMode::Pick;
    orbiting_ = false;
    alt_orbiting_ = false;
    panning_ = false;
    zooming_ = false;
    dragging_transform_ = false;
    setCursor(Qt::CrossCursor);
    emit StatusTextChanged("Material picker: click object");
}

void OpenGLViewport::CancelMaterialInteraction() {
    if (material_interaction_mode_ == MaterialInteractionMode::None) {
        return;
    }

    material_interaction_mode_ = MaterialInteractionMode::None;
    RestoreDefaultToolCursor();
    emit StatusTextChanged("Material tool canceled");
    update();
}

void OpenGLViewport::StopCameraAnimation(bool finish) {
    if (!sketch_camera_animation_ || sketch_camera_animation_->state() != QAbstractAnimation::Running) return;
    sketch_camera_animation_->stop();
    if (finish) camera_ = sketch_camera_destination_;
    update();
}

void OpenGLViewport::AnimateSketchCamera(const Camera& previous, bool was_orthographic) {
    if (!isVisible()) return;
    const Camera destination = camera_;
    Camera start = previous;
    if (!was_orthographic) {
        start.distance = std::max(kMinimumCameraDistance,
            previous.distance * std::tan(deg_to_rad(previous.vertical_fov_degrees) * 0.5f) / 0.42f);
    }
    if (!sketch_camera_animation_) sketch_camera_animation_ = new QVariantAnimation(this);
    sketch_camera_animation_->stop();
    sketch_camera_animation_->disconnect(this);
    sketch_camera_destination_ = destination;
    sketch_camera_animation_->setDuration(450);
    sketch_camera_animation_->setEasingCurve(QEasingCurve::InOutCubic);
    sketch_camera_animation_->setStartValue(0.0);
    sketch_camera_animation_->setEndValue(1.0);
    connect(sketch_camera_animation_, &QVariantAnimation::valueChanged, this, [this, start, destination](const QVariant& value) {
        const float t = value.toFloat();
        const auto q = [](Quaternion a) { return QQuaternion(a.w, a.x, a.y, a.z).normalized(); };
        const QQuaternion rotation = QQuaternion::slerp(q(start.orientation), q(destination.orientation), t);
        camera_.orientation = {rotation.scalar(), rotation.x(), rotation.y(), rotation.z()};
        camera_.target = start.target * (1-t) + destination.target * t;
        camera_.distance = start.distance * (1-t) + destination.distance * t;
        update();
    });
    connect(sketch_camera_animation_, &QVariantAnimation::finished, this, [this] {
        camera_ = sketch_camera_destination_;
        update();
    });
    camera_ = start;
    sketch_camera_animation_->start();
}

void OpenGLViewport::UpdateReferenceFaceHover(const QPoint& point) {
    unsigned long body = 0;
    int face = -1;
    const bool choosing = ReferencePlanePickerActive() || reference_plane_dialog_
        || sketch_waiting_for_face_ || solid_box_waiting_for_face_;
    if (document_ && choosing && HitReferencePlane(point) < 0) {
        float nearest = std::numeric_limits<float>::max();
        const auto project = [this](Vec3 p, DomPoint& screen, float& depth) {
            depth = dot(p - camera_position(camera_, orthographic_projection_), rotate(camera_.orientation, {0,0,-1}));
            return depth > 0 && renderer_.WorldToScreen(p, camera_, orthographic_projection_, width(), height(), screen);
        };
        for (const auto& object : document_->GetObjects()) {
            auto* solid = dynamic_cast<CSolid*>(object.get());
            if (!solid || !document_->IsObjectSelectable(*solid)) continue;
            int index = -1;
            float depth = 0;
            if (solid->HitTestFaceScreen({point.x(),point.y()}, project, false, index, depth) && depth < nearest) {
                nearest = depth;
                const auto* surface = solid->GetSurfaceFace(index);
                body = surface && surface->IsPlanar() ? solid->m_id : 0;
                face = body ? index : -1;
            }
        }
    }
    if (body != reference_hover_body_ || face != reference_hover_face_) {
        reference_hover_body_ = body;
        reference_hover_face_ = face;
        update();
    }
}

void OpenGLViewport::DrawReferenceFaceHover() {
    if (!document_ || !reference_hover_body_) return;
    auto* solid = dynamic_cast<CSolid*>(document_->FindObjectById(reference_hover_body_));
    if (!solid || !document_->IsObjectSelectable(*solid)) return;
    auto* face = solid->GetSurfaceFace(reference_hover_face_);
    if (!face || !face->pMesh3D) return;
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);
    Material highlight;
    highlight.diffuse = {0.95f, 0.64f, 0.22f};
    highlight.alpha = 0.45f;
    face->pMesh3D->RenderFaces(false, false, &highlight, true, false);
    glPopAttrib();
}

void OpenGLViewport::BeginSketch(const QString& name, SketchPlane plane) {
    sketch_shape_kind_ = 0;
    sketch_shape_points_.clear();
    multi_sketch_session_ = true;
    multi_sketch_id_ = 0;
    StopCameraAnimation(true);
    const Camera previous_camera = camera_;
    const bool previous_projection = orthographic_projection_;
    sketch_active_ = true;
    sketch_name_ = name;
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;
    sketch_polyline_points_.clear();
    sketch_polyline_preview_valid_ = false;
    highlighted_sketch_fillet_point_ = false;
    orthographic_projection_ = true;
    xy_plane_view_enabled_ = false;
    sketch_attachment_body_id_ = 0;
    sketch_attachment_face_index_ = -1;

    sketch_origin_ = {};
    if (plane == SketchPlane::XY) {
        sketch_u_ = {1.0f, 0.0f, 0.0f};
        sketch_v_ = {0.0f, 1.0f, 0.0f};
        sketch_normal_ = {0.0f, 0.0f, 1.0f};
        camera_.orientation = camera_orientation_from_yaw_pitch(0.0f, 0.0f);
    } else if (plane == SketchPlane::XZ) {
        sketch_u_ = {1.0f, 0.0f, 0.0f};
        sketch_v_ = {0.0f, 0.0f, 1.0f};
        sketch_normal_ = {0.0f, 1.0f, 0.0f};
        camera_.orientation = camera_orientation_from_yaw_pitch(0.0f, -89.9f);
    } else {
        sketch_u_ = {0.0f, 1.0f, 0.0f};
        sketch_v_ = {0.0f, 0.0f, 1.0f};
        sketch_normal_ = {1.0f, 0.0f, 0.0f};
        camera_.orientation = camera_orientation_from_yaw_pitch(-90.0f, 0.0f);
    }

    camera_.target = sketch_origin_;
    SetTool(ToolMode::SketchRectangle);
    emit StatusTextChanged(QString("%1: Rectangle, click first corner").arg(sketch_name_));
    AnimateSketchCamera(previous_camera, previous_projection);
}

void OpenGLViewport::BeginSketchFaceSelection(const QString& name) {
    sketch_active_ = false;
    sketch_waiting_for_face_ = true;
    sketch_name_ = name;
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;
    sketch_attachment_body_id_ = 0;
    sketch_attachment_face_index_ = -1;
    SetTool(ToolMode::SketchRectangle);
    setCursor(Qt::CrossCursor);
    emit StatusTextChanged(
        QString("%1: select a planar body face").arg(sketch_name_));
}

void OpenGLViewport::BeginSketchOnFace(const QString& name,
                                       Vec3 origin,
                                       Vec3 x_axis,
                                       Vec3 y_axis,
                                       Vec3 normal,
                                       unsigned long body_id,
                                       int face_index) {
    sketch_shape_kind_ = 0;
    sketch_shape_points_.clear();
    multi_sketch_session_ = true;
    multi_sketch_id_ = 0;
    StopCameraAnimation(true);
    const Camera previous_camera = camera_;
    const bool previous_projection = orthographic_projection_;
    const Vec3 face_normal = normalize(normal);
    // Keep the area the user is looking at centered on the face, rather than
    // jumping to the surface's (potentially distant) parameter origin.
    const Vec3 focus = camera_.target
        - face_normal * dot(camera_.target - origin, face_normal);
    float sketch_distance = camera_.distance;
    if (!orthographic_projection_) {
        const Vec3 forward = rotate(camera_.orientation, {0, 0, -1});
        const float depth = dot(focus - camera_position(camera_), forward);
        // Match the perspective image scale when switching to orthographic.
        const float view_depth = depth > 0 ? depth : camera_.distance;
        sketch_distance = std::max(kMinimumCameraDistance,
            view_depth * std::tan(deg_to_rad(camera_.vertical_fov_degrees) * 0.5f) / 0.42f);
    }
    sketch_active_ = true;
    sketch_name_ = name;
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;
    sketch_polyline_points_.clear();
    sketch_polyline_preview_valid_ = false;
    sketch_bezier_points_.clear();
    sketch_bezier_preview_valid_ = false;
    highlighted_sketch_fillet_point_ = false;
    orthographic_projection_ = true;
    xy_plane_view_enabled_ = false;

    sketch_origin_ = origin;
    sketch_u_ = normalize(x_axis);
    sketch_v_ = normalize(y_axis);
    sketch_normal_ = normalize(normal);
    sketch_attachment_body_id_ = body_id;
    sketch_attachment_face_index_ = face_index;
    camera_.target = focus;
    camera_.distance = sketch_distance;
    camera_.orientation = orientation_from_forward_up(
        sketch_normal_ * -1.0f, sketch_v_);

    SetTool(ToolMode::SketchRectangle);
    emit StatusTextChanged(
        QString("%1: sketch on body face, click first rectangle corner").arg(sketch_name_));
    AnimateSketchCamera(previous_camera, previous_projection);
}

void OpenGLViewport::SetSketchRectangleTool() {
    if (!sketch_active_) {
        return;
    }
    if (tool_ == ToolMode::SketchPolyline && sketch_polyline_points_.size() >= 2
        && !CommitSketchPolyline(false)) return;
    sketch_shape_kind_ = 0;
    sketch_shape_points_.clear();
    sketch_rectangle_has_first_point_ = false;
    sketch_polyline_points_.clear();
    sketch_polyline_preview_valid_ = false;
    SetTool(ToolMode::SketchRectangle);
    emit StatusTextChanged(QString("%1: Rectangle, click first corner").arg(sketch_name_));
}

bool OpenGLViewport::BeginEditSelectedSketch() {
    if (const auto* multi = document_ ? dynamic_cast<const CSketch*>(document_->GetSelectedObject()) : nullptr) {
        const auto& plane = multi->GetCoordinateSystem();
        const auto attachment = multi->GetFaceAttachment();
        BeginSketchOnFace(QString::fromStdString(multi->GetName()),
            point_to_vec3(plane.origin), point_to_vec3(plane.x_axis),
            point_to_vec3(plane.y_axis), point_to_vec3(plane.normal),
            attachment.body_id, attachment.face_index);
        multi_sketch_id_ = multi->m_id;
        SetTool(ToolMode::Select);
        editing_sketch_ = true;
        EditableSketch();
        emit StatusTextChanged(DomTranslate("Sketch: double-click a figure to edit its parameters, or a contour to edit its nodes."));
        return true;
    }
    const CSmartLine* sketch = document_ ? EditableSketch() : nullptr;
    if (!sketch) {
        return false;
    }
    multi_sketch_session_ = false;
    multi_sketch_id_ = 0;
    const SketchCoordinateSystem& system = sketch->GetCoordinateSystem();
    sketch_active_ = true;
    sketch_name_ = QString::fromStdString(sketch->GetName());
    sketch_origin_ = point_to_vec3(system.origin);
    sketch_u_ = point_to_vec3(system.x_axis);
    sketch_v_ = point_to_vec3(system.y_axis);
    sketch_normal_ = point_to_vec3(system.normal);
    if (sketch->HasFaceAttachment()) {
        const SketchFaceAttachment& attachment = sketch->GetFaceAttachment();
        sketch_attachment_body_id_ = attachment.body_id;
        sketch_attachment_face_index_ = attachment.face_index;
    } else {
        sketch_attachment_body_id_ = 0;
        sketch_attachment_face_index_ = -1;
    }
    sketch_rectangle_has_first_point_ = false;
    sketch_polyline_points_.clear();
    sketch_bezier_points_.clear();
    SetTool(ToolMode::Select);
    editing_sketch_ = true;
    emit StatusTextChanged(
        QString("%1: edit sketch or choose a creation tool").arg(sketch_name_));
    return true;
}

bool OpenGLViewport::BeginEditSelectedCurve() {
    if (!document_) {
        return false;
    }

    CAlfaObject* curve = document_->GetSelectedObject();
    if (dynamic_cast<CSmartLine*>(curve) || dynamic_cast<CSketch*>(curve)) {
        return BeginEditSelectedSketch();
    }

    if (!dynamic_cast<CPolyline*>(curve)
        && !dynamic_cast<CBSpline*>(curve)) {
        // Keep Shape / Edit Nodes useful before a curve is selected: the
        // common point tool can pick nodes from any supported visible curve.
        EndDirectCurveEdit();
        SetTool(ToolMode::EditPoint);
        emit StatusTextChanged(
            "Shape / Edit Nodes: click a curve node or drag a selection box");
        return true;
    }

    EndDirectCurveEdit();
    SetTool(ToolMode::Select);
    editing_polyline_ = true;
    document_->ClearPointSelection();
    direct_curve_edit_object_id_ = curve->m_id;
    emit SelectionChanged();
    emit StatusTextChanged(
        "Shape / Edit Nodes: drag nodes; double-click the curve to add a node; Esc to finish");
    update();
    return true;
}

void OpenGLViewport::EndDirectCurveEdit() {
    if (dragging_sketch_handle_ && document_) {
        if (auto* sketch=EditableSketch()) sketch->CancelEdit();
        sketch_before_.reset(); sketch_after_.reset();
    }
    if (!editing_polyline_ && !editing_sketch_) {
        return;
    }
    editing_polyline_ = false;
    editing_sketch_ = false;
    dragging_polyline_point_ = false;
    dragging_sketch_handle_ = false;
    curve_point_drag_changed_ = false;
    sketch_drag_changed_ = false;
    direct_curve_edit_object_id_ = 0;
    highlighted_polyline_handle_ = false;
    active_sketch_handle_kind_ = SketchHandleKind::None;
    highlighted_sketch_handle_kind_ = SketchHandleKind::None;
    curve_point_drag_has_plane_ = false;
    if (document_) {
        document_->ClearPointSelection();
    }
    RestoreDefaultToolCursor();
    update();
}

void OpenGLViewport::SetSketchPolylineTool() {
    if (!sketch_active_) {
        return;
    }
    if (tool_ == ToolMode::SketchPolyline && !sketch_polyline_points_.empty()) return;
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;
    sketch_polyline_points_.clear();
    sketch_polyline_preview_valid_ = false;
    QSettings settings("Dom3D", "Dom3D_Pro");
    sketch_alignment_angle_degrees_ = std::clamp(
        settings.value("preferences/modeling/angleAlignment", 7).toDouble(),
        0.0,
        45.0);
    SetTool(ToolMode::SketchPolyline);
    emit StatusTextChanged(
        QString("%1: Polyline, click first point (alignment %2°)")
            .arg(sketch_name_)
            .arg(sketch_alignment_angle_degrees_, 0, 'f', 0));
}

void OpenGLViewport::SetSketchBezierTool() {
    if (!sketch_active_) {
        return;
    }
    if (tool_ == ToolMode::SketchPolyline && sketch_polyline_points_.size() >= 2
        && !CommitSketchPolyline(false)) return;
    sketch_rectangle_has_first_point_ = false;
    sketch_polyline_points_.clear();
    sketch_polyline_preview_valid_ = false;
    sketch_bezier_points_.clear();
    sketch_bezier_preview_valid_ = false;
    SetTool(ToolMode::SketchBezier);
    emit StatusTextChanged(
        QString("%1: Bezier — click start, two controls and end").arg(sketch_name_));
}

void OpenGLViewport::BeginSketchConvertLineToBezier() {
    if (!document_ || !EditableSketch()) {
        emit StatusTextChanged("Convert to Bezier: select a sketch first");
        return;
    }
    SetTool(ToolMode::SketchConvertBezier);
    emit StatusTextChanged(
        QString("%1: click a straight segment to convert it to Bezier")
            .arg(sketch_name_));
}

void OpenGLViewport::BeginSketchConvertLineToArc() {
    if (!document_ || !EditableSketch()) {
        emit StatusTextChanged("Line to Arc: select a sketch first");
        return;
    }
    sketch_arc_has_line_ = false;
    SetTool(ToolMode::SketchConvertArc);
    emit StatusTextChanged(
        QString("%1: select a straight segment").arg(sketch_name_));
}

void OpenGLViewport::BeginSketchFillet(double radius) {
    if (!document_) {
        return;
    }
    CPolyline* selected_polyline = document_->GetSelectedPolyline();
    CSmartLine* selected_sketch = EditableSketch();
    if (!sketch_active_ && selected_sketch) {
        sketch_name_ = QString::fromStdString(EditableSketch()->GetName());
    } else if (!sketch_active_ && selected_polyline) {
        sketch_name_ = QString::fromStdString(selected_polyline->GetName());
        Vec3 forward{};
        viewport_camera_basis(camera_, forward, sketch_u_, sketch_v_);
    } else if (!sketch_active_) {
        sketch_name_ = "Fillets";
        Vec3 forward{};
        viewport_camera_basis(camera_, forward, sketch_u_, sketch_v_);
    }
    sketch_fillet_radius_ = radius;
    SetTool(ToolMode::SketchFillet);
    emit StatusTextChanged(QString("%1: Fillet R=%2; click a sketch corner to round").arg(sketch_name_).arg(sketch_fillet_radius_, 0, 'f', 2));
}

void OpenGLViewport::BeginSketchConstraintHorizontal() {
    SetTool(ToolMode::SketchConstraintHorizontal);
    emit StatusTextChanged("Geometry constraint: select a straight segment for Horizontal");
}

void OpenGLViewport::BeginSketchConstraintVertical() {
    SetTool(ToolMode::SketchConstraintVertical);
    emit StatusTextChanged("Geometry constraint: select a straight segment for Vertical");
}

void OpenGLViewport::BeginSketchUnion() { BeginSketchBoolean(BooleanOperation::Union); }
void OpenGLViewport::BeginSketchCut() { BeginSketchBoolean(BooleanOperation::Cut); }
void OpenGLViewport::BeginSketchBoolean(BooleanOperation operation) {
    auto* parent=document_?dynamic_cast<CSketch*>(document_->GetSelectedObject()):nullptr;
    if(!parent || parent->GetContourCount()<2) {
        emit StatusTextChanged(DomTranslate("Sketch Boolean: select a sketch with two closed contours."));return;
    }
    SetTool(ToolMode::SketchBoolean);
    sketch_boolean_parent_=parent->m_id;sketch_boolean_operation_=operation;
    emit StatusTextChanged(DomTranslate(operation==BooleanOperation::Cut
        ? "Sketch subtraction: click the contour to cut from. Esc cancels."
        : "Sketch union: click the first contour. Esc cancels."));
}

void OpenGLViewport::HandleSketchBooleanClick(const QPoint& point) {
    auto* parent=document_?dynamic_cast<CSketch*>(document_->GetSelectedObject()):nullptr;
    if(!parent || parent->m_id!=sketch_boolean_parent_) {
        SetTool(ToolMode::Select);return;
    }
    std::uint64_t picked=0;
    double nearest=12;
    // Only real boundaries are operands; empty space and dimension handles are not.
    for(size_t i=0;i<parent->GetContourCount();++i) {
        const auto contour=parent->MakeWorldContour(i);
        for(size_t j=0;j<contour.GetNumLines();++j) {
            const auto samples=contour.GetLine(j)->Sample(96);
            for(size_t k=1;k<samples.size();++k) {
                DomPoint a{},b{};
                if(!renderer_.WorldToScreen(point_to_vec3(contour.LocalToWorld(samples[k-1])),camera_,orthographic_projection_,width(),height(),a)
                    || !renderer_.WorldToScreen(point_to_vec3(contour.LocalToWorld(samples[k])),camera_,orthographic_projection_,width(),height(),b))continue;
                const auto distance=DistanceToScreenSegment({point.x(),point.y()},a,b);
                if(distance<nearest){nearest=distance;picked=parent->GetContourId(i);}
            }
        }
    }
    if(!picked)return;
    for(size_t i=0;i<parent->GetContourCount();++i)if(parent->GetContourId(i)==picked)
        if(parent->IsConstruction(i) || !parent->GetLocalContour(i).IsClosed()) {
            emit StatusTextChanged(DomTranslate("Sketch Boolean: choose two closed, non-construction contours."));return;
        }
    if(!sketch_boolean_first_) {
        sketch_boolean_first_=picked;
        emit StatusTextChanged(DomTranslate(sketch_boolean_operation_==BooleanOperation::Cut
            ? "Sketch subtraction: click the cutting contour. Esc cancels."
            : "Sketch union: click the second contour. Esc cancels."));
        update();return;
    }
    if(picked==sketch_boolean_first_) {
        emit StatusTextChanged(DomTranslate("Sketch Boolean: choose two different closed contours."));return;
    }
    std::string error;
    if(!parent->BooleanContours(sketch_boolean_first_,picked,sketch_boolean_operation_,error)) {
        emit StatusTextChanged(DomTranslate(error.c_str()));return;
    }
    edit_contour_id_=sketch_boolean_first_;edit_contour_source_=nullptr;edit_contour_.reset();
    sketch_before_.reset();sketch_after_.reset();
    SetTool(ToolMode::Select);editing_sketch_=true;
    NotifyDocumentChanged();emit SelectionChanged();
    emit StatusTextChanged(DomTranslate("Sketch Boolean completed"));update();
}

void OpenGLViewport::BeginSketchSmoothJoint() {
    SetTool(ToolMode::SketchSmoothJoint);
    emit StatusTextChanged("Smooth joint: click the node joining two segments");
}

void OpenGLViewport::BeginSketchSharpJoint() {
    SetTool(ToolMode::SketchSharpJoint);
    emit StatusTextChanged("Sharp joint: click a node to remove smoothness");
}

void OpenGLViewport::SetSketchFilletRadius(double radius) {
    if (radius <= 0.0) {
        return;
    }
    sketch_fillet_radius_ = radius;
    if (tool_ == ToolMode::SketchFillet) {
        emit StatusTextChanged(
            QString("%1: Fillet R=%2; click a sketch corner to round")
                .arg(sketch_name_)
                .arg(sketch_fillet_radius_, 0, 'f', 2));
        update();
    }
}

void OpenGLViewport::SetReferencePlaneSelection(bool enabled) {
    reference_hover_body_ = 0;
    if (!enabled) RestoreDefaultToolCursor();
    reference_plane_dialog_ = enabled;
    reference_plane_pending_ = enabled;
    hovered_reference_plane_ = -1;
    update();
}

bool OpenGLViewport::IsOriginPlaneVisible(int plane) const {
    return plane >= 0 && plane < 3 && origin_planes_visible_[plane];
}

void OpenGLViewport::SetOriginPlaneVisible(int plane, bool visible) {
    if (plane < 0 || plane >= 3) return;
    origin_planes_visible_[plane] = visible;
    if (!visible && selected_origin_plane_ == plane) selected_origin_plane_ = -1;
    update();
}

void OpenGLViewport::SelectOriginPlane(int plane) {
    if (plane < 0 || plane >= 3) return;
    selected_origin_plane_ = plane;
    origin_planes_visible_[plane] = true;
    if (document_) document_->ClearSelection();
    emit SelectionChanged();
    update();
}

void OpenGLViewport::SetWorkPlane(Vec3 origin, Vec3 normal, unsigned long source_object_id) {
    if (dot(normal, normal) < 1.0e-12f) return;
    work_plane_assigned_ = true;
    work_plane_source_id_ = source_object_id;
    work_plane_origin_ = origin;
    work_plane_normal_ = normalize(normal);
    ApplyWorkPlaneFrame();
    SetSnapTargetEnabled(SnapTarget::WorkPlane, true);
    update();
}

void OpenGLViewport::ClearWorkPlane() {
    work_plane_assigned_ = false;
    work_plane_source_id_ = 0;
    SetSnapTargetEnabled(SnapTarget::WorkPlane, false);
    update();
}

bool OpenGLViewport::ResolveWorkPlane() {
    if (!work_plane_assigned_) return false;
    if (work_plane_source_id_ == 0) return true;
    Vec3 origin{}, normal{};
    if (!document_ || !document_->GetObjectPlane(work_plane_source_id_, origin, normal)) {
        ClearWorkPlane();
        return false;
    }
    work_plane_origin_ = origin;
    work_plane_normal_ = normalize(normal);
    return true;
}

bool OpenGLViewport::RefreshWorkPlane() {
    const Vec3 previous_origin = work_plane_origin_;
    const Vec3 previous_normal = work_plane_normal_;
    if (!ResolveWorkPlane()) return false;
    const Vec3 translation = work_plane_origin_ - previous_origin;
    const Vec3 rotation = work_plane_normal_ - previous_normal;
    // A tree refresh must not overwrite an unrelated sketch's drawing frame.
    if (dot(translation, translation) > 1.0e-12f || dot(rotation, rotation) > 1.0e-12f) {
        ApplyWorkPlaneFrame();
        curve_preview_valid_ = false;
        update();
    }
    return true;
}

void OpenGLViewport::ApplyWorkPlaneFrame() {
    // Resolve the source again at tool entry, including after Undo/Redo.
    if (!ResolveWorkPlane()) return;
    sketch_origin_ = work_plane_origin_;
    sketch_normal_ = work_plane_normal_;
    const Vec3 axis = std::abs(sketch_normal_.x) < 0.9f ? Vec3{1,0,0} : Vec3{0,1,0};
    sketch_u_ = normalize(axis - sketch_normal_ * dot(axis, sketch_normal_));
    sketch_v_ = normalize(cross(sketch_normal_, sketch_u_));
}

bool OpenGLViewport::IsWorkPlane(Vec3 origin, Vec3 normal) const {
    normal = normalize(normal);
    return work_plane_assigned_
        && std::abs(dot(normal, work_plane_normal_)) > 0.99999f
        && std::abs(dot(origin - work_plane_origin_, work_plane_normal_)) < 0.001f;
}

bool OpenGLViewport::ReferencePlanesVisible() const {
    return ReferencePlanePickerActive()
        || std::any_of(origin_planes_visible_.begin(), origin_planes_visible_.end(), [](bool v) { return v; });
}

bool OpenGLViewport::ReferencePlanePickerActive() const {
    return reference_plane_pending_ && (reference_plane_dialog_
        || ((tool_ == ToolMode::SolidBoxRectangle || tool_ == ToolMode::SolidCylinderCircle)
            && !sketch_rectangle_has_first_point_));
}

QPolygonF OpenGLViewport::ReferencePlanePolygon(int plane) const {
    const Vec3 eye = camera_position(camera_, orthographic_projection_);
    const Vec3 forward = rotate(camera_.orientation, {0, 0, -1});
    const float depth = dot(Vec3{} - eye, forward);
    if (depth <= 0.0001f || height() <= 0) return {};
    // A pen-sized target at the world origin, independent of zoom and grid size.
    const float half_height = orthographic_projection_
        ? std::max(kMinimumOrthographicHalfHeight, camera_.distance * 0.42f)
        : depth * std::tan(deg_to_rad(camera_.vertical_fov_degrees) * 0.5f);
    const float size = half_height * 2.0f * 105.0f / height();
    const Vec3 u = plane == 2 ? Vec3{0, size, 0} : Vec3{size, 0, 0};
    const Vec3 v = plane == 0 ? Vec3{0, size, 0} : Vec3{0, 0, size};
    QPolygonF polygon;
    for (const Vec3 p : {Vec3{}, u, u + v, v}) {
        DomPoint screen;
        if (!renderer_.WorldToScreen(p, camera_, orthographic_projection_, width(), height(), screen)) return {};
        polygon << QPointF(screen.x, screen.y);
    }
    double area = 0;
    for (int i = 0; i < 4; ++i)
        area += polygon[i].x() * polygon[(i+1)%4].y() - polygon[(i+1)%4].x() * polygon[i].y();
    return std::abs(area) < 120.0 ? QPolygonF{} : polygon;
}

int OpenGLViewport::HitReferencePlane(const QPoint& point) const {
    if (!ReferencePlanesVisible()) return -1;
    const Vec3 eye = camera_position(camera_, orthographic_projection_);
    const Vec3 forward = rotate(camera_.orientation, {0, 0, -1});
    float nearest = std::numeric_limits<float>::max();
    int result = -1;
    for (int plane = 0; plane < 3; ++plane) {
        if (!ReferencePlanePickerActive() && !IsOriginPlaneVisible(plane)) continue;
        if (!ReferencePlanePolygon(plane).containsPoint(point, Qt::OddEvenFill)) continue;
        const Vec3 normal = plane == 0 ? Vec3{0,0,1} : plane == 1 ? Vec3{0,1,0} : Vec3{1,0,0};
        CPoint3d hit;
        if (!ScreenToWorldPlane(point, {}, normal, hit)) continue;
        const float depth = dot(Vec3{float(hit.x), float(hit.y), float(hit.z)} - eye, forward);
        if (depth > 0 && depth < nearest) { nearest = depth; result = plane; }
    }
    return result;
}

void OpenGLViewport::DrawReferencePlanes() {
    if (!ReferencePlanesVisible()) return;
    // Isolate the overlay from the modeling renderer's depth/stipple state.
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_LINE_STIPPLE);
    glDisable(GL_POLYGON_STIPPLE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    QPainter painter(this);
    painter.setFont(font());
    painter.setRenderHint(QPainter::Antialiasing);
    const char* names[] = {"XY", "XZ", "YZ"};
    // Draw the hovered patch last so feedback is unambiguous at intersections.
    for (int pass = 0; pass < 2; ++pass) {
        for (int plane = 0; plane < 3; ++plane) {
            if (!ReferencePlanePickerActive() && !IsOriginPlaneVisible(plane)) continue;
            const bool hovered = plane == hovered_reference_plane_
                || (plane == selected_origin_plane_ && (!document_ || !document_->HasSelection()));
            if (hovered != (pass == 1)) continue;
            const QPolygonF polygon = ReferencePlanePolygon(plane);
            if (polygon.isEmpty()) continue;
            painter.setPen(QPen(hovered ? QColor(255, 186, 66) : QColor(137, 171, 228), hovered ? 2.0 : 1.2));
            painter.setBrush(hovered ? QColor(255, 172, 42, 115) : QColor(110, 143, 204, 55));
            painter.drawPolygon(polygon);
            QPointF center;
            for (const auto& p : polygon) center += p;
            center /= polygon.size();
            painter.setPen(hovered ? QColor(255, 221, 155) : QColor(206, 223, 252));
            painter.drawText(QRectF(center - QPointF(16, 10), QSizeF(32, 20)), Qt::AlignCenter, names[plane]);
        }
    }
    painter.end();
    glPopAttrib();
}

void OpenGLViewport::leaveEvent(QEvent* event) {
    ++edge_quick_menu_generation_;
    pending_tangent_id_=0; ++tangent_hover_generation_;
    rotation_axis_hover_valid_ = false;
    reference_hover_body_ = 0;
    hovered_reference_plane_ = -1;
    update();
    QOpenGLWidget::leaveEvent(event);
}

void OpenGLViewport::SetSolidBoxCentered(bool centered) {
    solid_box_centered_ = centered;
    if (tool_ == ToolMode::SolidBoxRectangle && primitive_height_active_) {
        primitive_preview_solid_.reset();
        UpdatePrimitiveHeight(last_mouse_);
    }
    update();
}

void OpenGLViewport::BeginSolidBoxRectangle(SketchPlane plane) {
    sketch_active_ = false;
    solid_box_waiting_for_face_ = false;
    solid_box_target_body_id_ = 0;
    sketch_name_ = "BOX";
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;
    highlighted_sketch_fillet_point_ = false;

    sketch_origin_ = {};
    if (plane == SketchPlane::XY) {
        sketch_u_ = {1.0f, 0.0f, 0.0f};
        sketch_v_ = {0.0f, 1.0f, 0.0f};
        sketch_normal_ = {0.0f, 0.0f, 1.0f};
    } else if (plane == SketchPlane::XZ) {
        sketch_u_ = {1.0f, 0.0f, 0.0f};
        sketch_v_ = {0.0f, 0.0f, 1.0f};
        sketch_normal_ = {0.0f, 1.0f, 0.0f};
    } else {
        sketch_u_ = {0.0f, 1.0f, 0.0f};
        sketch_v_ = {0.0f, 0.0f, 1.0f};
        sketch_normal_ = {1.0f, 0.0f, 0.0f};
    }

    SetTool(ToolMode::SolidBoxRectangle);
    reference_plane_pending_ = true;
    setCursor(Qt::CrossCursor);
    emit StatusTextChanged("BOX: click first rectangle corner");
}

void OpenGLViewport::BeginSolidBoxFaceSelection() {
    sketch_active_ = false;
    solid_box_waiting_for_face_ = true;
    solid_box_target_body_id_ = 0;
    sketch_name_ = "BOX";
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;
    highlighted_sketch_fillet_point_ = false;
    SetTool(ToolMode::SolidBoxRectangle);
    reference_plane_pending_ = true;
    setCursor(Qt::CrossCursor);
    emit StatusTextChanged("BOX: select a planar body face");
}

void OpenGLViewport::BeginSolidCylinderCircle(SketchPlane plane) {
    sketch_active_ = false;
    solid_box_waiting_for_face_ = false;
    solid_box_target_body_id_ = 0;
    sketch_name_ = "CYLINDER";
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;

    sketch_origin_ = {};
    if (plane == SketchPlane::XY) {
        sketch_u_ = {1.0f, 0.0f, 0.0f};
        sketch_v_ = {0.0f, 1.0f, 0.0f};
        sketch_normal_ = {0.0f, 0.0f, 1.0f};
    } else if (plane == SketchPlane::XZ) {
        sketch_u_ = {1.0f, 0.0f, 0.0f};
        sketch_v_ = {0.0f, 0.0f, 1.0f};
        sketch_normal_ = {0.0f, 1.0f, 0.0f};
    } else {
        sketch_u_ = {0.0f, 1.0f, 0.0f};
        sketch_v_ = {0.0f, 0.0f, 1.0f};
        sketch_normal_ = {1.0f, 0.0f, 0.0f};
    }

    SetTool(ToolMode::SolidCylinderCircle);
    reference_plane_pending_ = true;
    setCursor(Qt::CrossCursor);
    emit StatusTextChanged("CYLINDER: click circle center");
}

void OpenGLViewport::BeginSolidCylinderFaceSelection() {
    sketch_active_ = false;
    solid_box_waiting_for_face_ = true;
    solid_box_target_body_id_ = 0;
    sketch_name_ = "CYLINDER";
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;
    SetTool(ToolMode::SolidCylinderCircle);
    reference_plane_pending_ = true;
    setCursor(Qt::CrossCursor);
    emit StatusTextChanged("CYLINDER: select a planar body face");
}

void OpenGLViewport::SetSolidPrimitivePlacement(SketchPlane plane, bool on_face) {
    const bool cylinder = tool_ == ToolMode::SolidCylinderCircle;
    if (!cylinder && tool_ != ToolMode::SolidBoxRectangle) return;
    const bool has_anchor = sketch_rectangle_has_first_point_;
    const CPoint3d anchor = sketch_rectangle_first_point_;
    if (cylinder) {
        if (on_face) BeginSolidCylinderFaceSelection();
        else BeginSolidCylinderCircle(plane);
    } else {
        if (on_face) BeginSolidBoxFaceSelection();
        else BeginSolidBoxRectangle(plane);
    }
    if (has_anchor) {
        sketch_rectangle_has_first_point_ = true;
        sketch_rectangle_first_point_ = anchor;
        // For a face, wait for its basis before projecting the existing anchor.
        if (!on_face) ReprojectSolidPrimitiveAnchor();
    }
    reference_plane_pending_ = false;
    update();
}

void OpenGLViewport::ReprojectSolidPrimitiveAnchor() {
    if (!sketch_rectangle_has_first_point_) return;
    const Vec3 point{static_cast<float>(sketch_rectangle_first_point_.x),
                     static_cast<float>(sketch_rectangle_first_point_.y),
                     static_cast<float>(sketch_rectangle_first_point_.z)};
    const Vec3 delta = point - sketch_origin_;
    const Vec3 projected = sketch_origin_
        + sketch_u_ * dot(delta, sketch_u_) + sketch_v_ * dot(delta, sketch_v_);
    sketch_rectangle_first_point_ = CPoint3d(projected.x, projected.y, projected.z);
    sketch_rectangle_preview_point_ = sketch_rectangle_first_point_;
    sketch_rectangle_preview_valid_ = true;
    emit StatusTextChanged(tool_ == ToolMode::SolidCylinderCircle
        ? "CYLINDER: click circle radius" : "BOX: click opposite rectangle corner");
}

void OpenGLViewport::SetSolidDimensionEdit(
    const ActiveParametricObject& active_object,
    const QString& primary_parameter) {
    SetBottleModelEdit(active_object);
    surface_fillet_normals_.clear();
    if(active_object.tool_id=="SurfaceFillet" && document_
        && active_object.object_index<document_->GetObjects().size()) {
        const auto* object=dynamic_cast<const CSolid*>(document_->GetObjects()[active_object.object_index].get());
        std::vector<ParametricParameterValue> values;
        for(const auto& p:active_object.parameters)values.push_back({p.id,p.value});
        if(object)surface_fillet_normals_=SurfaceFilletNormalGuides(*document_,ReadSurfaceFilletParameters(values),object->m_Shape);
    }
    const bool supports_dimensions =
        active_object.tool_id == "SolidBox"
        || active_object.tool_id == "SolidCylinder"
        || active_object.tool_id == "SolidPrismTool"
        || active_object.tool_id == "SolidBeamTool"
        || active_object.tool_id == "SolidHole"
        || active_object.tool_id == "ChamferSolid"
        || active_object.tool_id == "fillet_edge"
        || active_object.tool_id == "fillet_all_edges"
        || active_object.tool_id == "cabinet"
        || active_object.tool_id == "cabinet_advanced"
        || active_object.tool_id == "cabinet_advanced_slx"
        || active_object.tool_id == "cabinet_showcase";
    solid_dimension_object_ = supports_dimensions
        ? active_object
        : ActiveParametricObject{};
    solid_dimension_objects_.clear();
    if (supports_dimensions) {
        solid_dimension_objects_.push_back(active_object);
    }
    solid_dimensions_.clear();
    cylinder_center_grips_.clear();
    solid_dimension_hits_.clear();
    solid_dimension_primary_parameter_ = primary_parameter;

    const auto ensure_edge_anchor = [&]() {
                    if (fillet_anchor_valid_ || !document_) return;
                    const auto points=document_->GetLiveEdgeToolPoints(1025);
                    if (!fillet_anchor_valid_ && !points.empty()) {
                        fillet_anchor_=points[points.size()/2];
                        double picked_fraction=0.5;
                        double closest=std::numeric_limits<double>::max();
                        if(fillet_pick_valid_) for(size_t i=1;i<points.size();++i) {
                            DomPoint a{},b{};
                            if(!renderer_.WorldToScreen(point_to_vec3(points[i-1]),camera_,orthographic_projection_,width(),height(),a)
                                || !renderer_.WorldToScreen(point_to_vec3(points[i]),camera_,orthographic_projection_,width(),height(),b)) continue;
                            const double dx=b.x-a.x,dy=b.y-a.y,den=dx*dx+dy*dy;
                            const double t=den>0?std::clamp(((fillet_pick_screen_.x()-a.x)*dx+(fillet_pick_screen_.y()-a.y)*dy)/den,0.0,1.0):0;
                            const double dist=std::hypot(a.x+t*dx-fillet_pick_screen_.x(),a.y+t*dy-fillet_pick_screen_.y());
                            if(dist<closest){closest=dist;picked_fraction=(double(i-1)+t)/double(points.size()-1);fillet_anchor_=CPoint3d(points[i-1].x+t*(points[i].x-points[i-1].x),points[i-1].y+t*(points[i].y-points[i-1].y),points[i-1].z+t*(points[i].z-points[i-1].z));}
                        }
                        fillet_anchor_valid_=document_->GetLiveEdgeToolFrame(
                            picked_fraction,fillet_anchor_,fillet_drag_direction_);
                    }
    };

    std::vector<const ParametricFunction*> following_transforms;
    // Live fillet points come from the already transformed preview CAD in
    // world coordinates. The new operation still has its default index (0),
    // so replaying later Move/Rotate/Scale entries would transform it twice.
    // This also applies to the endpoints of variable-radius live handles.
    const bool live_fillet_frame = document_ && document_->HasLiveFillet()
        && (active_object.tool_id == "fillet_edge" || active_object.tool_id == "fillet_all_edges");
    if (document_ && !live_fillet_frame
        && solid_dimension_object_.object_index < document_->GetObjects().size()) {
        const auto* solid = dynamic_cast<const CSolid*>(
            document_->GetObjects()[solid_dimension_object_.object_index].get());
        if (solid) {
            for (size_t operation_index = solid_dimension_object_.operation_index + 1;
                 operation_index < static_cast<size_t>(solid->GetNumOperations());
                 ++operation_index) {
                const ParametricFunction* operation =
                    solid->GetOperation(static_cast<int>(operation_index));
                if (operation && operation->ToolId == "SolidTransform") {
                    following_transforms.push_back(operation);
                }
            }
        }
    }

    const auto transformed_point = [&following_transforms](CPoint3d point) {
        for (const ParametricFunction* operation : following_transforms) {
            point = transform_dimension_point(point, *operation, false);
        }
        return point;
    };
    const auto transformed_direction = [&following_transforms](CPoint3d direction) {
        for (const ParametricFunction* operation : following_transforms) {
            direction = transform_dimension_point(direction, *operation, true);
        }
        return normalized_dimension_point(direction);
    };
    const auto add_dimension = [this, &transformed_point, &transformed_direction](
                                   CPoint3d start,
                                   CPoint3d end,
                                   CPoint3d offset_direction,
                                   double offset,
                                   const char* parameter_id,
                                   const char* label,
                                   double value) {
        solid_dimensions_.emplace_back(
            transformed_point(start),
            transformed_point(end),
            transformed_direction(offset_direction),
            offset,
            parameter_id,
            label,
            value);
        solid_dimensions_.back().SetActive(
            QString::fromLatin1(parameter_id)
            == solid_dimension_primary_parameter_);
    };

    if (solid_dimension_object_.tool_id == "SolidBeamTool") {
        const auto& parameters = solid_dimension_object_.parameters;
        const double w = parameter_value(parameters, "width", 20.0);
        const double h = parameter_value(parameters, "height", 20.0);
        const double length = parameter_value(parameters, "length", 100.0);
        const double thick = parameter_value(parameters, "thick", 2.0);
        const int type = int(parameter_value(parameters, "type", 0.0));
        const double offset = std::max(2.0, std::min(w, h) * 0.2);
        if (type == 5) {
            // Tube uses the smaller section dimension as its outer diameter.
            const double radius = std::min(w, h) * 0.5;
            add_dimension({-radius,0,0},{radius,0,0},{0,-1,0},offset,
                w <= h ? "width" : "height", "Diameter", radius * 2);
            add_dimension({radius,0,0},{radius,0,length},{1,0,0},offset,
                "length", "Length", length);
            add_dimension({0,radius-thick,length},{0,radius,length},{1,0,0},offset,
                "thick", "Thickness", thick);
        } else {
            add_dimension({-w/2,-h/2,0},{w/2,-h/2,0},{0,-1,0},offset,
                "width", "Width", w);
            add_dimension({-w/2,-h/2,0},{-w/2,h/2,0},{-1,0,0},offset,
                "height", "Height", h);
            add_dimension({w/2,-h/2,0},{w/2,-h/2,length},{1,0,0},offset,
                "length", "Length", length);
            // T's flange is at +Y; the other profiles have a flange at -Y.
            const double y = type == 3 ? h/2-thick : -h/2;
            add_dimension({w/2,y,length},{w/2,y+thick,length},{1,0,0},offset,
                "thick", "Thickness", thick);
        }
    } else if (solid_dimension_object_.tool_id == "SolidBox") {
        const auto& parameters = solid_dimension_object_.parameters;
        const CPoint3d origin(
            parameter_value(parameters, "origin.x", 0.0),
            parameter_value(parameters, "origin.y", 0.0),
            parameter_value(parameters, "origin.z", 0.0));
        const CPoint3d u(
            parameter_value(parameters, "axis.u.x", 1.0),
            parameter_value(parameters, "axis.u.y", 0.0),
            parameter_value(parameters, "axis.u.z", 0.0));
        const CPoint3d v(
            parameter_value(parameters, "axis.v.x", 0.0),
            parameter_value(parameters, "axis.v.y", 1.0),
            parameter_value(parameters, "axis.v.z", 0.0));
        const CPoint3d n(
            parameter_value(parameters, "axis.n.x", 0.0),
            parameter_value(parameters, "axis.n.y", 0.0),
            parameter_value(parameters, "axis.n.z", 1.0));
        const double width_value = parameter_value(parameters, "width", 1.0);
        const double height_value = parameter_value(parameters, "height", 1.0);
        const double depth_value = parameter_value(parameters, "depth", 1.0);
        const double offset = std::max(
            2.0,
            std::min({std::abs(width_value), std::abs(height_value), std::abs(depth_value)}) * 0.14);
        const auto endpoint = [&origin](const CPoint3d& axis, double value) {
            return CPoint3d(
                origin.x + axis.x * value,
                origin.y + axis.y * value,
                origin.z + axis.z * value);
        };
        add_dimension(
            origin,
            endpoint(u, width_value),
            CPoint3d(-v.x, -v.y, -v.z),
            offset,
            "width",
            "Length",
            width_value);
        add_dimension(
            origin,
            endpoint(v, height_value),
            CPoint3d(-u.x, -u.y, -u.z),
            offset,
            "height",
            "Width",
            height_value);
        add_dimension(
            origin,
            endpoint(n, depth_value),
            u,
            offset,
            "depth",
            "Height",
            depth_value);
    } else if (solid_dimension_object_.tool_id == "SolidCylinder") {
        const auto& parameters = solid_dimension_object_.parameters;
        const CPoint3d origin(
            parameter_value(parameters, "origin.x", 0.0),
            parameter_value(parameters, "origin.y", 0.0),
            parameter_value(parameters, "origin.z", 0.0));
        const CPoint3d u(
            parameter_value(parameters, "axis.u.x", 1.0),
            parameter_value(parameters, "axis.u.y", 0.0),
            parameter_value(parameters, "axis.u.z", 0.0));
        const CPoint3d v(
            parameter_value(parameters, "axis.v.x", 0.0),
            parameter_value(parameters, "axis.v.y", 1.0),
            parameter_value(parameters, "axis.v.z", 0.0));
        const CPoint3d n(
            parameter_value(parameters, "axis.n.x", 0.0),
            parameter_value(parameters, "axis.n.y", 0.0),
            parameter_value(parameters, "axis.n.z", 1.0));
        CylinderCenterGrip center_grip;
        center_grip.origin=origin;
        center_grip.center=transformed_point(origin);
        const Vec3 base_normal=normalize(point_to_vec3(n));
        center_grip.local_u=normalize(point_to_vec3(u)-base_normal*dot(point_to_vec3(u),base_normal));
        center_grip.local_v=normalize(cross(base_normal,center_grip.local_u));
        const auto world_axis=[&](Vec3 axis) {
            const auto end=transformed_point(CPoint3d(origin.x+axis.x,origin.y+axis.y,origin.z+axis.z));
            return point_to_vec3(end)-point_to_vec3(center_grip.center);
        };
        center_grip.world_u=world_axis(center_grip.local_u);
        center_grip.world_v=world_axis(center_grip.local_v);
        center_grip.operation_index=int(active_object.operation_index);
        cylinder_center_grips_.push_back(center_grip);
        const double diameter = parameter_value(parameters, "diameter", 10.0);
        const double height = parameter_value(parameters, "height", 5.0);
        const double radius = diameter * 0.5;
        const double offset = std::max(2.0, std::min(std::abs(diameter), std::abs(height)) * 0.14);
        const auto shifted = [&origin](const CPoint3d& axis, double value) {
            return CPoint3d(
                origin.x + axis.x * value,
                origin.y + axis.y * value,
                origin.z + axis.z * value);
        };
        add_dimension(
            shifted(u, -radius),
            shifted(u, radius),
            CPoint3d(-v.x, -v.y, -v.z),
            offset,
            "diameter",
            "Diameter",
            diameter);
        add_dimension(
            shifted(u, radius),
            CPoint3d(origin.x + u.x * radius + n.x * height,
                origin.y + u.y * radius + n.y * height,
                origin.z + u.z * radius + n.z * height),
            u,
            offset,
            "height",
            "Height",
            height);
    } else if (solid_dimension_object_.tool_id == "SolidPrismTool") {
        const auto& parameters = solid_dimension_object_.parameters;
        const double length_value = parameter_value(parameters, "length", 20.0);
        const double height_value = parameter_value(parameters, "height", 40.0);
        const int quantity = std::clamp(
            static_cast<int>(parameter_value(parameters, "qty", 6.0)),
            3,
            128);
        const int axis = std::clamp(
            static_cast<int>(parameter_value(parameters, "axis", 1.0)),
            0,
            2);
        const double half_angle = 3.14159265358979323846 / quantity;
        const double rotation_angle = half_angle * 2.0;
        const CPoint3d first_2d(
            -length_value * 0.5,
            -length_value / std::tan(half_angle) * 0.5,
            0.0);
        const CPoint3d second_2d(
            first_2d.x * std::cos(rotation_angle) - first_2d.y * std::sin(rotation_angle),
            first_2d.x * std::sin(rotation_angle) + first_2d.y * std::cos(rotation_angle),
            0.0);
        const auto map_base_point = [axis](const CPoint3d& point) {
            if (axis == 0) {
                return CPoint3d(0.0, point.y, -point.x);
            }
            if (axis == 1) {
                return CPoint3d(point.x, 0.0, point.y);
            }
            return CPoint3d(point.x, point.y, 0.0);
        };
        const CPoint3d height_direction = axis == 0
            ? CPoint3d(1.0, 0.0, 0.0)
            : (axis == 1 ? CPoint3d(0.0, 1.0, 0.0) : CPoint3d(0.0, 0.0, 1.0));
        const CPoint3d first = map_base_point(first_2d);
        const CPoint3d second = map_base_point(second_2d);
        const CPoint3d side_midpoint = scale_dimension_point(
            add_dimension_point(first_2d, second_2d),
            0.5);
        const CPoint3d side_offset = map_base_point(
            normalized_dimension_point(side_midpoint));
        const CPoint3d height_offset = map_base_point(
            normalized_dimension_point(first_2d));
        const double offset = std::max(
            2.0,
            std::min(std::abs(length_value), std::abs(height_value)) * 0.14);

        add_dimension(
            first,
            second,
            side_offset,
            offset,
            "length",
            "Length",
            length_value);
        add_dimension(
            first,
            add_dimension_point(first, scale_dimension_point(height_direction, height_value)),
            height_offset,
            offset,
            "height",
            "Height",
            height_value);
    } else if (solid_dimension_object_.tool_id == "SolidHole") {
        const auto& parameters = solid_dimension_object_.parameters;
        if (parameter_value(parameters, "hole.refs.valid", 0.0) >= 0.5) {
            std::array<Vec3, 2> starts{};
            std::array<Vec3, 2> ends{};
            const Vec3 center = hole_center_for_dimensions(
                parameters, &starts, &ends);
            const double diameter = parameter_value(parameters, "diameter", 10.0);
            const double offset = std::max(1.0, std::abs(diameter) * 0.65);
            for (int edge = 0; edge < 2; ++edge) {
                const Vec3 direction = normalize(
                    ends[static_cast<size_t>(edge)]
                    - starts[static_cast<size_t>(edge)]);
                const Vec3 from_start = center - starts[static_cast<size_t>(edge)];
                const Vec3 foot = starts[static_cast<size_t>(edge)]
                    + direction * dot(from_start, direction);
                const double distance = parameter_value(parameters,
                    edge == 0 ? "hole.distance1" : "hole.distance2", 0.0);
                add_dimension(
                    CPoint3d(foot.x, foot.y, foot.z),
                    CPoint3d(center.x, center.y, center.z),
                    CPoint3d(direction.x, direction.y, direction.z),
                    edge == 0 ? offset : -offset,
                    edge == 0 ? "hole.distance1" : "hole.distance2",
                    edge == 0 ? "Distance to Edge 1" : "Distance to Edge 2",
                    distance);
            }
        }
    } else if (solid_dimension_object_.tool_id == "cabinet"
               || solid_dimension_object_.tool_id == "cabinet_advanced"
               || solid_dimension_object_.tool_id == "cabinet_advanced_slx"
               || solid_dimension_object_.tool_id == "cabinet_showcase") {
        const auto& parameters = solid_dimension_object_.parameters;
        const double width_value = parameter_value(parameters, "width", 600.0);
        const double depth_value = parameter_value(parameters, "depth", 560.0);
        const double height_value = parameter_value(parameters, "height", 800.0);
        const bool overhead =
            parameter_value(parameters, "overhead", 0.0) >= 0.5;
        const double mounting_height = overhead
            ? std::max(0.0, parameter_value(
                parameters, "mounting_height", 1300.0))
            : 0.0;
        const double left = -width_value * 0.5;
        const double right = width_value * 0.5;
        const double front = -depth_value * 0.5;
        const double back = depth_value * 0.5;
        const double offset = std::clamp(
            std::min({width_value, depth_value, height_value}) * 0.10,
            28.0, 70.0);
        add_dimension(
            CPoint3d(left, front, mounting_height + height_value),
            CPoint3d(right, front, mounting_height + height_value),
            CPoint3d(0.0, 0.0, 1.0),
            offset,
            "width", "Width", width_value);
        add_dimension(
            CPoint3d(right, front, mounting_height),
            CPoint3d(right, front, mounting_height + height_value),
            CPoint3d(1.0, 0.0, 0.0),
            offset,
            "height", "Height", height_value);
        add_dimension(
            CPoint3d(right, front, mounting_height),
            CPoint3d(right, back, mounting_height),
            CPoint3d(1.0, 0.0, 0.0),
            offset,
            "depth", "Depth", depth_value);
        if (overhead) {
            add_dimension(
                CPoint3d(left, back, 0.0),
                CPoint3d(left, back, mounting_height),
                CPoint3d(-1.0, 0.0, 0.0),
                offset,
                "mounting_height", "Height Hanger", mounting_height);
        }
    } else if (solid_dimension_object_.tool_id == "fillet_edge"
               || solid_dimension_object_.tool_id == "fillet_all_edges") {
        const int radius_mode = std::clamp(static_cast<int>(parameter_value(
            solid_dimension_object_.parameters, "radius_type", 0.0)), 0, 2);
        const double radius = parameter_value(
            solid_dimension_object_.parameters, "radius", 1.0);
        CSolid* solid = nullptr;
        if (document_
            && solid_dimension_object_.object_index
                   < document_->GetObjects().size()) {
            solid = dynamic_cast<CSolid*>(document_->GetObjects()[
                solid_dimension_object_.object_index].get());
        }
        if (solid) {
            std::vector<CPoint3d> law_points;
            std::vector<double> law_radii;
            std::vector<std::string> law_ids;
            if (radius_mode == 1 && document_->HasLiveFillet()) {
                CPoint3d edge_start{};
                CPoint3d edge_end{};
                if (document_->GetLiveFilletEndPoints(edge_start, edge_end)) {
                    law_points = {edge_start, edge_end};
                    law_radii = {
                        parameter_value(solid_dimension_object_.parameters,
                                        "radius_start", radius),
                        parameter_value(solid_dimension_object_.parameters,
                                        "radius_end", radius)};
                    law_ids = {"radius_start", "radius_end"};
                }
            } else if (radius_mode == 2 && document_->HasLiveFillet()) {
                law_points = document_->GetLiveFilletPoints(6);
                for (int index = 0; index < 6; ++index) {
                    const std::string id = "radius.point." + std::to_string(index);
                    law_ids.push_back(id);
                    law_radii.push_back(parameter_value(
                        solid_dimension_object_.parameters, id.c_str(), radius));
                }
            }
            if (!law_points.empty() && law_points.size() == law_radii.size()) {
                for (size_t index = 0; index < law_points.size(); ++index) {
                    const CPoint3d& previous = law_points[
                        index == 0 ? 0 : index - 1];
                    const CPoint3d& next = law_points[
                        index + 1 < law_points.size() ? index + 1 : index];
                    CPoint3d tangent(
                    next.x - previous.x,
                    next.y - previous.y,
                    next.z - previous.z);
                const double tangent_length = std::sqrt(
                    tangent.x * tangent.x + tangent.y * tangent.y
                    + tangent.z * tangent.z);
                if (tangent_length > 1.0e-9) {
                    tangent.x /= tangent_length;
                    tangent.y /= tangent_length;
                    tangent.z /= tangent_length;
                }
                CPoint3d reference = std::abs(tangent.z) < 0.85
                    ? CPoint3d(0.0, 0.0, 1.0)
                    : CPoint3d(0.0, 1.0, 0.0);
                CPoint3d direction(
                    tangent.y * reference.z - tangent.z * reference.y,
                    tangent.z * reference.x - tangent.x * reference.z,
                    tangent.x * reference.y - tangent.y * reference.x);
                const double direction_length = std::sqrt(
                    direction.x * direction.x + direction.y * direction.y
                    + direction.z * direction.z);
                if (direction_length > 1.0e-9) {
                    direction.x /= direction_length;
                    direction.y /= direction_length;
                    direction.z /= direction_length;
                }
                    const CPoint3d& point = law_points[index];
                    const double point_radius = law_radii[index];
                    const std::string label = radius_mode == 2
                        ? "Radius " + std::to_string(index * 20) + "%"
                        : (index == 0 ? "Start Radius" : "End Radius");
                    add_dimension(
                        point,
                        CPoint3d(point.x + direction.x * point_radius,
                                 point.y + direction.y * point_radius,
                                 point.z + direction.z * point_radius),
                        direction, 0.0, law_ids[index].c_str(), label.c_str(),
                        point_radius);
                }
            } else {
            std::vector<int> surface_indices;
            if (document_->HasLiveFillet()) {
                surface_indices = document_->GetLiveFilletCreatedSurfaceIndices();
            } else if (const ParametricFunction* operation = solid->GetOperation(
                           static_cast<int>(solid_dimension_object_.operation_index))) {
                surface_indices = operation->CreatedSurfaceIndices;
            }
            if (surface_indices.empty()) {
                surface_indices.reserve(static_cast<size_t>(solid->GetNumSurfaces()));
                for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
                    surface_indices.push_back(i);
                }
            }

            bool found = false;
            bool best_is_exact = false;
            double best_screen_length = -1.0;
            CPoint3d best_start{};
            CPoint3d best_end{};
            for (int surface_index : surface_indices) {
                Vec3 center{};
                Vec3 normal{};
                if (!solid->GetFaceCenterAndNormal(surface_index, center, normal)) {
                    continue;
                }
                CPoint3d start{};
                CPoint3d end{};
                const bool exact = exact_radius_dimension_points(
                    solid->GetTopoFace(surface_index), start, end);
                if (!exact) {
                    normal = normalize(normal);
                    end = CPoint3d(center.x, center.y, center.z);
                    start = CPoint3d(
                        center.x - normal.x * radius,
                        center.y - normal.y * radius,
                        center.z - normal.z * radius);
                }
                DomPoint start_screen{};
                DomPoint end_screen{};
                double screen_length = 0.0;
                if (renderer_.WorldToScreen(
                        point_to_vec3(start), camera_, orthographic_projection_,
                        width(), height(), start_screen)
                    && renderer_.WorldToScreen(
                        point_to_vec3(end), camera_, orthographic_projection_,
                        width(), height(), end_screen)) {
                    screen_length = std::hypot(
                        static_cast<double>(end_screen.x - start_screen.x),
                        static_cast<double>(end_screen.y - start_screen.y));
                }
                if (!found || (exact && !best_is_exact)
                    || (exact == best_is_exact
                        && screen_length > best_screen_length)) {
                    found = true;
                    best_is_exact = exact;
                    best_screen_length = screen_length;
                    best_start = start;
                    best_end = end;
                }
            }
            if (found) {
                if (document_->HasLiveFillet()) {
                    ensure_edge_anchor();
                    if(fillet_anchor_valid_) {
                        // The handle follows the CAD tangent at the picked point.
                        // Keep this frame fixed while the fillet is rebuilt.
                        const auto inward=fillet_drag_direction_;
                        best_start=fillet_anchor_;
                        best_end=CPoint3d(best_start.x+inward.x*radius,best_start.y+inward.y*radius,best_start.z+inward.z*radius);
                    }
                }
                solid_dimensions_.emplace_back(
                    transformed_point(best_start),
                    transformed_point(best_end),
                    CPoint3d(0.0, 0.0, 1.0),
                    0.0,
                    "radius",
                    "Radius",
                    radius);
                solid_dimensions_.back().SetActive(true);
            }
            }
        }
    }
    if (active_object.tool_id == "ChamferSolid" && document_ && document_->HasLiveChamfer()) {
        ensure_edge_anchor();
        if (fillet_anchor_valid_) {
            double distance = 1.0;
            for (const auto& parameter : active_object.parameters)
                if (parameter.id == "distance") distance = parameter.value;
            const CPoint3d end(fillet_anchor_.x + fillet_drag_direction_.x * distance,
                fillet_anchor_.y + fillet_drag_direction_.y * distance,
                fillet_anchor_.z + fillet_drag_direction_.z * distance);
            solid_dimensions_.emplace_back(fillet_anchor_, end,
                CPoint3d(0, 0, 1), 0.0, "distance", "Distance", distance);
            solid_dimensions_.back().SetActive(true);
        }
    }
    update();
}

void OpenGLViewport::SetSolidDimensionEdits(
    const std::vector<ActiveParametricObject>& active_objects,
    size_t primary_operation_index) {
    std::vector<CylinderCenterGrip> combined_centers;
    std::vector<CDimens3D> combined_dimensions;
    std::vector<ActiveParametricObject> combined_objects;

    for (const ActiveParametricObject& active_object : active_objects) {
        SetSolidDimensionEdit(active_object);
        combined_centers.insert(combined_centers.end(),cylinder_center_grips_.begin(),cylinder_center_grips_.end());
        if (solid_dimensions_.empty()) {
            continue;
        }
        const size_t source_index = combined_objects.size();
        combined_objects.push_back(active_object);
        for (CDimens3D dimension : solid_dimensions_) {
            dimension.SetSourceIndex(source_index);
            dimension.SetActive(active_object.operation_index == primary_operation_index);
            combined_dimensions.push_back(std::move(dimension));
        }
    }

    cylinder_center_grips_=std::move(combined_centers);
    solid_dimension_objects_ = std::move(combined_objects);
    solid_dimensions_ = std::move(combined_dimensions);
    solid_dimension_object_ = solid_dimension_objects_.empty()
        ? ActiveParametricObject{}
        : solid_dimension_objects_.front();
    solid_dimension_hits_.clear();
    solid_dimension_primary_parameter_.clear();
    update();
}

void OpenGLViewport::SetCabinetPreviewVisible(bool visible) {
    cabinet_preview_visible_ = visible;
    update();
}

void OpenGLViewport::ClearSolidDimensionEdit() {
    cylinder_center_grips_.clear();
    SetBottleModelEdit({});
    surface_fillet_normals_.clear();
    fillet_anchor_valid_ = false;
    solid_dimension_object_ = {};
    solid_dimension_objects_.clear();
    solid_dimensions_.clear();
    solid_dimension_hits_.clear();
    solid_dimension_primary_parameter_.clear();
    highlighted_solid_dimension_grip_.clear();
    highlighted_solid_dimension_operation_index_ = -1;
    active_solid_dimension_grip_.clear();
    cabinet_preview_visible_ = false;
    active_solid_dimension_operation_index_ = -1;
    dragging_solid_dimension_grip_ = false;
    update();
}

void OpenGLViewport::BeginPickXYPoint(const QString& prompt) {
    BeginPick3DPointOnPlane(CPoint3d(0,0,0), {0,0,1},
        prompt.isEmpty() ? "Pick Pc: click point on XY plane" : prompt);
    picking_3d_point_ = false;
    picking_xy_point_ = true;
}

void OpenGLViewport::BeginPick3DPoint(const QString& prompt) {
    ApplyWorkPlaneFrame();
    picking_xy_point_ = false;
    picking_solid_surface_ = false;
    creation_snap_active_ = false;
    point_pick_object_id_ = 0;
    point_pick_plane_enabled_ = false;
    picking_3d_point_ = true;
    orbiting_ = false;
    alt_orbiting_ = false;
    panning_ = false;
    zooming_ = false;
    setCursor(Qt::CrossCursor);
    emit StatusTextChanged(prompt.isEmpty()
        ? "GetPoint3D: pick a point in the scene"
        : prompt);
    setFocus();
}

void OpenGLViewport::BeginSpatialCurvePreview(
    SpatialCurvePreviewKind kind) {
    active_tangent_id_=pending_tangent_id_=0; ++tangent_hover_generation_;
    ApplyWorkPlaneFrame();
    spatial_curve_plane_valid_ = false;
    spatial_curve_preview_kind_ = kind;
    spatial_curve_preview_object_id_ = 0;
    spatial_curve_preview_points_.clear();
    curve_preview_valid_ = false;
    update();
}

void OpenGLViewport::SetSpatialCurvePreviewObject(
    unsigned long object_id) {
    spatial_curve_preview_object_id_ = object_id;
    update();
}

void OpenGLViewport::SetSpatialCurvePreviewPoints(
    const std::vector<CPoint3d>& points) {
    if (points.empty()) spatial_curve_plane_valid_ = false;
    if (!points.empty() && !spatial_curve_plane_valid_) {
        Vec3 forward{}, right{}, up{};
        viewport_camera_basis(camera_, forward, right, up);
        spatial_curve_plane_origin_ = point_to_vec3(points.front());
        spatial_curve_plane_normal_ = normalize(forward);
        spatial_curve_plane_valid_ = true;
    }
    spatial_curve_preview_points_ = points;
    update();
}

void OpenGLViewport::EndSpatialCurvePreview() {
    active_tangent_id_=pending_tangent_id_=0; ++tangent_hover_generation_;
    auxiliary_guide_visible_ = false;
    spatial_curve_plane_valid_ = false;
    spatial_curve_preview_kind_ = SpatialCurvePreviewKind::None;
    spatial_curve_preview_object_id_ = 0;
    spatial_curve_preview_points_.clear();
    curve_preview_valid_ = false;
    update();
}

void OpenGLViewport::BeginPickSolidSurface(unsigned long object_id) {
    BeginPick3DPointOnObject(object_id, "Sheet Bend: click the part to bend (Esc to return)");
    picking_solid_surface_ = true;
}

void OpenGLViewport::CancelSolidSurfacePick() {
    if (!picking_solid_surface_) return;
    picking_solid_surface_ = false;
    picking_3d_point_ = false;
    point_pick_object_id_ = 0;
    RestoreDefaultToolCursor();
}

void OpenGLViewport::SetSheetBendGuide(const std::vector<CPoint3d>& arc) {
    sheet_bend_guide_ = arc;
    update();
}

void OpenGLViewport::SetSurfaceAlignmentPreview(std::vector<std::vector<CPoint3d>> lines) {
    surface_alignment_preview_=std::move(lines);update();
}

void OpenGLViewport::SetSurfaceSplitPreview(std::vector<CPoint3d> points) {
    surface_split_preview_=std::move(points);
    update();
}

void OpenGLViewport::BeginPick3DPointOnObject(
    unsigned long object_id, const QString& prompt) {
    BeginPick3DPoint(prompt);
    point_pick_object_id_ = object_id;
}

void OpenGLViewport::BeginPick3DPointOnPlane(
    CPoint3d plane_origin, Vec3 plane_normal, const QString& prompt) {
    BeginPick3DPoint(prompt);
    point_pick_plane_enabled_ = true;
    point_pick_plane_origin_ = {
        static_cast<float>(plane_origin.x),
        static_cast<float>(plane_origin.y),
        static_cast<float>(plane_origin.z)};
    point_pick_plane_normal_ = normalize(plane_normal);
}

void OpenGLViewport::BeginPickArchitectureWall(const QString& prompt) {
    picking_xy_point_ = false;
    picking_3d_point_ = false;
    picking_architecture_wall_ = true;
    orbiting_ = false;
    alt_orbiting_ = false;
    panning_ = false;
    zooming_ = false;
    setCursor(Qt::CrossCursor);
    emit StatusTextChanged(prompt.isEmpty()
        ? "Window / Door: click the approximate position on a visible wall"
        : prompt);
    setFocus();
}

void OpenGLViewport::CancelArchitectureWallPick() {
    if (!picking_architecture_wall_) return;
    picking_architecture_wall_ = false;
    RestoreDefaultToolCursor();
}

void OpenGLViewport::SetPointPickMarkers(const std::vector<CPoint3d>& points) {
    point_pick_markers_ = points;
    update();
}

void OpenGLViewport::ClearPointPickMarkers() {
    if (point_pick_markers_.empty()) return;
    point_pick_markers_.clear();
    update();
}

void OpenGLViewport::BeginPickRotationAxis() {
    coordinate_axis_selection_ = false;
    picking_rotation_axis_ = true;
    rotation_axis_hover_valid_ = false;
    orbiting_ = false;
    alt_orbiting_ = false;
    panning_ = false;
    zooming_ = false;
    setCursor(Qt::CrossCursor);
    emit StatusTextChanged(
        "Rotate: select a coordinate axis, polyline/sketch segment, or straight solid edge");
    setFocus();
    update();
}

void OpenGLViewport::SetCoordinateAxisSelection(bool enabled) {
    coordinate_axis_selection_ = enabled;
    rotation_axis_hover_valid_ = false;
    if (!enabled) RestoreDefaultToolCursor();
    update();
}

void OpenGLViewport::BeginMovePointToPoint(bool repeat) {
    picking_3d_point_ = false;
    SetTool(ToolMode::MovePointToPoint);
    move_point_repeat_ = repeat;
    measurement_visible_ = false;
    measurement_waiting_for_second_point_ = false;
    measurement_preview_valid_ = false;
    move_point_stage_ = document_ && document_->HasSelection()
        ? MovePointStage::PickSource
        : MovePointStage::SelectObjects;
    setCursor(Qt::CrossCursor);
    emit StatusTextChanged(move_point_stage_ == MovePointStage::PickSource
        ? "Move Point to Point: pick source point"
        : "Move Point to Point: select object(s), then press Enter");
}

void OpenGLViewport::BeginMeasurePointToPoint() {
    picking_3d_point_ = false;
    picking_xy_point_ = false;
    picking_solid_surface_ = false;
    picking_rotation_axis_ = false;
    point_pick_object_id_ = 0;
    point_pick_plane_enabled_ = false;
    SetTool(ToolMode::MeasurePointToPoint);
    measurement_waiting_for_second_point_ = false;
    measurement_preview_valid_ = false;
    setCursor(Qt::CrossCursor);
    emit StatusTextChanged("Point-to-Point Dimension: pick first point");
}

void OpenGLViewport::EndSketch() {
    if (tool_ == ToolMode::SketchPolyline && sketch_polyline_points_.size() >= 2) {
        if (!CommitSketchPolyline(false)) return;
    }
    EndDirectCurveEdit();
    sketch_active_ = false;
    multi_sketch_session_ = false;
    multi_sketch_id_ = 0;
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;
    sketch_polyline_points_.clear();
    sketch_polyline_preview_valid_ = false;
    sketch_bezier_points_.clear();
    sketch_bezier_preview_valid_ = false;
    sketch_attachment_body_id_ = 0;
    sketch_attachment_face_index_ = -1;
    if (tool_ == ToolMode::SketchRectangle
        || tool_ == ToolMode::SketchPolyline
        || tool_ == ToolMode::SketchBezier
        || tool_ == ToolMode::SketchConvertBezier
        || tool_ == ToolMode::SketchConvertArc
        || tool_ == ToolMode::SketchBoolean
        || tool_ == ToolMode::SketchFillet
        || tool_ == ToolMode::SolidBoxRectangle
        || tool_ == ToolMode::SolidCylinderCircle) {
        SetTool(ToolMode::Select);
    } else {
        update();
    }
}

void OpenGLViewport::initializeGL() {
    renderer_.Initialize();
}

void OpenGLViewport::resizeGL(int, int) {
}

void OpenGLViewport::paintGL() {
    if (!document_) {
        return;
    }
   UpdateFPS();
    // The active curve already contains the confirmed points. While the next
    // point is previewed, drawing that stored version as well would leave a
    // second, stale fragment under the live curve. Hide it only for this
    // render pass; the document and Scene Tree visibility remain unchanged.
    CAlfaObject* preview_source = curve_preview_valid_
        && spatial_curve_preview_object_id_ != 0
        ? document_->FindObjectById(spatial_curve_preview_object_id_)
        : nullptr;
    const bool hide_preview_source = preview_source
        && preview_source->IsVisible()
        && !spatial_curve_preview_points_.empty();
    if (hide_preview_source) preview_source->SetVisible(false);
    CAlfaObject* primitive_target = primitive_preview_solid_ && primitive_height_ < -0.001
        && solid_box_target_body_id_ != 0
        ? document_->FindObjectById(solid_box_target_body_id_) : nullptr;
    const bool hide_primitive_target = primitive_target && primitive_target->IsVisible();
    if (hide_primitive_target) primitive_target->SetVisible(false);
    auto* editing_parent = dynamic_cast<CSketch*>(document_->GetSelectedObject());
    const auto* editing_contour = editing_parent ? EditableSketch() : nullptr;
    const bool preview_contour = editing_parent && editing_parent->IsVisible()
        && editing_contour && editing_contour->IsEditing();
    if (preview_contour) editing_parent->SetVisible(false);
    // Qt input and painter overlays use logical coordinates; glViewport uses
    // physical framebuffer pixels. Read the current DPR on every frame so
    // moving the window between monitors also updates the render extent.
    const QSize framebuffer_size = size() * devicePixelRatioF();
    renderer_.Render(*document_, camera_, orthographic_projection_, show_coordinate_axes_ || coordinate_axis_selection_, show_floor_grid_, xy_plane_view_enabled_, grid_size_, grid_step_, grid_subdivisions_, tool_, universal_transform_ ? TransformOperation::Universal : transform_operation_, highlighted_transform_axis_, transform_dialog_rotation_angle_degrees_, transform_dialog_rotation_axis_, highlighted_draft_face_gizmo_, framebuffer_size.width(), framebuffer_size.height());
    if (hide_preview_source) preview_source->SetVisible(true);
    if (hide_primitive_target) primitive_target->SetVisible(true);
    if (preview_contour) {
        editing_parent->SetVisible(true);
        for (size_t i = 0; i < editing_parent->GetContourCount(); ++i) {
            if (editing_parent->GetContourId(i) == edit_contour_id_) editing_contour->Render3d(true);
            else editing_parent->MakeWorldContour(i).Render3d(true);
        }
    }
    if(tool_==ToolMode::SketchBoolean && editing_parent && editing_parent->m_id==sketch_boolean_parent_) {
        for(size_t i=0;i<editing_parent->GetContourCount();++i)if(editing_parent->GetContourId(i)==sketch_boolean_first_) {
            auto first=editing_parent->MakeWorldContour(i);
            first.SetColor({0.f,1.f,1.f});first.SetLineWidth(4);first.Render3d(false);
        }
    }
    if (primitive_preview_solid_) primitive_preview_solid_->Render3d(false);
    DrawFaceExtrudePreviewWalls();
    DrawHoveredSolidEdge();
    DrawRotationAxisPickPreview();
    DrawReferenceFaceHover();
    if (show_coordinate_axes_ || coordinate_axis_selection_) {
        DrawCoordinateAxisLabels();
    }
    if ((tool_ == ToolMode::DrawCurve || tool_ == ToolMode::DrawBSpline) && curve_preview_valid_) {
        DrawCurveRubberBand();
    }
    if (tool_ == ToolMode::DrawSpline && drawing_spline_stroke_) {
        DrawSplinePreview();
    }
    if ((tool_ == ToolMode::SketchRectangle || tool_ == ToolMode::SolidBoxRectangle)
        && sketch_rectangle_has_first_point_
        && sketch_rectangle_preview_valid_) {
        DrawSketchRectanglePreview();
    }
    if (tool_ == ToolMode::SolidCylinderCircle
        && sketch_rectangle_has_first_point_
        && sketch_rectangle_preview_valid_) {
        DrawSolidCylinderCirclePreview();
    }
    if (primitive_height_active_) DrawPrimitiveHeightPreview();
    if (tool_ == ToolMode::SketchFillet && sketch_fillet_preview_valid_) {
        DrawSketchFilletRadiusPreview();
    }
    if (tool_ == ToolMode::SketchPolyline && !sketch_polyline_points_.empty()) {
        DrawSketchPolylinePreview();
    }
    if (tool_ == ToolMode::SketchBezier && !sketch_bezier_points_.empty()) {
        DrawSketchBezierPreview();
    }
    if (show_curve_points_ && document_) {
        DrawVisibleCurvePoints();
    }
    if ((show_curve_points_
         || tool_ == ToolMode::EditPoint
         || selection_mode_ == SelectionMode::Point
         || (tool_ == ToolMode::Select && editing_polyline_))
        && document_) {
        DrawSelectedCurvePointHandles();
    }
    if (document_ && ((show_curve_points_ && EditableSketch())
         || (tool_ == ToolMode::Select && editing_sketch_)
         || tool_ == ToolMode::SketchSmoothJoint || tool_ == ToolMode::SketchSharpJoint)) {
        DrawSketchEditHandles();
    }
    if (tool_ == ToolMode::EditPoint && selecting_edit_points_) {
        DrawEditPointSelectionRect();
    }
    if (selecting_with_rect_
        && (tool_ == ToolMode::Select
            || (tool_ == ToolMode::Transform
                && selection_mode_ == SelectionMode::Point))) {
        DrawSelectRubberBandRect();
    }
    if (tool_ == ToolMode::ZoomRect && zoom_rect_active_) {
        DrawZoomRubberBandRect();
    }
    if (cabinet_preview_visible_) {
        DrawCabinetPreview();
    }
    if (!solid_dimension_object_.tool_id.empty()) {
        DrawSolidDimensions();
    }
    if (measurement_visible_
        || (tool_ == ToolMode::MeasurePointToPoint
            && measurement_waiting_for_second_point_)
        || (tool_ == ToolMode::MovePointToPoint
            && move_point_stage_ == MovePointStage::PickTarget
            && measurement_waiting_for_second_point_)) {
        DrawPointToPointMeasurement();
    }
    if (!point_pick_markers_.empty()) {
        DrawPointPickMarkers();
    }
    if (!sheet_bend_guide_.empty()) DrawSheetBendGuide();
    if (!surface_split_preview_.empty() || !surface_alignment_preview_.empty()) DrawSurfaceSplitPreview();
    if (material_drag_active_) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QPixmap sphere = MaterialDrag::SpherePixmap(material_drag_preview_, 58, true);
        painter.drawPixmap(material_drag_pos_ - QPoint(sphere.width() / 2, sphere.height() / 2), sphere);
    }
    if (tool_ == ToolMode::Walk) {
        DrawWalkMiniMap();
    }
    DrawReferencePlanes();
    DrawSurfaceFilletNormals();
    DrawBottleModelControls();
    bool persistent_tangent=false;
    if (active_tangent_id_ && IsSnapTargetEnabled(SnapTarget::AuxLine)) {
        const auto* source=dynamic_cast<const CBSpline*>(document_->FindObjectById(active_tangent_id_));
        if (source && document_->IsObjectVisible(*source)
            && endpoint_tangent(*source,active_tangent_start_,auxiliary_guide_origin_,auxiliary_guide_direction_)) {
            persistent_tangent=true;
            auxiliary_guide_visible_=tangent_guide_visible_=true;
        } else {
            active_tangent_id_=0;
            auxiliary_guide_visible_=false;
        }
    }
    if (auxiliary_guide_visible_ && (creation_snap_active_ || persistent_tangent)
        && (IsSnapTargetEnabled(SnapTarget::AuxLine) || IsSnapTargetEnabled(SnapTarget::AuxLine45))) {
        DomPoint a{}, b{};
        if (renderer_.WorldToScreen(auxiliary_guide_origin_, camera_, orthographic_projection_, width(), height(), a)
            && renderer_.WorldToScreen(auxiliary_guide_origin_ + auxiliary_guide_direction_ * std::max(1.0f,camera_.distance*0.1f),
                                       camera_, orthographic_projection_, width(), height(), b)) {
            QPointF direction(b.x-a.x,b.y-a.y);
            const double length=std::hypot(direction.x(),direction.y());
            if (length>0.01) {
                direction *= (2.0*std::hypot(width(),height())/length);
                // Scene overlays can leave depth/stencil/line state enabled.
                // A QPainter alone does not reset all compatibility GL state.
                glPushAttrib(GL_ALL_ATTRIB_BITS);
                glDisable(GL_DEPTH_TEST);
                glDepthMask(GL_FALSE);
                glDisable(GL_LIGHTING);
                glDisable(GL_CULL_FACE);
                glDisable(GL_ALPHA_TEST);
                glDisable(GL_STENCIL_TEST);
                glDisable(GL_LINE_STIPPLE);
                glDisable(GL_POLYGON_STIPPLE);
                glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setPen(QPen(tangent_guide_visible_ ? QColor(220,100,255) : QColor(190,190,190),
                                    tangent_guide_visible_ ? 1.5 : 1.0,
                                    tangent_guide_visible_ ? Qt::SolidLine : Qt::DashLine));
                painter.drawLine(QPointF(a.x,a.y)-direction,QPointF(a.x,a.y)+direction);
                if (persistent_tangent) {
                    painter.setBrush(Qt::NoBrush);
                    painter.drawEllipse(QPointF(a.x,a.y),6,6);
                }
                painter.end();
                glPopAttrib();
            }
        }
    }
    DrawSubdivisionPreview();
    DrawFPS();

    if (!first_frame_rendered_) {
        first_frame_rendered_ = true;
        emit FirstFrameRendered();
    }
}

void OpenGLViewport::SetSurfaceFilletNormals(std::vector<SurfaceFilletNormalGuide> guides) {
    surface_fillet_normals_=std::move(guides);update();
}
bool OpenGLViewport::GetSurfaceSelectionPoint(unsigned long id,int face,CPoint3d& point) const {
    if(id!=surface_pick_body_||face!=surface_pick_face_)return false;
    point=surface_pick_point_;return true;
}

void OpenGLViewport::DrawSurfaceFilletNormals() {
    if(surface_fillet_normals_.empty())return;
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);glDisable(GL_ALPHA_TEST);glDisable(GL_STENCIL_TEST);
    glDisable(GL_LINE_STIPPLE);glDisable(GL_POLYGON_STIPPLE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
    for(size_t i=0;i<surface_fillet_normals_.size();++i) {
        const auto& guide=surface_fillet_normals_[i];
        const Vec3 origin{float(guide.origin.x),float(guide.origin.y),float(guide.origin.z)};
        const Vec3 normal{float(guide.direction.x),float(guide.direction.y),float(guide.direction.z)};
        DomPoint a{},b{};
        if(!renderer_.WorldToScreen(origin,camera_,orthographic_projection_,width(),height(),a)
            ||!renderer_.WorldToScreen(origin+normal*(camera_.distance*.12f),camera_,orthographic_projection_,width(),height(),b))continue;
        const QPointF start(a.x,a.y);
        QPointF direction(b.x-a.x,b.y-a.y);
        const double length=std::hypot(direction.x(),direction.y());
        const QColor color=i==0?QColor(255,215,40):QColor(70,230,255);
        QPointF tip=start;
        if(length>2) {
            direction/=length;tip=start+direction*std::min(80.0,length);
            const QPointF side(-direction.y(),direction.x());
            painter.setPen(QPen(Qt::black,5));painter.drawLine(start,tip);
            painter.setPen(QPen(color,2));painter.drawLine(start,tip);
            painter.setBrush(color);painter.setPen(QPen(Qt::black,1));
            painter.drawPolygon(QPolygonF{tip,tip-direction*13+side*5,tip-direction*13-side*5});
        }
        painter.setPen(QPen(Qt::black,1));painter.setBrush(color);painter.drawEllipse(start,3,3);
        const QString label=QString("N%1").arg(i+1);
        const QRectF box(tip+QPointF(8,-20),QSizeF(29,20));
        painter.fillRect(box,QColor(0,0,0,180));painter.setPen(color);painter.drawText(box,Qt::AlignCenter,label);
    }
    painter.end();glPopAttrib();
}

QImage OpenGLViewport::CaptureSceneImage(const QSize& requested_size) {
    if (!document_ || !isValid() || requested_size.isEmpty()) {
        return {};
    }

    makeCurrent();
    QOpenGLFunctions* functions = context() ? context()->functions() : nullptr;
    if (!functions) {
        doneCurrent();
        return {};
    }

    GLint maximum_size = 0;
    functions->glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maximum_size);
    QSize capture_size = requested_size;
    if (maximum_size > 0
        && (capture_size.width() > maximum_size
            || capture_size.height() > maximum_size)) {
        capture_size.scale(maximum_size, maximum_size, Qt::KeepAspectRatio);
    }
    capture_size.setWidth(std::max(1, capture_size.width()));
    capture_size.setHeight(std::max(1, capture_size.height()));

    GLint previous_framebuffer = 0;
    GLint previous_viewport[4]{};
    functions->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
    functions->glGetIntegerv(GL_VIEWPORT, previous_viewport);

    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    format.setSamples(4);
    auto framebuffer = std::make_unique<QOpenGLFramebufferObject>(
        capture_size, format);
    if (!framebuffer->isValid()) {
        format.setSamples(0);
        framebuffer = std::make_unique<QOpenGLFramebufferObject>(
            capture_size, format);
    }
    if (!framebuffer->isValid() || !framebuffer->bind()) {
        functions->glBindFramebuffer(
            GL_FRAMEBUFFER, static_cast<GLuint>(previous_framebuffer));
        functions->glViewport(previous_viewport[0], previous_viewport[1],
            previous_viewport[2], previous_viewport[3]);
        doneCurrent();
        return {};
    }

    // Print a clean client view: preserve the camera, materials, lighting and
    // background, but omit editing gizmos, grids, axes and FPS overlays.
    renderer_.Render(
        *document_, camera_, orthographic_projection_, false, false,
        xy_plane_view_enabled_, grid_size_, grid_step_, grid_subdivisions_,
        ToolMode::Select, TransformOperation::Move, TransformAxis::None,
        0.0f, {}, false, capture_size.width(), capture_size.height());
    functions->glFinish();
    QImage image = framebuffer->toImage(true);

    functions->glBindFramebuffer(
        GL_FRAMEBUFFER, static_cast<GLuint>(previous_framebuffer));
    functions->glViewport(previous_viewport[0], previous_viewport[1],
        previous_viewport[2], previous_viewport[3]);
    doneCurrent();
    update();
    return image;
}

void OpenGLViewport::mousePressEvent(QMouseEvent* event) {
    sketch_shape_circle_=event->modifiers().testFlag(Qt::ShiftModifier);
    if(event->button()==Qt::LeftButton && event->modifiers()==Qt::NoModifier
       && BeginBottleModelDrag(event->pos())) {event->accept();return;}

    ++edge_quick_menu_generation_;
    pending_tangent_id_=0; ++tangent_hover_generation_;
    if (property("arrayPreviewNavigationOnly").toBool()) {
        StopCameraAnimation(false); last_mouse_=event->pos();
        const auto drag=NavigationDragFor(*event);
        orbiting_=drag==NavigationDrag::Orbit; panning_=drag==NavigationDrag::Pan; zooming_=drag==NavigationDrag::Zoom;
        alt_orbiting_=false;
        orbit_drag_pivot_=rotation_pivot_enabled_?rotation_pivot_point_:NavigationSceneCenter();
        event->accept(); return;
    }

    if (coordinate_axis_selection_ && event->button() == Qt::LeftButton && event->modifiers() == Qt::NoModifier) {
        Vec3 start{}, end{};
        if (HitTestRotationAxisLine(event->pos(), start, end)) {
            const int axis = std::abs(end.x) > 0 ? 0 : std::abs(end.y) > 0 ? 1 : 2;
            emit CoordinateAxisSelected(axis);
            event->accept();
            update();
            return;
        }
    }
    StopCameraAnimation(event->button() == Qt::LeftButton && event->modifiers() == Qt::NoModifier);
    reference_hover_body_ = 0;
    last_mouse_ = event->pos();
    if (event->button() == Qt::LeftButton && event->modifiers() == Qt::NoModifier
        && ReferencePlanePickerActive()) {
        const int plane = HitReferencePlane(event->pos());
        if (plane >= 0) {
            if (!reference_plane_dialog_)
                SetSolidPrimitivePlacement(static_cast<SketchPlane>(plane), false);
            reference_plane_pending_ = false;
            hovered_reference_plane_ = -1;
            emit ReferencePlaneSelected(plane);
            setCursor(Qt::CrossCursor);
            update();
            event->accept();
            return;
        }
    }
    if (document_ && (ReferencePlanePickerActive() || reference_plane_dialog_)
        && event->button() == Qt::LeftButton && event->modifiers() == Qt::NoModifier) {
        UpdateReferenceFaceHover(event->pos());
        const auto project = [this](Vec3 world, DomPoint& screen, float& depth) {
            const Vec3 forward = rotate(camera_.orientation, {0,0,-1});
            depth = dot(world - camera_position(camera_, orthographic_projection_), forward);
            return depth > 0 && renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
        };
        if (reference_hover_body_ != 0
            && document_->SelectSolidPlanarFaceAtScreen({event->pos().x(), event->pos().y()}, project)) {
            reference_hover_body_ = 0;
            reference_plane_pending_ = false;
            hovered_reference_plane_ = -1;
            if (reference_plane_dialog_) {
                emit ReferenceBodyFaceSelected();
            } else {
                solid_box_waiting_for_face_ = true;
                if (tool_ == ToolMode::SolidCylinderCircle) HandleSolidCylinderCircleClick(event->pos());
                else HandleSolidBoxRectangleClick(event->pos());
                emit ReferencePlaneSelected(3);
            }
            update();
            event->accept();
            return;
        }
    }
    if (reference_plane_dialog_ && event->button() == Qt::LeftButton
        && event->modifiers() == Qt::NoModifier) {
        event->accept();
        return;
    }

    // Explicit material tools own a plain click before navigation and modeling tools.
    if (document_ && event->button() == Qt::LeftButton
        && event->modifiers() == Qt::NoModifier
        && material_interaction_mode_ != MaterialInteractionMode::None) {
        CAlfaObject* object = FindObjectForMaterialAt(event->pos());
        if (!object) {
            emit StatusTextChanged(material_interaction_mode_ == MaterialInteractionMode::Paint
                ? "Material brush: click a surface to paint"
                : "Material picker: click object");
            return;
        }

        if (material_interaction_mode_ == MaterialInteractionMode::Paint) {
            auto* solid = dynamic_cast<CSolid*>(object);
            int surface_index = -1;
            float depth = 0.0f;
            const auto project_world = [this](Vec3 world, DomPoint& screen, float& distance) {
                Vec3 forward{}, right{}, up{};
                viewport_camera_basis(camera_, forward, right, up);
                distance = dot(world - camera_position(camera_, orthographic_projection_), forward);
                return distance > 0.0f && renderer_.WorldToScreen(
                    world, camera_, orthographic_projection_, width(), height(), screen);
            };
            if (!solid || !solid->HitTestFaceScreen(
                    {event->pos().x(), event->pos().y()}, project_world, false, surface_index, depth)) {
                emit StatusTextChanged("Material brush: click a body surface");
                return;
            }
            const auto* surface = solid->GetSurfaceFace(surface_index);
            const auto before = surface->MaterialOverride;
            const unsigned long surface_id = static_cast<unsigned long>(surface->m_ID);
            const Material document_material = document_->UpsertMaterial(active_paint_material_);
            solid->SetSurfaceMaterial(surface_index, document_material);
            document_->ClearSelection();
            emit SelectionChanged();
            emit SurfaceMaterialPainted(solid->m_id, surface_id, before.enabled,
                                       before.material_id, before.material, document_material);
            emit StatusTextChanged(QString("Material applied: %1").arg(QString::fromStdString(document_material.name)));
            update();
            return;
        } else {
            const Material picked_material = object->GetMaterial();
            emit MaterialPicked(picked_material);
            document_->ClearSelection();
            emit SelectionChanged();
            emit StatusTextChanged(QString("Material picked: %1").arg(QString::fromStdString(picked_material.name)));
        }

        material_interaction_mode_ = MaterialInteractionMode::None;
        RestoreDefaultToolCursor();
        update();
        return;
    }

    if (tool_ == ToolMode::Walk
        && event->button() == Qt::LeftButton
        && PlaceWalkCameraFromMiniMap(event->pos())) {
        event->accept();
        return;
    }

    if (picking_architecture_wall_ && event->button() == Qt::LeftButton
        && event->modifiers() == Qt::NoModifier) {
        CPoint3d picked{};
        if (auto* wall = PickArchitecturePoint(event->pos(), picked)) {
            document_->EnsureObjectId(*wall);
            picking_architecture_wall_ = false;
            creation_snap_active_ = false;
            RestoreDefaultToolCursor();
            emit ArchitectureWallPicked(wall->m_id, picked);
        } else {
            emit StatusTextChanged("Window / Door: click a visible room wall");
        }
        event->accept();
        return;
    }

    // Explicit XY placement (including Text) takes priority over Orbit.
    if (picking_xy_point_ && event->button() == Qt::LeftButton
        && event->modifiers() == Qt::NoModifier) {
        CPoint3d point{};
        if (PickRequestedPoint(event->pos(), point)) {
            picking_xy_point_ = false;
            point_pick_plane_enabled_ = false;
            creation_snap_active_ = false;
            RestoreDefaultToolCursor();
            emit XYPointPicked(point);
            emit StatusTextChanged(QString("Pc picked: X %1, Y %2, Z %3")
                .arg(point.x, 0, 'f', 6)
                .arg(point.y, 0, 'f', 6)
                .arg(point.z, 0, 'f', 6));
            event->accept();
            return;
        }
        emit StatusTextChanged("Pick Pc: point is outside XY plane view");
        event->accept();
        return;
    }

    // Orbit normally owns LMB from the moment it is pressed. Give a plain
    // click on an interactive furniture handle priority, so presentation and
    // room-viewing mode can operate doors and drawers without switching to
    // Select. Modified drags (notably Alt + LMB) remain pure navigation.
    // Explicit point picking owns a plain click even while Orbit is active.
    if (picking_3d_point_ && document_
        && event->button() == Qt::LeftButton
        && (event->modifiers() == Qt::NoModifier
            || ((tool_ == ToolMode::DrawCurve || tool_ == ToolMode::DrawBSpline)
                && event->modifiers() == Qt::ShiftModifier))) {
        CPoint3d picked{};
        const bool point_found = PickRequestedPoint(event->pos(), picked);
        if (point_found) {
            ConstrainPolylinePoint(picked, event->modifiers());
            creation_snap_active_ = false;
            picking_3d_point_ = false;
            point_pick_plane_enabled_ = false;
            RestoreDefaultToolCursor();
            // Publish coordinates first. A command handling the point can
            // then replace this with its next-step or result message.
            emit StatusTextChanged(QString("Point: X %1, Y %2, Z %3")
                .arg(picked.x, 0, 'f', 3)
                .arg(picked.y, 0, 'f', 3)
                .arg(picked.z, 0, 'f', 3));
            emit Point3DPicked(picked);
        } else {
            // A saved scene does not include transient pick restrictions or
            // modeling preferences. Keep enough state to diagnose a failed
            // first click without altering either the scene or its settings.
            if constexpr (Dom3DDiagnosticsEnabled) {
                QFile diagnostic(QDir::temp().filePath("Dom3D-point-input.log"));
                const auto mode = diagnostic.size() > 65536 ? QIODevice::Truncate : QIODevice::Append;
                if (diagnostic.open(QIODevice::WriteOnly | QIODevice::Text | mode)) {
                    QTextStream log(&diagnostic);
                    log << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
                        << " exe=" << QCoreApplication::applicationFilePath()
                        << " tool=" << int(tool_) << " pixel=" << event->pos().x() << ',' << event->pos().y()
                        << " viewport=" << width() << ',' << height()
                        << " ortho=" << orthographic_projection_ << " xy=" << xy_plane_view_enabled_
                        << " snapping=" << snapping_enabled_ << " workPlane=" << IsSnapTargetEnabled(SnapTarget::WorkPlane)
                        << " object=" << point_pick_object_id_ << " surface=" << picking_solid_surface_
                        << " explicitPlane=" << point_pick_plane_enabled_
                        << " curveKind=" << int(spatial_curve_preview_kind_)
                        << " curvePoints=" << spatial_curve_preview_points_.size()
                        << " curvePlane=" << spatial_curve_plane_valid_
                        << " camera=" << camera_.target.x << ',' << camera_.target.y << ',' << camera_.target.z
                        << " distance=" << camera_.distance
                        << " rotation=" << camera_.orientation.w << ',' << camera_.orientation.x
                        << ',' << camera_.orientation.y << ',' << camera_.orientation.z << '\n';
                }
            }
            emit StatusTextChanged(point_pick_plane_enabled_
                ? "Point: current view ray is parallel to the selected face"
                : IsSnapTargetEnabled(SnapTarget::WorkPlane)
                    ? "Point: work plane is edge-on or behind the view; change view or disable Work Plane snapping"
                : xy_plane_view_enabled_
                    ? "Point: XY plane cannot be reached from this view; return to XY view"
                : IsSnapTargetEnabled(SnapTarget::Surface) ? "Surface: no visible surface under the cursor"
                : "GetPoint3D: move the cursor to a visible vertex or curve point");
        }
        event->accept();
        return;
    }

    const Qt::KeyboardModifiers navigation_modifiers = event->modifiers()
        & (Qt::ShiftModifier | Qt::ControlModifier
           | Qt::AltModifier | Qt::MetaModifier);
    // Plain clicks on parameter dimensions must reach the editor before
    // Orbit consumes LMB. Modified clicks retain the navigation preset.
    const bool dimension_click = tool_ == ToolMode::Orbit
        && event->button() == Qt::LeftButton
        && navigation_modifiers == Qt::NoModifier
        && !solid_dimension_object_.tool_id.empty()
        && std::any_of(solid_dimension_hits_.begin(), solid_dimension_hits_.end(),
            [&](const SolidDimensionHit& hit) {
                if (hit.is_grip) return hit.rect.contains(event->pos());
                if (hit.is_label) return hit.rect.adjusted(-4,-4,4,4).contains(event->pos());
                return DistanceToScreenSegment({event->pos().x(), event->pos().y()},
                    hit.line_start, hit.line_end) <= 10.0f;
            });
    if (tool_ == ToolMode::Orbit
        && event->button() == Qt::LeftButton
        && navigation_modifiers == Qt::NoModifier
        && !dimension_click
        && selection_mode_ == SelectionMode::Object
        && material_interaction_mode_ == MaterialInteractionMode::None) {
        if (CAlfaObject* handle =
                FindInteractiveFurnitureHandleAt(event->pos())) {
            emit FurnitureInteractionRequested(handle->m_id);
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::RightButton
        && document_
        && tool_ == ToolMode::Transform
        && event->modifiers().testFlag(Qt::ShiftModifier)) {
        const DomPoint screen_point{event->pos().x(), event->pos().y()};
        auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
            return renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
        };
        Vec3 origin{};
        if (document_->HasSelection()
            && document_->PickTransformGizmoOriginAtScreen(screen_point, world_to_screen, 14.0f, origin)) {
            document_->SetTransformGizmoOrigin(origin);
            emit StatusTextChanged(QString("Gizmo Origin: X %1, Y %2, Z %3")
                .arg(origin.x, 0, 'f', 3)
                .arg(origin.y, 0, 'f', 3)
                .arg(origin.z, 0, 'f', 3));
            update();
        } else {
            emit StatusTextChanged("Gizmo Origin: click a vertex or edge with Shift + right mouse button");
        }
        event->accept();
        return;
    }

    const NavigationDrag navigation_drag = dimension_click
        ? NavigationDrag::None : NavigationDragFor(*event);
    if (navigation_drag != NavigationDrag::None) {
        orbiting_ = navigation_drag == NavigationDrag::Orbit;
        if (orbiting_) {
            orbit_drag_pivot_ = rotation_pivot_enabled_
                ? rotation_pivot_point_ : NavigationSceneCenter();
            if (!rotation_pivot_enabled_) {
                PickNavigationPoint(event->pos(), orbit_drag_pivot_);
            }
        }
        alt_orbiting_ = false;
        panning_ = navigation_drag == NavigationDrag::Pan;
        zooming_ = navigation_drag == NavigationDrag::Zoom;
        dragging_transform_ = false;
        if (event->button() == Qt::RightButton) {
            right_navigation_active_ = true;
            right_button_press_ = event->pos();
            right_button_dragged_ = false;
        }
        if (panning_) {
            setCursor(pan_scene_cursor());
        } else if (orbiting_) {
            setCursor(orbit_cursor());
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::RightButton) {
        right_navigation_active_ = true;
        right_button_press_ = event->pos();
        right_button_dragged_ = false;
        orbiting_ = false;
        alt_orbiting_ = false;
        panning_ = false;
        zooming_ = false;
        dragging_transform_ = false;
        return;
    }

    if (event->button() != Qt::LeftButton || !document_) {
        return;
    }

    if (tool_ == ToolMode::Walk) {
        orbiting_ = true;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (picking_rotation_axis_) {
        Vec3 axis_start{};
        Vec3 axis_end{};
        if (HitTestRotationAxisLine(event->pos(), axis_start, axis_end)) {
            picking_rotation_axis_ = false;
            rotation_axis_hover_valid_ = false;
            RestoreDefaultToolCursor();
            emit RotationAxisPicked(
                CPoint3d(axis_start.x, axis_start.y, axis_start.z),
                CPoint3d(axis_end.x, axis_end.y, axis_end.z));
            emit StatusTextChanged(property("projectionDirectionPick").toBool()
                ? "Projection direction selected" : "Rotate: rotation axis selected");
            update();
        } else {
            emit StatusTextChanged(
                property("projectionDirectionPick").toBool()
                    ? "Projection: click a coordinate axis, straight segment, or straight edge"
                    : "Rotate: point to a coordinate axis, straight segment, or straight edge");
        }
        event->accept();
        return;
    }


    if (tool_ == ToolMode::ZoomRect) {
        zoom_rect_active_ = true;
        zoom_rect_start_ = event->pos();
        zoom_rect_current_ = event->pos();
        update();
        return;
    }

    if ((tool_ == ToolMode::Select || tool_ == ToolMode::Orbit || tool_ == ToolMode::SolidFillet)
        && !solid_dimension_object_.tool_id.empty()) {
        const SolidDimensionHit* best_hit = nullptr;
        float best_distance = std::numeric_limits<float>::max();

        // Grips are direct-manipulation handles and take precedence over
        // dimension labels and lines when their screen areas overlap.
        for (const SolidDimensionHit& dimension : solid_dimension_hits_) {
            if (!dimension.is_grip || !dimension.rect.contains(event->pos())) {
                continue;
            }
            const QPoint delta = event->pos() - dimension.rect.center();
            const float distance = std::hypot(
                static_cast<float>(delta.x()),
                static_cast<float>(delta.y()));
            if (distance < best_distance) {
                best_distance = distance;
                best_hit = &dimension;
            }
        }

        if (best_hit && best_hit->is_grip) {
            if (best_hit->source_index >= solid_dimension_objects_.size()) {
                return;
            }
            const ActiveParametricObject& dimension_object =
                solid_dimension_objects_[best_hit->source_index];
            if(best_hit->parameter_id=="origin.center") {
                for(const auto& grip:cylinder_center_grips_) if(grip.operation_index==int(dimension_object.operation_index)) {
                    const Vec3 normal=normalize(cross(grip.world_u,grip.world_v));
                    if(!ScreenToWorldPlane(event->pos(),point_to_vec3(grip.center),normal,cylinder_center_drag_start_))return;
                    cylinder_center_drag_=grip;
                    cylinder_center_drag_value_=grip.origin;
                    active_solid_dimension_grip_="origin.center";
                    active_solid_dimension_operation_index_=grip.operation_index;
                    dragging_solid_dimension_grip_=true;
                    setCursor(Qt::ClosedHandCursor);event->accept();return;
                }
                return;
            }
            const auto parameter = std::find_if(
                dimension_object.parameters.begin(),
                dimension_object.parameters.end(),
                [best_hit](const ToolParameter& candidate) {
                    return QString::fromStdString(candidate.id)
                        == best_hit->parameter_id;
                });
            const float line_dx = static_cast<float>(
                best_hit->line_end.x - best_hit->line_start.x);
            const float line_dy = static_cast<float>(
                best_hit->line_end.y - best_hit->line_start.y);
            const double line_length = std::hypot(line_dx, line_dy);
            if (parameter != dimension_object.parameters.end()
                && line_length > 0.5) {
                dragging_solid_dimension_grip_ = true;
                solid_dimension_drag_move_base_ = best_hit->move_base;
                active_solid_dimension_grip_ = best_hit->parameter_id;
                active_solid_dimension_operation_index_ =
                    static_cast<int>(dimension_object.operation_index);
                highlighted_solid_dimension_grip_ = best_hit->parameter_id;
                highlighted_solid_dimension_operation_index_ =
                    active_solid_dimension_operation_index_;
                solid_dimension_drag_start_mouse_ = event->pos();
                solid_dimension_drag_start_value_ = parameter->value;
                solid_dimension_drag_current_value_ = parameter->value;
                solid_dimension_drag_minimum_ = parameter->minimum;
                solid_dimension_drag_maximum_ = parameter->maximum;
                solid_dimension_drag_step_ = parameter->step;
                solid_dimension_drag_screen_direction_ = QPointF(
                    line_dx / line_length, line_dy / line_length);
                // A tiny fillet radius can project to only a few pixels. Keep
                // its real geometry, but give dragging a usable tablet/mouse
                // sensitivity instead of dividing by that tiny length.
                solid_dimension_drag_screen_length_ = (best_hit->parameter_id == "radius" || best_hit->parameter_id == "distance")
                    ? line_length : std::max(line_length, 24.0);
                setCursor(Qt::ClosedHandCursor);
                event->accept();
                return;
            }
        }

        best_hit = nullptr;
        best_distance = std::numeric_limits<float>::max();

        // Labels are the most explicit target. If several overlap, use the
        // one whose center is nearest to the click.
        for (const SolidDimensionHit& dimension : solid_dimension_hits_) {
            if (!dimension.is_label || dimension.is_grip
                || !dimension.rect.adjusted(-4, -4, 4, 4).contains(event->pos())) {
                continue;
            }
            const QPoint delta = event->pos() - dimension.rect.center();
            const float distance = std::sqrt(
                static_cast<float>(delta.x() * delta.x() + delta.y() * delta.y()));
            if (distance < best_distance) {
                best_distance = distance;
                best_hit = &dimension;
            }
        }

        // When no label was clicked, test the actual dimension segment rather
        // than its bounding rectangle (which can cover unrelated dimensions).
        if (!best_hit) {
            const DomPoint mouse{event->pos().x(), event->pos().y()};
            constexpr float kDimensionHitTolerance = 10.0f;
            for (const SolidDimensionHit& dimension : solid_dimension_hits_) {
                if (dimension.is_label || dimension.is_grip) {
                    continue;
                }
                const float distance = DistanceToScreenSegment(
                    mouse, dimension.line_start, dimension.line_end);
                if (distance <= kDimensionHitTolerance && distance < best_distance) {
                    best_distance = distance;
                    best_hit = &dimension;
                }
            }
        }

        if (best_hit) {
            if (best_hit->source_index < solid_dimension_objects_.size()) {
                emit SolidDimensionEditRequested(
                    static_cast<int>(solid_dimension_objects_[best_hit->source_index].operation_index),
                    best_hit->parameter_id,
                    best_hit->value);
            }
            event->accept();
            return;
        }
    }


    if (event->button() == Qt::LeftButton
        && !event->modifiers().testFlag(Qt::ShiftModifier)
        && !event->modifiers().testFlag(Qt::ControlModifier)
        && !event->modifiers().testFlag(Qt::AltModifier)
        && (tool_ == ToolMode::Select || tool_ == ToolMode::Orbit)
        && selection_mode_ == SelectionMode::Object) {
        if (CAlfaObject* handle =
                FindInteractiveFurnitureHandleAt(event->pos())) {
            emit FurnitureInteractionRequested(handle->m_id);
            event->accept();
            return;
        }
    }

    // A pending point request owns creation. Modified clicks that were not
    // consumed by point picking or navigation must never reach the legacy
    // active-curve handlers and append a point to an existing curve.
    if (picking_3d_point_
        && (tool_ == ToolMode::DrawCurve || tool_ == ToolMode::DrawBSpline)) {
        event->accept();
        return;
    }

    if (tool_ == ToolMode::DrawCurve) {
        DrawCurveAt(event->pos(), event->modifiers());
        return;
    }

    if (tool_ == ToolMode::DrawBSpline) {
        DrawBSplineAt(event->pos());
        return;
    }

    if (tool_ == ToolMode::DrawSpline) {
        BeginDrawSplineStroke(event->pos());
        event->accept();
        return;
    }

    if (tool_ == ToolMode::SketchRectangle) {
        HandleSketchRectangleClick(event->pos());
        return;
    }

    if (tool_ == ToolMode::SketchPolyline) {
        HandleSketchPolylineClick(event->pos());
        return;
    }
    if (tool_ == ToolMode::SketchBezier) {
        HandleSketchBezierClick(event->pos());
        return;
    }
    if (tool_ == ToolMode::SketchConvertBezier) {
        HandleSketchConvertLineToBezierClick(event->pos());
        return;
    }
    if (tool_ == ToolMode::SketchConvertArc) {
        HandleSketchConvertLineToArcClick(event->pos());
        return;
    }

    if (tool_ == ToolMode::SolidBoxRectangle) {
        if (primitive_height_active_) {
            UpdatePrimitiveHeight(event->pos());
            FinishPrimitiveCreation();
            return;
        }
        primitive_base_pressed_ = !solid_box_waiting_for_face_;
        primitive_second_press_ = sketch_rectangle_has_first_point_;
        primitive_press_point_ = event->pos();
        HandleSolidBoxRectangleClick(event->pos(), event->modifiers());
        return;
    }

    if (tool_ == ToolMode::SolidCylinderCircle) {
        if (primitive_height_active_) {
            UpdatePrimitiveHeight(event->pos());
            FinishPrimitiveCreation();
            return;
        }
        primitive_base_pressed_ = !solid_box_waiting_for_face_;
        primitive_second_press_ = sketch_rectangle_has_first_point_;
        primitive_press_point_ = event->pos();
        HandleSolidCylinderCircleClick(event->pos());
        return;
    }

    if (tool_ == ToolMode::SketchBoolean) {
        HandleSketchBooleanClick(event->pos());
        return;
    }

    if (tool_ == ToolMode::SketchFillet) {
        HandleSketchFilletClick(event->pos());
        return;
    }

    if (tool_ == ToolMode::SketchConstraintHorizontal
        || tool_ == ToolMode::SketchConstraintVertical
        || tool_ == ToolMode::SketchSmoothJoint
        || tool_ == ToolMode::SketchSharpJoint) {
        HandleSketchConstraintClick(event->pos());
        return;
    }

    if (tool_ == ToolMode::EditPoint) {
        const DomPoint screen_point{event->pos().x(), event->pos().y()};
        auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
            return renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
        };
        CPoint3d point{};
        const bool hit_selected_point = document_->PickSelectedCurvePointAtScreen(screen_point, world_to_screen, 11.0f, point);
        if (hit_selected_point || document_->SelectCurvePointAtScreen(screen_point, world_to_screen, 11.0f, SelectionAction::Replace)) {
            if (hit_selected_point || document_->GetSelectedPointPosition(point)) {
                BeginCurvePointDrag(point);
                emit SelectionChanged();
                update();
            }
        } else {
            selecting_edit_points_ = true;
            edit_point_selection_action_ = event->modifiers().testFlag(Qt::ShiftModifier)
                ? SelectionAction::Add
                : SelectionAction::Replace;
            edit_point_selection_start_ = event->pos();
            edit_point_selection_current_ = event->pos();
        }
        return;
    }

    if (tool_ == ToolMode::Boolean) {
        HandleBooleanClick(event->pos());
        return;
    }

    if (tool_ == ToolMode::FaceExtrude) {
        HandleFaceExtrudeClick(event->pos());
        return;
    }

    if (tool_ == ToolMode::DraftFace) {
        HandleDraftFaceClick(event->pos());
        return;
    }

    if (tool_ == ToolMode::ThickSolid) {
        HandleThickSolidClick(event->pos());
        return;
    }

    if (tool_ == ToolMode::Transform) {
        if (transform_operation_ == TransformOperation::Rotate
            && dot(transform_dialog_rotation_axis_,
                   transform_dialog_rotation_axis_) > 0.000001f
            && !xy_plane_view_enabled_
            && !sketch_active_) {
            orbiting_ = true;
            return;
        }
        if (selection_mode_ == SelectionMode::Point
            && HitTestTransformGizmo(event->pos()) == TransformAxis::None) {
            SelectionAction action = SelectionAction::Replace;
            if (event->modifiers().testFlag(Qt::ControlModifier)) {
                action = SelectionAction::Remove;
            } else if (event->modifiers().testFlag(Qt::ShiftModifier)) {
                action = SelectionAction::Add;
            }
            selecting_with_rect_ = true;
            rect_selection_action_ = action;
            rect_selection_start_ = event->pos();
            rect_selection_current_ = event->pos();
            update();
            return;
        }
        HandleTransformClick(event->pos(), event->modifiers().testFlag(Qt::ControlModifier));
        return;
    }

    if (tool_ == ToolMode::MovePointToPoint) {
        HandleMovePointToPointClick(event->pos());
        return;
    }

    if (tool_ == ToolMode::MeasurePointToPoint) {
        HandleMeasurePointToPointClick(event->pos());
        return;
    }

    if (tool_ == ToolMode::Select) {
        if (editing_sketch_) {
            PickSketchContour(event->pos());
            SketchHandleKind kind = SketchHandleKind::None;
            size_t index = 0;
            if (HitTestSelectedSketchHandle(event->pos(), kind, index)) {
                active_sketch_handle_kind_ = kind;
                active_sketch_handle_index_ = index;
                dragging_sketch_handle_ = true;
                if (auto* sketch=EditableSketch()) {
                    sketch_before_=std::make_shared<CSmartLine>(sketch->MakeCopy());
                    sketch_after_.reset(); sketch_change_id_=sketch->m_id;
                    sketch->BeginEdit();
                }
                sketch_drag_changed_ = false;
                last_mouse_ = event->pos();
                setCursor(Qt::ClosedHandCursor);
                update();
                return;
            }
            // Whole figures intentionally have no node handles. Retain the
            // picked contour on the first press so the following Qt double-
            // click can open its parameters instead of reselecting the sketch.
            if (const auto* sketch = EditableSketch(); sketch && sketch->GetPrimitive().kind
                && sketch->HitTestScreen({event->pos().x(), event->pos().y()},
                    [this](Vec3 world, DomPoint& screen) {
                        return renderer_.WorldToScreen(world, camera_, orthographic_projection_,
                            width(), height(), screen);
                    }, 12.0f)) {
                selecting_with_rect_ = false;
                highlighted_sketch_handle_kind_ = SketchHandleKind::None;
                update();
                event->accept();
                return;
            }
            // A click away from edit handles and whole figures finishes direct
            // Sketch editing and continues through the ordinary Select path below.
            // Previously every click was swallowed here until Esc was
            // pressed, which looked like object selection had frozen.
            EndDirectCurveEdit();
            emit StatusTextChanged("Sketch edit finished");
        }
        if (editing_polyline_ && HitTestSelectedPolylineHandle(event->pos())) {
            const DomPoint screen_point{event->pos().x(), event->pos().y()};
            auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
                return renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
            };
            document_->SelectCurvePointAtScreen(screen_point, world_to_screen, 10.0f);
            CPoint3d point{};
            if (document_->GetSelectedPointPosition(point)) {
                BeginCurvePointDrag(point);
                last_mouse_ = event->pos();
                emit SelectionChanged();
                update();
                return;
            }
        }

        SelectionAction action = SelectionAction::Replace;
        const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
        const bool control = event->modifiers().testFlag(Qt::ControlModifier);
        if (control) {
            action = SelectionAction::Remove;
        } else if (shift) {
            action = SelectionAction::Add;
        }
        selecting_with_rect_ = true;
        rect_selection_action_ = action;
        rect_selection_start_ = event->pos();
        rect_selection_current_ = event->pos();
        ++edge_quick_menu_generation_;
        update();
        return;
    }

}

void OpenGLViewport::mouseDoubleClickEvent(QMouseEvent* event) {
    const bool direct_curve_edit_tool = tool_ == ToolMode::Select
        || tool_ == ToolMode::EditPoint
        || tool_ == ToolMode::Orbit
        || tool_ == ToolMode::DrawSpline;
    if (!document_ || !direct_curve_edit_tool
        || event->button() != Qt::LeftButton) {
        QOpenGLWidget::mouseDoubleClickEvent(event);
        return;
    }

    if (tool_ == ToolMode::Select && editing_sketch_) {
        PickSketchContour(event->pos());
        auto* sketch=EditableSketch();
        if(sketch && sketch->GetPrimitive().kind && sketch->HitTestScreen(
            {event->pos().x(),event->pos().y()},[this](Vec3 world,DomPoint& screen) {
                return renderer_.WorldToScreen(world,camera_,orthographic_projection_,width(),height(),screen);
            },12.0f)) {
            selecting_with_rect_=false; orbiting_=false; alt_orbiting_=false;
            if(auto* old=findChild<QDialog*>("sketchPrimitiveParameters")) {
                old->setObjectName({}); old->close();
            }
            auto* dialog=new QDialog(this,Qt::Tool);
            dialog->setObjectName("sketchPrimitiveParameters");
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->setWindowTitle(DomTranslate("Figure parameters"));
            dialog->setModal(false);
            const auto resolve=[this,owner=sketch->m_id,contour=edit_contour_id_]() -> CSmartLine* {
                if(!document_ || !editing_sketch_ || tool_!=ToolMode::Select
                    || !document_->GetSelectedObject() || document_->GetSelectedObject()->m_id!=owner) return nullptr;
                auto* current=EditableSketch();
                return current && edit_contour_id_==contour && current->GetPrimitive().kind ? current : nullptr;
            };
            auto* layout=new QFormLayout(dialog);
            const auto p=sketch->GetPrimitive();
            const auto field=[&](const char* label,const char* name,double value,double minimum,double maximum) {
                auto* input=new QDoubleSpinBox(dialog); input->setObjectName(name);
                input->setDecimals(6); input->setRange(minimum,maximum); input->setValue(value);
                layout->addRow(DomTranslate(label),input); return input;
            };
            auto* u=field("Center U","centerU",p.u,-1e9,1e9);
            auto* v=field("Center V","centerV",p.v,-1e9,1e9);
            auto* radius=field(p.kind==3 ? "Radius" : "Radius / first semi-axis","radius",p.radius,0.000002,1e9);
            auto* minor=p.kind==3 ? nullptr : field("Second semi-axis","minorRadius",p.kind==1 ? p.radius : p.minor_radius,0.000002,1e9);
            constexpr double degrees=180/3.14159265358979323846;
            auto* angle=field("Rotation (degrees)","rotation",p.angle*degrees,-360000,360000);
            QSpinBox* sides=nullptr; QComboBox* mode=nullptr;
            QDoubleSpinBox* start=nullptr; QDoubleSpinBox* sweep=nullptr;
            if(p.kind==3) {
                sides=new QSpinBox(dialog); sides->setObjectName("sides"); sides->setRange(3,1000); sides->setValue(p.sides);
                layout->addRow(DomTranslate("Number of sides"),sides);
            } else {
                mode=new QComboBox(dialog); mode->setObjectName("conicMode");
                for(const auto* label : {"Full figure","Arc","Pie sector"}) mode->addItem(DomTranslate(label));
                mode->setCurrentIndex(p.IsFullConic() ? 0 : p.pie ? 2 : 1);
                layout->addRow(DomTranslate("Figure type"),mode);
                start=field("Start angle (degrees)","startAngle",p.start_angle*degrees,-360000,360000);
                sweep=field("Sweep angle (degrees)","sweepAngle",p.sweep_angle*degrees,.001,360);
            }
            auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,dialog);
            layout->addRow(buttons);
            connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::close);
            const auto apply=[this,dialog,resolve](const std::function<void(SketchPrimitive&)>& edit) {
                auto* current=resolve();
                if(!current) { dialog->close(); return; }
                if(current->IsEditing()) return;
                auto value=current->GetPrimitive(); edit(value);
                auto before=std::make_shared<CSmartLine>(current->MakeCopy());
                const auto revisions=current->GetRevisions();
                if(current->SetPrimitive(value) && (current->GetRevisions().geometry!=revisions.geometry
                    || current->GetRevisions().topology!=revisions.topology)) {
                    sketch_before_=before; sketch_after_=std::make_shared<CSmartLine>(current->MakeCopy()); sketch_change_id_=current->m_id;
                    NotifyDocumentChanged(); emit SelectionChanged(); update();
                }
            };
            connect(u,qOverload<double>(&QDoubleSpinBox::valueChanged),dialog,[apply](double value) { apply([=](auto& p){p.u=value;}); });
            connect(v,qOverload<double>(&QDoubleSpinBox::valueChanged),dialog,[apply](double value) { apply([=](auto& p){p.v=value;}); });
            connect(radius,qOverload<double>(&QDoubleSpinBox::valueChanged),dialog,[apply](double value) { apply([=](auto& p){p.radius=value;if(p.kind!=2)p.minor_radius=value;}); });
            connect(angle,qOverload<double>(&QDoubleSpinBox::valueChanged),dialog,[apply](double value) { apply([=](auto& p){p.angle=value/degrees;}); });
            if(minor) connect(minor,qOverload<double>(&QDoubleSpinBox::valueChanged),dialog,[apply](double value) { apply([=](auto& p){p.kind=2;p.minor_radius=value;}); });
            if(sides) connect(sides,qOverload<int>(&QSpinBox::valueChanged),dialog,[apply](int value) { apply([=](auto& p){p.sides=value;}); });
            if(start) connect(start,qOverload<double>(&QDoubleSpinBox::valueChanged),dialog,[apply](double value) { apply([=](auto& p){p.start_angle=value/degrees;}); });
            if(sweep) connect(sweep,qOverload<double>(&QDoubleSpinBox::valueChanged),dialog,[apply](double value) { apply([=](auto& p){p.sweep_angle=value/degrees;}); });
            if(mode) connect(mode,qOverload<int>(&QComboBox::currentIndexChanged),dialog,[apply](int value) {
                apply([=](auto& p){ p.pie=value==2; if(value==0)p.sweep_angle=6.28318530717958647692;
                    else if(p.IsFullConic())p.sweep_angle=4.71238898038468985769; });
            });
            auto* timer=new QTimer(dialog);
            connect(timer,&QTimer::timeout,dialog,[dialog,resolve,u,v,radius,minor,angle,sides,mode,start,sweep] {
                auto* current=resolve(); if(!current) { dialog->close(); return; }
                const auto p=current->GetPrimitive();
                const bool dragging=current->IsEditing();
                const auto sync=[dragging](QDoubleSpinBox* field,double value) {
                    if(field && (dragging || !field->hasFocus())) { const QSignalBlocker block(field); field->setValue(value); }
                };
                sync(u,p.u); sync(v,p.v); sync(radius,p.radius); sync(minor,p.kind==1 ? p.radius : p.minor_radius);
                sync(angle,p.angle*degrees); sync(start,p.start_angle*degrees); sync(sweep,p.sweep_angle*degrees);
                if(sides && (dragging || !sides->hasFocus())) { const QSignalBlocker block(sides); sides->setValue(p.sides); }
                if(mode) { const QSignalBlocker block(mode); mode->setCurrentIndex(p.IsFullConic() ? 0 : p.pie ? 2 : 1); }
            });
            timer->start(50); dialog->show();
            update(); event->accept(); return;
        }
    }

    // The second press of a Qt double-click reaches mousePressEvent first.
    // Cancel the selection/orbit gesture it may have started, otherwise the
    // following release would replace the point selection we establish here.
    selecting_with_rect_ = false;
    selecting_edit_points_ = false;
    dragging_polyline_point_ = false;
    orbiting_ = false;
    alt_orbiting_ = false;

    const DomPoint screen_point{event->pos().x(), event->pos().y()};
    auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
        return renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };

    if(tool_!=ToolMode::DrawSpline)if(auto planItem=PickKitchenPlanModule(*document_,screen_point,world_to_screen)) {
        EndDirectCurveEdit();document_->SelectObjectById(planItem,SelectionAction::Replace);
        emit SelectionChanged();emit ObjectDoubleClicked();update();event->accept();return;
    }

    // A node has priority over the curve segment beneath it: double-clicking
    // the marker deletes that node, while double-clicking an empty part of the
    // curve continues to insert a node as before.
    if (tool_ == ToolMode::Select || tool_ == ToolMode::EditPoint) {
        size_t point_index = 0;
        if (HitTestSelectedPolylineHandle(event->pos(), &point_index)) {
            const auto* spline = document_->GetSelectedBSpline();
            if (spline && spline->IsBezierChain() && point_index % 3 != 0) {
                emit StatusTextChanged(
                    "Curve edit: double-click a green node, not a blue Bezier handle");
            } else {
                document_->SelectCurvePoint(
                    document_->GetSelectedObjectIndex(), point_index);
                emit SelectionChanged();
                emit CurveNodeDeleteRequested();
            }
            update();
            event->accept();
            return;
        }
    }

    if (tool_ == ToolMode::Select && editing_sketch_) {
        PickSketchContour(event->pos());
        CSmartLine* sketch = EditableSketch();
        if (sketch) {
            double best_distance = 10.0;
            double best_parameter = 0.0;
            std::size_t best_line = sketch->GetNumLines();
            const auto consider_screen_segment = [&](std::size_t line_index,
                                                       DomPoint start_screen,
                                                       DomPoint end_screen,
                                                       double start_parameter,
                                                       double end_parameter) {
                const double dx = static_cast<double>(
                    end_screen.x - start_screen.x);
                const double dy = static_cast<double>(
                    end_screen.y - start_screen.y);
                const double length_squared = dx * dx + dy * dy;
                if (length_squared <= 1.0e-9) {
                    return;
                }
                const double segment_parameter = std::clamp(
                    ((screen_point.x - start_screen.x) * dx
                     + (screen_point.y - start_screen.y) * dy)
                        / length_squared,
                    0.0,
                    1.0);
                const double offset_x = screen_point.x
                    - (start_screen.x + segment_parameter * dx);
                const double offset_y = screen_point.y
                    - (start_screen.y + segment_parameter * dy);
                const double distance = std::hypot(offset_x, offset_y);
                if (distance < best_distance) {
                    best_distance = distance;
                    best_parameter = start_parameter
                        + (end_parameter - start_parameter)
                            * segment_parameter;
                    best_line = line_index;
                }
            };
            for (std::size_t line_index = 0;
                 line_index < sketch->GetNumLines(); ++line_index) {
                const CLinkLine* line = sketch->GetLine(line_index);
                if (!line) {
                    continue;
                }
                if (line->GetType() == LinkLineType::Bezier) {
                    constexpr int samples = 48;
                    DomPoint previous_screen{};
                    if (!world_to_screen(
                            point_to_vec3(sketch->LocalToWorld(
                                line->GetPoint(0.0))),
                            previous_screen)) {
                        continue;
                    }
                    for (int sample = 1; sample <= samples; ++sample) {
                        const double parameter =
                            static_cast<double>(sample) / samples;
                        DomPoint current_screen{};
                        if (!world_to_screen(
                                point_to_vec3(sketch->LocalToWorld(
                                    line->GetPoint(parameter))),
                                current_screen)) {
                            break;
                        }
                        consider_screen_segment(
                            line_index, previous_screen, current_screen,
                            static_cast<double>(sample - 1) / samples,
                            parameter);
                        previous_screen = current_screen;
                    }
                    continue;
                }
                if (line->GetType() != LinkLineType::Segment
                    && line->GetType() != LinkLineType::Horizontal
                    && line->GetType() != LinkLineType::Vertical) {
                    continue;
                }
                DomPoint start_screen{};
                DomPoint end_screen{};
                if (!world_to_screen(
                        point_to_vec3(sketch->LocalToWorld(line->GetStart())),
                        start_screen)
                    || !world_to_screen(
                        point_to_vec3(sketch->LocalToWorld(line->GetEnd())),
                        end_screen)) {
                    continue;
                }
                consider_screen_segment(
                    line_index, start_screen, end_screen, 0.0, 1.0);
            }
            if (best_line < sketch->GetNumLines()) {
                const CLinkLine* selected_line = sketch->GetLine(best_line);
                const auto* selected_bezier =
                    dynamic_cast<const CBezierSpline*>(selected_line);
                if (selected_bezier) {
                    const SketchCoordinateSystem& system =
                        sketch->GetCoordinateSystem();
                    CPoint3d clicked_world{};
                    if (ScreenToWorldPlane(
                            event->pos(),
                            point_to_vec3(system.origin),
                            point_to_vec3(system.normal),
                            clicked_world)) {
                        CPoint3d clicked_local =
                            sketch->WorldToLocal(clicked_world);
                        clicked_local.z = 0.0;
                        best_parameter = selected_bezier->GetNearestParameter(
                            clicked_local);
                    }
                }
                if (best_parameter <= 0.02 || best_parameter >= 0.98) {
                    emit StatusTextChanged(
                        "Sketch edit: double-click farther from an existing node");
                } else if ((selected_bezier
                                && sketch->SplitBezierLine(
                                    best_line, best_parameter))
                           || (!selected_bezier
                               && sketch->SplitLine(
                                   best_line,
                                   selected_line->GetPoint(
                                       best_parameter)))) {
                    NotifyDocumentChanged();
                    emit SelectionChanged();
                    emit StatusTextChanged(selected_bezier
                        ? "Sketch edit: node inserted; Bezier split without changing its shape"
                        : "Sketch edit: node inserted; LinkLine split into two segments");
                }
                highlighted_sketch_handle_kind_ = SketchHandleKind::None;
                update();
                event->accept();
                return;
            }
        }
    }

    if (tool_ == ToolMode::Select && editing_polyline_
        && document_->GetSelectedObject()
        && document_->GetSelectedObject()->m_id == direct_curve_edit_object_id_) {
        CAlfaObject* selected_curve = document_->GetSelectedObject();
        double best_distance = 10.0;
        double best_parameter = 0.0;
        size_t best_segment = 0;
        CPoint3d best_point{};
        bool found = false;
        const auto consider_segment = [&](DomPoint start_screen,
                                          DomPoint end_screen,
                                          CPoint3d start_world,
                                          CPoint3d end_world,
                                          double start_parameter,
                                          double end_parameter,
                                          size_t segment) {
            const double dx = static_cast<double>(end_screen.x - start_screen.x);
            const double dy = static_cast<double>(end_screen.y - start_screen.y);
            const double length_squared = dx * dx + dy * dy;
            if (length_squared <= 1.0e-9) return;
            const double local = std::clamp(
                ((screen_point.x - start_screen.x) * dx
                 + (screen_point.y - start_screen.y) * dy) / length_squared,
                0.0, 1.0);
            const double offset_x = screen_point.x
                - (start_screen.x + local * dx);
            const double offset_y = screen_point.y
                - (start_screen.y + local * dy);
            const double distance = std::hypot(offset_x, offset_y);
            if (distance >= best_distance) return;
            best_distance = distance;
            best_parameter = start_parameter
                + (end_parameter - start_parameter) * local;
            best_segment = segment;
            best_point = CPoint3d(
                start_world.x + (end_world.x - start_world.x) * local,
                start_world.y + (end_world.y - start_world.y) * local,
                start_world.z + (end_world.z - start_world.z) * local);
            found = true;
        };

        if (const auto* polyline = dynamic_cast<const CPolyline*>(selected_curve)) {
            const auto& points = polyline->GetPoints();
            const size_t segment_count = points.size() >= 2
                ? points.size() - 1 + (polyline->IsClosed() ? 1 : 0) : 0;
            for (size_t segment = 0; segment < segment_count; ++segment) {
                const size_t next = (segment + 1) % points.size();
                DomPoint start_screen{};
                DomPoint end_screen{};
                if (!world_to_screen(point_to_vec3(points[segment]), start_screen)
                    || !world_to_screen(point_to_vec3(points[next]), end_screen)) {
                    continue;
                }
                consider_segment(start_screen, end_screen,
                                 points[segment], points[next],
                                 static_cast<double>(segment) / segment_count,
                                 static_cast<double>(segment + 1) / segment_count,
                                 segment);
            }
        } else if (const auto* spline = dynamic_cast<const CBSpline*>(selected_curve)) {
            const int samples = std::max(
                96, static_cast<int>(spline->GetPointCount()) * 32);
            CPoint3d previous_world = spline->Evaluate(0.0f);
            DomPoint previous_screen{};
            bool previous_valid = world_to_screen(
                point_to_vec3(previous_world), previous_screen);
            for (int sample = 1; sample <= samples; ++sample) {
                const double parameter = static_cast<double>(sample) / samples;
                const CPoint3d current_world = spline->Evaluate(
                    static_cast<float>(parameter));
                DomPoint current_screen{};
                const bool current_valid = world_to_screen(
                    point_to_vec3(current_world), current_screen);
                if (previous_valid && current_valid) {
                    consider_segment(
                        previous_screen, current_screen,
                        previous_world, current_world,
                        static_cast<double>(sample - 1) / samples,
                        parameter, static_cast<size_t>(sample - 1));
                }
                previous_world = current_world;
                previous_screen = current_screen;
                previous_valid = current_valid;
            }
        }

        bool inserted = false;
        if (found) {
            if (auto* polyline = dynamic_cast<CPolyline*>(selected_curve)) {
                inserted = polyline->InsertPoint(best_segment + 1, best_point);
            } else if (auto* spline = dynamic_cast<CBSpline*>(selected_curve)) {
                inserted = spline->InsertShapePreservingPoint(best_parameter);
            }
        }
        if (inserted) {
            document_->ClearPointSelection();
            NotifyDocumentChanged();
            emit SelectionChanged();
            emit StatusTextChanged(
                "Curve edit: node inserted; drag any green node");
            update();
            event->accept();
            return;
        }
    }

    if (document_->SelectPolylineAtScreen(
            screen_point, world_to_screen, 8.0f,
            SelectionAction::Replace)) {
        if (dynamic_cast<CDrawingText*>(document_->GetSelectedObject())) {
            emit SelectionChanged();
            emit ObjectDoubleClicked();
            update();
            event->accept();
            return;
        }
        if (tool_ != ToolMode::Select) {
            SetTool(ToolMode::Select);
        }
        PickSketchContour(event->pos());
        if (dynamic_cast<CSketch*>(document_->GetSelectedObject())) {
            const auto camera = camera_;
            const bool orthographic = orthographic_projection_;
            BeginEditSelectedSketch();
            SetCamera(camera);
            SetOrthographicProjection(orthographic);
        }
        editing_sketch_ = EditableSketch() != nullptr;
        editing_polyline_ = !editing_sketch_;
        if (editing_polyline_) {
            document_->ClearPointSelection();
            if (const CAlfaObject* curve = document_->GetSelectedObject()) {
                direct_curve_edit_object_id_ = curve->m_id;
            }
        } else {
            direct_curve_edit_object_id_ = 0;
        }
        highlighted_polyline_handle_ = false;
        highlighted_sketch_handle_kind_ = SketchHandleKind::None;
        emit SelectionChanged();
        emit StatusTextChanged(editing_sketch_
            ? "Sketch edit: drag green points or red fillet handles, Esc to finish"
            : "Curve edit: drag handles, Esc to finish");
        update();
        event->accept();
        return;
    }

    auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        depth = dot(world - camera_position(camera_, orthographic_projection_), forward);
        return depth > 0.0f && renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };

    // A nested assembly remains the editable/movable unit. Only a simple solid
    // directly wrapped by a catalog Part needs drilling through that wrapper.
    CSolid* hit_solid = document_->FindSolidAtScreen(screen_point, project_world);
    bool selected_object = document_->SelectSolidMeshAtScreen(
        screen_point, project_world, SelectionAction::Replace);
    if (selected_object && hit_solid
        && (dynamic_cast<CPart*>(document_->GetSelectedObject())
            || FindKitchenModule(*document_, hit_solid->m_id) != 0)) {
        auto* module=FindKitchenModuleObject(*document_,hit_solid->m_id);
        selected_object = document_->SelectObjectById(
            module?module->m_id:hit_solid->m_id, SelectionAction::Replace);
    }
    if (!selected_object) {
        selected_object = document_->SelectMeshAtScreen(
            screen_point, project_world, SelectionAction::Replace);
    }
    const auto project_plan=[&](Vec3 world,DomPoint& screen){float depth=0;return project_world(world,screen,depth);};
    if(!selected_object)if(auto planItem=PickKitchenPlanModule(*document_,screen_point,project_plan))selected_object=document_->SelectObjectById(planItem,SelectionAction::Replace);
    if(!selected_object)selected_object=document_->SelectPolylineAtScreen(screen_point,project_plan,7.0f,SelectionAction::Replace);
    if (!selected_object) {
        CurvePoint scene_point{};
        selected_object = renderer_.ScreenToFloor(event->pos().x(), event->pos().y(), width(), height(), camera_, orthographic_projection_, scene_point)
            && document_->SelectObjectAt(scene_point, 0.35f, false);
    }

    if (selected_object) {
        emit SelectionChanged();
        emit ObjectDoubleClicked();
        update();
        event->accept();
        return;
    }

    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void OpenGLViewport::UpdateTangentHover(const QPoint& point, bool eligible) {
    const bool drawing=spatial_curve_preview_kind_==SpatialCurvePreviewKind::BSpline
        || spatial_curve_preview_kind_==SpatialCurvePreviewKind::Nurbs;
    unsigned long candidate_id=0; bool candidate_start=false;
    double nearest=capture_distance_pixels_;
    if (eligible && drawing && !spatial_curve_preview_points_.empty()
        && document_ && IsSnapTargetEnabled(SnapTarget::AuxLine)) {
        for (const auto& item:document_->GetObjects()) {
            const auto* spline=dynamic_cast<const CBSpline*>(item.get());
            if (!spline || spline->m_id==spatial_curve_preview_object_id_
                || !document_->IsObjectVisible(*spline)) continue;
            for (bool start:{false,true}) {
                Vec3 origin,direction; DomPoint pixel{};
                if (!endpoint_tangent(*spline,start,origin,direction)
                    || !renderer_.WorldToScreen(origin,camera_,orthographic_projection_,width(),height(),pixel)) continue;
                const double distance=std::hypot(double(pixel.x-point.x()),double(pixel.y-point.y()));
                if (distance>nearest) continue;
                nearest=distance; candidate_id=spline->m_id; candidate_start=start;
            }
        }
    }
    if (candidate_id==pending_tangent_id_ && (!candidate_id || candidate_start==pending_tangent_start_)) return;
    pending_tangent_id_=candidate_id; pending_tangent_start_=candidate_start;
    const auto generation=++tangent_hover_generation_;
    if (!candidate_id) return;
    QTimer::singleShot(500,Qt::PreciseTimer,this,[this,generation,candidate_id,candidate_start,point] {
        if (generation!=tangent_hover_generation_ || !document_
            || !IsSnapTargetEnabled(SnapTarget::AuxLine)
            || spatial_curve_preview_kind_==SpatialCurvePreviewKind::None) return;
        const auto* source=dynamic_cast<const CBSpline*>(document_->FindObjectById(candidate_id));
        Vec3 origin,direction; DomPoint pixel{};
        if (!source || !document_->IsObjectVisible(*source)
            || !endpoint_tangent(*source,candidate_start,origin,direction)
            || !renderer_.WorldToScreen(origin,camera_,orthographic_projection_,width(),height(),pixel)
            || std::hypot(double(pixel.x-point.x()),double(pixel.y-point.y()))>capture_distance_pixels_) return;
        active_tangent_id_=candidate_id; active_tangent_start_=candidate_start;
        update();
    });
}

void OpenGLViewport::mouseMoveEvent(QMouseEvent* event) {
    sketch_shape_circle_=event->modifiers().testFlag(Qt::ShiftModifier);
    if(MoveBottleModelDrag(event->pos())) {event->accept();return;}

    UpdateTangentHover(event->pos(),event->buttons()==Qt::NoButton && event->modifiers()==Qt::NoModifier);
    if (event->buttons() == Qt::NoButton && event->modifiers() == Qt::NoModifier)
        UpdateReferenceFaceHover(event->pos());
    else if (reference_hover_body_ != 0) { reference_hover_body_ = 0; update(); }
    const int plane = (tool_ == ToolMode::Select || ReferencePlanePickerActive())
        && event->buttons() == Qt::NoButton && event->modifiers() == Qt::NoModifier
        ? HitReferencePlane(event->pos()) : -1;
    if (hovered_reference_plane_ != plane) {
        hovered_reference_plane_ = plane;
        update();
    }
    if (plane >= 0) {
        setCursor(Qt::PointingHandCursor);
        last_mouse_ = event->pos();
        event->accept();
        return;
    }
    if (reference_hover_body_ != 0) {
        setCursor(Qt::PointingHandCursor);
        last_mouse_ = event->pos();
        event->accept();
        return;
    }
    if (ReferencePlanePickerActive() || reference_plane_dialog_ || sketch_waiting_for_face_ || solid_box_waiting_for_face_)
        setCursor(Qt::CrossCursor);
    CPoint3d cursor_world{};
    const bool cursor_world_valid = xy_plane_view_enabled_
        ? ScreenToWorldPlane(
            event->pos(), {0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f}, cursor_world)
        : ScreenToViewPlane(event->pos(), camera_.target, cursor_world);
    emit CursorWorldPositionChanged(
        cursor_world.x, cursor_world.y,
        xy_plane_view_enabled_ ? 0.0 : cursor_world.z,
        cursor_world_valid);
    const QPoint delta = event->pos() - last_mouse_;

    if (document_ && event->buttons() == Qt::NoButton && event->modifiers() == Qt::NoModifier
        && (picking_architecture_wall_ || (tool_ == ToolMode::SketchConvertArc && sketch_arc_has_line_))) {
        CPoint3d point{};bool snapped = false;
        if (picking_architecture_wall_) PickArchitecturePoint(event->pos(), point, &snapped);
        else if (auto* sketch = EditableSketch()) {
            const auto& system = sketch->GetCoordinateSystem();
            PickPointOnPlane(event->pos(), point_to_vec3(system.origin), point_to_vec3(system.normal), point, &snapped);
        }
        setCursor(Qt::CrossCursor);
        SetCreationSnapCursor(snapped);
        last_mouse_ = event->pos();
        return;
    }

    // Hover feedback belongs to the pending point command, not to Orbit or
    // selection handles. Curve drawing also uses point picking: let it reach
    // the rubber-band update below. Leave modifier-driven navigation available.
    if ((picking_3d_point_ || picking_xy_point_) && document_
        && tool_ != ToolMode::DrawCurve && tool_ != ToolMode::DrawBSpline
        && event->buttons() == Qt::NoButton
        && event->modifiers() == Qt::NoModifier) {
        CPoint3d hover_point{};
        bool snapped = false;
        const bool found = PickRequestedPoint(event->pos(), hover_point, &snapped);
        // Navigation may have temporarily replaced the capture cursor.
        creation_snap_active_ = false;
        setCursor(Qt::CrossCursor);
        SetCreationSnapCursor(found && snapped);
        last_mouse_ = event->pos();
        return;
    }

    if (right_navigation_active_
        && event->buttons().testFlag(Qt::RightButton)) {
        const QPoint total_delta = event->pos() - right_button_press_;
        if (!right_button_dragged_
            && total_delta.x() * total_delta.x()
                   + total_delta.y() * total_delta.y() > 6 * 6) {
            right_button_dragged_ = true;
        }
    }

    if (event->buttons() == Qt::NoButton) {
        pan_navigation_modifier_down_ =
            navigation_preset_ == "dom3d"
            && tool_ == ToolMode::Orbit
            && event->modifiers().testFlag(Qt::ControlModifier);
        alt_navigation_modifier_down_ =
            UsesAltNavigationModifier()
            && event->modifiers().testFlag(Qt::AltModifier)
            && !xy_plane_view_enabled_ && !sketch_active_;
        if (tool_ == ToolMode::Orbit
            || pan_navigation_modifier_down_
            || alt_navigation_modifier_down_) {
            RestoreDefaultToolCursor();
            last_mouse_ = event->pos();
            // Orbit still needs the idle hover pass below so interactive
            // furniture handles can replace the orbit cursor with a hand.
            // Modifier-driven navigation keeps its cursor unconditionally.
            if (pan_navigation_modifier_down_
                || alt_navigation_modifier_down_) {
                return;
            }
        }
    }

    if (tool_ == ToolMode::DrawSpline
        && drawing_spline_stroke_
        && event->buttons().testFlag(Qt::LeftButton)) {
        AppendDrawSplineStroke(event->pos());
        last_mouse_ = event->pos();
        event->accept();
        return;
    }

    if (tool_ == ToolMode::Select
        && selection_mode_ == SelectionMode::Edge
        && event->buttons() == Qt::NoButton
        && !editing_polyline_
        && !editing_sketch_) {
        UpdateHoveredSolidEdge(event->pos());
    } else {
        ClearHoveredSolidEdge();
    }

    if (dragging_solid_dimension_grip_
        && (tool_ == ToolMode::Select || tool_ == ToolMode::Orbit || tool_ == ToolMode::SolidFillet)
        && !active_solid_dimension_grip_.isEmpty()) {
        if(active_solid_dimension_grip_=="origin.center") {
            const auto& grip=cylinder_center_drag_;
            CPoint3d picked;
            const Vec3 normal=normalize(cross(grip.world_u,grip.world_v));
            if(ScreenToWorldPlane(event->pos(),point_to_vec3(grip.center),normal,picked)) {
                const Vec3 delta=point_to_vec3(picked)-point_to_vec3(cylinder_center_drag_start_);
                const double uu=dot(grip.world_u,grip.world_u), uv=dot(grip.world_u,grip.world_v), vv=dot(grip.world_v,grip.world_v);
                const double determinant=uu*vv-uv*uv;
                if(determinant>1.e-12) {
                    const double du=(dot(delta,grip.world_u)*vv-dot(delta,grip.world_v)*uv)/determinant;
                    const double dv=(dot(delta,grip.world_v)*uu-dot(delta,grip.world_u)*uv)/determinant;
                    cylinder_center_drag_value_=CPoint3d(grip.origin.x+du*grip.local_u.x+dv*grip.local_v.x,
                        grip.origin.y+du*grip.local_u.y+dv*grip.local_v.y,grip.origin.z+du*grip.local_u.z+dv*grip.local_v.z);
                    emit CylinderCenterChanged(grip.operation_index,cylinder_center_drag_value_,false);
                }
            }
            setCursor(Qt::ClosedHandCursor);last_mouse_=event->pos();event->accept();return;
        }
        const QPointF mouse_delta = event->pos() - solid_dimension_drag_start_mouse_;
        const double projected_pixels =
            mouse_delta.x() * solid_dimension_drag_screen_direction_.x()
            + mouse_delta.y() * solid_dimension_drag_screen_direction_.y();
        double value = solid_dimension_drag_start_value_
            * (1.0 + (solid_dimension_drag_move_base_ ? -1.0 : 1.0)
                * projected_pixels / solid_dimension_drag_screen_length_);
        value = std::clamp(
            value,
            solid_dimension_drag_minimum_,
            solid_dimension_drag_maximum_);

        // SolidBox permits a signed extrusion height, but an exact zero has
        // no valid solid. Keep a tiny continuous dead zone while allowing the
        // user to drag through the base plane and reverse the direction.
        if (active_solid_dimension_grip_ == QStringLiteral("depth")
            && std::abs(value) < 0.01) {
            value = value < 0.0 ? -0.01 : 0.01;
        }
        if (std::abs(value - solid_dimension_drag_current_value_) > 1.0e-6) {
            solid_dimension_drag_current_value_ = value;
            emit SolidDimensionGripChanged(
                active_solid_dimension_operation_index_,
                active_solid_dimension_grip_, value, false, solid_dimension_drag_move_base_);
        }
        last_mouse_ = event->pos();
        return;
    }

    if ((tool_ == ToolMode::Select || tool_ == ToolMode::Orbit || tool_ == ToolMode::SolidFillet)
        && !solid_dimension_object_.tool_id.empty()) {
        QString hovered_grip;
        int hovered_operation_index = -1;
        for (const SolidDimensionHit& dimension : solid_dimension_hits_) {
            if (dimension.is_grip && dimension.rect.contains(event->pos())) {
                hovered_grip = dimension.parameter_id;
                if (dimension.source_index < solid_dimension_objects_.size()) {
                    hovered_operation_index = static_cast<int>(
                        solid_dimension_objects_[dimension.source_index].operation_index);
                }
                break;
            }
        }
        if (hovered_grip != highlighted_solid_dimension_grip_
            || hovered_operation_index != highlighted_solid_dimension_operation_index_) {
            highlighted_solid_dimension_grip_ = hovered_grip;
            highlighted_solid_dimension_operation_index_ = hovered_operation_index;
            if (hovered_grip.isEmpty()) {
                RestoreDefaultToolCursor();
            } else {
                setCursor(Qt::OpenHandCursor);
            }
            update();
        }
        // Orbit resets its cursor on every idle move, even while the same
        // dimension remains highlighted.
        if (!hovered_grip.isEmpty()) setCursor(Qt::OpenHandCursor);
    }

    if (dragging_sketch_handle_ && tool_ == ToolMode::Select && editing_sketch_ && document_) {
        CSmartLine* sketch = EditableSketch();
        if (sketch) {
            const SketchCoordinateSystem& system = sketch->GetCoordinateSystem();
            CPoint3d point{};
            if (ScreenToWorldPlane(
                    event->pos(),
                    point_to_vec3(system.origin),
                    point_to_vec3(system.normal),
                    point)) {
                const bool snapped = SnapCreationPoint(event->pos(), point, true);
                SetCreationSnapCursor(snapped);
                bool changed = false;
                if(active_sketch_handle_kind_==SketchHandleKind::Primitive && sketch_before_) {
                    auto p=sketch_before_->GetPrimitive();
                    const auto local=sketch->WorldToLocal(point);
                    const double dx=local.x-p.u,dy=local.y-p.v;
                    const double x=dx*std::cos(p.angle)+dy*std::sin(p.angle);
                    const double y=-dx*std::sin(p.angle)+dy*std::cos(p.angle);
                    if(active_sketch_handle_index_>=4) {
                        const double t=std::atan2(y/(p.kind==1 ? p.radius : p.minor_radius),x/p.radius);
                        const auto positive=[](double a) { const double tau=6.28318530717958647692; a=std::fmod(a,tau); return a<0 ? a+tau : a; };
                        if(active_sketch_handle_index_==4) {
                            p.sweep_angle=positive(p.start_angle+p.sweep_angle-t); p.start_angle=t;
                        } else p.sweep_angle=positive(t-p.start_angle);
                    } else if(active_sketch_handle_index_==0) { p.u=local.x; p.v=local.y; }
                    else if(active_sketch_handle_index_==1) {
                        p.radius=std::hypot(dx,dy); p.angle=std::atan2(dy,dx);
                        if(p.kind!=2) p.minor_radius=p.radius;
                    } else if(active_sketch_handle_index_==2) { p.kind=2; p.minor_radius=std::abs(y); }
                    else {
                        p.kind=2; p.radius=std::abs(x); p.minor_radius=std::abs(y);
                        if(event->modifiers().testFlag(Qt::ShiftModifier)) {
                            p.kind=1; p.radius=std::max(p.radius,p.minor_radius); p.minor_radius=p.radius;
                        }
                    }
                    // A cursor crossing the center is not a failed transaction.
                    if(p.radius>1e-6 && p.minor_radius>1e-6 && p.sweep_angle>=1e-5) changed=sketch->SetPrimitive(p);
                } else if (active_sketch_handle_kind_ == SketchHandleKind::Node) {
                    changed = sketch->MoveNodeWorld(active_sketch_handle_index_, point);
                } else if (active_sketch_handle_kind_ == SketchHandleKind::Fillet) {
                    changed = sketch->SetFilletRadiusFromWorld(active_sketch_handle_index_, point);
                } else if (active_sketch_handle_kind_ == SketchHandleKind::BezierControl) {
                    changed = sketch->MoveBezierControlPointWorld(active_sketch_handle_index_, point);
                } else if (active_sketch_handle_kind_ == SketchHandleKind::ArcControl) {
                    changed = sketch->MoveArcGripWorld(active_sketch_handle_index_, point);
                }
                if (changed) {
                    sketch_drag_changed_ = true;
                    update();
                }
            }
        }
        last_mouse_ = event->pos();
        return;
    }

    if ((picking_rotation_axis_ || coordinate_axis_selection_) && document_) {
        Vec3 axis_start{};
        Vec3 axis_end{};
        const bool found = HitTestRotationAxisLine(
            event->pos(), axis_start, axis_end);
        if (found != rotation_axis_hover_valid_
            || (found
                && (dot(axis_start - rotation_axis_hover_start_,
                        axis_start - rotation_axis_hover_start_) > 0.000001f
                    || dot(axis_end - rotation_axis_hover_end_,
                           axis_end - rotation_axis_hover_end_) > 0.000001f))) {
            rotation_axis_hover_valid_ = found;
            if (found) {
                rotation_axis_hover_start_ = axis_start;
                rotation_axis_hover_end_ = axis_end;
                setCursor(Qt::PointingHandCursor);
            } else {
                setCursor(Qt::CrossCursor);
            }
            update();
        }
    } else if ((picking_3d_point_ || picking_xy_point_) && document_) {
        CPoint3d hover_point{};
        bool snapped = false;
        const bool point_found = PickRequestedPoint(event->pos(), hover_point, &snapped);
        SetCreationSnapCursor(point_found && snapped);
    } else if ((tool_ == ToolMode::DrawCurve || tool_ == ToolMode::DrawBSpline)
        && document_) {
        CPoint3d snap_point{};
        const bool valid = spatial_curve_preview_kind_ != SpatialCurvePreviewKind::None
            ? PickModelingPoint(event->pos(), snap_point)
            : ScreenToCurvePlane(event->pos(), snap_point);
        SetCreationSnapCursor(
            valid && SnapCreationPoint(event->pos(), snap_point, false));
    } else if ((tool_ == ToolMode::SketchRectangle
               || tool_ == ToolMode::SketchPolyline
               || tool_ == ToolMode::SketchBezier
               || (tool_ == ToolMode::SolidBoxRectangle
                   && !solid_box_waiting_for_face_ && !primitive_height_active_)
               || (tool_ == ToolMode::SolidCylinderCircle
                   && !solid_box_waiting_for_face_ && !primitive_height_active_))
               && document_) {
        CPoint3d snap_point{};
        const bool valid = ScreenToSketchPlane(event->pos(), snap_point);
        bool snapped = valid
            && SnapCreationPoint(event->pos(), snap_point, true);
        if (tool_ == ToolMode::SketchPolyline
            && IsNearSketchPolylineFirstPoint(event->pos())) {
            snapped = true;
        } else if (tool_ == ToolMode::SketchBezier
                   && sketch_bezier_points_.size() == 3
                   && IsNearSelectedSketchFirstPoint(event->pos())) {
            snapped = true;
        }
        SetCreationSnapCursor(snapped);
    }

    if ((tool_ == ToolMode::MeasurePointToPoint
         || (tool_ == ToolMode::MovePointToPoint
             && move_point_stage_ != MovePointStage::SelectObjects))
        && document_) {
        CPoint3d snapped_point{};
        const bool snapped = SnapCreationPoint(event->pos(), snapped_point, false);
        const bool found = tool_ == ToolMode::MovePointToPoint
            ? PickModelingPoint(event->pos(), snapped_point) : snapped;
        setCursor(Qt::CrossCursor);
        SetCreationSnapCursor(snapped);
        if (measurement_waiting_for_second_point_
            && (found != measurement_preview_valid_
                || (found
                    && (std::fabs(snapped_point.x - measurement_preview_.x) > 0.0001
                        || std::fabs(snapped_point.y - measurement_preview_.y) > 0.0001
                        || std::fabs(snapped_point.z - measurement_preview_.z) > 0.0001)))) {
            measurement_preview_valid_ = found;
            if (found) {
                measurement_preview_ = snapped_point;
            }
            update();
        }
    }

    if ((tool_ == ToolMode::DrawCurve || tool_ == ToolMode::DrawBSpline) && document_) {
        last_mouse_ = event->pos();
        CPoint3d preview_point{};
        // Use exactly the same projection as the following click.  The old
        // ScreenToCurvePlane path depended on a legacy "active" curve, so a
        // newly created parametric curve often had no live segment until
        // several points had already been entered.
        const bool preview_valid = PickRequestedPoint(event->pos(), preview_point);
        if (preview_valid) ConstrainPolylinePoint(preview_point, event->modifiers());
        if (preview_valid != curve_preview_valid_
            || (preview_valid
                && (std::fabs(preview_point.x - curve_preview_point_.x) > 0.0001
                    || std::fabs(preview_point.y - curve_preview_point_.y) > 0.0001
                    || std::fabs(preview_point.z - curve_preview_point_.z) > 0.0001))) {
            curve_preview_valid_ = preview_valid;
            curve_preview_point_ = preview_point;
            update();
        }
    }

    if (primitive_height_active_ && !orbiting_ && !panning_ && !zooming_) {
        UpdatePrimitiveHeight(event->pos());
        last_mouse_ = event->pos();
        update();
        return;
    }

    if ((tool_ == ToolMode::SketchRectangle
         || tool_ == ToolMode::SolidBoxRectangle
         || tool_ == ToolMode::SolidCylinderCircle)
        && sketch_rectangle_has_first_point_
        && document_ && !primitive_height_active_) {
        last_mouse_ = event->pos();
        CPoint3d preview_point{};
        const bool preview_valid = ScreenToSketchPlane(event->pos(), preview_point);
        if (preview_valid) {
            SnapCreationPoint(event->pos(), preview_point, true);
            ConstrainPrimitiveBase(preview_point, event->modifiers());
        }
        if (preview_valid != sketch_rectangle_preview_valid_
            || (preview_valid
                && (std::fabs(preview_point.x - sketch_rectangle_preview_point_.x) > 0.0001
                    || std::fabs(preview_point.y - sketch_rectangle_preview_point_.y) > 0.0001
                    || std::fabs(preview_point.z - sketch_rectangle_preview_point_.z) > 0.0001))) {
            sketch_rectangle_preview_valid_ = preview_valid;
            sketch_rectangle_preview_point_ = preview_point;
            update();
        }
    }

    if (tool_ == ToolMode::SketchPolyline && !sketch_polyline_points_.empty() && document_) {
        CPoint3d preview_point{};
        bool preview_valid = ScreenToSketchPlane(event->pos(), preview_point);
        if (preview_valid) {
            if (IsNearSketchPolylineFirstPoint(event->pos())) {
                preview_point = sketch_polyline_points_.front();
            } else if (!SnapCreationPoint(event->pos(), preview_point, true)) {
                preview_point = AlignSketchPolylinePoint(preview_point);
            }
        }
        if (preview_valid != sketch_polyline_preview_valid_
            || (preview_valid
                && (std::fabs(preview_point.x - sketch_polyline_preview_point_.x) > 0.0001
                    || std::fabs(preview_point.y - sketch_polyline_preview_point_.y) > 0.0001
                    || std::fabs(preview_point.z - sketch_polyline_preview_point_.z) > 0.0001))) {
            sketch_polyline_preview_valid_ = preview_valid;
            sketch_polyline_preview_point_ = preview_point;
            update();
        }
    }
    if (tool_ == ToolMode::SketchBezier && !sketch_bezier_points_.empty() && document_) {
        CPoint3d preview_point{};
        const bool preview_valid = ScreenToSketchPlane(event->pos(), preview_point);
        if (preview_valid
            && sketch_bezier_points_.size() == 3
            && IsNearSelectedSketchFirstPoint(event->pos())) {
            preview_point = EditableSketch()->GetNodeWorld(0);
        } else if (preview_valid) {
            SnapCreationPoint(event->pos(), preview_point, true);
        }
        if (preview_valid != sketch_bezier_preview_valid_
            || (preview_valid
                && (std::fabs(preview_point.x - sketch_bezier_preview_point_.x) > 0.0001
                    || std::fabs(preview_point.y - sketch_bezier_preview_point_.y) > 0.0001
                    || std::fabs(preview_point.z - sketch_bezier_preview_point_.z) > 0.0001))) {
            sketch_bezier_preview_valid_ = preview_valid;
            sketch_bezier_preview_point_ = preview_point;
            update();
        }
    }

    if (tool_ == ToolMode::SketchFillet && document_) {
        CPoint3d preview_center{};
        const bool preview_valid = ScreenToSketchPlane(event->pos(), preview_center);
        const DomPoint screen_point{event->pos().x(), event->pos().y()};
        auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
            return renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
        };
        size_t object_index = 0;
        size_t point_index = 0;
        const bool hovered = document_->FindPolylinePointAtScreen(screen_point, world_to_screen, 40.0f, object_index, point_index);
        const bool preview_changed = preview_valid != sketch_fillet_preview_valid_
            || (preview_valid
                && (std::fabs(preview_center.x - sketch_fillet_preview_center_.x) > 0.0001
                    || std::fabs(preview_center.y - sketch_fillet_preview_center_.y) > 0.0001
                    || std::fabs(preview_center.z - sketch_fillet_preview_center_.z) > 0.0001));
        if (preview_changed) {
            sketch_fillet_preview_valid_ = preview_valid;
            sketch_fillet_preview_center_ = preview_center;
        }
        if (hovered != highlighted_sketch_fillet_point_) {
            highlighted_sketch_fillet_point_ = hovered;
            if (hovered) {
                setCursor(Qt::PointingHandCursor);
            } else {
                setCursor(Qt::CrossCursor);
            }
        }
        if (preview_changed) {
            update();
        }
    }

    if (dragging_polyline_point_
        && ((tool_ == ToolMode::Select && editing_polyline_) || tool_ == ToolMode::EditPoint)
        && document_) {
        CPoint3d point{};
        bool has_point = false;
        if (curve_point_drag_has_plane_) {
            has_point = ScreenToWorldPlane(event->pos(), curve_point_drag_plane_point_, curve_point_drag_plane_normal_, point);
        }
        if (!has_point && !curve_point_drag_has_plane_) {
            has_point = xy_plane_view_enabled_
                ? ScreenToPlaneY(event->pos(), polyline_drag_plane_y_, point)
                : ScreenToViewPlane(event->pos(), curve_point_drag_anchor_, point);
        }
        if (has_point) {
            if (xy_plane_view_enabled_) point.z = 0.0;
            const bool snapped = SnapCreationPoint(event->pos(), point, false);
            const bool constrain_axis = event->modifiers().testFlag(Qt::ShiftModifier)
                && dynamic_cast<const CPolyline*>(document_->GetSelectedObject());
            if (constrain_axis) {
                // Keep the drag on a screen-horizontal/vertical axis through
                // the original node, without changing its view-plane depth.
                Vec3 forward{}, right{}, up{};
                viewport_camera_basis(camera_, forward, right, up);
                if (xy_plane_view_enabled_) {
                    right = {1.0f, 0.0f, 0.0f};
                    up = {0.0f, 1.0f, 0.0f};
                }
                const Vec3 delta = point_to_vec3(point) - curve_point_drag_anchor_;
                const float horizontal = dot(delta, right);
                const float vertical = dot(delta, up);
                const Vec3 constrained = curve_point_drag_anchor_
                    + (std::abs(horizontal) >= std::abs(vertical)
                        ? right * horizontal : up * vertical);
                point = CPoint3d(constrained.x, constrained.y, constrained.z);
            }
            SetCreationSnapCursor(snapped && !constrain_axis);
            const Vec3 move_delta{
                static_cast<float>(point.x - curve_point_drag_last_.x),
                static_cast<float>(point.y - curve_point_drag_last_.y),
                static_cast<float>(point.z - curve_point_drag_last_.z)
            };
            const std::vector<CPoint3d> selected_points = document_->GetSelectedCurvePointPositions();
            const bool moved = selected_points.size() > 1
                ? document_->MoveSelectedCurvePoints(
                    move_delta, xy_plane_view_enabled_)
                : document_->MoveSelectedPoint(point);
            if (moved) {
                curve_point_drag_last_ = point;
                curve_point_drag_changed_ = true;
                update();
            }
        }
        last_mouse_ = event->pos();
        return;
    }

    if (selecting_edit_points_ && tool_ == ToolMode::EditPoint) {
        edit_point_selection_current_ = event->pos();
        update();
        last_mouse_ = event->pos();
        return;
    }

    if (selecting_with_rect_
        && (tool_ == ToolMode::Select
            || (tool_ == ToolMode::Transform
                && selection_mode_ == SelectionMode::Point))) {
        rect_selection_current_ = event->pos();
        update();
        last_mouse_ = event->pos();
        return;
    }

    if (zoom_rect_active_ && tool_ == ToolMode::ZoomRect) {
        zoom_rect_current_ = event->pos();
        update();
        last_mouse_ = event->pos();
        return;
    }

    if (dragging_face_extrude_ && tool_ == ToolMode::FaceExtrude) {
        setCursor(Qt::ClosedHandCursor);
        HandleFaceExtrudeDrag(event->pos());
        return;
    }

    if (tool_ == ToolMode::FaceExtrude && event->buttons() == Qt::NoButton
        && !orbiting_ && !alt_orbiting_ && !panning_) {
        const bool has_gizmo = document_
            && document_->GetSelectedSolidFaceCenterAndNormal(face_extrude_center_, face_extrude_normal_);
        if (has_gizmo && HitTestFaceExtrudeGizmo(event->pos())) setCursor(Qt::OpenHandCursor);
        else RestoreDefaultToolCursor();
        last_mouse_ = event->pos();
        return;
    }

    if (dragging_draft_face_ && tool_ == ToolMode::DraftFace) {
        HandleDraftFaceDrag(event->pos());
        return;
    }

    if (dragging_transform_ && tool_ == ToolMode::Transform) {
        HandleTransformDrag(event->pos(), event->modifiers());
        return;
    }

    if (tool_ == ToolMode::Transform && document_ && document_->HasSelection()) {
        const TransformAxis hovered_axis = HitTestTransformGizmo(event->pos());
        if (hovered_axis != highlighted_transform_axis_) {
            highlighted_transform_axis_ = hovered_axis;
            update();
        }
    }

    if (((tool_ == ToolMode::Select && editing_polyline_)
         || tool_ == ToolMode::EditPoint)
        && document_ && event->buttons() == Qt::NoButton) {
        const bool hovered = HitTestSelectedPolylineHandle(event->pos());
        if (hovered) {
            setCursor(Qt::OpenHandCursor);
        } else if (material_interaction_mode_ == MaterialInteractionMode::None) {
            RestoreDefaultToolCursor();
        }
        if (hovered != highlighted_polyline_handle_) {
            highlighted_polyline_handle_ = hovered;
            update();
        }
    }

    if (tool_ == ToolMode::Select && editing_sketch_ && document_) {
        SketchHandleKind kind = SketchHandleKind::None;
        size_t index = 0;
        HitTestSelectedSketchHandle(event->pos(), kind, index);
        if (kind != highlighted_sketch_handle_kind_ || index != highlighted_sketch_handle_index_) {
            highlighted_sketch_handle_kind_ = kind;
            highlighted_sketch_handle_index_ = index;
            if (kind != SketchHandleKind::None) {
                setCursor(Qt::OpenHandCursor);
            } else if (material_interaction_mode_ == MaterialInteractionMode::None) {
                RestoreDefaultToolCursor();
            }
            update();
        }
    }

    if (tool_ == ToolMode::DraftFace && document_ && document_->HasDraftFaceAxis()) {
        const bool hovered = HitTestDraftFaceGizmo(event->pos());
        if (hovered != highlighted_draft_face_gizmo_) {
            highlighted_draft_face_gizmo_ = hovered;
            update();
        }
    }

    if ((tool_ == ToolMode::Select || tool_ == ToolMode::Orbit)
        && selection_mode_ == SelectionMode::Object
        && event->buttons() == Qt::NoButton
        && material_interaction_mode_ == MaterialInteractionMode::None
        && !editing_polyline_
        && !editing_sketch_
        && !dragging_solid_dimension_grip_
        && highlighted_solid_dimension_grip_.isEmpty()) {
        constexpr int kFurnitureHandleCaptureDistance = 20;
        const QPoint captured_delta =
            event->pos() - furniture_handle_capture_anchor_;
        const bool keep_captured_handle = hovering_furniture_handle_
            && hovered_furniture_handle_id_ != 0
            && captured_delta.x() * captured_delta.x()
                    + captured_delta.y() * captured_delta.y()
                <= kFurnitureHandleCaptureDistance
                    * kFurnitureHandleCaptureDistance;

        CAlfaObject* hovered_handle = keep_captured_handle
            ? document_->FindObjectById(hovered_furniture_handle_id_)
            : FindInteractiveFurnitureHandleAt(event->pos());
        const bool hovering_handle = hovered_handle != nullptr;
        if (hovering_handle && !keep_captured_handle) {
            hovered_furniture_handle_id_ = hovered_handle->m_id;
            furniture_handle_capture_anchor_ = event->pos();
        } else if (!hovering_handle) {
            hovered_furniture_handle_id_ = 0;
        }
        const bool was_hovering_handle = hovering_furniture_handle_;
        hovering_furniture_handle_ = hovering_handle;
        if (hovering_handle) {
            // Orbit restores its own cursor near the start of every idle
            // mouse move. Reassert the hand on every pass while the handle is
            // captured, not only on the first false -> true transition.
            setCursor(Qt::PointingHandCursor);
        } else if (was_hovering_handle) {
            RestoreDefaultToolCursor();
        }
    }

    if (!xy_plane_view_enabled_ && (orbiting_ || alt_orbiting_)) {
        const Quaternion previous_orientation = camera_.orientation;
        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            set_view_by_camera_ray(camera_);
        } else {
            const float yaw_delta = -static_cast<float>(delta.x()) * 0.35f;
            const float pitch_delta = static_cast<float>(delta.y()) * 0.25f;
            if (tool_ == ToolMode::Walk) {
                const Vec3 eye = camera_position(camera_);
                architectural_orbit_camera(camera_, yaw_delta, pitch_delta);
                const Vec3 forward = normalize(rotate(
                    camera_.orientation, {0.0f, 0.0f, -1.0f}));
                camera_.target = eye + forward * camera_.distance;
            } else if (orbit_mode_ == OrbitMode::Architectural) {
                architectural_orbit_camera(camera_, yaw_delta, pitch_delta);
            } else {
                cad_orbit_camera(camera_, yaw_delta, pitch_delta);
            }
        }
        if (tool_ != ToolMode::Walk) {
            // Rotate the entire camera frame about the chosen world point.
            // Changing target on mouse-down would visibly recenter the scene.
            const Quaternion inverse{previous_orientation.w, -previous_orientation.x,
                -previous_orientation.y, -previous_orientation.z};
            camera_.target = orbit_drag_pivot_ + rotate(
                camera_.orientation * inverse, camera_.target - orbit_drag_pivot_);
        }
        last_mouse_ = event->pos();
        update();
        return;
    }

    if (panning_) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        const int viewport_height = std::max(1, height());
        float world_per_pixel = camera_.distance * 0.0018f;
        if (orthographic_projection_) {
            const float half_height = std::max(
                kMinimumOrthographicHalfHeight, camera_.distance * 0.42f);
            world_per_pixel = (2.0f * half_height) / static_cast<float>(viewport_height);
        } else {
            const float depth = std::max(0.001f, dot(camera_.target - camera_position(camera_, orthographic_projection_), forward));
            world_per_pixel = (2.0f * depth * std::tan(
                deg_to_rad(camera_.vertical_fov_degrees) * 0.5f))
                / static_cast<float>(viewport_height);
        }
        camera_.target = camera_.target - right * (static_cast<float>(delta.x()) * world_per_pixel)
            + up * (static_cast<float>(delta.y()) * world_per_pixel);
        last_mouse_ = event->pos();
        update();
        return;
    }

    if (zooming_) {
        const QPoint total_delta = event->pos() - right_button_press_;
        if (!right_button_dragged_
            && total_delta.x() * total_delta.x() + total_delta.y() * total_delta.y() > 6 * 6) {
            right_button_dragged_ = true;
        }
        if (!right_button_dragged_) {
            return;
        }
        const float zoom_factor = std::pow(1.01f, -static_cast<float>(delta.y()));
        camera_.distance *= zoom_factor;
        camera_.distance = std::clamp(
            camera_.distance, kMinimumCameraDistance, 100000.0f);
        last_mouse_ = event->pos();
        update();
    }
}

void OpenGLViewport::mouseReleaseEvent(QMouseEvent* event) {
    if(event->button()==Qt::LeftButton && MoveBottleModelDrag(event->pos(),true)) {event->accept();return;}

    const unsigned int release_generation = ++edge_quick_menu_generation_;
    if (property("arrayPreviewNavigationOnly").toBool()) {
        orbiting_=false; panning_=false; zooming_=false; alt_orbiting_=false;
        RestoreDefaultToolCursor(); event->accept(); return;
    }

    if (event->button() == Qt::LeftButton && primitive_base_pressed_) {
        primitive_base_pressed_ = false;
        const QPoint drag = event->pos() - primitive_press_point_;
        if (sketch_rectangle_has_first_point_ && !solid_box_waiting_for_face_
            && (primitive_second_press_ || drag.manhattanLength() >= 5)
            && UpdatePrimitiveBase(event->pos(), event->modifiers())) {
            const Vec3 delta = point_to_vec3(sketch_rectangle_preview_point_)
                - point_to_vec3(sketch_rectangle_first_point_);
            const double u = std::abs(dot(delta, sketch_u_));
            const double v = std::abs(dot(delta, sketch_v_));
            if (tool_ == ToolMode::SolidCylinderCircle ? std::hypot(u, v) > 0.001
                                                     : u > 0.001 && v > 0.001) {
                primitive_height_active_ = true;
                primitive_height_start_ = event->pos();
                primitive_height_ = 0.0;
                SetCreationSnapCursor(false);
                emit StatusTextChanged("Move cursor along the normal to set height; click or Enter to finish");
            }
        }
        update();
        event->accept();
        return;
    }
    if (event->button() == Qt::RightButton && right_navigation_active_) {
        const bool show_popup = !right_button_dragged_;
        right_navigation_active_ = false;
        zooming_ = false;
        orbiting_ = false;
        alt_orbiting_ = false;
        panning_ = false;
        right_button_dragged_ = false;
        RestoreDefaultToolCursor();
        if(show_popup && event->modifiers()==Qt::NoModifier && IsCurveNodeEditing() && document_
            && document_->GetSelectedCurvePoints().size()<=1) {
            CPoint3d picked;
            if(document_->PickSelectedCurvePointAtScreen({event->pos().x(),event->pos().y()},
                [this](Vec3 world,DomPoint& screen){return renderer_.WorldToScreen(world,camera_,orthographic_projection_,width(),height(),screen);},12.f,picked)) {
                emit CurveNodeTypeCycleRequested();event->accept();return;
            }
        }
        if (show_popup) {
            emit ViewportPopupMenuRequested(mapToGlobal(event->pos()));
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton
        && dragging_solid_dimension_grip_) {
        dragging_solid_dimension_grip_ = false;
        if(active_solid_dimension_grip_=="origin.center")
            emit CylinderCenterChanged(active_solid_dimension_operation_index_,cylinder_center_drag_value_,true);
        else emit SolidDimensionGripChanged(
            active_solid_dimension_operation_index_,
            active_solid_dimension_grip_,
            solid_dimension_drag_current_value_,
            true, solid_dimension_drag_move_base_);
        active_solid_dimension_grip_.clear();
        active_solid_dimension_operation_index_ = -1;
        if (highlighted_solid_dimension_grip_.isEmpty()) {
            RestoreDefaultToolCursor();
        } else {
            setCursor(Qt::OpenHandCursor);
        }
        NotifyDocumentChanged();
        update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton
        && tool_ == ToolMode::DrawSpline
        && drawing_spline_stroke_) {
        AppendDrawSplineStroke(event->pos());
        FinishDrawSplineStroke();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton
        && zoom_rect_active_
        && tool_ == ToolMode::ZoomRect) {
        zoom_rect_current_ = event->pos();
        zoom_rect_active_ = false;
        if (ApplyZoomRect()) {
            SetTool(ToolMode::Select);
            emit StatusTextChanged(
                orthographic_projection_
                    ? "Zoom By Rect applied (Ortho)"
                    : "Zoom By Rect applied (Perspective)");
        } else {
            emit StatusTextChanged("Zoom By Rect: drag a larger rectangle");
            update();
        }
        event->accept();
        return;
    }

    bool select_click_completed = false;
    if (event->button() == Qt::LeftButton
        && selecting_with_rect_
        && (tool_ == ToolMode::Select
            || (tool_ == ToolMode::Transform
                && selection_mode_ == SelectionMode::Point))
        && document_) {
        rect_selection_current_ = event->pos();
        const QPoint drag = rect_selection_current_ - rect_selection_start_;
        const int distance_squared = drag.x() * drag.x() + drag.y() * drag.y();
        if (distance_squared < 5 * 5) {
            SelectAt(event->pos(), rect_selection_action_);
            select_click_completed = true;
        } else {
            const DomRect rect{
                rect_selection_start_.x(),
                rect_selection_start_.y(),
                rect_selection_current_.x(),
                rect_selection_current_.y()
            };
            auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
                return renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
            };
            if (selection_mode_ == SelectionMode::Object) {
                document_->SelectObjectsInScreenRect(rect, world_to_screen, rect_selection_action_);
            } else if (selection_mode_ == SelectionMode::Face) {
                document_->SelectSolidFacesInScreenRect(rect, world_to_screen, rect_selection_action_);
            } else if (selection_mode_ == SelectionMode::Edge) {
                document_->SelectSolidEdgesInScreenRect(rect, world_to_screen, rect_selection_action_);
            } else if (selection_mode_ == SelectionMode::Point) {
                document_->SelectCurvePointsInScreenRect(rect, world_to_screen, rect_selection_action_);
            }
            emit SelectionChanged();
        }
        selecting_with_rect_ = false;
        update();
    }

    if (event->button() == Qt::LeftButton) {
        const SelectionMode requested_mode = selection_mode_;
        const bool quick_menu_available = tool_ == ToolMode::Select
            && document_
            && ((requested_mode == SelectionMode::Edge
                 && document_->HasSelectedSolidEdge())
                || (requested_mode == SelectionMode::Face
                    && document_->HasSelectedSolidFace())
                || (requested_mode == SelectionMode::Object
                    && document_->HasSelection()));
        if (select_click_completed && quick_menu_available
            && release_generation == edge_quick_menu_generation_) {
            edge_quick_menu_anchor_ = event->pos();
            const unsigned int generation = edge_quick_menu_generation_;
            QTimer::singleShot(1000, this, [this, generation, requested_mode]() {
                const bool selection_is_still_valid = document_
                    && ((requested_mode == SelectionMode::Edge
                         && document_->HasSelectedSolidEdge())
                        || (requested_mode == SelectionMode::Face
                            && document_->HasSelectedSolidFace())
                        || (requested_mode == SelectionMode::Object
                            && document_->HasSelection()));
                if (generation != edge_quick_menu_generation_
                    || tool_ != ToolMode::Select
                    || selection_mode_ != requested_mode
                    || !selection_is_still_valid
                    || !isVisible()
                    || QApplication::activePopupWidget()
                    || QApplication::activeModalWidget()
                    || QApplication::activeWindow() != window()
                    || QApplication::mouseButtons() != Qt::NoButton) {
                    return;
                }
                const QPoint cursor_position = mapFromGlobal(QCursor::pos());
                const QPoint delta = cursor_position - edge_quick_menu_anchor_;
                if (delta.x() * delta.x() + delta.y() * delta.y() >= 7 * 7) {
                    return;
                }
                const QPoint menu_position =
                    mapToGlobal(edge_quick_menu_anchor_ + QPoint(10, 10));
                if (requested_mode == SelectionMode::Edge) {
                    emit EdgeQuickMenuRequested(menu_position);
                } else if (requested_mode == SelectionMode::Face) {
                    emit FaceQuickMenuRequested(menu_position);
                } else if (requested_mode == SelectionMode::Object) {
                    if (EditableSketch() || dynamic_cast<const CSketch*>(document_->GetSelectedObject())) {
                        emit SketchQuickMenuRequested(menu_position);
                    } else {
                        emit ObjectQuickMenuRequested(menu_position);
                    }
                }
            });
        }
    }
    if (selecting_edit_points_ && tool_ == ToolMode::EditPoint && document_) {
        edit_point_selection_current_ = event->pos();
        const DomRect rect{
            edit_point_selection_start_.x(),
            edit_point_selection_start_.y(),
            edit_point_selection_current_.x(),
            edit_point_selection_current_.y()
        };
        auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
            return renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
        };
        document_->SelectCurvePointsInScreenRect(rect, world_to_screen, edit_point_selection_action_);
        selecting_edit_points_ = false;
        emit SelectionChanged();
        update();
    }
    if (dragging_face_extrude_ && tool_ == ToolMode::FaceExtrude) {
        CommitFaceExtrudeDrag();
    }
    if (dragging_draft_face_ && tool_ == ToolMode::DraftFace) {
        CommitDraftFaceDrag();
    }
    if (dragging_transform_ && tool_ == ToolMode::Transform) {
        CommitTransformDrag();
    }
    const bool commit_curve_point_drag = event->button() == Qt::LeftButton
        && dragging_polyline_point_ && curve_point_drag_changed_;
    bool commit_sketch_drag = event->button() == Qt::LeftButton
        && dragging_sketch_handle_ && sketch_drag_changed_;
    if (dragging_sketch_handle_ && document_) {
        auto* sketch=EditableSketch();
        if (sketch && commit_sketch_drag && sketch->CommitEdit()) {
            const auto before=sketch_before_->GetRevisions(), after=sketch->GetRevisions();
            if(before.geometry!=after.geometry || before.topology!=after.topology || before.placement!=after.placement)
                sketch_after_=std::make_shared<CSmartLine>(sketch->MakeCopy());
            else { sketch_before_.reset(); sketch_after_.reset(); commit_sketch_drag=false; }
        } else {
            if (sketch) sketch->CancelEdit();
            sketch_before_.reset(); sketch_after_.reset(); commit_sketch_drag=false;
        }
    }
    dragging_polyline_point_ = false;
    curve_point_drag_changed_ = false;
    dragging_sketch_handle_ = false;
    sketch_drag_changed_ = false;
    creation_snap_active_ = false;
    active_sketch_handle_kind_ = SketchHandleKind::None;
    curve_point_drag_has_plane_ = false;
    orbiting_ = false;
    alt_orbiting_ = false;
    panning_ = false;
    zooming_ = false;
    dragging_transform_ = false;
    dragging_face_extrude_ = false;
    dragging_draft_face_ = false;
    selecting_edit_points_ = false;
    selecting_with_rect_ = false;
    zoom_rect_active_ = false;
    transform_drag_has_preview_ = false;
    transform_drag_move_delta_ = {};
    transform_drag_rotation_input_angle_ = 0.0f;
    transform_drag_rotation_angle_ = 0.0f;
    transform_drag_scale_factor_ = 1.0f;
    active_transform_axis_ = TransformAxis::None;
    if (commit_sketch_drag) {
        NotifyDocumentChanged();
        emit StatusTextChanged(
            "Sketch edit: profile updated; dependent objects rebuilt");
    }
    if (commit_curve_point_drag) {
        FinalizeCurvePointChange();
        NotifyDocumentChanged();
        emit StatusTextChanged("Curve edit: node moved");
    }
    highlighted_transform_axis_ = TransformAxis::None;
    RestoreDefaultToolCursor();
    update();
}

void OpenGLViewport::keyPressEvent(QKeyEvent* event) {
    if(event->key()==Qt::Key_Shift && tool_==ToolMode::SketchRectangle && sketch_shape_kind_==2) {
        sketch_shape_circle_=true; update(); event->accept(); return;
    }
    if(event->key()==Qt::Key_Escape && bottle_drag_graph_>=0) {
        const int graph=bottle_drag_graph_,knot=bottle_drag_knot_;bottle_drag_graph_=-1;
        bottle_model_settings_->bottle_graphs[graph].value[knot]=bottle_drag_start_;
        emit BottleModelDragFinished(true);
        RestoreDefaultToolCursor();update();event->accept();return;
    }

    if (event->key()==Qt::Key_Escape) {
        active_tangent_id_=pending_tangent_id_=0; ++tangent_hover_generation_;
        auxiliary_guide_visible_=false; update();
    }
    if (primitive_height_active_
        && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        FinishPrimitiveCreation();
        event->accept();
        return;
    }
    if (reference_plane_dialog_ && event->key() == Qt::Key_Escape) {
        emit ReferencePlaneSelectionCanceled();
        event->accept();
        return;
    }
    if(event->key()==Qt::Key_Escape && dragging_sketch_handle_ && document_) {
        if(auto* sketch=EditableSketch()) sketch->CancelEdit();
        sketch_before_.reset(); sketch_after_.reset();
        dragging_sketch_handle_=false; sketch_drag_changed_=false;
        active_sketch_handle_kind_=SketchHandleKind::None;
        RestoreDefaultToolCursor(); update(); event->accept(); return;
    }
    if (event->key() == Qt::Key_Shift && (dragging_polyline_point_ || tool_ == ToolMode::DrawCurve
        || (tool_ == ToolMode::SolidBoxRectangle && !primitive_height_active_))) {
        QMouseEvent move(QEvent::MouseMove, QPointF(last_mouse_), QPointF(mapToGlobal(last_mouse_)),
                         Qt::NoButton, Qt::NoButton, event->modifiers() | Qt::ShiftModifier);
        mouseMoveEvent(&move);
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Control) {
        pan_navigation_modifier_down_ = navigation_preset_ == "dom3d"
            && tool_ == ToolMode::Orbit;
        RestoreDefaultToolCursor();
    } else if (event->key() == Qt::Key_Alt
               && UsesAltNavigationModifier()
               && !xy_plane_view_enabled_ && !sketch_active_) {
        alt_navigation_modifier_down_ = true;
        RestoreDefaultToolCursor();
    }

    if (tool_ == ToolMode::Walk
        && (event->key() == Qt::Key_Left
            || event->key() == Qt::Key_Right
            || event->key() == Qt::Key_Up
            || event->key() == Qt::Key_Down)) {
        MoveWalkCamera(event->key(), event->modifiers());
        event->accept();
        return;
    }
    if (tool_ == ToolMode::Walk && event->key() == Qt::Key_Escape) {
        SetTool(ToolMode::Orbit);
        emit StatusTextChanged("Walk camera finished. Orbit camera is active");
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && picking_architecture_wall_) {
        CancelArchitectureWallPick();
        emit ArchitectureWallPickCanceled();
        emit StatusTextChanged("Window / Door placement canceled");
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_C && picking_3d_point_) {
        picking_3d_point_ = false;
        point_pick_plane_enabled_ = false;
        RestoreDefaultToolCursor();
        emit Point3DPickCloseRequested();
        event->accept();
        return;
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && picking_3d_point_) {
        picking_3d_point_ = false;
        point_pick_plane_enabled_ = false;
        RestoreDefaultToolCursor();
        emit Point3DPickFinished();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && picking_rotation_axis_) {
        picking_rotation_axis_ = false;
        rotation_axis_hover_valid_ = false;
        RestoreDefaultToolCursor();
        emit RotationAxisPickCanceled();
        emit StatusTextChanged(property("projectionDirectionPick").toBool()
            ? "Projection: enter a vector or pick another direction" : "Rotate canceled");
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && picking_3d_point_) {
        picking_3d_point_ = false;
        point_pick_plane_enabled_ = false;
        RestoreDefaultToolCursor();
        if (spatial_curve_preview_kind_
            != SpatialCurvePreviewKind::None) {
            // In the spatial-curve workflow Esc accepts the points already
            // entered and ends creation, matching the classic Dom3D/3DCoat
            // interaction. Other 3D point-picking commands still use Esc as
            // cancellation.
            emit Point3DPickFinished();
        } else {
            emit Point3DPickCanceled();
            emit StatusTextChanged("GetPoint3D canceled");
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && picking_xy_point_) {
        picking_xy_point_ = false;
        point_pick_plane_enabled_ = false;
        creation_snap_active_ = false;
        RestoreDefaultToolCursor();
        emit XYPointPickCanceled();
        emit StatusTextChanged("Point pick canceled");
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::SketchBoolean) {
        SetTool(ToolMode::Select); editing_sketch_=true;
        emit StatusTextChanged(DomTranslate("Sketch Boolean canceled"));
        event->accept(); return;
    }
    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::Boolean) {
        SetTool(ToolMode::Select);
        emit StatusTextChanged("Boolean operation canceled");
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::SketchFillet) {
        SetTool(ToolMode::Select);
        emit StatusTextChanged("Sketch Fillet canceled. Select tool is active");
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::ZoomRect) {
        zoom_rect_active_ = false;
        SetTool(ToolMode::Select);
        emit StatusTextChanged("Zoom By Rect canceled. Select tool is active");
        event->accept();
        return;
    }

    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && tool_ == ToolMode::MovePointToPoint
        && move_point_stage_ == MovePointStage::SelectObjects) {
        if (document_ && document_->HasSelection()) {
            move_point_stage_ = MovePointStage::PickSource;
            emit StatusTextChanged("Move Point to Point: pick source point");
        } else {
            emit StatusTextChanged("Move Point to Point: select at least one object");
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::MovePointToPoint) {
        measurement_waiting_for_second_point_ = false;
        measurement_preview_valid_ = false;
        measurement_visible_ = false;
        SetTool(ToolMode::Select);
        emit StatusTextChanged("Move Point to Point canceled. Select tool is active");
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape
        && tool_ == ToolMode::MeasurePointToPoint) {
        measurement_waiting_for_second_point_ = false;
        measurement_preview_valid_ = false;
        SetTool(ToolMode::Select);
        emit StatusTextChanged(
            "Point-to-Point Dimension canceled. Select tool is active");
        event->accept();
        return;
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && tool_ == ToolMode::SketchPolyline) {
        if (!CommitSketchPolyline(false)) {
            emit StatusTextChanged("Sketch Polyline: need at least 2 points");
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_C && tool_ == ToolMode::SketchPolyline) {
        if (!CommitSketchPolyline(true)) {
            emit StatusTextChanged("Sketch Polyline: need at least 3 points to close");
        }
        event->accept();
        return;
    }

    if (selection_confirmation_mode_
        && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        emit SelectionConfirmed();
        event->accept();
        return;
    }

    if (selection_confirmation_mode_ && event->key() == Qt::Key_Escape) {
        selection_confirmation_mode_ = false;
        emit SelectionCommandCanceled();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::SketchPolyline) {
        if (!sketch_polyline_points_.empty()) {
            sketch_polyline_points_.clear();
            sketch_polyline_preview_valid_ = false;
            emit StatusTextChanged(QString("%1: Polyline canceled").arg(sketch_name_));
            update();
        } else {
            EndSketch();
            emit StatusTextChanged("Sketch closed");
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::SketchBezier) {
        if (!sketch_bezier_points_.empty()) {
            sketch_bezier_points_.clear();
            sketch_bezier_preview_valid_ = false;
            emit StatusTextChanged(QString("%1: Bezier canceled").arg(sketch_name_));
            update();
        } else {
            EndSketch();
            emit StatusTextChanged("Sketch closed");
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::SketchConvertBezier) {
        BeginEditSelectedSketch();
        emit StatusTextChanged("Convert to Bezier canceled");
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::SketchConvertArc) {
        if (sketch_arc_has_line_) {
            sketch_arc_has_line_ = false;
            emit StatusTextChanged(
                QString("%1: select a straight segment").arg(sketch_name_));
            update();
        } else {
            BeginEditSelectedSketch();
            emit StatusTextChanged("Line to Arc canceled");
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::SketchRectangle) {
        if (sketch_waiting_for_face_) {
            sketch_waiting_for_face_ = false;
            SetTool(ToolMode::Select);
            emit SketchFaceSelectionFinished(false);
            emit StatusTextChanged("Sketch face selection canceled");
            event->accept();
            return;
        }
        if (sketch_rectangle_has_first_point_) {
            sketch_rectangle_has_first_point_ = false;
            sketch_rectangle_preview_valid_ = false;
            emit StatusTextChanged("Sketch Rectangle: first point canceled");
            update();
        } else {
            EndSketch();
            emit StatusTextChanged("Sketch closed");
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape
        && (tool_ == ToolMode::SolidBoxRectangle
            || tool_ == ToolMode::SolidCylinderCircle)) {
        if (sketch_rectangle_has_first_point_) {
            primitive_height_active_ = false;
            primitive_preview_solid_.reset();
            primitive_base_pressed_ = false;
            sketch_rectangle_has_first_point_ = false;
            sketch_rectangle_preview_valid_ = false;
            emit StatusTextChanged(
                tool_ == ToolMode::SolidBoxRectangle
                    ? "BOX: first point canceled"
                    : "CYLINDER: center canceled");
            update();
        } else {
            SetTool(ToolMode::Select);
            emit StatusTextChanged(
                tool_ == ToolMode::SolidBoxRectangle
                    ? "BOX canceled"
                    : "CYLINDER canceled");
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && material_interaction_mode_ != MaterialInteractionMode::None) {
        CancelMaterialInteraction();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_C && document_ && tool_ == ToolMode::DrawCurve) {
        if (document_->CloseSelectedOrActivePolyline()) {
            NotifyDocumentChanged();
            emit StatusTextChanged("Polyline closed");
            update();
        } else {
            emit StatusTextChanged("Polyline: need at least 3 points");
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_C && document_ && tool_ == ToolMode::DrawBSpline) {
        if (document_->CloseSelectedOrActiveBSpline()) {
            curve_preview_valid_ = false;
            SetTool(ToolMode::Select);
            NotifyDocumentChanged();
            emit StatusTextChanged("B-Spline closed");
            update();
        } else {
            emit StatusTextChanged("B-Spline: need at least 3 points");
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape
        && (tool_ == ToolMode::DrawCurve
            || tool_ == ToolMode::DrawBSpline
            || tool_ == ToolMode::DrawSpline)) {
        CancelDrawSplineStroke();
        SetTool(ToolMode::Select);
        emit StatusTextChanged("Curve tool canceled");
        update();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::EditPoint) {
        document_->ClearPointSelection();
        SetTool(ToolMode::Select);
        emit SelectionChanged();
        emit StatusTextChanged("Edit Point finished");
        update();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::Transform) {
        if (dragging_transform_ && transform_drag_has_preview_) {
            CommitTransformDrag();
        }
        dragging_transform_ = false;
        transform_drag_has_preview_ = false;
        transform_drag_move_delta_ = {};
        transform_drag_axis_ = {};
        transform_drag_rotation_input_angle_ = 0.0f;
        transform_drag_rotation_angle_ = 0.0f;
        transform_drag_scale_factor_ = 1.0f;
        SetTool(ToolMode::Select);
        emit StatusTextChanged("Transform finished. Select tool is active");
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::Select && editing_polyline_) {
        editing_polyline_ = false;
        dragging_polyline_point_ = false;
        curve_point_drag_changed_ = false;
        direct_curve_edit_object_id_ = 0;
        highlighted_polyline_handle_ = false;
        if (material_interaction_mode_ == MaterialInteractionMode::None) {
            RestoreDefaultToolCursor();
        }
        emit StatusTextChanged("Polyline edit finished");
        update();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && tool_ == ToolMode::Select && editing_sketch_) {
        editing_sketch_ = false;
        dragging_sketch_handle_ = false;
        active_sketch_handle_kind_ = SketchHandleKind::None;
        highlighted_sketch_handle_kind_ = SketchHandleKind::None;
        if (material_interaction_mode_ == MaterialInteractionMode::None) {
            RestoreDefaultToolCursor();
        }
        emit StatusTextChanged("Sketch edit finished");
        update();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape
        && (tool_ == ToolMode::FaceExtrude
            || tool_ == ToolMode::DraftFace)) {
        const bool cancel_extrude = tool_ == ToolMode::FaceExtrude;
        if (document_) {
            if (cancel_extrude) {
                document_->CancelLiveExtrudeSelectedSolidFace();
            } else {
                document_->CancelLiveDraftFace();
                emit DraftFaceEditFinished(false);
            }
        }
        dragging_face_extrude_ = false;
        dragging_draft_face_ = false;
        face_extrude_distance_ = 0.0f;
        draft_face_angle_degrees_ = 0.0;
        SetTool(ToolMode::Select);
        emit SelectionChanged();
        if (cancel_extrude) NotifyDocumentChanged();
        emit StatusTextChanged(
            cancel_extrude ? "Extrude Face canceled" : "Draft Face canceled");
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape && document_) {
        if (dragging_face_extrude_) {
            document_->CancelLiveExtrudeSelectedSolidFace();
            dragging_face_extrude_ = false;
            face_extrude_distance_ = 0.0f;
        } else if (dragging_draft_face_) {
            document_->CancelLiveDraftFace();
            emit DraftFaceEditFinished(false);
            dragging_draft_face_ = false;
            draft_face_angle_degrees_ = 0.0;
        } else if (tool_ == ToolMode::ThickSolid && document_->HasLiveThickSolid()) {
            document_->CancelLiveThickSolid();
            emit StatusTextChanged("ThickSolid canceled");
        } else {
            document_->ClearSelection();
        }
        dragging_transform_ = false;
        active_transform_axis_ = TransformAxis::None;
        highlighted_transform_axis_ = TransformAxis::None;
        emit SelectionChanged();
        emit StatusTextChanged("Selection cleared");
        update();
        event->accept();
        return;
    }

    QOpenGLWidget::keyPressEvent(event);
}

void OpenGLViewport::keyReleaseEvent(QKeyEvent* event) {
    if(event->key()==Qt::Key_Shift && tool_==ToolMode::SketchRectangle && sketch_shape_kind_==2) {
        sketch_shape_circle_=false; update(); event->accept(); return;
    }
    if (event->key() == Qt::Key_Shift && (dragging_polyline_point_ || tool_ == ToolMode::DrawCurve
        || (tool_ == ToolMode::SolidBoxRectangle && !primitive_height_active_))) {
        QMouseEvent move(QEvent::MouseMove, QPointF(last_mouse_), QPointF(mapToGlobal(last_mouse_)),
                         Qt::NoButton, Qt::NoButton, event->modifiers() & ~Qt::ShiftModifier);
        mouseMoveEvent(&move);
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Control) {
        pan_navigation_modifier_down_ = false;
        RestoreDefaultToolCursor();
    } else if (event->key() == Qt::Key_Alt) {
        alt_navigation_modifier_down_ = false;
        RestoreDefaultToolCursor();
    }
    QOpenGLWidget::keyReleaseEvent(event);
}

Vec3 OpenGLViewport::NavigationSceneCenter() const {
    Vec3 minimum{}, maximum{};
    bool found = false;
    if (document_) for (const auto& object : document_->GetObjects()) {
        Vec3 lo{}, hi{};
        if (!object || !document_->IsObjectVisible(*object)
            || !object->GetBounds(lo, hi)) continue;
        if (!found) {
            minimum = lo;
            maximum = hi;
            found = true;
        } else {
            minimum = {std::min(minimum.x, lo.x), std::min(minimum.y, lo.y), std::min(minimum.z, lo.z)};
            maximum = {std::max(maximum.x, hi.x), std::max(maximum.y, hi.y), std::max(maximum.z, hi.z)};
        }
    }
    return found ? (minimum + maximum) * 0.5f : Vec3{};
}

bool OpenGLViewport::PickNavigationPoint(const QPoint& point, Vec3& result) const {
    if (!document_) return false;
    CPoint3d on_plane;
    if (!ScreenToViewPlane(point, camera_.target, on_plane)) return false;
    const Vec3 forward = rotate(camera_.orientation, {0, 0, -1});
    const Vec3 eye = camera_position(camera_, orthographic_projection_);
    const Vec3 plane = point_to_vec3(on_plane);
    const Vec3 origin = orthographic_projection_
        ? plane - forward * dot(plane - eye, forward) : eye;
    const Vec3 direction = orthographic_projection_ ? forward : normalize(plane - eye);
    float nearest = std::numeric_limits<float>::max();
    bool found = false;
    CPoint3d surface;
    if (PickVisibleSurface(point, surface)) {
        result = point_to_vec3(surface);
        nearest = dot(result - origin, direction);
        found = true;
    }
    for (const auto& object : document_->GetObjects()) {
        if (!object || !document_->IsObjectVisible(*object)) continue;
        if (const auto* mesh = dynamic_cast<const CMesh3D*>(object.get())) {
            const auto& vertices = mesh->GetVertices();
            for (const auto& face : mesh->GetFaces()) {
                if (face.deleted || face.corners.size() < 3) continue;
                for (size_t i = 1; i + 1 < face.corners.size(); ++i) {
                    const auto a = face.corners[0].v;
                    const auto b = face.corners[i].v;
                    const auto c = face.corners[i + 1].v;
                    if (a >= vertices.size() || b >= vertices.size() || c >= vertices.size()) continue;
                    const Vec3 edge1 = vertices[b] - vertices[a];
                    const Vec3 edge2 = vertices[c] - vertices[a];
                    const Vec3 p = cross(direction, edge2);
                    const float determinant = dot(edge1, p);
                    if (std::abs(determinant) < 1.e-12f) continue;
                    const Vec3 offset = origin - vertices[a];
                    const float u = dot(offset, p) / determinant;
                    if (u < 0 || u > 1) continue;
                    const Vec3 q = cross(offset, edge1);
                    const float v = dot(direction, q) / determinant;
                    if (v < 0 || u + v > 1) continue;
                    const float distance = dot(edge2, q) / determinant;
                    if (distance < 0 || distance >= nearest) continue;
                    nearest = distance;
                    result = origin + direction * distance;
                    found = true;
                }
            }
        } else if (const auto* curve = dynamic_cast<const CSmartLine*>(object.get())) {
            const auto points = curve->GetProfilePointsWorld();
            for (size_t i = 1; i < points.size(); ++i) {
                const Vec3 a = point_to_vec3(points[i - 1]), b = point_to_vec3(points[i]);
                DomPoint sa{}, sb{};
                if (!renderer_.WorldToScreen(a, camera_, orthographic_projection_, width(), height(), sa)
                    || !renderer_.WorldToScreen(b, camera_, orthographic_projection_, width(), height(), sb)) continue;
                const float dx = static_cast<float>(sb.x - sa.x), dy = static_cast<float>(sb.y - sa.y);
                const float length2 = dx * dx + dy * dy;
                float t = length2 > 0 ? std::clamp(((point.x() - sa.x) * dx + (point.y() - sa.y) * dy) / length2, 0.0f, 1.0f) : 0;
                const float px = sa.x + t * dx - point.x(), py = sa.y + t * dy - point.y();
                if (px * px + py * py > 25.0f) continue;
                const float za = dot(a - eye, forward), zb = dot(b - eye, forward);
                if (za <= 0 || zb <= 0) continue;
                if (!orthographic_projection_) t = t * za / ((1 - t) * zb + t * za);
                const Vec3 hit = a + (b - a) * t;
                const float distance = dot(hit - origin, direction);
                if (distance < 0 || distance >= nearest) continue;
                nearest = distance;
                result = hit;
                found = true;
            }
        }
    }
    return found;
}

void OpenGLViewport::wheelEvent(QWheelEvent* event) {
    StopCameraAnimation(false);
    Vec3 anchor = camera_.target;
    if (!PickNavigationPoint(event->position().toPoint(), anchor)) {
        CPoint3d point;
        if (ScreenToViewPlane(event->position().toPoint(), camera_.target, point)) {
            anchor = point_to_vec3(point);
        }
    }
    const float wheel_steps = static_cast<float>(event->angleDelta().y()) / 120.0f;
    const float zoom_factor = std::pow(1.12f, -wheel_steps);
    const float previous_distance = camera_.distance;
    camera_.distance *= zoom_factor;
    camera_.distance = std::clamp(
        camera_.distance, kMinimumCameraDistance, 100000.0f);
    if (previous_distance > 0.0f) {
        // A homothety about the hit keeps its perspective projection fixed,
        // even when the surface is far from the camera's target plane.
        Vec3 offset = camera_.target - anchor;
        float ratio = camera_.distance / previous_distance;
        if (orthographic_projection_) {
            ratio = std::max(kMinimumOrthographicHalfHeight, camera_.distance * 0.42f)
                / std::max(kMinimumOrthographicHalfHeight, previous_distance * 0.42f);
            const Vec3 forward = rotate(camera_.orientation, {0, 0, -1});
            offset = offset - forward * dot(offset, forward);
        }
        camera_.target = camera_.target + offset * (ratio - 1.0f);
    }
    event->accept();
    update();
}

void OpenGLViewport::dragEnterEvent(QDragEnterEvent* event) {
    Material material;
    if (!MaterialDrag::Decode(event->mimeData(), material)) {
        if (event->mimeData()->hasUrls()) {
            for (const QUrl& url : event->mimeData()->urls()) {
                if (url.isLocalFile()) {
                    event->acceptProposedAction();
                    return;
                }
            }
        }
        event->ignore();
        return;
    }

    material_drag_preview_ = material;
    material_drag_pos_ = event->position().toPoint();
    material_drag_active_ = true;
    event->acceptProposedAction();
    update();
}

void OpenGLViewport::dragMoveEvent(QDragMoveEvent* event) {
    Material material;
    if (!MaterialDrag::Decode(event->mimeData(), material)) {
        if (event->mimeData()->hasUrls()) {
            for (const QUrl& url : event->mimeData()->urls()) {
                if (url.isLocalFile()) {
                    event->acceptProposedAction();
                    return;
                }
            }
        }
        event->ignore();
        return;
    }

    material_drag_preview_ = material;
    material_drag_pos_ = event->position().toPoint();
    event->acceptProposedAction();
    update();
}

void OpenGLViewport::dragLeaveEvent(QDragLeaveEvent* event) {
    material_drag_active_ = false;
    event->accept();
    update();
}

void OpenGLViewport::dropEvent(QDropEvent* event) {
    Material material;
    if (!MaterialDrag::Decode(event->mimeData(), material)) {
        QStringList paths;
        if (event->mimeData()->hasUrls()) {
            for (const QUrl& url : event->mimeData()->urls()) {
                if (url.isLocalFile()) {
                    paths.push_back(url.toLocalFile());
                }
            }
        }
        if (!paths.isEmpty()) {
            material_drag_active_ = false;
            event->acceptProposedAction();
            emit FilesDropped(paths);
            update();
            return;
        }
        event->ignore();
        return;
    }

    material_drag_active_ = false;
    if (ApplyMaterialDrop(event->position().toPoint(), material)) {
        if (document_) {
            document_->ClearSelection();
        }
        event->acceptProposedAction();
        emit SelectionChanged();
        NotifyDocumentChanged();
        emit StatusTextChanged(QString("Material applied: %1").arg(QString::fromStdString(material.name)));
    } else {
        event->ignore();
        emit StatusTextChanged("Material: drop on an object");
    }
    update();
}

void OpenGLViewport::SelectAt(const QPoint& point, SelectionAction action) {
    const int origin_plane = HitReferencePlane(point);
    if (origin_plane >= 0) {
        SelectOriginPlane(origin_plane);
        return;
    }
    selected_origin_plane_ = -1;
    const DomPoint screen_point{point.x(), point.y()};
    auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
        return renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };
    auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        depth = dot(world - camera_position(camera_, orthographic_projection_), forward);
        return depth > 0.0f && renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };

    if (selection_mode_ == SelectionMode::Face) {
        if (document_->SelectSolidFaceAtScreen(screen_point, project_world, false, action)) {
            surface_pick_body_=0;surface_pick_face_=-1;
            for(const auto& object:document_->GetObjects())if(const auto* picked=dynamic_cast<const CSolid*>(object.get())) {
                const int face=picked->GetSelectedFaceIndex();
                if(picked->HasSelectedFace()&&face>=0&&PickVisibleSurface(point,surface_pick_point_,picked->m_id,face)) {
                    surface_pick_body_=picked->m_id;surface_pick_face_=face;break;
                }
            }
            Vec3 origin{}, x_axis{}, y_axis{}, normal{};
            unsigned long body_id = 0;
            int face_index = -1;
            CPoint3d clicked_point;
            if (document_->GetSelectedSolidFaceSketchPlane(
                    origin, x_axis, y_axis, normal, body_id, face_index)
                && ScreenToWorldPlane(point, origin, normal, clicked_point)) {
                emit PlanarFaceClicked(clicked_point);
            }
            emit SelectionChanged();
            update();
            return;
        }
        emit StatusTextChanged("Select Face: face not found");
        return;
    }

    if (selection_mode_ == SelectionMode::Edge) {
        if (document_->SelectSolidEdgeAtScreen(
                screen_point, world_to_screen,
                static_cast<float>(capture_distance_pixels_), action)) {
            fillet_pick_screen_ = point;
            fillet_pick_valid_ = true;
            fillet_anchor_valid_ = false;
            emit SelectionChanged();
            update();
            return;
        }
        if(property("surfaceCapPick").toBool()
            && document_->SelectPolylineAtScreen(screen_point,world_to_screen,8.0f,action)) {
            emit SelectionChanged();update();return;
        }
        emit StatusTextChanged(property("surfaceCapPick").toBool()
            ? "Cap: click a closed spline or a closed surface edge"
            : "Select Edge: edge not found");
        return;
    }

    if (selection_mode_ == SelectionMode::Object
        && document_->SelectPolylineAtScreen(screen_point, world_to_screen, 8.0f, action)) {
        emit SelectionChanged();
        update();
        return;
    }

    if (selection_mode_ == SelectionMode::Object && document_->SelectSolidMeshAtScreen(screen_point, project_world, action)) {
        emit SelectionChanged();
        update();
        return;
    }

    if (selection_mode_ == SelectionMode::Object && document_->SelectMeshAtScreen(screen_point, project_world, action)) {
        emit SelectionChanged();
        update();
        return;
    }

    if (selection_mode_ == SelectionMode::Point
        && document_->SelectCurvePointAtScreen(screen_point, world_to_screen, 10.0f, action)) {
        emit SelectionChanged();
        update();
        return;
    }
    if (selection_mode_ == SelectionMode::Point) {
        emit StatusTextChanged("Select Point:operation failed; check the selected geometry and parameters");
        update();
        return;
    }

    CurvePoint scene_point{};
    if (!renderer_.ScreenToFloor(point.x(), point.y(), width(), height(), camera_, orthographic_projection_, scene_point)) {
        return;
    }

    if (action == SelectionAction::Add) {
        document_->AddObjectToSelectionAt(scene_point, 0.35f, false);
    } else if (action == SelectionAction::Remove) {
        document_->RemoveObjectFromSelectionAt(scene_point, 0.35f, false);
    } else {
        document_->SelectObjectAt(scene_point, 0.35f, false);
    }

    emit SelectionChanged();
    update();
}

bool OpenGLViewport::ApplyMaterialDrop(const QPoint& point, const Material& material) {
    if (!document_) {
        return false;
    }

    CAlfaObject* object = FindObjectForMaterialAt(point);
    if (!object) {
        return false;
    }

    material_drop_object_id_ = object->m_id;
    material_drop_before_ = object->GetMaterial();
    material_drop_before_id_ = object->GetMaterialId();
    const Material& document_material = document_->UpsertMaterial(material);
    object->SetMaterial(document_material);
    object->SetMaterialId(document_material.id);
    material_drop_after_ = document_material;
    material_drop_after_id_ = document_material.id;
    material_drop_change_pending_ = material_drop_object_id_ != 0;
    return true;
}

CAlfaObject* OpenGLViewport::FindInteractiveFurnitureHandleAt(
    const QPoint& point) {
    if (!document_) {
        return nullptr;
    }

    constexpr int kFurnitureHandleCaptureDistance = 20;
    const QPoint captured_delta = point - furniture_handle_capture_anchor_;
    if (hovering_furniture_handle_ && hovered_furniture_handle_id_ != 0
        && captured_delta.x() * captured_delta.x()
                + captured_delta.y() * captured_delta.y()
            <= kFurnitureHandleCaptureDistance
                * kFurnitureHandleCaptureDistance) {
        if (CAlfaObject* captured =
                document_->FindObjectById(hovered_furniture_handle_id_)) {
            return captured;
        }
    }

    // First test only the small handle bodies. This prevents an ordinary
    // Orbit press on a complex imported room from ray-testing every triangle.
    auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        depth = dot(
            world - camera_position(camera_, orthographic_projection_),
            forward);
        return depth > 0.0f && renderer_.WorldToScreen(
            world, camera_, orthographic_projection_, width(), height(),
            screen);
    };

    // Thin handles are hard to hit after perspective projection. Probe a
    // compact 12-pixel halo around the cursor, then latch the result in the
    // hover code for a wider 20-pixel hysteresis zone.
    const std::array<QPoint, 13> probe_offsets = {
        QPoint{0, 0},
        QPoint{6, 0}, QPoint{-6, 0},
        QPoint{0, 6}, QPoint{0, -6},
        QPoint{6, 6}, QPoint{6, -6},
        QPoint{-6, 6}, QPoint{-6, -6},
        QPoint{12, 0}, QPoint{-12, 0},
        QPoint{0, 12}, QPoint{0, -12}};

    CAlfaObject* best_handle = nullptr;
    QPoint best_probe = point;
    int best_probe_distance_squared = std::numeric_limits<int>::max();
    float best_depth = std::numeric_limits<float>::max();
    for (const auto& object : document_->GetObjects()) {
        auto* solid = object ? dynamic_cast<CSolid*>(object.get()) : nullptr;
        if (!solid || !document_->IsObjectSelectable(*solid)
            || !is_interactive_furniture_handle_name(solid->GetName())) {
            continue;
        }
        for (const QPoint& offset : probe_offsets) {
            const QPoint probe = point + offset;
            const DomPoint screen_point{probe.x(), probe.y()};
            float depth = 0.0f;
            if (!solid->HitTestMeshScreen(
                    screen_point, project_world, depth)) {
                continue;
            }
            const int distance_squared =
                offset.x() * offset.x() + offset.y() * offset.y();
            if (distance_squared < best_probe_distance_squared
                || (distance_squared == best_probe_distance_squared
                    && depth < best_depth)) {
                best_handle = solid;
                best_probe = probe;
                best_probe_distance_squared = distance_squared;
                best_depth = depth;
            }
        }
    }
    if (!best_handle) {
        return nullptr;
    }

    // Confirm normal scene occlusion only after the cheap handle test. This
    // keeps a handle behind another object from reacting to the click.
    CAlfaObject* visible_object = FindObjectForMaterialAt(best_probe);
    return visible_object
            && is_interactive_furniture_handle_name(visible_object->GetName())
        ? visible_object : nullptr;
}

CAlfaObject* OpenGLViewport::FindObjectForMaterialAt(const QPoint& point) {
    if (!document_) {
        return nullptr;
    }

    const DomPoint screen_point{point.x(), point.y()};
    auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        depth = dot(world - camera_position(camera_, orthographic_projection_), forward);
        return depth > 0.0f && renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };
    if (CSolid* solid = document_->FindSolidAtScreen(screen_point, project_world)) {
        return solid;
    }

    auto& objects = document_->GetObjects();
    CMesh3D* best_mesh = nullptr;
    float best_depth = std::numeric_limits<float>::max();
    for (const auto& object : objects) {
        auto* mesh = dynamic_cast<CMesh3D*>(object.get());
        if (!mesh || !document_->IsObjectSelectable(*mesh)) {
            continue;
        }
        float depth = 0.0f;
        if (mesh->HitTestMeshScreen(screen_point, project_world, depth) && depth < best_depth) {
            best_mesh = mesh;
            best_depth = depth;
        }
    }
    return best_mesh;
}

void OpenGLViewport::ConstrainPolylinePoint(CPoint3d& point, Qt::KeyboardModifiers modifiers) const {
    if (IsSnapTargetEnabled(SnapTarget::Surface)) return;
    if (tool_ != ToolMode::DrawCurve || modifiers != Qt::ShiftModifier || !document_) return;
    const auto& points = spatial_curve_preview_kind_ == SpatialCurvePreviewKind::Polyline
        ? spatial_curve_preview_points_ : document_->GetActivePolyline().GetPoints();
    if (points.empty()) return;
    const CPoint3d& anchor = points.back();
    if (xy_plane_view_enabled_) {
        if (std::abs(point.x - anchor.x) >= std::abs(point.y - anchor.y)) point.y = anchor.y;
        else point.x = anchor.x;
        point.z = anchor.z;
        return;
    }
    Vec3 forward{}, right{}, up{};
    viewport_camera_basis(camera_, forward, right, up);
    const double dx = point.x - anchor.x, dy = point.y - anchor.y, dz = point.z - anchor.z;
    const double horizontal = dx * right.x + dy * right.y + dz * right.z;
    const double vertical = dx * up.x + dy * up.y + dz * up.z;
    const bool use_horizontal = std::abs(horizontal) >= std::abs(vertical);
    const Vec3 axis = use_horizontal ? right : up;
    const double distance = use_horizontal ? horizontal : vertical;
    point = CPoint3d(anchor.x + axis.x * distance, anchor.y + axis.y * distance, anchor.z + axis.z * distance);
}

void OpenGLViewport::DrawCurveAt(const QPoint& point, Qt::KeyboardModifiers modifiers) {
    CPoint3d scene_point{};
    if (!ScreenToCurvePlane(point, scene_point)) {
        return;
    }
    const bool snapped = SnapCreationPoint(point, scene_point, false);
    SetCreationSnapCursor(snapped);

    ConstrainPolylinePoint(scene_point, modifiers);
    document_->AddCurvePoint(scene_point);
    curve_preview_point_ = scene_point;
    curve_preview_valid_ = true;
    NotifyDocumentChanged();
    update();
}

void OpenGLViewport::HandleBooleanClick(const QPoint& point) {
    if (!document_) {
        return;
    }

    const DomPoint screen_point{point.x(), point.y()};
    auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        depth = dot(world - camera_position(camera_, orthographic_projection_), forward);
        return depth > 0.0f && renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };
    bool selected_solid = document_->SelectSolidMeshAtScreen(screen_point, project_world, SelectionAction::Replace);
    if (!selected_solid) {
        CurvePoint scene_point{};
        if (!renderer_.ScreenToFloor(point.x(), point.y(), width(), height(), camera_, orthographic_projection_, scene_point)) {
            SetBooleanOperation(boolean_operation_);
            return;
        }
        selected_solid = document_->SelectObjectAt(scene_point, 0.35f, false) && dynamic_cast<CSolid*>(document_->GetSelectedObject());
    }

    if (!selected_solid) {
        emit SelectionChanged();
        NotifyDocumentChanged();
        emit StatusTextChanged(has_boolean_body_ ? "Boolean: tool operation failed; check the selected geometry and parameters" : "Boolean: body operation failed; check the selected geometry and parameters");
        update();
        return;
    }

    const size_t clicked_index = document_->GetSelectedObjectIndex();
    emit SelectionChanged();
    NotifyDocumentChanged();

    if (!has_boolean_body_) {
        boolean_body_index_ = clicked_index;
        has_boolean_body_ = true;
        SetBooleanOperation(boolean_operation_);
        update();
        return;
    }

    if (clicked_index == boolean_body_index_) {
        SetBooleanOperation(boolean_operation_);
        update();
        return;
    }

    const bool applied = document_->ApplyBooleanToSolids(boolean_body_index_, clicked_index, boolean_operation_);
    if (!applied) {
        emit StatusTextChanged("Boolean:operation failed; check the selected geometry and parameters");
        update();
        return;
    }

    has_boolean_body_ = false;
    boolean_body_index_ = 0;
    document_->ClearSelection();
    SetTool(ToolMode::Select);
    emit SelectionChanged();
    NotifyDocumentChanged();
    emit BooleanFinished();
    emit StatusTextChanged("Boolean:operation completed");
    update();
}

void OpenGLViewport::DrawBSplineAt(const QPoint& point) {
    CPoint3d scene_point{};
    if (!ScreenToCurvePlane(point, scene_point)) {
        return;
    }
    const bool snapped = SnapCreationPoint(point, scene_point, false);
    SetCreationSnapCursor(snapped);

    document_->AddBSplinePoint(scene_point);
    curve_preview_point_ = scene_point;
    curve_preview_valid_ = true;
    NotifyDocumentChanged();
    update();
}

void OpenGLViewport::HandleFaceExtrudeClick(const QPoint& point) {
    if (!document_) {
        return;
    }

    if (document_->GetSelectedSolidFaceCenterAndNormal(face_extrude_center_, face_extrude_normal_) && HitTestFaceExtrudeGizmo(point)) {
        if (!document_->BeginLiveExtrudeSelectedSolidFace(face_extrude_taper_angle_degrees_)) {
            emit StatusTextChanged("Extrude Face: operation failed");
            return;
        }
        face_extrude_boundary_.clear();
        if(const auto* solid=document_->GetSelectedFaceSolid()) {
            if(const auto* surface=solid->GetSurfaceFace(solid->GetSelectedFaceIndex())) {
                for(int i=0;i<surface->GetEdgeCount();++i) {
                    std::vector<Vec3> points;
                    if(surface->GetEdgePolylinePoints(i,points)&&points.size()>1)
                        face_extrude_boundary_.push_back(std::move(points));
                }
            }
        }
        dragging_face_extrude_ = true;
        setCursor(Qt::ClosedHandCursor);
        face_extrude_distance_ = 0.0f;
        last_mouse_ = point;
        update();
        return;
    }

    const DomPoint screen_point{point.x(), point.y()};
    auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        depth = dot(world - camera_position(camera_, orthographic_projection_), forward);
        return depth > 0.0f && renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };

    if (document_->SelectSolidFaceAtScreen(screen_point, project_world)) {
        document_->GetSelectedSolidFaceCenterAndNormal(face_extrude_center_, face_extrude_normal_);
        emit SelectionChanged();
        NotifyDocumentChanged();
        emit StatusTextChanged("Extrude Face: drag the orange normal gizmo");
        update();
        return;
    }

    emit StatusTextChanged("Extrude Face: select a solid face");
    update();
}

void OpenGLViewport::HandleFaceExtrudeDrag(const QPoint& point) {
    if (!document_ || !document_->IsLiveExtrudeSelectedSolidFaceActive()) {
        dragging_face_extrude_ = false;
        return;
    }

    DomPoint center_screen{};
    DomPoint end_screen{};
    const float gizmo_size = std::max(0.8f, camera_.distance * 0.10f);
    if (!renderer_.WorldToScreen(face_extrude_center_, camera_, orthographic_projection_, width(), height(), center_screen)
        || !renderer_.WorldToScreen(face_extrude_center_ + face_extrude_normal_ * gizmo_size, camera_, orthographic_projection_, width(), height(), end_screen)) {
        return;
    }

    const float mouse_dx = static_cast<float>(point.x() - last_mouse_.x());
    const float mouse_dy = static_cast<float>(point.y() - last_mouse_.y());
    const float axis_dx = static_cast<float>(end_screen.x - center_screen.x);
    const float axis_dy = static_cast<float>(end_screen.y - center_screen.y);
    const float axis_len_sq = axis_dx * axis_dx + axis_dy * axis_dy;
    if (axis_len_sq <= 0.0001f) {
        return;
    }

    const float axis_len = std::sqrt(axis_len_sq);
    const float pixels_along_axis = (mouse_dx * axis_dx + mouse_dy * axis_dy) / axis_len;
    const float pixels_per_world = axis_len / gizmo_size;
    const float world_delta = pixels_along_axis / pixels_per_world;
    const float next_distance = face_extrude_distance_ + world_delta;
    if (std::fabs(world_delta) > 0.0001f && document_->PreviewLiveExtrudeSelectedSolidFace(next_distance)) {
        face_extrude_center_ = face_extrude_center_ + face_extrude_normal_ * world_delta;
        face_extrude_distance_ += world_delta;
        last_mouse_ = point;

        update();
    }
}

void OpenGLViewport::CommitFaceExtrudeDrag() {
    if (!document_) {
        return;
    }

    emit StatusTextChanged("Extrude Face: building final solid...");
    repaint();
    const bool valid=std::abs(face_extrude_distance_)>0.0001f
        &&document_->UpdateLiveExtrudeSelectedSolidFace(face_extrude_distance_);
    if(valid)document_->FinishLiveExtrudeSelectedSolidFace();
    else document_->CancelLiveExtrudeSelectedSolidFace();
    face_extrude_distance_ = 0.0f;
    SetTool(ToolMode::Select);
    emit SelectionChanged();
    if(valid)NotifyDocumentChanged();
    emit StatusTextChanged(valid?"Extrude Face: done":"Extrude Face: no valid extrusion; original body restored");
}

bool OpenGLViewport::HitTestFaceExtrudeGizmo(const QPoint& point) const {
    if (!document_ || !document_->HasSelectedSolidFace()) {
        return false;
    }

    DomPoint center_screen{};
    DomPoint end_screen{};
    const float gizmo_size = std::max(0.8f, camera_.distance * 0.10f);
    if (!renderer_.WorldToScreen(face_extrude_center_, camera_, orthographic_projection_, width(), height(), center_screen)
        || !renderer_.WorldToScreen(face_extrude_center_ + face_extrude_normal_ * gizmo_size, camera_, orthographic_projection_, width(), height(), end_screen)) {
        return false;
    }

    return DistanceToScreenSegment({point.x(), point.y()}, center_screen, end_screen) <= 12.0f;
}

void OpenGLViewport::HandleDraftFaceClick(const QPoint& point) {
    if (!document_) {
        return;
    }

    if (document_->GetDraftFaceAxis(draft_face_axis_center_, draft_face_axis_dir_) && HitTestDraftFaceGizmo(point)) {
        if (!document_->BeginLiveDraftFace()) {
            emit StatusTextChanged("Draft Face: operation failed");
            return;
        }
        emit DraftFaceEditStarted();
        DomPoint center_screen{};
        if (renderer_.WorldToScreen(draft_face_axis_center_, camera_, orthographic_projection_, width(), height(), center_screen)) {
            draft_face_start_mouse_angle_ = std::atan2(static_cast<double>(point.y() - center_screen.y),
                                                       static_cast<double>(point.x() - center_screen.x));
        } else {
            draft_face_start_mouse_angle_ = 0.0;
        }
        dragging_draft_face_ = true;
        draft_face_angle_degrees_ = 0.0;
        last_mouse_ = point;
        update();
        return;
    }

    const DomPoint screen_point{point.x(), point.y()};
    auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
        return renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };
    auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        depth = dot(world - camera_position(camera_, orthographic_projection_), forward);
        return depth > 0.0f && renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };

    if (!document_->HasDraftFace()) {
        if (document_->SelectSolidPlanarFaceAtScreen(screen_point, project_world)) {
            if (document_->BeginDraftFaceFromSelectedFace()) {
                emit SelectionChanged();
                NotifyDocumentChanged();
                emit StatusTextChanged("Draft Face: choose a straight edge axis");
                update();
                return;
            }
            emit StatusTextChanged("Draft Face: unavailable after Non Uniform Scale");
            update();
            return;
        }
        emit StatusTextChanged("Draft Face: select a planar face");
        update();
        return;
    }

    if (document_->SelectDraftFaceAxisEdgeAtScreen(screen_point, world_to_screen, 10.0f)) {
        document_->GetDraftFaceAxis(draft_face_axis_center_, draft_face_axis_dir_);
        emit SelectionChanged();
        NotifyDocumentChanged();
        emit StatusTextChanged("Draft Face: drag the rotation gizmo");
        update();
        return;
    }

    emit StatusTextChanged("Draft Face: choose a straight edge on selected face");
    update();
}

void OpenGLViewport::HandleThickSolidClick(const QPoint& point) {
    if (!document_) {
        return;
    }

    const DomPoint screen_point{point.x(), point.y()};
    auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        depth = dot(world - camera_position(camera_, orthographic_projection_), forward);
        return depth > 0.0f && renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };

    if (!document_->HasLiveThickSolid()) {
        if (document_->SelectSolidMeshAtScreen(screen_point, project_world, SelectionAction::Replace)
            && document_->BeginLiveThickSolidFromSelectedSolid(thick_solid_thickness_)) {
            emit SelectionChanged();
            NotifyDocumentChanged();
            emit StatusTextChanged("ThickSolid: select a solid and the faces to remove; set wall thickness and confirm");
            update();
            return;
        }
        emit StatusTextChanged("ThickSolid: select a solid and the faces to remove; set wall thickness and confirm");
        update();
        return;
    }

    if (document_->SelectLiveThickSolidFaceAtScreen(screen_point, project_world)) {
        emit SelectionChanged();
        NotifyDocumentChanged();
        emit StatusTextChanged(QString("ThickSolid: Faces %1, Thick %2")
            .arg(static_cast<int>(document_->GetLiveThickSolidFaceCount()))
            .arg(thick_solid_thickness_, 0, 'f', 2));
        update();
        return;
    }

    emit StatusTextChanged("ThickSolid: select a solid and the faces to remove; set wall thickness and confirm");
    update();
}

void OpenGLViewport::HandleDraftFaceDrag(const QPoint& point) {
    if (!document_ || !document_->IsLiveDraftFaceActive()) {
        dragging_draft_face_ = false;
        return;
    }

    DomPoint center_screen{};
    DomPoint axis_screen{};
    const float gizmo_size = std::max(0.8f, camera_.distance * 0.10f);
    if (!renderer_.WorldToScreen(draft_face_axis_center_, camera_, orthographic_projection_, width(), height(), center_screen)
        || !renderer_.WorldToScreen(draft_face_axis_center_ + draft_face_axis_dir_ * gizmo_size, camera_, orthographic_projection_, width(), height(), axis_screen)) {
        return;
    }

    const float mouse_dx = static_cast<float>(point.x() - last_mouse_.x());
    const float mouse_dy = static_cast<float>(point.y() - last_mouse_.y());
    const float axis_dx = static_cast<float>(axis_screen.x - center_screen.x);
    const float axis_dy = static_cast<float>(axis_screen.y - center_screen.y);
    const float axis_len_sq = axis_dx * axis_dx + axis_dy * axis_dy;
    if (axis_len_sq <= 0.0001f) {
        return;
    }

    const float axis_len = std::sqrt(axis_len_sq);
    const float pixels_around_axis = (mouse_dx * -axis_dy + mouse_dy * axis_dx) / axis_len;
    const double delta_degrees = static_cast<double>(pixels_around_axis) * 0.08;
    const double next_angle = std::clamp(draft_face_angle_degrees_ + delta_degrees, -89.0, 89.0);
    if (std::fabs(delta_degrees) <= 0.0001) {
        return;
    }

    if (document_->UpdateLiveDraftFace(next_angle)) {
        draft_face_angle_degrees_ = next_angle;
        last_mouse_ = point;
        emit StatusTextChanged(QString("Draft Face: angle %1").arg(draft_face_angle_degrees_, 0, 'f', 2));
        update();
    } else {
        last_mouse_ = point;
        emit StatusTextChanged("Draft Face: angle failed");
    }
}

void OpenGLViewport::CommitDraftFaceDrag() {
    if (!document_) {
        return;
    }

    const bool accepted = std::abs(draft_face_angle_degrees_) > 0.0001;
    document_->FinishLiveDraftFace();
    emit DraftFaceEditFinished(accepted);
    draft_face_angle_degrees_ = 0.0;
    SetTool(ToolMode::Select);
    emit SelectionChanged();
    emit StatusTextChanged("Draft Face: done");
}

bool OpenGLViewport::HitTestDraftFaceGizmo(const QPoint& point) const {
    if (!document_) {
        return false;
    }

    Vec3 center{};
    Vec3 axis{};
    if (!document_->GetDraftFaceAxis(center, axis)) {
        return false;
    }

    Vec3 forward{};
    Vec3 right{};
    Vec3 up{};
    viewport_camera_basis(camera_, forward, right, up);
    Vec3 tangent{};
    Vec3 bitangent{};
    rotation_arc_basis(axis, forward, tangent, bitangent);

    constexpr int kSegments = 96;
    const float radius = std::max(0.8f, camera_.distance * 0.10f) * 1.26f;
    const DomPoint mouse_point{point.x(), point.y()};
    DomPoint previous{};
    bool has_previous = false;
    float best_distance = 18.0f;
    for (int i = 0; i <= kSegments; ++i) {
        const float angle = static_cast<float>(i) * 2.0f * 3.14159265f / static_cast<float>(kSegments);
        const Vec3 world_point = center + tangent * (std::cos(angle) * radius) + bitangent * (std::sin(angle) * radius);
        DomPoint screen_point{};
        if (!renderer_.WorldToScreen(world_point, camera_, orthographic_projection_, width(), height(), screen_point)) {
            has_previous = false;
            continue;
        }
        if (has_previous) {
            best_distance = std::min(best_distance, DistanceToScreenSegment(mouse_point, previous, screen_point));
        }
        previous = screen_point;
        has_previous = true;
    }
    return best_distance < 18.0f;
}

void OpenGLViewport::HandleTransformClick(const QPoint& point, bool add_to_selection) {
    if (document_->HasSelection()) {
        // Preserve the handle shown to the user.  In particular, once the
        // central move ring is highlighted an overlapping axis must not take
        // the press between the hover and mouse-down events.
        TransformOperation picked_operation = transform_operation_;
        const TransformAxis axis = !universal_transform_ && transform_operation_ == TransformOperation::Move
            && highlighted_transform_axis_ == TransformAxis::ScreenPlane
            ? TransformAxis::ScreenPlane : HitTestTransformGizmo(point, &picked_operation);
        if (axis != TransformAxis::None) {
            transform_operation_ = picked_operation;
            active_transform_axis_ = axis;
            highlighted_transform_axis_ = axis;
            dragging_transform_ = true;
            transform_drag_has_preview_ = false;
            transform_drag_move_delta_ = {};
            transform_drag_rotation_input_angle_ = 0.0f;
            transform_drag_rotation_angle_ = 0.0f;
            transform_drag_scale_factor_ = 1.0f;
            document_->GetTransformGizmoCenter(transform_drag_center_);
            transform_drag_axis_ = AxisVector(axis);
            if (selection_mode_ == SelectionMode::Point) {
                CaptureCurvePointChangeBefore();
            }
            last_mouse_ = point;
            update();
            return;
        }
    }

    document_->ClearTransformGizmoOrigin();

    const DomPoint screen_point{point.x(), point.y()};
    auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
        return renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };
    auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        depth = dot(world - camera_position(camera_, orthographic_projection_), forward);
        return depth > 0.0f && renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };

    const SelectionAction solid_action = add_to_selection ? SelectionAction::Add : SelectionAction::Replace;
    if (selection_mode_ == SelectionMode::Point) {
        if (document_->SelectCurvePointAtScreen(screen_point, world_to_screen, 10.0f, solid_action)) {
            emit SelectionChanged();
            NotifyDocumentChanged();
            update();
        }
        return;
    }
    if (document_->SelectPolylineAtScreen(screen_point, world_to_screen, 8.0f, solid_action)) {
        document_->ExpandSelectedGroups();
        emit SelectionChanged();
        NotifyDocumentChanged();
        update();
        return;
    }

    if (document_->SelectSolidMeshAtScreen(screen_point, project_world, solid_action)) {
        document_->ExpandSelectedGroups();
        emit SelectionChanged();
        NotifyDocumentChanged();
        update();
        return;
    }

    if (document_->SelectMeshAtScreen(screen_point, project_world, solid_action)) {
        document_->ExpandSelectedGroups();
        emit SelectionChanged();
        NotifyDocumentChanged();
        update();
        return;
    }

    CurvePoint scene_point{};
    if (!renderer_.ScreenToFloor(point.x(), point.y(), width(), height(), camera_, orthographic_projection_, scene_point)) {
        return;
    }
    if (add_to_selection) {
        document_->ToggleObjectSelectionAt(scene_point, 0.35f, false);
    } else {
        document_->SelectObjectAt(scene_point, 0.35f, false);
    }
    document_->ExpandSelectedGroups();

    emit SelectionChanged();
    NotifyDocumentChanged();
    update();
}

void OpenGLViewport::HandleTransformDrag(const QPoint& point, Qt::KeyboardModifiers modifiers) {
    if (!document_ || active_transform_axis_ == TransformAxis::None) {
        return;
    }

    if (!document_->HasSelection()) {
        dragging_transform_ = false;
        active_transform_axis_ = TransformAxis::None;
        return;
    }
    const Vec3 center = transform_drag_center_;

    const float mouse_dx = static_cast<float>(point.x() - last_mouse_.x());
    const float mouse_dy = static_cast<float>(point.y() - last_mouse_.y());

    if (transform_operation_ == TransformOperation::Move && active_transform_axis_ == TransformAxis::ScreenPlane) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);

        const int viewport_height = std::max(1, height());
        float world_per_pixel = 1.0f;
        if (orthographic_projection_) {
            world_per_pixel = (
                std::max(kMinimumOrthographicHalfHeight, camera_.distance * 0.42f)
                * 2.0f) / static_cast<float>(viewport_height);
        } else {
            const float depth = std::max(0.001f, dot(center - camera_position(camera_, orthographic_projection_), forward));
            world_per_pixel = (2.0f * depth * std::tan(
                deg_to_rad(camera_.vertical_fov_degrees) * 0.5f))
                / static_cast<float>(viewport_height);
        }

        const Vec3 delta = right * (mouse_dx * world_per_pixel) - up * (mouse_dy * world_per_pixel);
        const bool moved = selection_mode_ == SelectionMode::Point
            ? document_->MoveSelectedCurvePoints(delta, xy_plane_view_enabled_)
            : document_->PreviewMoveSelectedObjects(delta);
        if (std::sqrt(dot(delta, delta)) > 0.000001f && moved) {
            transform_drag_move_delta_ = transform_drag_move_delta_ + delta;
            transform_drag_has_preview_ = true;
            last_mouse_ = point;
            update();
        }
        return;
    }

    if (transform_operation_ == TransformOperation::Scale && active_transform_axis_ == TransformAxis::UniformScale) {
        const float pixels = mouse_dx - mouse_dy;
        const float factor = std::clamp(1.0f + pixels * 0.01f, 0.05f, 20.0f);
        const bool scaled = selection_mode_ == SelectionMode::Point
            ? document_->ScaleSelectedCurvePoints(center, {}, factor)
            : document_->PreviewUniformScaleSelectedObjects(center, factor);
        if (std::fabs(pixels) > 0.0001f && scaled) {
            transform_drag_scale_factor_ *= factor;
            transform_drag_has_preview_ = true;
            last_mouse_ = point;
            update();
        }
        return;
    }

    const Vec3 axis = AxisVector(active_transform_axis_);
    const float gizmo_size = std::max(0.8f, camera_.distance * 0.10f);
    const float axis_distance =
        gizmo_size * TransformGizmoGeometry::kAxisDistanceScale;
    DomPoint center_screen{};
    DomPoint axis_screen{};
    if (!renderer_.WorldToScreen(center, camera_, orthographic_projection_, width(), height(), center_screen)
        || !renderer_.WorldToScreen(center + axis * axis_distance, camera_, orthographic_projection_, width(), height(), axis_screen)) {
        return;
    }

    const float axis_dx = static_cast<float>(axis_screen.x - center_screen.x);
    const float axis_dy = static_cast<float>(axis_screen.y - center_screen.y);
    const float axis_len_sq = axis_dx * axis_dx + axis_dy * axis_dy;
    float pixels_along_axis = 0.0f;
    float world_delta = 0.0f;
    float pixels_around_axis = 0.0f;
    if (axis_len_sq > 16.0f) {
        const float axis_len = std::sqrt(axis_len_sq);
        pixels_along_axis = (mouse_dx * axis_dx + mouse_dy * axis_dy) / axis_len;
        const float pixels_per_world = axis_len / axis_distance;
        world_delta = pixels_along_axis / pixels_per_world;
        pixels_around_axis = (mouse_dx * -axis_dy + mouse_dy * axis_dx) / axis_len;
    } else if (transform_operation_ == TransformOperation::Rotate) {
        // When looking exactly along the rotation axis, its screen projection
        // is a point. Measure the cursor's angular movement around the arc
        // center instead of trying to derive a perpendicular projected axis.
        const float previous_x = static_cast<float>(last_mouse_.x() - axis_screen.x);
        const float previous_y = static_cast<float>(axis_screen.y - last_mouse_.y());
        const float current_x = static_cast<float>(point.x() - axis_screen.x);
        const float current_y = static_cast<float>(axis_screen.y - point.y());
        const float previous_length_sq = previous_x * previous_x + previous_y * previous_y;
        const float current_length_sq = current_x * current_x + current_y * current_y;
        if (previous_length_sq <= 1.0f || current_length_sq <= 1.0f) {
            last_mouse_ = point;
            return;
        }
        const float cross = previous_x * current_y - previous_y * current_x;
        const float dot_product = previous_x * current_x + previous_y * current_y;
        const float angular_delta = std::atan2(cross, dot_product);
        pixels_around_axis = angular_delta / 0.01f;
    } else {
        return;
    }

    bool transformed = false;
    if (transform_operation_ == TransformOperation::Move) {
        const Vec3 delta = axis * world_delta;
        transformed = selection_mode_ == SelectionMode::Point
            ? document_->MoveSelectedCurvePoints(delta, xy_plane_view_enabled_)
            : document_->PreviewMoveSelectedObjects(delta);
        if (transformed) {
            transform_drag_move_delta_ = transform_drag_move_delta_ + delta;
        }
    } else if (transform_operation_ == TransformOperation::Rotate) {
        constexpr float kRotationSnapAngle = 3.14159265f / 4.0f;
        transform_drag_rotation_input_angle_ += pixels_around_axis * 0.01f;
        float target_angle = transform_drag_rotation_input_angle_;
        if (modifiers.testFlag(Qt::ControlModifier)) {
            target_angle = std::trunc(target_angle / kRotationSnapAngle) * kRotationSnapAngle;
        }
        const float angle_to_apply = target_angle - transform_drag_rotation_angle_;
        if (std::fabs(angle_to_apply) > 0.000001f) {
            transformed = selection_mode_ == SelectionMode::Point
                ? document_->RotateSelectedCurvePoints(center, axis, angle_to_apply)
                : document_->PreviewRotateSelectedObjects(center, axis, angle_to_apply);
        }
        if (transformed) {
            transform_drag_rotation_angle_ = target_angle;
            transform_drag_axis_ = axis;
        }
        // Mouse movement must continue accumulating while Ctrl snapping keeps
        // the object at its current 45-degree step.
        last_mouse_ = point;
    } else {
        const float factor = std::clamp(1.0f + pixels_along_axis * 0.01f, 0.05f, 20.0f);
        transformed = selection_mode_ == SelectionMode::Point
            ? document_->ScaleSelectedCurvePoints(center, axis, factor)
            : document_->PreviewScaleSelectedObjects(center, axis, factor);
        if (transformed) {
            transform_drag_scale_factor_ *= factor;
            transform_drag_axis_ = axis;
        }
    }

    const float active_pixels = transform_operation_ == TransformOperation::Rotate ? pixels_around_axis : pixels_along_axis;
    if (std::fabs(active_pixels) > 0.0001f && transformed) {
        transform_drag_has_preview_ = true;
        if (transform_operation_ != TransformOperation::Rotate) {
            last_mouse_ = point;
        }
        update();
    }
}

void OpenGLViewport::SetDrawSplineSimplification(int percent) {
    draw_spline_simplification_ = std::clamp(percent, 0, 100);
    if (drawing_spline_stroke_) {
        UpdateDrawSplinePreview();
        update();
    }
}

void OpenGLViewport::BeginDrawSplineStroke(const QPoint& point) {
    if (!document_) {
        return;
    }
    draw_spline_screen_points_.clear();
    draw_spline_raw_points_.clear();
    draw_spline_preview_points_.clear();

    CPoint3d world{};
    if (!ScreenToCurvePlane(point, world)) {
        emit StatusTextChanged(IsSnapTargetEnabled(SnapTarget::Surface)
            ? "Draw Spline: no visible surface under the cursor"
            : "Draw Spline: point is outside the drawing plane");
        return;
    }
    document_->ClearSelection();
    emit SelectionChanged();
    drawing_spline_stroke_ = true;
    draw_spline_screen_points_.push_back(point);
    draw_spline_raw_points_.push_back(world);
    draw_spline_preview_points_.push_back(world);
    emit StatusTextChanged("Draw Spline: hold the left mouse button and draw");
    update();
}

void OpenGLViewport::AppendDrawSplineStroke(const QPoint& point) {
    if (!drawing_spline_stroke_) {
        return;
    }
    if (!draw_spline_screen_points_.empty()) {
        const QPoint delta = point - draw_spline_screen_points_.back();
        if (delta.x() * delta.x() + delta.y() * delta.y() < 2 * 2) {
            return;
        }
    }
    CPoint3d world{};
    if (!ScreenToCurvePlane(point, world)) {
        return;
    }
    draw_spline_screen_points_.push_back(point);
    draw_spline_raw_points_.push_back(world);
    UpdateDrawSplinePreview();
    update();
}

void OpenGLViewport::UpdateDrawSplinePreview() {
    draw_spline_preview_points_.clear();
    if (draw_spline_raw_points_.empty()) {
        return;
    }
    if (draw_spline_raw_points_.size() <= 2) {
        draw_spline_preview_points_ = draw_spline_raw_points_;
        return;
    }

    // Ramer-Douglas-Peucker is evaluated in screen pixels, so the slider has
    // the same feel at every camera zoom level. Fifty percent corresponds to
    // roughly 12 px, matching the moderately simplified reference stroke.
    const double tolerance = 0.5
        + 23.0 * static_cast<double>(draw_spline_simplification_) / 100.0;
    std::vector<bool> keep(draw_spline_screen_points_.size(), false);
    keep.front() = true;
    keep.back() = true;
    simplify_screen_stroke(draw_spline_screen_points_, 0,
                           draw_spline_screen_points_.size() - 1,
                           tolerance * tolerance, keep);
    for (size_t index = 0; index < keep.size(); ++index) {
        if (keep[index]) {
            draw_spline_preview_points_.push_back(draw_spline_raw_points_[index]);
        }
    }
}

void OpenGLViewport::FinishDrawSplineStroke() {
    if (!drawing_spline_stroke_) {
        return;
    }
    UpdateDrawSplinePreview();
    if (!document_ || draw_spline_preview_points_.size() < 2) {
        CancelDrawSplineStroke();
        emit StatusTextChanged("Draw Spline: draw a longer stroke");
        return;
    }

    auto spline = std::make_unique<CBSpline>(
        "Draw Spline " + std::to_string(document_->GetObjects().size() + 1));
    spline->SetCurveType(SplineCurveType::BSpline);
    spline->SetDegree(std::min(
        3, static_cast<int>(draw_spline_preview_points_.size()) - 1));
    for (const CPoint3d& point : draw_spline_preview_points_) {
        spline->AddPoint(point);
    }
    spline->SetParametricDefinition(
        "DrawSpline",
        {{"simplification", static_cast<double>(draw_spline_simplification_)}});
    const size_t point_count = draw_spline_preview_points_.size();
    document_->AddObject(std::move(spline));

    drawing_spline_stroke_ = false;
    draw_spline_screen_points_.clear();
    draw_spline_raw_points_.clear();
    draw_spline_preview_points_.clear();
    emit SelectionChanged();
    NotifyDocumentChanged();
    emit StatusTextChanged(QString("Draw Spline: B-Spline created (%1 control points). Draw the next stroke or press Esc")
        .arg(point_count));
    update();
}

void OpenGLViewport::CancelDrawSplineStroke() {
    drawing_spline_stroke_ = false;
    draw_spline_screen_points_.clear();
    draw_spline_raw_points_.clear();
    draw_spline_preview_points_.clear();
    update();
}

void OpenGLViewport::CommitTransformDrag() {
    if (!document_ || !transform_drag_has_preview_) {
        return;
    }

    bool committed = selection_mode_ == SelectionMode::Point && document_->HasSelectedPoint();
    if (selection_mode_ == SelectionMode::Point) {
        if (committed) {
            FinalizeCurvePointChange();
            NotifyDocumentChanged();
        }
        return;
    }
    std::vector<unsigned long> transformed_root_ids;
    if (transform_operation_ == TransformOperation::Move) {
        transformed_root_ids = document_->GetSelectedTransformRootIds();
        committed = document_->CommitMoveSelectedSolids(transform_drag_move_delta_);
    } else if (transform_operation_ == TransformOperation::Rotate) {
        committed = document_->CommitRotateSelectedSolids(transform_drag_center_, transform_drag_axis_, transform_drag_rotation_angle_);
    } else if (active_transform_axis_ == TransformAxis::UniformScale) {
        committed = document_->CommitUniformScaleSelectedSolids(transform_drag_center_, transform_drag_scale_factor_);
    } else {
        committed = document_->CommitScaleSelectedSolids(transform_drag_center_, transform_drag_axis_, transform_drag_scale_factor_);
    }

    if (committed) {
        if (transform_operation_ == TransformOperation::Move
            && !transformed_root_ids.empty()) {
            object_move_change_pending_ = true;
            object_move_change_ids_ = std::move(transformed_root_ids);
            object_move_change_delta_ = transform_drag_move_delta_;
        }
        NotifyDocumentChanged();
    }
}

TransformAxis OpenGLViewport::HitTestTransformGizmo(const QPoint& point, TransformOperation* operation) const {
    if (operation) *operation = transform_operation_;
    if (!universal_transform_) return HitTestTransformGizmoForOperation(point, transform_operation_);
    Vec3 center{};
    if (!document_ || !document_->GetTransformGizmoCenter(center)) return TransformAxis::None;
    DomPoint scale{};
    if (renderer_.WorldToScreen(center, camera_, orthographic_projection_, width(), height(), scale)
        && std::hypot(float(point.x()-scale.x),float(point.y()-scale.y)) < 12.f) {
        if (operation) *operation = TransformOperation::Scale;
        return TransformAxis::UniformScale;
    }
    const auto move = HitTestTransformGizmoForOperation(point, TransformOperation::Move);
    if (move == TransformAxis::ScreenPlane) {
        if (operation) *operation = TransformOperation::Move;
        return move;
    }
    const auto rotate = HitTestTransformGizmoForOperation(point, TransformOperation::Rotate);
    if (rotate != TransformAxis::None) {
        if (operation) *operation = TransformOperation::Rotate;
        return rotate;
    }
    if (operation) *operation = TransformOperation::Move;
    return move;
}

TransformAxis OpenGLViewport::HitTestTransformGizmoForOperation(const QPoint& point, TransformOperation operation) const {
    if (!document_) {
        return TransformAxis::None;
    }

    Vec3 center{};
    if (!document_->GetTransformGizmoCenter(center)) {
        return TransformAxis::None;
    }

    const float gizmo_size = std::max(0.8f, camera_.distance * 0.10f);
    const float axis_distance =
        gizmo_size * TransformGizmoGeometry::kAxisDistanceScale;
    DomPoint center_screen{};
    if (!renderer_.WorldToScreen(center, camera_, orthographic_projection_, width(), height(), center_screen)) {
        return TransformAxis::None;
    }

    if (operation == TransformOperation::Move) {
        const float ring_radius =
            TransformGizmoGeometry::kMoveCenterRingRadiusScale * gizmo_size;
        DomPoint ring_edge{};
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        if (renderer_.WorldToScreen(center + right * ring_radius, camera_, orthographic_projection_, width(), height(), ring_edge)) {
            const float ring_dx = static_cast<float>(ring_edge.x - center_screen.x);
            const float ring_dy = static_cast<float>(ring_edge.y - center_screen.y);
            const float ring_radius_px = std::sqrt(ring_dx * ring_dx + ring_dy * ring_dy);
            const float mouse_dx = static_cast<float>(point.x() - center_screen.x);
            const float mouse_dy = static_cast<float>(point.y() - center_screen.y);
            const float mouse_radius_px = std::sqrt(mouse_dx * mouse_dx + mouse_dy * mouse_dy);
            // The white circle represents a free screen-plane move handle.
            // Treat its complete interior as the handle so the three axes
            // meeting at the center cannot steal the interaction.
            if (ring_radius_px > 1.0f && mouse_radius_px <= ring_radius_px + 8.0f) {
                return TransformAxis::ScreenPlane;
            }
        }
    } else if (operation == TransformOperation::Scale) {
        const float cube_half_size = 0.075f * gizmo_size;
        DomPoint cube_edge{};
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);
        if (renderer_.WorldToScreen(center + right * cube_half_size, camera_, orthographic_projection_, width(), height(), cube_edge)) {
            const float cube_dx = static_cast<float>(cube_edge.x - center_screen.x);
            const float cube_dy = static_cast<float>(cube_edge.y - center_screen.y);
            const float cube_radius_px = std::max(8.0f, std::sqrt(cube_dx * cube_dx + cube_dy * cube_dy) + 5.0f);
            const float mouse_dx = static_cast<float>(point.x() - center_screen.x);
            const float mouse_dy = static_cast<float>(point.y() - center_screen.y);
            if (std::sqrt(mouse_dx * mouse_dx + mouse_dy * mouse_dy) <= cube_radius_px) {
                return TransformAxis::UniformScale;
            }
        }
    } else if (operation == TransformOperation::Rotate) {
        Vec3 forward{};
        Vec3 right{};
        Vec3 up{};
        viewport_camera_basis(camera_, forward, right, up);

        TransformAxis best_arc_axis = TransformAxis::None;
        float best_arc_distance = 18.0f;
        constexpr int kSegments = 72;
        const float arc_radius =
            gizmo_size * TransformGizmoGeometry::kRotationArcRadiusScale;
        const TransformAxis axes[] = {TransformAxis::X, TransformAxis::Y, TransformAxis::Z};
        const DomPoint mouse_point{point.x(), point.y()};
        for (TransformAxis axis : axes) {
            const Vec3 direction = AxisVector(axis);
            const Vec3 arc_center = center + direction * axis_distance;
            Vec3 tangent{};
            Vec3 bitangent{};
            rotation_arc_basis(direction, forward, tangent, bitangent);

            DomPoint previous{};
            bool has_previous = false;
            for (int i = 0; i <= kSegments; ++i) {
                const float angle = TransformGizmoGeometry::kRotationArcStart
                    + TransformGizmoGeometry::kRotationArcSweep
                        * static_cast<float>(i) / static_cast<float>(kSegments);
                const Vec3 world_point = arc_center + tangent * (std::cos(angle) * arc_radius) + bitangent * (std::sin(angle) * arc_radius);
                DomPoint screen_point{};
                if (!renderer_.WorldToScreen(world_point, camera_, orthographic_projection_, width(), height(), screen_point)) {
                    has_previous = false;
                    continue;
                }

                if (has_previous) {
                    const float distance = DistanceToScreenSegment(mouse_point, previous, screen_point);
                    if (distance < best_arc_distance) {
                        best_arc_distance = distance;
                        best_arc_axis = axis;
                    }
                }
                previous = screen_point;
                has_previous = true;
            }
        }

        return best_arc_axis;
    }

    TransformAxis best_axis = TransformAxis::None;
    float best_distance = 12.0f;
    const TransformAxis axes[] = {TransformAxis::X, TransformAxis::Y, TransformAxis::Z};
    for (TransformAxis axis : axes) {
        DomPoint axis_end{};
        if (!renderer_.WorldToScreen(center + AxisVector(axis) * axis_distance, camera_, orthographic_projection_, width(), height(), axis_end)) {
            continue;
        }

        const DomPoint mouse_point{point.x(), point.y()};
        const float distance = DistanceToScreenSegment(mouse_point, center_screen, axis_end);
        if (distance < best_distance) {
            best_distance = distance;
            best_axis = axis;
        }
    }

    return best_axis;
}

bool OpenGLViewport::HitTestSelectedPolylineHandle(const QPoint& point, size_t* point_index) const {
    if (!document_) {
        return false;
    }

    const CPolyline* polyline = document_->GetSelectedPolyline();
    const CBSpline* spline = document_->GetSelectedBSpline();
    const std::vector<CPoint3d>* points = nullptr;
    if (polyline) {
        points = &polyline->GetPoints();
    } else if (spline) {
        points = &spline->GetPoints();
    }
    if (!points) {
        return false;
    }

    const DomPoint mouse_point{point.x(), point.y()};
    bool found = false;
    size_t best_index = 0;
    float best_distance = 10.0f;
    for (size_t i = 0; i < points->size(); ++i) {
        const CPoint3d& curve_point = (*points)[i];
        const Vec3 world{
            static_cast<float>(curve_point.x),
            static_cast<float>(curve_point.y),
            static_cast<float>(curve_point.z)
        };

        DomPoint screen{};
        if (!renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen)) {
            continue;
        }

        const float dx = static_cast<float>(mouse_point.x - screen.x);
        const float dy = static_cast<float>(mouse_point.y - screen.y);
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance <= best_distance) {
            best_distance = distance;
            best_index = i;
            found = true;
        }
    }

    if (found && point_index) {
        *point_index = best_index;
    }
    return found;
}

bool OpenGLViewport::HitTestSelectedSketchHandle(
    const QPoint& point,
    SketchHandleKind& kind,
    size_t& index) const {
    kind = SketchHandleKind::None;
    index = 0;
    if (!document_) {
        return false;
    }

    const CSmartLine* sketch = EditableSketch();
    if (!sketch) {
        return false;
    }

    const DomPoint mouse{point.x(), point.y()};
    float best_distance = 12.0f;
    auto consider = [&](CPoint3d world_point, SketchHandleKind candidate_kind, size_t candidate_index) {
        DomPoint screen{};
        if (!renderer_.WorldToScreen(
                point_to_vec3(world_point),
                camera_,
                orthographic_projection_,
                width(),
                height(),
                screen)) {
            return;
        }
        const float dx = static_cast<float>(mouse.x - screen.x);
        const float dy = static_cast<float>(mouse.y - screen.y);
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance <= best_distance) {
            best_distance = distance;
            kind = candidate_kind;
            index = candidate_index;
        }
    };

    if(sketch->GetPrimitive().kind) {
        for(size_t i=0;i<primitive_grip_count(*sketch);++i) consider(primitive_grip(*sketch,i),SketchHandleKind::Primitive,i);
        return kind!=SketchHandleKind::None;
    }
    for (size_t node_index = 0; node_index < sketch->GetNodeCount(); ++node_index) {
        consider(sketch->GetNodeWorld(node_index), SketchHandleKind::Node, node_index);
    }
    for (size_t fillet_index = 0; fillet_index < sketch->GetNumFillets(); ++fillet_index) {
        const CFillet* fillet = sketch->GetFillet(fillet_index);
        if (!fillet) {
            continue;
        }
        const CLinkLine* first = sketch->GetLine(fillet->GetFirstLineIndex());
        const CLinkLine* second = sketch->GetLine(fillet->GetSecondLineIndex());
        if (first && second && fillet->Calculate(*first, *second).valid) {
            consider(sketch->GetFilletGripWorld(fillet_index), SketchHandleKind::Fillet, fillet_index);
        }
    }
    for (size_t control_index = 0;
         control_index < sketch->GetBezierControlPointCount();
         ++control_index) {
        consider(
            sketch->GetBezierControlPointWorld(control_index),
            SketchHandleKind::BezierControl,
            control_index);
    }
    for (size_t grip_index = 0;
         grip_index < sketch->GetArcGripCount();
         ++grip_index) {
        consider(
            sketch->GetArcGripWorld(grip_index),
            SketchHandleKind::ArcControl,
            grip_index);
    }
    return kind != SketchHandleKind::None;
}

bool OpenGLViewport::ApplyCurvePointCoordinates(CPoint3d point) {
    if(!document_ || !IsCurveNodeEditing() || dragging_polyline_point_
        || !std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))return false;
    CPoint3d previous;if(!document_->GetSelectedPointPosition(previous))return false;
    if(point.x==previous.x && point.y==previous.y && point.z==previous.z)return true;
    CaptureCurvePointChangeBefore();
    if(!document_->MoveSelectedPoint(point))return false;
    FinalizeCurvePointChange();NotifyDocumentChanged();emit SelectionChanged();update();return true;
}

void OpenGLViewport::BeginCurvePointDrag(const CPoint3d& point) {
    dragging_polyline_point_ = true;
    curve_point_drag_changed_ = false;
    CaptureCurvePointChangeBefore();
    polyline_drag_plane_y_ = point.y;
    curve_point_drag_anchor_ = point_to_vec3(point);
    curve_point_drag_last_ = point;
    if (xy_plane_view_enabled_) {
        curve_point_drag_plane_point_ = {0.0f, 0.0f, 0.0f};
        curve_point_drag_plane_normal_ = {0.0f, 0.0f, 1.0f};
        curve_point_drag_has_plane_ = true;
        curve_point_drag_last_.z = 0.0;
        return;
    }
    // A free 3D curve point follows the cursor in the view plane passing
    // through the point grabbed by the user.  Lock the plane at mouse-down so
    // Polyline and B-Spline handles behave identically and retain their depth.
    Vec3 forward{};
    Vec3 right{};
    Vec3 up{};
    viewport_camera_basis(camera_, forward, right, up);
    curve_point_drag_plane_point_ = curve_point_drag_anchor_;
    curve_point_drag_plane_normal_ = normalize(forward);
    curve_point_drag_has_plane_ = dot(curve_point_drag_plane_normal_,
                                      curve_point_drag_plane_normal_) > 0.000001f;
}

void OpenGLViewport::CaptureCurvePointChangeBefore() {
    curve_point_drag_change_pending_ = false;
    curve_point_drag_object_id_ = 0;
    curve_point_drag_before_points_.clear();
    curve_point_drag_after_points_.clear();
    if (document_) {
        if (const CAlfaObject* selected = document_->GetSelectedObject()) {
            curve_point_drag_object_id_ = selected->m_id;
            if (const auto* polyline = dynamic_cast<const CPolyline*>(selected)) {
                curve_point_drag_before_points_ = polyline->GetPoints();
            } else if (const auto* spline = dynamic_cast<const CBSpline*>(selected)) {
                curve_point_drag_before_points_ = spline->GetPoints();
            }
        }
    }
}

void OpenGLViewport::FinalizeCurvePointChange() {
    if (!document_ || curve_point_drag_object_id_ == 0) return;
    const CAlfaObject* object = document_->FindObjectById(
        curve_point_drag_object_id_);
    if (const auto* polyline = dynamic_cast<const CPolyline*>(object)) {
        curve_point_drag_after_points_ = polyline->GetPoints();
    } else if (const auto* spline = dynamic_cast<const CBSpline*>(object)) {
        curve_point_drag_after_points_ = spline->GetPoints();
    }
    curve_point_drag_change_pending_ =
        !curve_point_drag_before_points_.empty()
        && curve_point_drag_before_points_.size()
            == curve_point_drag_after_points_.size();
}

bool OpenGLViewport::TakeCurvePointDragChange(
    unsigned long& object_id,
    std::vector<CPoint3d>& before,
    std::vector<CPoint3d>& after) {
    if (!curve_point_drag_change_pending_) return false;
    object_id = curve_point_drag_object_id_;
    before = std::move(curve_point_drag_before_points_);
    after = std::move(curve_point_drag_after_points_);
    curve_point_drag_change_pending_ = false;
    curve_point_drag_object_id_ = 0;
    return object_id != 0 && !before.empty() && before.size() == after.size();
}

bool OpenGLViewport::TakeObjectMoveChange(
    std::vector<unsigned long>& object_ids,
    Vec3& delta) {
    if (!object_move_change_pending_) return false;
    object_ids = std::move(object_move_change_ids_);
    delta = object_move_change_delta_;
    object_move_change_pending_ = false;
    object_move_change_ids_.clear();
    object_move_change_delta_ = {};
    return !object_ids.empty();
}

bool OpenGLViewport::TakeMaterialDropChange(
    unsigned long& object_id,
    Material& before_material,
    unsigned long& before_material_id,
    Material& after_material,
    unsigned long& after_material_id) {
    if (!material_drop_change_pending_) return false;
    object_id = material_drop_object_id_;
    before_material = material_drop_before_;
    before_material_id = material_drop_before_id_;
    after_material = material_drop_after_;
    after_material_id = material_drop_after_id_;
    material_drop_change_pending_ = false;
    material_drop_object_id_ = 0;
    return object_id != 0;
}

bool OpenGLViewport::CurrentSelectedCurvePlane(Vec3& plane_point, Vec3& plane_normal) const {
    if (!document_) {
        return false;
    }

    if (const CPolyline* polyline = document_->GetSelectedPolyline()) {
        if (polyline->GetLockedPlane(plane_point, plane_normal)) {
            return true;
        }
        return plane_from_points(polyline->GetPoints(), plane_point, plane_normal);
    }
    if (const CBSpline* spline = document_->GetSelectedBSpline()) {
        return plane_from_points(spline->GetPoints(), plane_point, plane_normal);
    }
    return false;
}

void OpenGLViewport::HandleSketchRectangleClick(const QPoint& point) {
    if (document_ && sketch_active_ && sketch_shape_kind_ != 0) {
        HandleSketchShapeClick(point);
        return;
    }
    if (!document_ || !sketch_active_) {
        if (!document_ || !sketch_waiting_for_face_) {
            return;
        }

        const DomPoint screen_point{point.x(), point.y()};
        auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
            Vec3 forward{};
            Vec3 right{};
            Vec3 up{};
            viewport_camera_basis(camera_, forward, right, up);
            depth = dot(world - camera_position(camera_, orthographic_projection_), forward);
            return depth > 0.0f
                && renderer_.WorldToScreen(
                    world,
                    camera_,
                    orthographic_projection_,
                    width(),
                    height(),
                    screen);
        };
        if (!document_->SelectSolidPlanarFaceAtScreen(screen_point, project_world)) {
            emit StatusTextChanged(
                QString("%1: planar body face not found").arg(sketch_name_));
            update();
            return;
        }

        Vec3 origin{};
        Vec3 x_axis{};
        Vec3 y_axis{};
        Vec3 normal{};
        unsigned long body_id = 0;
        int face_index = -1;
        if (!document_->GetSelectedSolidFaceSketchPlane(
                origin,
                x_axis,
                y_axis,
                normal,
                body_id,
                face_index)) {
            emit StatusTextChanged(
                QString("%1: selected face is not planar").arg(sketch_name_));
            update();
            return;
        }

        sketch_waiting_for_face_ = false;
        BeginSketchOnFace(
            sketch_name_,
            origin,
            x_axis,
            y_axis,
            normal,
            body_id,
            face_index);
        emit SketchFaceSelectionFinished(true);
        emit SelectionChanged();
        update();
        return;
    }

    CPoint3d sketch_point{};
    if (!ScreenToSketchPlane(point, sketch_point)) {
        emit StatusTextChanged("Sketch Rectangle: point is outside sketch plane");
        return;
    }
    SetCreationSnapCursor(SnapCreationPoint(point, sketch_point, true));

    if (!sketch_rectangle_has_first_point_) {
        sketch_rectangle_first_point_ = sketch_point;
        sketch_rectangle_preview_point_ = sketch_point;
        sketch_rectangle_has_first_point_ = true;
        sketch_rectangle_preview_valid_ = true;
        emit StatusTextChanged("Sketch Rectangle: click opposite corner");
        update();
        return;
    }

    const std::vector<CPoint3d> points = SketchRectanglePoints(sketch_rectangle_first_point_, sketch_point);
    const bool created = CreateSessionPolyline(points, true);
    if (!created) {
        if (!multi_sketch_session_) emit StatusTextChanged("Sketch Rectangle: cannot create contour");
        return;
    }
    ApplyPendingSketchAttachment();
    sketch_rectangle_has_first_point_ = false;
    sketch_rectangle_preview_valid_ = false;
    NotifyDocumentChanged();
    emit SelectionChanged();
    emit StatusTextChanged(QString("%1: Rectangle created").arg(sketch_name_));
    update();
}

void OpenGLViewport::HandleSketchPolylineClick(const QPoint& point) {
    if (!document_ || !sketch_active_) {
        return;
    }

    if (IsNearSketchPolylineFirstPoint(point)) {
        CommitSketchPolyline(true);
        return;
    }

    CPoint3d sketch_point{};
    if (!ScreenToSketchPlane(point, sketch_point)) {
        emit StatusTextChanged("Sketch Polyline: point is outside sketch plane");
        return;
    }
    const bool snapped = SnapCreationPoint(point, sketch_point, true);
    SetCreationSnapCursor(snapped);
    if (!sketch_polyline_points_.empty()) {
        if (!snapped) {
            sketch_point = AlignSketchPolylinePoint(sketch_point);
        }
        const CPoint3d& last = sketch_polyline_points_.back();
        const double dx = sketch_point.x - last.x;
        const double dy = sketch_point.y - last.y;
        const double dz = sketch_point.z - last.z;
        if (std::sqrt(dx * dx + dy * dy + dz * dz) <= 1.0e-8) {
            return;
        }
    }

    sketch_polyline_points_.push_back(sketch_point);
    sketch_polyline_preview_point_ = sketch_point;
    sketch_polyline_preview_valid_ = true;
    emit StatusTextChanged(sketch_polyline_points_.size() == 1
        ? QString("%1: click next point").arg(sketch_name_)
        : QString("%1: click next point, first point closes, Enter finishes").arg(sketch_name_));
    update();
}

CPoint3d OpenGLViewport::AlignSketchPolylinePoint(const CPoint3d& point) const {
    if (sketch_polyline_points_.empty()) {
        return point;
    }

    const Vec3 last = point_to_vec3(sketch_polyline_points_.back());
    const Vec3 candidate = point_to_vec3(point);
    const Vec3 delta = candidate - last;
    float u_distance = dot(delta, sketch_u_);
    float v_distance = dot(delta, sketch_v_);
    const double angle = std::atan2(std::abs(v_distance), std::abs(u_distance))
        * 180.0 / 3.14159265358979323846;
    if (angle <= sketch_alignment_angle_degrees_) {
        v_distance = 0.0f;
    } else if (90.0 - angle <= sketch_alignment_angle_degrees_) {
        u_distance = 0.0f;
    }

    const Vec3 aligned = last + sketch_u_ * u_distance + sketch_v_ * v_distance;
    return CPoint3d(aligned.x, aligned.y, aligned.z);
}

bool OpenGLViewport::IsNearSketchPolylineFirstPoint(const QPoint& point) const {
    if (sketch_polyline_points_.size() < 3) {
        return false;
    }
    DomPoint first_screen{};
    if (!renderer_.WorldToScreen(
            point_to_vec3(sketch_polyline_points_.front()),
            camera_,
            orthographic_projection_,
            width(),
            height(),
            first_screen)) {
        return false;
    }
    const float dx = static_cast<float>(point.x() - first_screen.x);
    const float dy = static_cast<float>(point.y() - first_screen.y);
    return IsSnapTargetEnabled(SnapTarget::Knot)
        && std::sqrt(dx * dx + dy * dy) <= static_cast<float>(capture_distance_pixels_);
}

bool OpenGLViewport::IsNearSelectedSketchFirstPoint(const QPoint& point) const {
    const CSmartLine* sketch = document_ ? EditableSketch() : nullptr;
    if (!sketch || sketch->IsClosed() || sketch->GetNumLines() == 0) {
        return false;
    }
    DomPoint first_screen{};
    if (!renderer_.WorldToScreen(
            point_to_vec3(sketch->GetNodeWorld(0)),
            camera_,
            orthographic_projection_,
            width(),
            height(),
            first_screen)) {
        return false;
    }
    const float dx = static_cast<float>(point.x() - first_screen.x);
    const float dy = static_cast<float>(point.y() - first_screen.y);
    return IsSnapTargetEnabled(SnapTarget::Knot)
        && std::sqrt(dx * dx + dy * dy) <= static_cast<float>(capture_distance_pixels_);
}

bool OpenGLViewport::CommitSketchPolyline(bool closed) {
    const std::size_t minimum_points = closed ? 3 : 2;
    if (!document_ || sketch_polyline_points_.size() < minimum_points) {
        return false;
    }

    const bool created = CreateSessionPolyline(sketch_polyline_points_, closed);
    if (!created) {
        if (!multi_sketch_session_) emit StatusTextChanged("Sketch Polyline: cannot create contour");
        return false;
    }
    ApplyPendingSketchAttachment();

    sketch_polyline_points_.clear();
    sketch_polyline_preview_valid_ = false;
    NotifyDocumentChanged();
    emit SelectionChanged();
    emit StatusTextChanged(closed
        ? QString("%1: closed polyline created").arg(sketch_name_)
        : QString("%1: open polyline created").arg(sketch_name_));
    update();
    return true;
}

void OpenGLViewport::HandleSketchBezierClick(const QPoint& point) {
    if (!document_ || !sketch_active_) {
        return;
    }
    CPoint3d sketch_point{};
    if (!ScreenToSketchPlane(point, sketch_point)) {
        emit StatusTextChanged("Sketch Bezier: point is outside sketch plane");
        return;
    }
    bool snapped = false;
    if (sketch_bezier_points_.size() == 3
        && IsNearSelectedSketchFirstPoint(point)) {
        sketch_point = EditableSketch()->GetNodeWorld(0);
        snapped = true;
    } else {
        snapped = SnapCreationPoint(point, sketch_point, true);
    }
    SetCreationSnapCursor(snapped);
    sketch_bezier_points_.push_back(sketch_point);
    sketch_bezier_preview_point_ = sketch_point;
    sketch_bezier_preview_valid_ = true;
    if (sketch_bezier_points_.size() == 4) {
        CommitSketchBezier();
        return;
    }
    static const char* prompts[] = {
        "click first control point",
        "click second control point",
        "click end point"
    };
    emit StatusTextChanged(
        QString("%1: Bezier — %2")
            .arg(sketch_name_)
            .arg(prompts[sketch_bezier_points_.size() - 1]));
    update();
}

bool OpenGLViewport::CommitSketchBezier() {
    if (!document_ || sketch_bezier_points_.size() != 4) {
        return false;
    }

    bool created = false;
    bool closed = false;
    CSmartLine* selected_sketch = EditableSketch();
    if (multi_sketch_session_) {
        CSmartLine contour;
        const Vec3 normal = cross(sketch_u_, sketch_v_);
        contour.SetCoordinateSystem(CPoint3d(sketch_origin_.x, sketch_origin_.y, sketch_origin_.z),
            CPoint3d(sketch_u_.x, sketch_u_.y, sketch_u_.z), CPoint3d(normal.x, normal.y, normal.z));
        if (const auto* parent = dynamic_cast<CSketch*>(document_->FindObjectById(multi_sketch_id_))) {
            const auto& plane = parent->GetCoordinateSystem();
            contour.SetCoordinateSystem(plane.origin, plane.x_axis, plane.normal);
        }
        if (!contour.AddBezierWorld(sketch_bezier_points_[0], sketch_bezier_points_[1],
            sketch_bezier_points_[2], sketch_bezier_points_[3])) {
            emit StatusTextChanged("Sketch Bezier: cannot create curve");
            return false;
        }
        created = StoreSketchContour(contour);
    } else if (selected_sketch && !selected_sketch->IsClosed()) {
        const CPoint3d first_point = selected_sketch->GetNodeWorld(0);
        const CPoint3d& end_point = sketch_bezier_points_[3];
        const double dx = end_point.x - first_point.x;
        const double dy = end_point.y - first_point.y;
        const double dz = end_point.z - first_point.z;
        const bool close_requested =
            selected_sketch->GetNumLines() > 0
            && dx * dx + dy * dy + dz * dz <= 1.0e-12;
        created = selected_sketch->AddBezierWorld(
            sketch_bezier_points_[0],
            sketch_bezier_points_[1],
            sketch_bezier_points_[2],
            sketch_bezier_points_[3],
            selected_sketch->GetNumLines() > 0);
        if (created && close_requested) {
            closed = selected_sketch->SetClosed(true);
            created = closed;
        }
    } else {
        created = document_->CreateSketchBezier(
            sketch_bezier_points_,
            sketch_name_.toStdString(),
            CPoint3d(sketch_origin_.x, sketch_origin_.y, sketch_origin_.z),
            CPoint3d(sketch_u_.x, sketch_u_.y, sketch_u_.z),
            CPoint3d(sketch_v_.x, sketch_v_.y, sketch_v_.z));
    }
    if (!created) {
        if (!multi_sketch_session_) emit StatusTextChanged("Sketch Bezier: cannot create curve");
        return false;
    }
    ApplyPendingSketchAttachment();

    sketch_bezier_points_.clear();
    sketch_bezier_preview_valid_ = false;
    NotifyDocumentChanged();
    emit SelectionChanged();
    emit StatusTextChanged(closed
        ? QString("%1: Bezier contour closed").arg(sketch_name_)
        : QString("%1: Bezier created; click next start point").arg(sketch_name_));
    update();
    return true;
}

void OpenGLViewport::ApplyPendingSketchAttachment() {
    if (!document_ || sketch_attachment_body_id_ == 0
        || sketch_attachment_face_index_ < 0) {
        return;
    }
    if (CSmartLine* sketch = EditableSketch()) {
        sketch->SetFaceAttachment(
            sketch_attachment_body_id_, sketch_attachment_face_index_);
    }
    if (auto* sketch = dynamic_cast<CSketch*>(document_->GetSelectedObject()))
        sketch->SetFaceAttachment(sketch_attachment_body_id_, sketch_attachment_face_index_);
}

bool OpenGLViewport::CreateSessionPolyline(const std::vector<CPoint3d>& points, bool closed) {
    CPoint3d origin(sketch_origin_.x, sketch_origin_.y, sketch_origin_.z);
    CPoint3d u(sketch_u_.x, sketch_u_.y, sketch_u_.z), v(sketch_v_.x, sketch_v_.y, sketch_v_.z);
    if (!multi_sketch_session_)
        return document_->CreateSketchPolyline(points, closed, sketch_name_.toStdString(), origin, u, v);
    if (const auto* parent = dynamic_cast<CSketch*>(document_->FindObjectById(multi_sketch_id_))) {
        const auto& plane = parent->GetCoordinateSystem();
        origin = plane.origin; u = plane.x_axis; v = plane.y_axis;
    }
    CSmartLine contour;
    if (!contour.CreateFromWorldPoints(points, closed, origin, u, v)) {
        emit StatusTextChanged("Sketch: cannot create contour from these points.");
        return false;
    }
    return StoreSketchContour(contour);
}

void OpenGLViewport::SetSketchShapeTool(int kind, int sides) {
    if (!sketch_active_ || kind < 1 || kind > 3 || sides < 3 || sides > 128) return;
    SetSketchRectangleTool();
    if (tool_ != ToolMode::SketchRectangle) return;
    sketch_shape_kind_ = kind;
    sketch_shape_circle_=QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier);
    sketch_polygon_sides_ = sides;
    emit StatusTextChanged(kind == 1 ? "Circle: click center, then radius."
        : kind == 2 ? DomTranslate("Ellipse / Circle: click center, then frame corner. Hold Shift for a circle.")
                    : "Polygon: click center, then a vertex.");
    setFocus();
}

std::unique_ptr<CSmartLine> OpenGLViewport::BuildSketchShape(CPoint3d cursor) const {
    if (sketch_shape_points_.empty()) return {};
    auto shape = std::make_unique<CSmartLine>();
    const Vec3 normal = cross(sketch_u_, sketch_v_);
    shape->SetCoordinateSystem({sketch_origin_.x,sketch_origin_.y,sketch_origin_.z},
        {sketch_u_.x,sketch_u_.y,sketch_u_.z},{normal.x,normal.y,normal.z});
    if (const auto* parent = document_ ? dynamic_cast<const CSketch*>(document_->FindObjectById(multi_sketch_id_)) : nullptr) {
        const auto& frame = parent->GetCoordinateSystem();
        shape->SetCoordinateSystem(frame.origin,frame.x_axis,frame.normal);
    }
    const auto center = shape->WorldToLocal(sketch_shape_points_.front());
    const auto corner=shape->WorldToLocal(cursor);
    const double dx=corner.x-center.x,dy=corner.y-center.y;
    SketchPrimitive primitive;
    primitive.kind=sketch_shape_kind_; primitive.u=center.x; primitive.v=center.y;
    primitive.sides=sketch_polygon_sides_;
    if(sketch_shape_kind_==2) {
        primitive.radius=std::abs(dx); primitive.minor_radius=std::abs(dy);
        if(sketch_shape_circle_) {
            primitive.kind=1;
            primitive.radius=std::max(primitive.radius,primitive.minor_radius);
            primitive.minor_radius=primitive.radius;
        }
    } else {
        primitive.radius=std::hypot(dx,dy); primitive.minor_radius=primitive.radius;
        primitive.angle=std::atan2(dy,dx);
    }
    if (!shape->SetPrimitive(primitive)) return {};
    return shape;
}

void OpenGLViewport::HandleSketchShapeClick(const QPoint& point) {
    CPoint3d world;
    if (!ScreenToSketchPlane(point,world)) return;
    SetCreationSnapCursor(SnapCreationPoint(point,world,true));
    if (!sketch_rectangle_has_first_point_) {
        sketch_shape_points_={world};
        sketch_rectangle_has_first_point_=true;
        sketch_rectangle_first_point_=world;
        sketch_rectangle_preview_point_=world;
        sketch_rectangle_preview_valid_=true;
        emit StatusTextChanged(sketch_shape_kind_ == 2 ? DomTranslate("Click frame corner. Hold Shift for a circle.")
            : sketch_shape_kind_ == 1 ? "Circle: click radius point." : "Polygon: click a vertex.");
        return;
    }
    auto shape=BuildSketchShape(world);
    if (!shape) { emit StatusTextChanged("Sketch: radius must be greater than zero."); return; }
    if (multi_sketch_session_) { if (!StoreSketchContour(*shape)) return; }
    else document_->AddObject(std::move(shape));
    ApplyPendingSketchAttachment();
    sketch_shape_points_.clear();
    sketch_rectangle_has_first_point_=false;
    sketch_rectangle_preview_valid_=false;
    NotifyDocumentChanged();
    emit SelectionChanged();
    emit StatusTextChanged("Sketch contour added. Click the next center, or Esc to finish.");
    update();
}

bool OpenGLViewport::StoreSketchContour(const CSmartLine& contour) {
    auto* sketch = dynamic_cast<CSketch*>(document_->FindObjectById(multi_sketch_id_));
    std::unique_ptr<CSketch> owned;
    if (!sketch) {
        if (multi_sketch_id_) {
            emit StatusTextChanged("Sketch: this sketch was removed. Reopen a sketch or start a new one.");
            return false;
        }
        owned = std::make_unique<CSketch>(sketch_name_.toStdString());
        const auto& plane = contour.GetCoordinateSystem();
        owned->SetCoordinateSystem(plane.origin, plane.x_axis, plane.normal);
        sketch = owned.get();
    }
    std::string error;
    if (!sketch->AddDrawnContour(contour, &error)) {
        emit StatusTextChanged(QString::fromStdString(error));
        return false;
    }
    if (owned) document_->AddObject(std::move(owned));
    multi_sketch_id_ = sketch->m_id;
    document_->SelectObjectById(multi_sketch_id_);
    return true;
}

CSmartLine* OpenGLViewport::EditableSketch() const {
    const auto* parent = document_ ? dynamic_cast<const CSketch*>(document_->GetSelectedObject()) : nullptr;
    if (!parent) {
        edit_contour_.reset(); edit_contour_source_ = nullptr;
        edit_sketch_parent_id_ = 0; edit_contour_id_ = 0;
        return document_ ? document_->GetSelectedSketch() : nullptr;
    }
    if (edit_sketch_parent_id_ != parent->m_id) {
        edit_contour_id_ = 0; edit_contour_source_ = nullptr;
        edit_sketch_parent_id_ = parent->m_id;
    }
    if (!parent->GetContourCount()) return nullptr;
    size_t index = 0;
    for (size_t i = 0; i < parent->GetContourCount(); ++i)
        if (parent->GetContourId(i) == edit_contour_id_) { index = i; break; }
    const auto* source = &parent->GetLocalContour(index);
    const auto same_point = [](CPoint3d a, CPoint3d b) {
        return std::hypot(std::hypot(a.x-b.x,a.y-b.y),a.z-b.z) < 1e-10;
    };
    const auto& frame = parent->GetCoordinateSystem();
    const bool frame_changed = edit_contour_ &&
        (!same_point(frame.origin,edit_contour_->GetCoordinateSystem().origin)
         || !same_point(frame.x_axis,edit_contour_->GetCoordinateSystem().x_axis)
         || !same_point(frame.normal,edit_contour_->GetCoordinateSystem().normal));
    // Undo/Redo and contour replacement invalidate the working copy. Geometry
    // is always edited in the parent's world frame, never by mutating UV data.
    if (!edit_contour_ || edit_contour_source_ != source || frame_changed
        || edit_contour_id_ != parent->GetContourId(index)) {
        edit_contour_ = std::make_shared<CSmartLine>(parent->MakeWorldContour(index));
        edit_contour_->m_id = parent->m_id;
        edit_contour_source_ = source;
        edit_contour_id_ = parent->GetContourId(index);
        edit_contour_revisions_ = edit_contour_->GetRevisions();
    }
    return edit_contour_.get();
}

void OpenGLViewport::PickSketchContour(const QPoint& point) {
    if (dragging_sketch_handle_) return;
    const auto* parent = document_ ? dynamic_cast<const CSketch*>(document_->GetSelectedObject()) : nullptr;
    if (!parent) return;
    EditableSketch();
    double best = 16.0;
    auto picked = edit_contour_id_;
    const DomPoint mouse{point.x(), point.y()};
    for (size_t i = 0; i < parent->GetContourCount(); ++i) {
        const auto contour = parent->MakeWorldContour(i);
        const auto consider = [&](CPoint3d p) {
            DomPoint screen{};
            if (!renderer_.WorldToScreen(point_to_vec3(p),camera_,orthographic_projection_,width(),height(),screen)) return;
            const double distance = std::hypot(double(screen.x-point.x()),double(screen.y-point.y()));
            if (distance < best) { best = distance; picked = parent->GetContourId(i); }
        };
        if(contour.GetPrimitive().kind) {
            for(size_t n=0;n<primitive_grip_count(contour);++n) consider(primitive_grip(contour,n));
        } else {
        for (size_t n = 0; n < contour.GetNodeCount(); ++n) consider(contour.GetNodeWorld(n));
        for (size_t n = 0; n < contour.GetBezierControlPointCount(); ++n) consider(contour.GetBezierControlPointWorld(n));
        for (size_t n = 0; n < contour.GetArcGripCount(); ++n) consider(contour.GetArcGripWorld(n));
        for (size_t n = 0; n < contour.GetNumFillets(); ++n) consider(contour.GetFilletGripWorld(n));
        }
        for (size_t n = 0; n < contour.GetNumLines(); ++n) {
            const auto samples = contour.GetLine(n)->Sample(32);
            for (size_t k = 1; k < samples.size(); ++k) {
                DomPoint a{}, b{};
                if (!renderer_.WorldToScreen(point_to_vec3(contour.LocalToWorld(samples[k-1])),camera_,orthographic_projection_,width(),height(),a)
                    || !renderer_.WorldToScreen(point_to_vec3(contour.LocalToWorld(samples[k])),camera_,orthographic_projection_,width(),height(),b)) continue;
                const double distance = DistanceToScreenSegment(mouse,a,b);
                if (distance < best) { best = distance; picked = parent->GetContourId(i); }
            }
        }
    }
    if (picked != edit_contour_id_) {
        edit_contour_id_ = picked; edit_contour_source_ = nullptr;
        highlighted_sketch_handle_kind_ = SketchHandleKind::None;
        EditableSketch();
    }
}

void OpenGLViewport::NotifyDocumentChanged() {
    auto* parent = document_ ? dynamic_cast<CSketch*>(document_->GetSelectedObject()) : nullptr;
    if (parent && edit_contour_ && edit_sketch_parent_id_ == parent->m_id) {
        auto* contour = EditableSketch();
        if (contour && !contour->IsEditing()) {
            const auto revisions = contour->GetRevisions();
            if (revisions.geometry != edit_contour_revisions_.geometry
                || revisions.topology != edit_contour_revisions_.topology) {
                if (!parent->ReplaceLocalContour(edit_contour_id_, *contour)) return;
                for (size_t i = 0; i < parent->GetContourCount(); ++i)
                    if (parent->GetContourId(i) == edit_contour_id_) edit_contour_source_ = &parent->GetLocalContour(i);
                edit_contour_revisions_ = revisions;
            }
        }
        // Multi-contour edits use the document snapshot command, which retains
        // every contour, its IDs, constraints, and the dependent feature tree.
        sketch_before_.reset(); sketch_after_.reset();
    }
    emit DocumentChanged();
}

void OpenGLViewport::HandleSketchConvertLineToBezierClick(const QPoint& point) {
    PickSketchContour(point);
    CSmartLine* sketch = document_ ? EditableSketch() : nullptr;
    if (!sketch) {
        emit StatusTextChanged("Convert to Bezier: select a sketch first");
        return;
    }

    const DomPoint mouse{point.x(), point.y()};
    double best_distance = 12.0;
    std::size_t best_line = sketch->GetNumLines();
    for (std::size_t line_index = 0; line_index < sketch->GetNumLines(); ++line_index) {
        const CLinkLine* line = sketch->GetLine(line_index);
        if (!line || line->GetType() == LinkLineType::Bezier) {
            continue;
        }
        const CPoint3d start = sketch->LocalToWorld(line->GetStart());
        const CPoint3d end = sketch->LocalToWorld(line->GetEnd());
        DomPoint start_screen{};
        DomPoint end_screen{};
        if (!renderer_.WorldToScreen(
                point_to_vec3(start),
                camera_,
                orthographic_projection_,
                width(),
                height(),
                start_screen)
            || !renderer_.WorldToScreen(
                point_to_vec3(end),
                camera_,
                orthographic_projection_,
                width(),
                height(),
                end_screen)) {
            continue;
        }
        const double distance = DistanceToScreenSegment(mouse, start_screen, end_screen);
        if (distance < best_distance) {
            best_distance = distance;
            best_line = line_index;
        }
    }

    if (best_line >= sketch->GetNumLines() || !sketch->ConvertLineToBezier(best_line)) {
        emit StatusTextChanged("Convert to Bezier: click closer to a straight segment");
        return;
    }

    NotifyDocumentChanged();
    BeginEditSelectedSketch();
    emit StatusTextChanged(
        QString("%1: segment converted to Bezier; drag the blue control points")
            .arg(sketch_name_));
    update();
}

void OpenGLViewport::HandleSketchConstraintClick(const QPoint& point) {
    PickSketchContour(point);
    CSmartLine* sketch = document_ ? EditableSketch() : nullptr;
    if (!sketch) {
        emit StatusTextChanged("Geometry constraint: select a sketch first");
        return;
    }

    if (tool_ == ToolMode::SketchSmoothJoint || tool_ == ToolMode::SketchSharpJoint) {
        const bool smooth = tool_ == ToolMode::SketchSmoothJoint;
        double nearest = 12.0;
        std::size_t node = sketch->GetNodeCount();
        for (std::size_t i = 0; i < sketch->GetNodeCount(); ++i) {
            DomPoint screen{};
            if (!renderer_.WorldToScreen(point_to_vec3(sketch->GetNodeWorld(i)),
                    camera_, orthographic_projection_, width(), height(), screen)) continue;
            const double distance = std::hypot(double(screen.x - point.x()), double(screen.y - point.y()));
            if (distance < nearest) { nearest = distance; node = i; }
        }
        if (node == sketch->GetNodeCount()) {
            emit StatusTextChanged("Joint: click closer to the node joining two segments");
            return;
        }
        auto before = std::make_shared<CSmartLine>(sketch->MakeCopy());
        if (!sketch->SetNodeSmooth(node, smooth)) {
            emit StatusTextChanged(smooth
                ? "Smooth joint: choose a shared node with a Bezier segment and nonzero handles"
                : "Sharp joint: choose the shared node of two adjacent segments");
            return;
        }
        const auto before_revisions = before->GetRevisions();
        const auto after_revisions = sketch->GetRevisions();
        if (before_revisions.geometry != after_revisions.geometry
            || before_revisions.topology != after_revisions.topology) {
            sketch_change_id_ = sketch->m_id;
            sketch_before_ = std::move(before);
            sketch_after_ = std::make_shared<CSmartLine>(sketch->MakeCopy());
            NotifyDocumentChanged();
        }
        emit SelectionChanged();
        BeginEditSelectedSketch();
        emit StatusTextChanged(smooth
            ? "Smooth joint applied: both segments share a tangent"
            : "Sharp joint: smoothness removed; the handles are independent");
        update();
        return;
    }
    const DomPoint mouse{point.x(), point.y()};
    double best_distance = 12.0;
    std::size_t best_line = sketch->GetNumLines();
    for (std::size_t line_index = 0;
         line_index < sketch->GetNumLines(); ++line_index) {
        const CLinkLine* line = sketch->GetLine(line_index);
        if (!line || (line->GetType() != LinkLineType::Segment
            && line->GetType() != LinkLineType::Horizontal
            && line->GetType() != LinkLineType::Vertical)) {
            continue;
        }

        const int segment_count = 1;
        DomPoint previous{};
        if (!renderer_.WorldToScreen(
                point_to_vec3(sketch->LocalToWorld(line->GetPoint(0.0))),
                camera_, orthographic_projection_, width(), height(), previous)) {
            continue;
        }
        for (int segment = 1; segment <= segment_count; ++segment) {
            DomPoint current{};
            if (!renderer_.WorldToScreen(
                    point_to_vec3(sketch->LocalToWorld(line->GetPoint(
                        static_cast<double>(segment) / segment_count))),
                    camera_, orthographic_projection_, width(), height(), current)) {
                break;
            }
            const double distance = DistanceToScreenSegment(
                mouse, previous, current);
            if (distance < best_distance) {
                best_distance = distance;
                best_line = line_index;
            }
            previous = current;
        }
    }

    if (best_line >= sketch->GetNumLines()) {
        emit StatusTextChanged("Geometry constraint: click closer to a straight segment");
        return;
    }

    bool applied = false;
    QString name;
    if (tool_ == ToolMode::SketchConstraintHorizontal) {
        applied = sketch->ConstrainHorizontal(best_line);
        name = "Horizontal";
    } else {
        applied = sketch->ConstrainVertical(best_line);
        name = "Vertical";
    }
    if (!applied) {
        emit StatusTextChanged(
            QString("%1: the required adjacent segment is missing").arg(name));
        return;
    }

    NotifyDocumentChanged();
    emit SelectionChanged();
    BeginEditSelectedSketch();
    emit StatusTextChanged(QString("Geometry constraint added: %1").arg(name));
    update();
}

void OpenGLViewport::HandleSketchConvertLineToArcClick(const QPoint& point) {
    if (!sketch_arc_has_line_) PickSketchContour(point);
    CSmartLine* sketch = document_ ? EditableSketch() : nullptr;
    if (!sketch) {
        emit StatusTextChanged("Line to Arc: select a sketch first");
        return;
    }

    if (!sketch_arc_has_line_) {
        const DomPoint mouse{point.x(), point.y()};
        double best_distance = 12.0;
        std::size_t best_line = sketch->GetNumLines();
        for (std::size_t line_index = 0; line_index < sketch->GetNumLines(); ++line_index) {
            const CLinkLine* line = sketch->GetLine(line_index);
            if (!line || (line->GetType() != LinkLineType::Segment
                          && line->GetType() != LinkLineType::Horizontal
                          && line->GetType() != LinkLineType::Vertical)) {
                continue;
            }
            DomPoint start_screen{};
            DomPoint end_screen{};
            if (!renderer_.WorldToScreen(
                    point_to_vec3(sketch->LocalToWorld(line->GetStart())), camera_,
                    orthographic_projection_, width(), height(), start_screen)
                || !renderer_.WorldToScreen(
                    point_to_vec3(sketch->LocalToWorld(line->GetEnd())), camera_,
                    orthographic_projection_, width(), height(), end_screen)) {
                continue;
            }
            const double distance = DistanceToScreenSegment(mouse, start_screen, end_screen);
            if (distance < best_distance) {
                best_distance = distance;
                best_line = line_index;
            }
        }
        if (best_line >= sketch->GetNumLines()) {
            emit StatusTextChanged("Line to Arc: click closer to a straight segment");
            return;
        }
        sketch_arc_line_index_ = best_line;
        sketch_arc_has_line_ = true;
        emit StatusTextChanged("Line to Arc: click a point on the desired arc");
        return;
    }

    CPoint3d point_on_arc;
    const SketchCoordinateSystem& system = sketch->GetCoordinateSystem();
    if (!PickPointOnPlane(
            point, point_to_vec3(system.origin), point_to_vec3(system.normal), point_on_arc)
        || !sketch->ConvertLineToArc(sketch_arc_line_index_, point_on_arc)) {
        emit StatusTextChanged(
            "Line to Arc: point is too close to the line; choose a point farther away");
        return;
    }
    sketch_arc_has_line_ = false;
    NotifyDocumentChanged();
    BeginEditSelectedSketch();
    emit StatusTextChanged(QString("%1: circular arc created").arg(sketch_name_));
    update();
}

void OpenGLViewport::ConstrainPrimitiveBase(CPoint3d& point, Qt::KeyboardModifiers modifiers) const {
    if (tool_ != ToolMode::SolidBoxRectangle || !sketch_rectangle_has_first_point_
        || !modifiers.testFlag(Qt::ShiftModifier)) return;
    const Vec3 first = point_to_vec3(sketch_rectangle_first_point_);
    const Vec3 delta = point_to_vec3(point) - first;
    const float u = dot(delta, sketch_u_), v = dot(delta, sketch_v_);
    const float side = std::max(std::abs(u), std::abs(v));
    const Vec3 constrained = first + sketch_u_ * std::copysign(side, u)
        + sketch_v_ * std::copysign(side, v);
    point = CPoint3d(constrained.x, constrained.y, constrained.z);
}

bool OpenGLViewport::UpdatePrimitiveBase(const QPoint& point, Qt::KeyboardModifiers modifiers) {
    CPoint3d world;
    if (!ScreenToSketchPlane(point, world)) return false;
    SnapCreationPoint(point, world, true);
    ConstrainPrimitiveBase(world, modifiers);
    sketch_rectangle_preview_point_ = world;
    sketch_rectangle_preview_valid_ = true;
    return true;
}

void OpenGLViewport::UpdatePrimitiveHeight(const QPoint& point) {
    const double previous_height = primitive_height_;
    const Vec3 anchor = point_to_vec3(sketch_rectangle_preview_point_);
    const Vec3 normal = normalize(sketch_normal_);
    const Vec3 forward = rotate(camera_.orientation, {0, 0, -1});
    const Vec3 plane_normal = forward - normal * dot(forward, normal);
    CPoint3d hit;
    if (dot(plane_normal, plane_normal) > 0.01f
        && ScreenToWorldPlane(point, anchor, plane_normal, hit)) {
        primitive_height_ = dot(point_to_vec3(hit) - anchor, normal);
    } else {
        // Looking straight down the normal collapses the guide to a point.
        // Use a vertical screen gesture in that view, without moving the base.
        float span = 2 * std::max(kMinimumOrthographicHalfHeight, camera_.distance * 0.42f);
        if (!orthographic_projection_) {
            const float depth = std::max(0.001f, dot(anchor - camera_position(camera_), forward));
            span = 2 * depth * std::tan(deg_to_rad(camera_.vertical_fov_degrees) * 0.5f);
        }
        primitive_height_ = (primitive_height_start_.y() - point.y()) * span / std::max(1, height());
    }
    primitive_height_ = std::clamp(primitive_height_, -900.0, 900.0);
    if (std::abs(primitive_height_) < 0.001) {
        primitive_preview_solid_.reset();
    } else if (!primitive_preview_solid_ || std::abs(previous_height - primitive_height_) > 1.e-6) {
        const bool cylinder = tool_ == ToolMode::SolidCylinderCircle;
        auto parameters = cylinder
            ? SolidCylinderParametersFromCircle(sketch_rectangle_first_point_, sketch_rectangle_preview_point_)
            : SolidBoxParametersFromRectangle(sketch_rectangle_first_point_, sketch_rectangle_preview_point_);
        for (auto& parameter : parameters)
            if (parameter.id == (cylinder ? "height" : "depth")) parameter.value = primitive_height_;
        // Build the same BRep and display mesh as the finished primitive. Keep
        // it outside the document until confirmation, so Escape leaves no body
        // or undo operation behind and existing-object editing stays unchanged.
        auto solid = std::make_shared<CSolid>();
        solid->SetColor(cylinder ? SolidCylinderTool().GetColor() : SolidBoxTool().GetColor());
        const bool built = cylinder ? SolidCylinderTool().RebuildShape(*solid, parameters)
                                    : SolidBoxTool().RebuildShape(*solid, parameters);
        if (built && primitive_height_ < -0.001 && solid_box_target_body_id_ != 0) {
            // Preview the subtraction while dragging, before the final click
            // creates the document tool and opens its parameter editor.
            auto* body = document_ ? dynamic_cast<CSolid*>(
                document_->FindObjectById(solid_box_target_body_id_)) : nullptr;
            try {
                if (body && !body->m_Shape.IsNull()) {
                    BRepAlgoAPI_Cut cut(body->m_Shape, solid->m_Shape);
                    cut.Build();
                    if (cut.IsDone() && !cut.Shape().IsNull()) {
                        solid->m_Shape = cut.Shape();
                        solid->SetColor(body->GetColor());
                        solid->SetMaterial(body->GetMaterial());
                        solid->SetLineWidth(body->GetLineWidth());
                        solid->SetLineStyle(body->GetLineStyle());
                        if (!solid->BuildImportedRenderMesh(false)) solid.reset();
                    } else solid.reset();
                } else solid.reset();
            } catch (const Standard_Failure&) {
                solid.reset();
            }
        }
        primitive_preview_solid_ = built ? std::move(solid) : nullptr;
    }
    emit StatusTextChanged(QString("Height: %1 — click or Enter to finish").arg(primitive_height_, 0, 'f', 3));
}

void OpenGLViewport::FinishPrimitiveCreation() {
    if (!primitive_height_active_) return;
    if (std::abs(primitive_height_) < 0.001) {
        emit StatusTextChanged("Move cursor to set a non-zero height");
        return;
    }
    const bool cylinder = tool_ == ToolMode::SolidCylinderCircle;
    auto parameters = cylinder
        ? SolidCylinderParametersFromCircle(sketch_rectangle_first_point_, sketch_rectangle_preview_point_)
        : SolidBoxParametersFromRectangle(sketch_rectangle_first_point_, sketch_rectangle_preview_point_);
    for (auto& parameter : parameters) {
        if (parameter.id == (cylinder ? "height" : "depth")) parameter.value = primitive_height_;
    }
    SetTool(ToolMode::Select);
    if (cylinder) emit SolidCylinderCircleFinished(parameters);
    else emit SolidBoxRectangleFinished(parameters);
    update();
}

void OpenGLViewport::DrawPrimitiveHeightPreview() {
    std::vector<Vec3> base;
    const bool cylinder = tool_ == ToolMode::SolidCylinderCircle;
    const Vec3 first = point_to_vec3(sketch_rectangle_first_point_);
    const Vec3 anchor = point_to_vec3(sketch_rectangle_preview_point_);
    const Vec3 normal = normalize(sketch_normal_);
    if (cylinder) {
        const Vec3 delta = anchor - first;
        const float radius = std::hypot(dot(delta, sketch_u_), dot(delta, sketch_v_));
        for (int i = 0; i < 64; ++i) {
            const float angle = 2 * kPi * i / 64;
            base.push_back(first + sketch_u_ * (radius * std::cos(angle))
                + sketch_v_ * (radius * std::sin(angle)));
        }
    } else {
        for (const auto& point : SketchRectanglePoints(sketch_rectangle_first_point_, sketch_rectangle_preview_point_))
            base.push_back(point_to_vec3(point));
    }
    if (base.empty()) return;
    const Vec3 offset = normal * static_cast<float>(primitive_height_);
    const float guide_length = std::max(static_cast<float>(std::abs(primitive_height_) * 1.2),
        camera_.distance * 0.4f);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_LINE_STIPPLE);
    glDisable(GL_POLYGON_STIPPLE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (!primitive_preview_solid_) {
        glColor4f(0.65f, 0.2f, 0.5f, 0.12f);
        glBegin(GL_QUADS);
        for (size_t i = 0; i < base.size(); ++i) {
            const Vec3 a = base[i], b = base[(i + 1) % base.size()];
            for (Vec3 p : {a, b, b + offset, a + offset}) glVertex3f(p.x, p.y, p.z);
        }
        glEnd();
        glColor4f(1, 0.15f, 0.6f, 1);
        glLineWidth(2);
        glBegin(GL_LINE_LOOP);
        for (Vec3 p : base) { p = p + offset; glVertex3f(p.x, p.y, p.z); }
        glEnd();
        glBegin(GL_LINES);
        for (size_t i = 0; i < base.size(); i += cylinder ? 16 : 1) {
            const Vec3 a = base[i], b = a + offset;
            glVertex3f(a.x, a.y, a.z); glVertex3f(b.x, b.y, b.z);
        }
        glEnd();
    }
    glColor4f(0.2f, 0.25f, 1, 1);
    glBegin(GL_LINES);
    for (float sign : {-1.0f, 1.0f}) {
        const Vec3 p = anchor + normal * (sign * guide_length);
        glVertex3f(p.x, p.y, p.z);
    }
    glEnd();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const auto screen = [this](Vec3 p, QPointF& result) {
        DomPoint projected;
        if (!renderer_.WorldToScreen(p, camera_, orthographic_projection_, width(), height(), projected)) return false;
        result = QPointF(projected.x, projected.y);
        return true;
    };
    QPointF start, end;
    painter.setPen(QPen(QColor(255, 50, 145), 2));
    if (screen(anchor - normal * (guide_length * 0.8f), start)
        && screen(anchor + normal * (guide_length * 0.8f), end)) {
        QPointF direction = end - start;
        const double length = std::hypot(direction.x(), direction.y());
        if (length > 2) {
            direction /= length;
            const QPointF side(-direction.y(), direction.x());
            for (int sign : {-1, 1}) {
                const QPointF tip = sign > 0 ? end : start;
                const QPointF tail = tip - direction * (sign * 18);
                painter.drawLine(tip, tail + side * 6);
                painter.drawLine(tip, tail - side * 6);
            }
        }
    }
    painter.setPen(QColor(255, 210, 55));
    if (screen(first, start)) painter.drawText(start + QPointF(8, -10), cylinder ? "C" : "P1");
    if (screen(anchor, end)) painter.drawText(end + QPointF(8, -10), "P2");
    if (screen(anchor + offset, end)) painter.drawText(end + QPointF(12, 20), QString::number(primitive_height_, 'f', 2));
    painter.end();
    glPopAttrib();
}

void OpenGLViewport::HandleSolidBoxRectangleClick(const QPoint& point, Qt::KeyboardModifiers modifiers) {
    if (!document_) {
        return;
    }

    if (solid_box_waiting_for_face_) {
        const DomPoint screen_point{point.x(), point.y()};
        auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
            Vec3 forward{};
            Vec3 right{};
            Vec3 up{};
            viewport_camera_basis(camera_, forward, right, up);
            depth = dot(world - camera_position(camera_, orthographic_projection_), forward);
            return depth > 0.0f
                && renderer_.WorldToScreen(
                    world,
                    camera_,
                    orthographic_projection_,
                    width(),
                    height(),
                    screen);
        };
        if (!document_->SelectSolidPlanarFaceAtScreen(screen_point, project_world)) {
            emit StatusTextChanged("BOX: planar body face not found");
            update();
            return;
        }

        Vec3 x_axis{};
        Vec3 y_axis{};
        unsigned long body_id = 0;
        int face_index = -1;
        if (!document_->GetSelectedSolidFaceSketchPlane(
                sketch_origin_,
                x_axis,
                y_axis,
                sketch_normal_,
                body_id,
                face_index)) {
            emit StatusTextChanged("BOX: selected face is not planar");
            update();
            return;
        }

        sketch_u_ = x_axis;
        sketch_v_ = y_axis;
        solid_box_target_body_id_ = body_id;
        solid_box_waiting_for_face_ = false;
        reference_plane_pending_ = false;
        emit SelectionChanged();
        emit StatusTextChanged("BOX: click first rectangle corner on the selected face");
        ReprojectSolidPrimitiveAnchor();
        update();
        return;
    }

    CPoint3d sketch_point{};
    if (!ScreenToSketchPlane(point, sketch_point)) {
        emit StatusTextChanged("BOX: point is outside placement plane");
        return;
    }
    SetCreationSnapCursor(SnapCreationPoint(point, sketch_point, true));

    if (!sketch_rectangle_has_first_point_) {
        sketch_rectangle_first_point_ = sketch_point;
        sketch_rectangle_preview_point_ = sketch_point;
        sketch_rectangle_has_first_point_ = true;
        sketch_rectangle_preview_valid_ = true;
        emit StatusTextChanged("BOX: click opposite rectangle corner");
        update();
        return;
    }

    ConstrainPrimitiveBase(sketch_point, modifiers);
    sketch_rectangle_preview_point_ = sketch_point;
    sketch_rectangle_preview_valid_ = true;
    update();
}

void OpenGLViewport::HandleSolidCylinderCircleClick(const QPoint& point) {
    if (!document_) {
        return;
    }

    if (solid_box_waiting_for_face_) {
        const DomPoint screen_point{point.x(), point.y()};
        auto project_world = [this](Vec3 world, DomPoint& screen, float& depth) {
            Vec3 forward{};
            Vec3 right{};
            Vec3 up{};
            viewport_camera_basis(camera_, forward, right, up);
            depth = dot(world - camera_position(camera_, orthographic_projection_), forward);
            return depth > 0.0f
                && renderer_.WorldToScreen(
                    world, camera_, orthographic_projection_, width(), height(), screen);
        };
        if (!document_->SelectSolidPlanarFaceAtScreen(screen_point, project_world)) {
            emit StatusTextChanged("CYLINDER: planar body face not found");
            update();
            return;
        }

        Vec3 x_axis{};
        Vec3 y_axis{};
        unsigned long body_id = 0;
        int face_index = -1;
        if (!document_->GetSelectedSolidFaceSketchPlane(
                sketch_origin_, x_axis, y_axis, sketch_normal_, body_id, face_index)) {
            emit StatusTextChanged("CYLINDER: selected face is not planar");
            update();
            return;
        }
        sketch_u_ = x_axis;
        sketch_v_ = y_axis;
        solid_box_target_body_id_ = body_id;
        solid_box_waiting_for_face_ = false;
        reference_plane_pending_ = false;
        emit SelectionChanged();
        emit StatusTextChanged("CYLINDER: click circle center on the selected face");
        ReprojectSolidPrimitiveAnchor();
        update();
        return;
    }

    CPoint3d sketch_point{};
    if (!ScreenToSketchPlane(point, sketch_point)) {
        emit StatusTextChanged("CYLINDER: point is outside placement plane");
        return;
    }
    SetCreationSnapCursor(SnapCreationPoint(point, sketch_point, true));

    if (!sketch_rectangle_has_first_point_) {
        sketch_rectangle_first_point_ = sketch_point;
        sketch_rectangle_preview_point_ = sketch_point;
        sketch_rectangle_has_first_point_ = true;
        sketch_rectangle_preview_valid_ = true;
        emit StatusTextChanged("CYLINDER: click circle radius");
        update();
        return;
    }

    sketch_rectangle_preview_point_ = sketch_point;
    sketch_rectangle_preview_valid_ = true;
    update();
}

void OpenGLViewport::HandleSketchFilletClick(const QPoint& point) {
    if (!document_) {
        return;
    }
    if (dynamic_cast<CSketch*>(document_->GetSelectedObject())) {
        PickSketchContour(point);
        auto* contour = EditableSketch();
        if (!contour) return;
        size_t node = contour->GetNodeCount();
        double best = 40.0;
        for (size_t i = 0; i < contour->GetNodeCount(); ++i) {
            if (!contour->IsClosed() && (i == 0 || i+1 == contour->GetNodeCount())) continue;
            DomPoint screen{};
            if (!renderer_.WorldToScreen(point_to_vec3(contour->GetNodeWorld(i)),camera_,orthographic_projection_,width(),height(),screen)) continue;
            const double distance = std::hypot(double(screen.x-point.x()),double(screen.y-point.y()));
            if (distance < best) { best=distance; node=i; }
        }
        size_t incoming = contour->GetNumLines();
        if (node < contour->GetNodeCount()) {
            const auto position = contour->WorldToLocal(contour->GetNodeWorld(node));
            for (size_t i=0;i<contour->GetNumLines();++i) {
                const auto end = contour->GetLine(i)->GetEnd();
                if (std::hypot(end.x-position.x,end.y-position.y)<1e-8) { incoming=i; break; }
            }
        }
        if (incoming == contour->GetNumLines() || !contour->AddFillet(incoming, sketch_fillet_radius_)) {
            emit StatusTextChanged("Sketch: cannot round this corner with the selected radius.");
            return;
        }
        NotifyDocumentChanged();
        emit SelectionChanged();
        update();
        return;
    }

    const DomPoint screen_point{point.x(), point.y()};
    auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
        return renderer_.WorldToScreen(world, camera_, orthographic_projection_, width(), height(), screen);
    };

    size_t object_index = 0;
    size_t point_index = 0;
    if (!document_->FindPolylinePointAtScreen(screen_point, world_to_screen, 40.0f, object_index, point_index)) {
        highlighted_sketch_fillet_point_ = false;
        setCursor(Qt::CrossCursor);
        emit StatusTextChanged(QString("%1:operation failed; check the selected geometry and parameters").arg(sketch_name_));
        update();
        return;
    }

    if (!document_->ApplyFilletToPolylinePointAtScreen(screen_point, world_to_screen, 40.0f, sketch_fillet_radius_)) {
        emit StatusTextChanged(QString("%1: Fillet operation failed; check the selected geometry and parameters").arg(sketch_name_));
        update();
        return;
    }

    NotifyDocumentChanged();
    emit SelectionChanged();
    emit StatusTextChanged(
        QString("%1: Fillet R=%2 operation completed")
            .arg(sketch_name_)
            .arg(sketch_fillet_radius_, 0, 'f', 2));
    update();
}

bool OpenGLViewport::ScreenToSketchPlane(const QPoint& point, CPoint3d& result) const {
    const int viewport_width = std::max(1, width());
    const int viewport_height = std::max(1, height());
    const float ndc_x = 2.0f * static_cast<float>(point.x()) / static_cast<float>(viewport_width) - 1.0f;
    const float ndc_y = 1.0f - 2.0f * static_cast<float>(point.y()) / static_cast<float>(viewport_height);
    const float aspect = static_cast<float>(viewport_width) / static_cast<float>(viewport_height);

    Vec3 forward{};
    Vec3 right{};
    Vec3 up{};
    viewport_camera_basis(camera_, forward, right, up);

    Vec3 ray_origin = camera_position(camera_, orthographic_projection_);
    Vec3 ray_direction{};
    if (orthographic_projection_) {
        const float half_height = std::max(
            kMinimumOrthographicHalfHeight, camera_.distance * 0.42f);
        const float half_width = half_height * aspect;
        ray_origin = camera_position(camera_, orthographic_projection_) + right * (ndc_x * half_width) + up * (ndc_y * half_height);
        ray_direction = forward;
    } else {
        const float tan_half_fov = std::tan(
            deg_to_rad(camera_.vertical_fov_degrees) * 0.5f);
        ray_direction = normalize(forward + right * (ndc_x * aspect * tan_half_fov) + up * (ndc_y * tan_half_fov));
    }

    const float denominator = dot(ray_direction, sketch_normal_);
    if (std::fabs(denominator) <= 0.000001f) {
        return false;
    }

    const float t = dot(sketch_origin_ - ray_origin, sketch_normal_) / denominator;
    if (t <= 0.0f) {
        return false;
    }

    const Vec3 hit = ray_origin + ray_direction * t;
    result = CPoint3d(hit.x, hit.y, hit.z);
    return true;
}

bool OpenGLViewport::TakeSketchEditChange(unsigned long& id, std::shared_ptr<CSmartLine>& before, std::shared_ptr<CSmartLine>& after) {
    if (!sketch_before_ || !sketch_after_) return false;
    id=sketch_change_id_; before=std::move(sketch_before_); after=std::move(sketch_after_); return true;
}

bool OpenGLViewport::SnapCreationPoint(const QPoint& point,
                                       CPoint3d& result,
                                       bool require_sketch_plane) const {
    auxiliary_guide_visible_ = false;
    tangent_guide_visible_ = false;
    const bool measuring = tool_ == ToolMode::MeasurePointToPoint;
    // Dimension endpoints are spatial picks, independent of drawing-plane
    // restrictions. Always offer exact vertices without changing snap settings.
    const auto snap_enabled = [this, measuring](SnapTarget target) {
        if (measuring && target == SnapTarget::Knot) return true;
        if (measuring && target == SnapTarget::WorkPlane) return false;
        return IsSnapTargetEnabled(target);
    };
    const auto pick_object_id = measuring ? 0UL : point_pick_object_id_;
    if ((!snapping_enabled_ && !measuring) || !document_) {
        return false;
    }

    // CAD boundaries are line targets too. Resolve them before the surface
    // fallback, otherwise a near-boundary click always lands inside the face.
    if (snap_enabled(SnapTarget::Line) || snap_enabled(SnapTarget::Knot)) {
        Vec3 forward, right, up;
        viewport_camera_basis(camera_, forward, right, up);
        const Vec3 eye = camera_position(camera_, orthographic_projection_);
        const auto component = [&](const gp_Pnt& p, Vec3 axis) {
            return (p.X()-eye.x)*axis.x + (p.Y()-eye.y)*axis.y + (p.Z()-eye.z)*axis.z;
        };
        const auto screen_distance = [&](const gp_Pnt& p) {
            const double z = component(p, forward);
            if (z <= 0.0001) return std::numeric_limits<double>::infinity();
            const double half = orthographic_projection_
                ? std::max(kMinimumOrthographicHalfHeight, camera_.distance*0.42f)
                : z*std::tan(deg_to_rad(camera_.vertical_fov_degrees)*0.5f);
            const double scale = std::max(1, height())/(2*half);
            return std::hypot(width()*0.5 + component(p,right)*scale-point.x(),
                              height()*0.5 - component(p,up)*scale-point.y());
        };
        const auto visible = [&](const gp_Pnt& p) {
            const gp_Vec toward = orthographic_projection_
                ? gp_Vec(-forward.x,-forward.y,-forward.z)
                : gp_Vec(p,gp_Pnt(eye.x,eye.y,eye.z));
            const double reach = orthographic_projection_ ? component(p,forward) : toward.Magnitude();
            if (reach <= 0.0001) return false;
            for (const auto& item : document_->GetObjects()) {
                const auto* body = dynamic_cast<const CSolid*>(item.get());
                if (!body || body->m_Shape.IsNull() || !document_->IsObjectVisible(*body)) continue;
                IntCurvesFace_ShapeIntersector hit;
                hit.Load(body->m_Shape,1.e-7);
                hit.Perform(gp_Lin(p,gp_Dir(toward)),1.e-4,reach);
                if (hit.IsDone() && hit.NbPnt()>0) return false;
            }
            return true;
        };
        double best = capture_distance_pixels_;
        bool found_edge = false;
        const bool on_plane = require_sketch_plane || snap_enabled(SnapTarget::WorkPlane);
        // Exact visible vertices take priority over nearby points on edges
        // or faces. Otherwise a few pixels of click offset shorten dimensions.
        bool found_vertex = false;
        if (snap_enabled(SnapTarget::Knot)) {
            for (const auto& item : document_->GetObjects()) {
                const auto* body = dynamic_cast<const CSolid*>(item.get());
                if (!body || body->m_Shape.IsNull() || !document_->IsObjectVisible(*body)
                    || (pick_object_id && body->m_id != pick_object_id)) continue;
                for (TopExp_Explorer it(body->m_Shape, TopAbs_VERTEX); it.More(); it.Next()) {
                    const gp_Pnt vertex = BRep_Tool::Pnt(TopoDS::Vertex(it.Current()));
                    const double distance = screen_distance(vertex);
                    if (distance > best) continue;
                    const CPoint3d candidate(vertex.X(), vertex.Y(), vertex.Z());
                    if (on_plane && std::abs(dot(point_to_vec3(candidate)-sketch_origin_,sketch_normal_))>0.001f) continue;
                    if (!visible(vertex)) continue;
                    best = distance; result = candidate; found_vertex = true;
                }
            }
        }
        if (found_vertex) return true;
        for (const auto& item : document_->GetObjects()) {
            if (!snap_enabled(SnapTarget::Line)) break;
            const auto* body = dynamic_cast<const CSolid*>(item.get());
            if (!body || body->m_Shape.IsNull() || !document_->IsObjectVisible(*body)
                || (pick_object_id && body->m_id != pick_object_id)) continue;
            TopTools_IndexedMapOfShape edges;
            TopExp::MapShapes(body->m_Shape,TopAbs_EDGE,edges);
            for (int i=1; i<=edges.Extent(); ++i) try {
                const auto edge = TopoDS::Edge(edges(i));
                if (BRep_Tool::Degenerated(edge)) continue;
                BRepAdaptor_Curve curve(edge);
                const double first=curve.FirstParameter(), last=curve.LastParameter();
                if (!std::isfinite(first) || !std::isfinite(last) || last<=first) continue;
                // Locate projected local minima, then evaluate the exact CAD
                // curve, never a chord of the display tessellation.
                constexpr int count=64;
                double distances[count+1];
                for (int j=0;j<=count;++j)
                    distances[j]=screen_distance(curve.Value(first+(last-first)*j/count));
                for (int j=0;j<=count;++j) {
                    if ((j && distances[j]>distances[j-1]) || (j<count && distances[j]>distances[j+1])) continue;
                    double lo=first+(last-first)*std::max(0,j-1)/count;
                    double hi=first+(last-first)*std::min(count,j+1)/count;
                    for (int iteration=0;iteration<40;++iteration) {
                        const double a=lo+(hi-lo)/3, b=hi-(hi-lo)/3;
                        if (screen_distance(curve.Value(a))<screen_distance(curve.Value(b))) hi=b;
                        else lo=a;
                    }
                    gp_Pnt candidate=curve.Value((lo+hi)*0.5);
                    // Preserve exact topological ends when they are closest.
                    for (double t : {first,last})
                        if (screen_distance(curve.Value(t))<screen_distance(candidate)) candidate=curve.Value(t);
                    const double distance=screen_distance(candidate);
                    if (distance>best) continue;
                    const CPoint3d p(candidate.X(),candidate.Y(),candidate.Z());
                    if (on_plane && std::abs(dot(point_to_vec3(p)-sketch_origin_,sketch_normal_))>0.001f) continue;
                    if (!visible(candidate)) continue;
                    best=distance; result=p; found_edge=true;
                }
            } catch (const Standard_Failure&) {
                // Ignore an invalid imported edge, keeping other targets usable.
            }
        }
        if (found_edge) return true;
    }

    if (!require_sketch_plane && snap_enabled(SnapTarget::Surface)) {
        if (PickVisibleSurface(point, result, pick_object_id)) return true;
        if (pick_object_id != 0) return false;
    }

    require_sketch_plane = require_sketch_plane || snap_enabled(SnapTarget::WorkPlane);
    const DomPoint mouse{point.x(), point.y()};
    float best_distance = static_cast<float>(capture_distance_pixels_);
    bool found = false;
    CPoint3d grid_point = result;
    if (require_sketch_plane) {
        if (ScreenToSketchPlane(point, grid_point))
            found = SnapSketchGridPoint(point, sketch_origin_, sketch_u_, sketch_v_, grid_point, best_distance);
    } else {
        // CView3d draws the scene grid in XY in both 2D and 3D views.
        // The old XZ floor projection captured unrelated, invisible nodes.
        if (ScreenToWorldPlane(point, {}, {0,0,1}, grid_point))
            found = SnapSketchGridPoint(point, {}, {1,0,0}, {0,1,0}, grid_point, best_distance);
    }
    if (found) result = grid_point;
    bool found_point_candidate = false;
    const CAlfaObject* active_curve = nullptr;
    if (spatial_curve_preview_kind_ != SpatialCurvePreviewKind::None) {
        active_curve = spatial_curve_preview_object_id_ != 0
            ? document_->FindObjectById(spatial_curve_preview_object_id_) : nullptr;
    } else if (tool_ == ToolMode::DrawCurve) {
        active_curve = &document_->GetActivePolyline();
    } else if (tool_ == ToolMode::DrawBSpline) {
        active_curve = &document_->GetActiveBSpline();
    }

    // Moving nodes (and the curve segments that follow them) are not stable
    // snap targets. Keep the other nodes of the same curve available.
    using LinkedEnd = std::pair<unsigned long, bool>;
    std::vector<LinkedEnd> moving_linked_ends;
    const auto curve_size = [](const CAlfaObject* object) -> size_t {
        if (const auto* curve = dynamic_cast<const CPolyline*>(object)) return curve->GetPoints().size();
        if (const auto* curve = dynamic_cast<const CBSpline*>(object)) return curve->GetPoints().size();
        return 0;
    };
    if (dragging_polyline_point_) {
        for (const auto& selected : document_->GetSelectedCurvePoints()) {
            if (selected.first >= document_->GetObjects().size()) continue;
            const auto* object = document_->GetObjects()[selected.first].get();
            const size_t count = curve_size(object);
            if (count >= 2 && (selected.second == 0 || selected.second == count - 1))
                moving_linked_ends.push_back({object->m_id, selected.second != 0});
        }
        for (size_t i = 0; i < moving_linked_ends.size(); ++i) {
            for (const auto& link : document_->GetCurveEndpointLinks()) {
                const LinkedEnd a{link.first_id, link.first_end}, b{link.second_id, link.second_end};
                if (a != moving_linked_ends[i] && b != moving_linked_ends[i]) continue;
                const auto other = a == moving_linked_ends[i] ? b : a;
                if (std::find(moving_linked_ends.begin(), moving_linked_ends.end(), other) == moving_linked_ends.end())
                    moving_linked_ends.push_back(other);
            }
        }
    }
    const auto moving_node = [&](const CAlfaObject* object, size_t index) {
        if(dragging_sketch_handle_ && object==EditableSketch()
            && active_sketch_handle_kind_==SketchHandleKind::Primitive) return true;
        if (dragging_sketch_handle_ && object == EditableSketch()
            && active_sketch_handle_kind_ == SketchHandleKind::Node
            && active_sketch_handle_index_ == index) return true;
        if (!dragging_polyline_point_) return false;
        const size_t count = curve_size(object);
        if (object && count >= 2 && (index == 0 || index == count - 1)
            && std::find(moving_linked_ends.begin(), moving_linked_ends.end(),
                         LinkedEnd{object->m_id, index != 0}) != moving_linked_ends.end()) return true;
        for (const auto& selected : document_->GetSelectedCurvePoints()) {
            if (selected.first < document_->GetObjects().size()
                && document_->GetObjects()[selected.first].get() == object
                && selected.second == index) return true;
        }
        return false;
    };
    const auto moving_curve = [&](const CAlfaObject* object) {
        if (!dragging_polyline_point_) return false;
        if (object && std::any_of(moving_linked_ends.begin(), moving_linked_ends.end(),
            [&](const auto& end) { return end.first == object->m_id; })) return true;
        for (const auto& selected : document_->GetSelectedCurvePoints()) {
            if (selected.first < document_->GetObjects().size()
                && document_->GetObjects()[selected.first].get() == object) return true;
        }
        return false;
    };

    const auto consider = [&](const CAlfaObject* object,
                              const CPoint3d& candidate,
                              bool is_last_active_point, bool auxiliary = false) {
        if (!snap_enabled(auxiliary ? SnapTarget::AuxLine : SnapTarget::Knot) || is_last_active_point || !object
            || !document_->IsObjectVisible(*object)) {
            return;
        }
        if (require_sketch_plane) {
            const Vec3 delta = point_to_vec3(candidate) - sketch_origin_;
            if (std::fabs(dot(delta, sketch_normal_)) > 0.001f) {
                return;
            }
        }
        DomPoint screen{};
        if (!renderer_.WorldToScreen(
                point_to_vec3(candidate),
                camera_,
                orthographic_projection_,
                width(),
                height(),
                screen)) {
            return;
        }
        const float dx = static_cast<float>(mouse.x - screen.x);
        const float dy = static_cast<float>(mouse.y - screen.y);
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance <= best_distance) {
            best_distance = distance;
            result = candidate;
            found = true;
            found_point_candidate = true;
        }
    };
    const auto consider_segment = [&](const CAlfaObject* object,
                                      const CPoint3d& start,
                                      const CPoint3d& end, bool auxiliary = false) {
        // An explicit node inside the capture radius has priority over a
        // sampled point on the adjacent curve.  This is essential when the
        // user clicks the first node to close a curve.
        if (!snap_enabled(auxiliary ? SnapTarget::AuxLine : SnapTarget::Line) || found_point_candidate || !object || moving_curve(object)
            || !document_->IsObjectVisible(*object)) return;
        DomPoint start_screen{};
        DomPoint end_screen{};
        if (!renderer_.WorldToScreen(point_to_vec3(start), camera_,
                orthographic_projection_, width(), height(), start_screen)
            || !renderer_.WorldToScreen(point_to_vec3(end), camera_,
                orthographic_projection_, width(), height(), end_screen)) return;
        const double dx = end_screen.x - start_screen.x;
        const double dy = end_screen.y - start_screen.y;
        const double squared = dx * dx + dy * dy;
        const double t = squared <= 1.0e-12 ? 0.0 : std::clamp(
            ((mouse.x - start_screen.x) * dx + (mouse.y - start_screen.y) * dy)
                / squared, 0.0, 1.0);
        const double screen_x = start_screen.x + dx * t;
        const double screen_y = start_screen.y + dy * t;
        const double distance = std::hypot(mouse.x - screen_x, mouse.y - screen_y);
        if (distance > best_distance) return;
        const CPoint3d candidate(
            start.x + (end.x - start.x) * t,
            start.y + (end.y - start.y) * t,
            start.z + (end.z - start.z) * t);
        if (require_sketch_plane) {
            const Vec3 delta = point_to_vec3(candidate) - sketch_origin_;
            if (std::fabs(dot(delta, sketch_normal_)) > 0.001f) return;
        }
        best_distance = static_cast<float>(distance);
        result = candidate;
        found = true;
    };

    for (const auto& object_ptr : document_->GetObjects()) {
        const CAlfaObject* object = object_ptr.get();
        if (!object || !document_->IsObjectVisible(*object)) {
            continue;
        }
        if (pick_object_id != 0 && object->m_id != pick_object_id) {
            continue;
        }
        if (const auto* polyline = dynamic_cast<const CPolyline*>(object)) {
            const auto& points = polyline->GetPoints();
            for (std::size_t index = 0; index < points.size(); ++index) {
                consider(
                    object,
                    points[index],
                    moving_node(object, index)
                        || (object == active_curve && index + 1 == points.size()));
            }
            const std::vector<CPoint3d> path = polyline->GetRoundedPathPoints();
            for (size_t index = 0; index + 1 < path.size(); ++index) {
                consider_segment(object, path[index], path[index + 1]);
            }
        } else if (const auto* spline = dynamic_cast<const CBSpline*>(object)) {
            const auto& points = spline->GetPoints();
            for (std::size_t index = 0; index < points.size(); ++index) {
                consider(
                    object,
                    points[index],
                    moving_node(object, index)
                        || (object == active_curve && index + 1 == points.size()));
            }
            CPoint3d previous = spline->Evaluate(0.0f);
            const int sample_count = std::max(
                64, static_cast<int>(spline->GetPointCount()) * 24);
            for (int sample = 1; sample <= sample_count; ++sample) {
                const CPoint3d current = spline->Evaluate(
                    static_cast<float>(sample) / sample_count);
                consider_segment(object, previous, current);
                previous = current;
            }
        } else if (const auto* sketch = dynamic_cast<const CSmartLine*>(object)) {
            for (std::size_t index = 0; index < sketch->GetNodeCount(); ++index) {
                consider(object, sketch->GetNodeWorld(index), moving_node(object, index));
            }
        } else if (const auto* sketch = dynamic_cast<const CSketch*>(object)) {
            for (size_t contour = 0; contour < sketch->GetContourCount(); ++contour) {
                const auto world = sketch->MakeWorldContour(contour);
                for (size_t node = 0; node < world.GetNodeCount(); ++node)
                    consider(object, world.GetNodeWorld(node), dragging_sketch_handle_
                        && sketch->m_id == edit_sketch_parent_id_ && sketch->GetContourId(contour) == edit_contour_id_
                        && moving_node(EditableSketch(), node));
            }
        } else if (const auto* mesh = dynamic_cast<const CMesh3D*>(object)) {
            for (const Vec3& vertex : mesh->GetVertices()) {
                consider(object,
                    CPoint3d(vertex.x, vertex.y, vertex.z), false);
            }
        } else if (const auto* solid = dynamic_cast<const CSolid*>(object)) {
            if (snap_enabled(SnapTarget::AuxLine)) for (const auto& axis : solid->GetCenterlines()) {
                if (axis.points.size() < 2) continue;
                const auto point3 = [](const gp_Pnt& p) { return CPoint3d(p.X(),p.Y(),p.Z()); };
                consider(object, point3(axis.points.front()), false, true);
                if (!axis.closed) consider(object, point3(axis.points.back()), false, true);
                if (axis.points.size() == 2)
                    consider(object, point3(gp_Pnt((axis.points.front().XYZ()+axis.points.back().XYZ())*0.5)), false, true);
                for (size_t i = 1; i < axis.points.size(); ++i)
                    consider_segment(object, point3(axis.points[i-1]), point3(axis.points[i]), true);
            }
            const std::string& name = object->GetName();
            const bool furniture_part = name.rfind("Nika ", 0) == 0
                || name.rfind("Corner ", 0) == 0
                || name.rfind("Cabinet ", 0) == 0;
            Vec3 minimum{};
            Vec3 maximum{};
            if (furniture_part && solid->GetBounds(minimum, maximum)) {
                for (float x : {minimum.x, maximum.x}) {
                    for (float y : {minimum.y, maximum.y}) {
                        for (float z : {minimum.z, maximum.z}) {
                            consider(object, CPoint3d(x, y, z), false);
                        }
                    }
                }
            } else {
                for (TopExp_Explorer explorer(
                         solid->m_Shape, TopAbs_VERTEX);
                     explorer.More(); explorer.Next()) {
                    const gp_Pnt vertex = BRep_Tool::Pnt(
                        TopoDS::Vertex(explorer.Current()));
                    consider(object,
                        CPoint3d(vertex.X(), vertex.Y(), vertex.Z()), false);
                }
            }
        }
    }
    // The second pole sets the start tangent of the new open spline.
    // Offer the exact outward tangent at the joined endpoint, rather than a
    // chord of its display tessellation. Other construction stages are unchanged.
    if (snap_enabled(SnapTarget::AuxLine) && !found_point_candidate
        && !spatial_curve_preview_points_.empty()
        && (spatial_curve_preview_points_.size()==1 || active_tangent_id_)
        && (spatial_curve_preview_kind_==SpatialCurvePreviewKind::BSpline
            || spatial_curve_preview_kind_==SpatialCurvePreviewKind::Nurbs)) {
        const CPoint3d start=spatial_curve_preview_points_.front();
        float tangent_distance=static_cast<float>(capture_distance_pixels_);
        bool captured=false;
        Vec3 forward,right,up;
        viewport_camera_basis(camera_,forward,right,up);
        const Vec3 eye=camera_position(camera_,orthographic_projection_);
        for (const auto& item:document_->GetObjects()) {
            const auto* spline=dynamic_cast<const CBSpline*>(item.get());
            if (!spline || spline==active_curve || spline->IsClosed()
                || !document_->IsObjectVisible(*spline)) continue;
            try {
                CBSpline normalized=*spline;
                if (normalized.GetKnots().empty() && normalized.GetPointCount()>=2)
                    normalized.SetDegree(std::min(normalized.GetDegree(),int(normalized.GetPointCount())-1));
                const auto curve=curve_cut::Spline(normalized);
                if (curve.IsNull()) continue;
                for (bool at_start:{false,true}) {
                    gp_Pnt end; gp_Vec tangent;
                    curve->D1(at_start?curve->FirstParameter():curve->LastParameter(),end,tangent);
                    const bool activated=spline->m_id==active_tangent_id_ && at_start==active_tangent_start_;
                    if ((!activated && (active_tangent_id_ || spatial_curve_preview_points_.size()!=1
                        || end.SquareDistance(gp_Pnt(start.x,start.y,start.z))>1.e-8))
                        || tangent.SquareMagnitude()<1.e-20) continue;
                    if (at_start) tangent.Reverse();
                    tangent.Normalize();
                    const Vec3 origin{float(end.X()),float(end.Y()),float(end.Z())};
                    const Vec3 direction{float(tangent.X()),float(tangent.Y()),float(tangent.Z())};
                    const float span=std::max(1.0f,camera_.distance*.1f);
                    const Vec3 next=origin+direction*span;
                    DomPoint a{},b{};
                    if (!renderer_.WorldToScreen(origin,camera_,orthographic_projection_,width(),height(),a)
                        || !renderer_.WorldToScreen(next,camera_,orthographic_projection_,width(),height(),b)) continue;
                    const double dx=b.x-a.x,dy=b.y-a.y, squared=dx*dx+dy*dy;
                    if (squared<1) continue;
                    const double t=((point.x()-a.x)*dx+(point.y()-a.y)*dy)/squared;
                    double fraction=t;
                    if (!orthographic_projection_) {
                        const double z0=dot(origin-eye,forward), z1=dot(next-eye,forward);
                        const double denominator=z1*(1-t)+t*z0;
                        if (std::abs(denominator)<1.e-9) continue;
                        fraction=t*z0/denominator;
                    }
                    if (fraction<=0) continue;
                    const Vec3 candidate=origin+direction*float(span*fraction);
                    if (require_sketch_plane && std::abs(dot(candidate-sketch_origin_,sketch_normal_))>.001f) continue;
                    if (xy_plane_view_enabled_ && !require_sketch_plane && std::abs(candidate.z)>.001f) continue;
                    const double distance=std::hypot(point.x()-a.x-t*dx,point.y()-a.y-t*dy);
                    if (distance>tangent_distance) continue;
                    tangent_distance=float(distance);
                    result=CPoint3d(candidate.x,candidate.y,candidate.z);
                    auxiliary_guide_visible_=true;
                    tangent_guide_visible_=true;
                    auxiliary_guide_origin_=origin;
                    auxiliary_guide_direction_=direction;
                    captured=true;
                }
            } catch (const Standard_Failure&) {
                // An invalid imported curve must not interrupt point input.
            }
        }
        if (captured) return true;
    }
    if (snap_enabled(SnapTarget::AuxLine)
        || snap_enabled(SnapTarget::AuxLine45)) {
        const bool existing_snap = found;
        const std::vector<CPoint3d>* points = nullptr;
        if (!spatial_curve_preview_points_.empty()) points = &spatial_curve_preview_points_;
        else if (require_sketch_plane && !sketch_polyline_points_.empty()) points = &sketch_polyline_points_;
        else if (const auto* polyline = dynamic_cast<const CPolyline*>(active_curve)) {
            points = &polyline->GetPoints();
        } else if (const auto* spline = dynamic_cast<const CBSpline*>(active_curve)) {
            points = &spline->GetPoints();
        }
        if (points && !points->empty()) for (size_t anchor_index : {points->size()-1,size_t(0)}) {
            const CPoint3d* anchor = &(*points)[anchor_index];
            Vec3 forward, u, v;
            viewport_camera_basis(camera_, forward, u, v);
            if (require_sketch_plane || snap_enabled(SnapTarget::WorkPlane)) {
                u = sketch_u_; v = sketch_v_; forward = sketch_normal_;
            } else if (xy_plane_view_enabled_) {
                u = {1,0,0}; v = {0,1,0}; forward = {0,0,1};
            }
            Vec3 origin = point_to_vec3(*anchor);
            if (!require_sketch_plane && !snap_enabled(SnapTarget::WorkPlane)
                && !xy_plane_view_enabled_ && spatial_curve_plane_valid_
                && spatial_curve_preview_kind_ != SpatialCurvePreviewKind::None) {
                forward = spatial_curve_plane_normal_;
                // Synthetic guides must not inherit an off-plane geometry snap.
                origin = origin - forward * dot(origin-spatial_curve_plane_origin_,forward);
                u = u - forward * dot(u,forward);
                if (dot(u,u)<1.e-6f) u = cross(v,forward);
                u = normalize(u); v = normalize(cross(forward,u));
            }
            CPoint3d guide_point;
            const bool valid_guide = ScreenToWorldPlane(point, origin,
                require_sketch_plane ? sketch_normal_ : forward, guide_point);
            const Vec3 delta = point_to_vec3(guide_point) - origin;
            for (int axis = 0; valid_guide && axis < 4; ++axis) {
                if (axis < 2 && !snap_enabled(SnapTarget::AuxLine)) continue;
                if (axis >= 2 && !snap_enabled(SnapTarget::AuxLine45)) continue;
                const Vec3 direction = axis == 0 ? u : axis == 1 ? v
                    : normalize(axis == 2 ? u + v : u - v);
                Vec3 candidate = origin + direction * dot(delta, direction);
                if (existing_snap) {
                    // Keep geometry/grid priority, but show a guide when the
                    // already captured point lies on it as well.
                    candidate = point_to_vec3(result);
                    const Vec3 offset = candidate-origin;
                    const Vec3 perpendicular = offset-direction*dot(offset,direction);
                    if (dot(perpendicular,perpendicular)>1.e-6f) continue;
                    auxiliary_guide_visible_ = anchor_index == 0;
                    auxiliary_guide_origin_ = origin;
                    auxiliary_guide_direction_ = direction;
                    continue;
                }
                DomPoint screen;
                if (!renderer_.WorldToScreen(candidate, camera_, orthographic_projection_, width(), height(), screen)) continue;
                const double distance = std::hypot(double(screen.x - mouse.x), double(screen.y - mouse.y));
                if (distance <= best_distance) {
                    best_distance = static_cast<float>(distance);
                    result = CPoint3d(candidate.x, candidate.y, candidate.z);
                    found = true;
                    // Last-point alignment still snaps, but only the first
                    // point supplies the visible closing/alignment guide.
                    auxiliary_guide_visible_ = anchor_index == 0;
                    auxiliary_guide_origin_ = origin;
                    auxiliary_guide_direction_ = direction;
                }
            }
        }
    }
    if (found && !measuring && xy_plane_view_enabled_ && !require_sketch_plane) {
        result.z = 0.0;
    }
    return found;
}

bool OpenGLViewport::PickSolidSurface(const QPoint& point, CPoint3d& result) const {
    return point_pick_object_id_ != 0 && PickVisibleSurface(point, result, point_pick_object_id_);
}

bool OpenGLViewport::PickVisibleSurface(const QPoint& point, CPoint3d& result,
                                        unsigned long object_id,int face_index) const {
    if (!document_) return false;
    const int viewport_width = std::max(1, width());
    const int viewport_height = std::max(1, height());
    const float ndc_x = 2.0f * static_cast<float>(point.x()) / static_cast<float>(viewport_width) - 1.0f;
    const float ndc_y = 1.0f - 2.0f * static_cast<float>(point.y()) / static_cast<float>(viewport_height);
    const float aspect = static_cast<float>(viewport_width) / static_cast<float>(viewport_height);

    Vec3 forward{};
    Vec3 right{};
    Vec3 up{};
    viewport_camera_basis(camera_, forward, right, up);

    Vec3 ray_origin = camera_position(camera_, orthographic_projection_);
    Vec3 ray_direction{};
    if (orthographic_projection_) {
        const float half_height = std::max(
            kMinimumOrthographicHalfHeight, camera_.distance * 0.42f);
        const float half_width = half_height * aspect;
        ray_origin = camera_position(camera_, orthographic_projection_) + right * (ndc_x * half_width) + up * (ndc_y * half_height);
        ray_direction = forward;
    } else {
        const float tan_half_fov = std::tan(
            deg_to_rad(camera_.vertical_fov_degrees) * 0.5f);
        ray_direction = normalize(forward + right * (ndc_x * aspect * tan_half_fov) + up * (ndc_y * tan_half_fov));
    }

    const gp_Lin ray(gp_Pnt(ray_origin.x, ray_origin.y, ray_origin.z),
                     gp_Dir(ray_direction.x, ray_direction.y, ray_direction.z));
    double nearest = 1.e10;
    bool found = false;
    for (const auto& object : document_->GetObjects()) {
        const auto* solid = dynamic_cast<const CSolid*>(object.get());
        if (!solid || solid->m_Shape.IsNull() || !document_->IsObjectVisible(*solid)
            || (object_id != 0 && solid->m_id != object_id)) continue;
        try {
            IntCurvesFace_ShapeIntersector hit;
            TopoDS_Shape target=solid->m_Shape;
            if(face_index>=0) {
                int index=0;target.Nullify();
                for(TopExp_Explorer ex(solid->m_Shape,TopAbs_FACE);ex.More();ex.Next(),++index)
                    if(index==face_index){target=ex.Current();break;}
                if(target.IsNull())continue;
            }
            hit.Load(target, 1.e-6);
            hit.Perform(ray, 0.0, nearest);
            if (!hit.IsDone()) continue;
            for (int i = 1; i <= hit.NbPnt(); ++i) {
                const double distance = hit.WParameter(i);
                if (distance >= 0.0 && distance < nearest) {
                    nearest = distance;
                    const auto p = hit.Pnt(i);
                    result = CPoint3d(p.X(), p.Y(), p.Z());
                    found = true;
                }
            }
        } catch (const Standard_Failure&) {
            // A failed imported face must not hide a valid hit on another body.
        }
    }
    return found;
}

// Common point acquisition for GetPoint3D and plane-constrained GetPoint2D.
// Hover and click must resolve the same snap and the same plane restriction.
CSolid* OpenGLViewport::PickArchitecturePoint(const QPoint& point, CPoint3d& result,
                                             bool* snapped) const {
    if (snapped) *snapped = false;
    if (!document_) return nullptr;
    const DomPoint screen_point{point.x(), point.y()};
    const Vec3 camera_forward = normalize(camera_.target
        - camera_position(camera_, orthographic_projection_));
    auto project_world = [this, camera_forward](
                             Vec3 world, DomPoint& screen, float& depth) {
        depth = dot(
            world - camera_position(camera_, orthographic_projection_),
            camera_forward);
        return depth > 0.0f && renderer_.WorldToScreen(
            world, camera_, orthographic_projection_,
            width(), height(), screen);
    };
    CSolid* wall = document_->FindSolidAtScreen(
        screen_point, project_world);
    Vec3 minimum{};
    Vec3 maximum{};
    if (!wall || !wall->GetBounds(minimum, maximum)) {
        return nullptr;
    }

    const bool along_x = maximum.x - minimum.x
        >= maximum.y - minimum.y;
    const Vec3 plane_point{
        (minimum.x + maximum.x) * 0.5f,
        (minimum.y + maximum.y) * 0.5f,
        (minimum.z + maximum.z) * 0.5f};
    const Vec3 plane_normal = along_x
        ? Vec3{0.0f, 1.0f, 0.0f}
        : Vec3{1.0f, 0.0f, 0.0f};
    if (!PickPointOnPlane(point, plane_point, plane_normal, result, snapped)) {
        return nullptr;
    }
    return wall;
}

bool OpenGLViewport::PickRequestedPoint(const QPoint& point, CPoint3d& result,
                                        bool* snapped) const {
    if (snapped) *snapped = false;
    if (!point_pick_plane_enabled_) {
        if (!PickModelingPoint(point, result)) return false;
        CPoint3d capture = result;
        if (snapped) *snapped = !picking_solid_surface_ && SnapCreationPoint(point, capture, false);
        return true;
    }
    return PickPointOnPlane(point, point_pick_plane_origin_, point_pick_plane_normal_, result, snapped);
}

bool OpenGLViewport::PickPointOnPlane(const QPoint& point, Vec3 origin, Vec3 plane_normal,
                                     CPoint3d& result, bool* snapped) const {
    if (snapped) *snapped = false;
    CPoint3d capture{};
    if (SnapCreationPoint(point, capture, false)) {
        const Vec3 normal = normalize(plane_normal);
        const double distance = (capture.x - origin.x) * normal.x
            + (capture.y - origin.y) * normal.y
            + (capture.z - origin.z) * normal.z;
        result = CPoint3d(capture.x - normal.x * distance,
                         capture.y - normal.y * distance,
                         capture.z - normal.z * distance);
        DomPoint screen{};
        if (renderer_.WorldToScreen(point_to_vec3(result), camera_, orthographic_projection_,
                                    width(), height(), screen)
            && std::hypot(double(screen.x-point.x()), double(screen.y-point.y())) <= capture_distance_pixels_) {
            if (snapped) *snapped = true;
            return true;
        }
    }
    if (!ScreenToWorldPlane(point, origin, plane_normal, result)) return false;
    // Grid belongs to the requested plane, not to the floor projected onto it.
    const Vec3 normal = normalize(plane_normal);
    const Vec3 u = std::abs(normal.z) > 0.99f ? Vec3{1,0,0}
        : normalize(cross(normal, {0,0,1}));
    const Vec3 v = normalize(cross(normal, u));
    float distance = static_cast<float>(capture_distance_pixels_);
    if (snapping_enabled_ && SnapSketchGridPoint(point, origin, u, v, result, distance)) {
        if (snapped) *snapped = true;
    }
    return true;
}

bool OpenGLViewport::PickModelingPoint(const QPoint& point,
                                       CPoint3d& result) const {
    if (picking_solid_surface_) return PickSolidSurface(point, result);
    if (SnapCreationPoint(point, result, false)) {
        if (xy_plane_view_enabled_ && !IsSnapTargetEnabled(SnapTarget::WorkPlane) && !IsSnapTargetEnabled(SnapTarget::Surface)
            && !IsSnapTargetEnabled(SnapTarget::Line)) result.z = 0.0;
        return true;
    }
    if (point_pick_object_id_ != 0) {
        return false;
    }
    if (xy_plane_view_enabled_ && !IsSnapTargetEnabled(SnapTarget::WorkPlane)) {
        return ScreenToWorldPlane(
            point,
            {0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            result);
    }
    if (IsSnapTargetEnabled(SnapTarget::WorkPlane)) {
        if (ScreenToSketchPlane(point, result)) return true;
        // A spatial curve must remain drawable when the work plane is
        // edge-on. Use its usual view plane without changing snap settings.
        // Other point-picking tools retain their explicit plane constraint.
        if (spatial_curve_preview_kind_ == SpatialCurvePreviewKind::None) return false;
    }
    if (spatial_curve_preview_kind_ != SpatialCurvePreviewKind::None
        && spatial_curve_plane_valid_) {
        return ScreenToWorldPlane(point, spatial_curve_plane_origin_,
                                  spatial_curve_plane_normal_, result);
    }
    return ScreenToViewPlane(point, camera_.target, result);
}

void OpenGLViewport::HandleMovePointToPointClick(const QPoint& point) {
    if (!document_) {
        return;
    }
    if (move_point_stage_ == MovePointStage::SelectObjects) {
        SelectionAction action = SelectionAction::Replace;
        if (QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier)) {
            action = SelectionAction::Add;
        } else if (QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier)) {
            action = SelectionAction::Remove;
        }
        SelectAt(point, action);
        emit StatusTextChanged("Move Point to Point: selection ready, press Enter");
        return;
    }

    CPoint3d picked{};
    if (!PickModelingPoint(point, picked)) {
        emit StatusTextChanged("Move Point to Point: click a visible vertex or sketch point");
        return;
    }
    if (move_point_stage_ == MovePointStage::PickSource) {
        move_point_source_ = picked;
        move_point_stage_ = MovePointStage::PickTarget;
        measurement_start_ = picked;
        measurement_preview_ = picked;
        measurement_waiting_for_second_point_ = true;
        measurement_preview_valid_ = false;
        emit StatusTextChanged("Move Point to Point: pick target point");
        update();
        return;
    }

    const Vec3 delta{
        static_cast<float>(picked.x - move_point_source_.x),
        static_cast<float>(picked.y - move_point_source_.y),
        static_cast<float>(picked.z - move_point_source_.z)};
    if (ApplyPreciseMove(delta)) {
        emit SelectionChanged();
        emit StatusTextChanged("Move Point to Point completed");
    } else {
        emit StatusTextChanged("Move Point to Point: selected objects cannot be moved");
    }
    measurement_waiting_for_second_point_ = false;
    measurement_preview_valid_ = false;
    measurement_visible_ = false;
    if (move_point_repeat_) {
        move_point_stage_ = MovePointStage::PickSource;
        setCursor(Qt::CrossCursor);
        emit StatusTextChanged("Move: pick the next source point or use an axis button");
    } else {
        SetTool(ToolMode::Select);
    }
    update();
}

void OpenGLViewport::HandleMeasurePointToPointClick(const QPoint& point) {
    if (!document_) {
        return;
    }
    CPoint3d picked{};
    if (!SnapCreationPoint(point, picked, false)) {
        emit StatusTextChanged(
            "Point-to-Point Dimension: click a visible vertex or sketch point");
        return;
    }

    if (!measurement_waiting_for_second_point_) {
        measurement_start_ = picked;
        measurement_end_ = picked;
        measurement_preview_ = picked;
        measurement_visible_ = false;
        measurement_preview_valid_ = true;
        measurement_waiting_for_second_point_ = true;
        emit StatusTextChanged("Point-to-Point Dimension: pick second point");
        update();
        return;
    }

    measurement_end_ = picked;
    measurement_preview_ = picked;
    measurement_visible_ = true;
    measurement_preview_valid_ = false;
    measurement_waiting_for_second_point_ = false;
    const double dx = measurement_end_.x - measurement_start_.x;
    const double dy = measurement_end_.y - measurement_start_.y;
    const double dz = measurement_end_.z - measurement_start_.z;
    const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    const DisplayLengthUnit unit = LoadDisplayLengthUnit();
    emit StatusTextChanged(
        QString("Point-to-Point Dimension: %1 %2. Pick next first point")
            .arg(MillimetersToDisplay(distance, unit), 0, 'f', 2)
            .arg(DisplayLengthUnitSuffix(unit)));
    update();
    emit PointToPointMeasurementFinished(distance);
}

bool OpenGLViewport::SnapSketchGridPoint(const QPoint& point,
                                         Vec3 origin,
                                         Vec3 u_axis,
                                         Vec3 v_axis,
                                         CPoint3d& result,
                                         float& best_distance) const {
    const float snap_step = grid_step_ / static_cast<float>(
        std::max(1, grid_subdivisions_));
    if (!show_floor_grid_ || !IsSnapTargetEnabled(SnapTarget::Grid) || snap_step <= 0.0f) {
        return false;
    }

    if (dot(u_axis, u_axis) <= 0.000001f
        || dot(v_axis, v_axis) <= 0.000001f) {
        return false;
    }
    u_axis = normalize(u_axis);
    v_axis = normalize(v_axis);

    const Vec3 source = point_to_vec3(result);
    const Vec3 delta = source - origin;
    const float u_coordinate = dot(delta, u_axis);
    const float v_coordinate = dot(delta, v_axis);
    const float grid_u = std::round(u_coordinate / snap_step) * snap_step;
    const float grid_v = std::round(v_coordinate / snap_step) * snap_step;
    const auto capture = [&](float u, float v) {
        const Vec3 candidate = origin + u_axis * u + v_axis * v;
        DomPoint screen{};
        if (!renderer_.WorldToScreen(candidate, camera_, orthographic_projection_,
                                     width(), height(), screen)) return false;
        const float distance = std::hypot(float(point.x() - screen.x),
                                          float(point.y() - screen.y));
        if (distance > best_distance) return false;
        best_distance = distance;
        result = CPoint3d(candidate.x, candidate.y, candidate.z);
        return true;
    };

    // A grid intersection wins even when either grid line is closer.
    if (capture(grid_u, grid_v)) return true;
    // Otherwise constrain just one coordinate, preserving motion along the
    // line. Compare both families in pixels, using the usual capture radius.
    const bool captured_u = capture(grid_u, v_coordinate);
    const bool captured_v = capture(u_coordinate, grid_v);
    return captured_u || captured_v;
}

void OpenGLViewport::SetCreationSnapCursor(bool snapped) {
    // The guide may change while the cursor remains captured (or while a
    // constrained preview point stays still). Repaint its overlay as well.
    update();
    // Plane picking/navigation can replace the cursor between snap updates.
    // Restore capture feedback even when the snap state did not change.
    creation_snap_active_ = snapped;
    if (snapped) {
        setCursor(captured_point_cursor());
    } else if (dragging_sketch_handle_ || dragging_polyline_point_) {
        setCursor(Qt::ClosedHandCursor);
    } else if (tool_ == ToolMode::DrawCurve
               || tool_ == ToolMode::DrawBSpline
               || tool_ == ToolMode::SketchRectangle
               || tool_ == ToolMode::SketchPolyline
               || tool_ == ToolMode::SketchBezier
               || tool_ == ToolMode::SolidBoxRectangle
               || tool_ == ToolMode::SolidCylinderCircle
               || picking_3d_point_ || picking_xy_point_) {
        setCursor(Qt::CrossCursor);
    }
}

std::vector<CPoint3d> OpenGLViewport::SketchRectanglePoints(const CPoint3d& first, const CPoint3d& second) const {
    // Match the orthonormal, double-precision basis used by CSmartLine.
    // Float world-space addition can move corners off an inclined face by
    // more than the sketch planarity tolerance, rejecting the second click.
    gp_Vec u(sketch_u_.x, sketch_u_.y, sketch_u_.z);
    const gp_Vec normal = u.Crossed(gp_Vec(sketch_v_.x, sketch_v_.y, sketch_v_.z)).Normalized();
    u = (u - normal * u.Dot(normal)).Normalized();
    const gp_Vec v = normal.Crossed(u).Normalized();
    const gp_Pnt origin(sketch_origin_.x, sketch_origin_.y, sketch_origin_.z);
    const gp_Vec a(origin, gp_Pnt(first.x, first.y, first.z));
    const gp_Vec b(origin, gp_Pnt(second.x, second.y, second.z));
    double x0 = a.Dot(u), y0 = a.Dot(v);
    const double x1 = b.Dot(u), y1 = b.Dot(v);
    if (tool_ == ToolMode::SolidBoxRectangle && solid_box_centered_) {
        x0 = 2 * x0 - x1;
        y0 = 2 * y0 - y1;
    }
    const auto corner = [&](double x, double y) {
        const gp_Pnt p = origin.Translated(u * x + v * y);
        return CPoint3d(p.X(), p.Y(), p.Z());
    };
    return {corner(x0, y0), corner(x1, y0), corner(x1, y1), corner(x0, y1)};
}

static double initial_primitive_height(const Camera& camera, Vec3 origin, Vec3 normal,
    int viewport_height, bool orthographic) {
    // Choose the initial height once at creation, targeting a readable
    // 80-pixel dimension. Bound foreshortening near a top view so the box
    // does not become arbitrarily tall when its normal points at the camera.
    Vec3 forward{}, right{}, up{};
    viewport_camera_basis(camera, forward, right, up);
    double span = 2.0 * std::max(kMinimumOrthographicHalfHeight, camera.distance * 0.42f);
    if (!orthographic) {
        const double depth = std::max(0.001f, dot(origin-camera_position(camera, false),forward));
        span = 2.0*depth*std::tan(deg_to_rad(camera.vertical_fov_degrees)*0.5);
    }
    const double projected_normal = std::hypot(dot(normal,right),dot(normal,up));
    return std::clamp(80.0*span
        / (std::max(1,viewport_height)*std::max(0.35,projected_normal)),0.001,900.0);
}

std::vector<ToolParameter> OpenGLViewport::SolidBoxParametersFromRectangle(const CPoint3d& first, const CPoint3d& second) const {
    Vec3 a{static_cast<float>(first.x), static_cast<float>(first.y), static_cast<float>(first.z)};
    const Vec3 b{static_cast<float>(second.x), static_cast<float>(second.y), static_cast<float>(second.z)};
    if (solid_box_centered_) a = a * 2.0f - b;
    const Vec3 delta = b - a;
    const float length_signed = dot(delta, sketch_u_);
    const float width_signed = dot(delta, sketch_v_);
    const float length = std::max(std::fabs(length_signed), 0.001f);
    const float width = std::max(std::fabs(width_signed), 0.001f);
    Vec3 origin = a;
    if (length_signed < 0.0f) {
        origin = origin + sketch_u_ * length_signed;
    }
    if (width_signed < 0.0f) {
        origin = origin + sketch_v_ * width_signed;
    }
    const Vec3 u = sketch_u_;
    const Vec3 v = sketch_v_;
    const Vec3 normal = sketch_normal_;
    const double initial_height = initial_primitive_height(camera_, origin, normal, height(), orthographic_projection_);
    std::vector<ToolParameter> parameters{
        {"width", "Length", length, 0.001, 900.0, 0.5},
        {"height", "Width", width, 0.001, 900.0, 0.5},
        {"depth", "Height", initial_height, -900.0, 900.0, 0.5},
        {"origin.x", "Origin X", origin.x, -1000000.0, 1000000.0, 0.1},
        {"origin.y", "Origin Y", origin.y, -1000000.0, 1000000.0, 0.1},
        {"origin.z", "Origin Z", origin.z, -1000000.0, 1000000.0, 0.1},
        {"axis.u.x", "U X", u.x, -1.0, 1.0, 0.01},
        {"axis.u.y", "U Y", u.y, -1.0, 1.0, 0.01},
        {"axis.u.z", "U Z", u.z, -1.0, 1.0, 0.01},
        {"axis.v.x", "V X", v.x, -1.0, 1.0, 0.01},
        {"axis.v.y", "V Y", v.y, -1.0, 1.0, 0.01},
        {"axis.v.z", "V Z", v.z, -1.0, 1.0, 0.01},
        {"axis.n.x", "Normal X", normal.x, -1.0, 1.0, 0.01},
        {"axis.n.y", "Normal Y", normal.y, -1.0, 1.0, 0.01},
        {"axis.n.z", "Normal Z", normal.z, -1.0, 1.0, 0.01}
    };
    if (solid_box_target_body_id_ != 0) {
        parameters.push_back({"boolean.overlap", "Boolean Overlap", 0.001, 0.0, 1000000.0, 0.001});
        parameters.push_back({
            "boolean.body_id",
            "Boolean Body",
            static_cast<double>(solid_box_target_body_id_),
            0.0,
            static_cast<double>(std::numeric_limits<unsigned long>::max()),
            1.0});
    }
    return parameters;
}

std::vector<ToolParameter> OpenGLViewport::SolidCylinderParametersFromCircle(
    const CPoint3d& center,
    const CPoint3d& radius_point) const {
    const Vec3 c{
        static_cast<float>(center.x),
        static_cast<float>(center.y),
        static_cast<float>(center.z)};
    const Vec3 p{
        static_cast<float>(radius_point.x),
        static_cast<float>(radius_point.y),
        static_cast<float>(radius_point.z)};
    const Vec3 delta = p - c;
    const double du = dot(delta, sketch_u_);
    const double dv = dot(delta, sketch_v_);
    const double radius = std::max(std::sqrt(du * du + dv * dv), 0.001);
    const Vec3 u = sketch_u_;
    const Vec3 v = sketch_v_;
    const Vec3 normal = sketch_normal_;
    std::vector<ToolParameter> parameters{
        {"diameter", "Diameter", radius * 2.0, 0.001, 900.0, 0.1},
        {"height", "Height", initial_primitive_height(camera_, c, normal, height(), orthographic_projection_), -900.0, 900.0, 0.1},
        {"base_chamfer", "Hole Entrance Chamfer (45 deg)", 0.0, 0.0, 1.0, 1.0, ToolParameterType::Checkbox},
        {"base_chamfer_size", "Chamfer Size", 1.0, 0.01, 900.0, 0.1,
            ToolParameterType::Number, {}, ToolParameterUnit::Length},
        {"origin.x", "Origin X", c.x, -1000000.0, 1000000.0, 0.1},
        {"origin.y", "Origin Y", c.y, -1000000.0, 1000000.0, 0.1},
        {"origin.z", "Origin Z", c.z, -1000000.0, 1000000.0, 0.1},
        {"axis.u.x", "U X", u.x, -1.0, 1.0, 0.01},
        {"axis.u.y", "U Y", u.y, -1.0, 1.0, 0.01},
        {"axis.u.z", "U Z", u.z, -1.0, 1.0, 0.01},
        {"axis.v.x", "V X", v.x, -1.0, 1.0, 0.01},
        {"axis.v.y", "V Y", v.y, -1.0, 1.0, 0.01},
        {"axis.v.z", "V Z", v.z, -1.0, 1.0, 0.01},
        {"axis.n.x", "Normal X", normal.x, -1.0, 1.0, 0.01},
        {"axis.n.y", "Normal Y", normal.y, -1.0, 1.0, 0.01},
        {"axis.n.z", "Normal Z", normal.z, -1.0, 1.0, 0.01}
    };
    if (solid_box_target_body_id_ != 0) {
        parameters.push_back({"boolean.overlap", "Boolean Overlap", 0.001, 0.0, 1000000.0, 0.001});
        parameters.push_back({
            "boolean.body_id",
            "Boolean Body",
            static_cast<double>(solid_box_target_body_id_),
            0.0,
            static_cast<double>(std::numeric_limits<unsigned long>::max()),
            1.0});
    }
    return parameters;
}

bool OpenGLViewport::ScreenToWorldPlane(const QPoint& point, Vec3 plane_point, Vec3 plane_normal, CPoint3d& result) const {
    const int viewport_width = std::max(1, width());
    const int viewport_height = std::max(1, height());
    const float ndc_x = 2.0f * static_cast<float>(point.x()) / static_cast<float>(viewport_width) - 1.0f;
    const float ndc_y = 1.0f - 2.0f * static_cast<float>(point.y()) / static_cast<float>(viewport_height);
    const float aspect = static_cast<float>(viewport_width) / static_cast<float>(viewport_height);

    Vec3 forward{};
    Vec3 right{};
    Vec3 up{};
    viewport_camera_basis(camera_, forward, right, up);

    Vec3 ray_origin = camera_position(camera_, orthographic_projection_);
    Vec3 ray_direction{};
    if (orthographic_projection_) {
        const float half_height = std::max(
            kMinimumOrthographicHalfHeight, camera_.distance * 0.42f);
        const float half_width = half_height * aspect;
        ray_origin = camera_position(camera_, orthographic_projection_) + right * (ndc_x * half_width) + up * (ndc_y * half_height);
        ray_direction = forward;
    } else {
        const float tan_half_fov = std::tan(
            deg_to_rad(camera_.vertical_fov_degrees) * 0.5f);
        ray_direction = normalize(forward + right * (ndc_x * aspect * tan_half_fov) + up * (ndc_y * tan_half_fov));
    }

    const Vec3 normal = normalize(plane_normal);
    const float denominator = dot(ray_direction, normal);
    if (std::fabs(denominator) <= 0.000001f) {
        return false;
    }

    const float t = dot(plane_point - ray_origin, normal) / denominator;
    if (t <= 0.0f) {
        return false;
    }

    const Vec3 hit = ray_origin + ray_direction * t;
    result = CPoint3d(hit.x, hit.y, hit.z);
    return true;
}

bool OpenGLViewport::ScreenToCurvePlane(const QPoint& point, CPoint3d& result) {
    if (SnapCreationPoint(point, result, false)) return true;
    if (IsSnapTargetEnabled(SnapTarget::WorkPlane)) {
        if (ScreenToSketchPlane(point, result)) return true;
        if (tool_ != ToolMode::DrawSpline) return false;
    }
    if (xy_plane_view_enabled_) {
        return ScreenToWorldPlane(point, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, result);
    }

    if (tool_ == ToolMode::DrawSpline) {
        if (draw_spline_raw_points_.empty()) {
            // Freehand drawing is a screen-oriented operation. The floor is
            // edge-on in front/side views, so a camera ray cannot intersect
            // it there. Start the stroke on the view plane through the camera
            // target; all following samples stay on that same plane.
            return ScreenToViewPlane(point, camera_.target, result);
        }
        const CPoint3d& anchor = draw_spline_raw_points_.front();
        return ScreenToViewPlane(
            point,
            {static_cast<float>(anchor.x), static_cast<float>(anchor.y),
             static_cast<float>(anchor.z)},
            result);
    }

    const std::vector<CPoint3d>& points = tool_ == ToolMode::DrawBSpline
        ? document_->GetActiveBSpline().GetPoints()
        : document_->GetActivePolyline().GetPoints();
    if (points.empty()) {
        CurvePoint floor_point{};
        if (!renderer_.ScreenToFloor(point.x(), point.y(), width(), height(), camera_, orthographic_projection_, floor_point)) {
            return false;
        }
        result = CPoint3d(floor_point.x, kCurvePlaneY, floor_point.z);
        return true;
    }

    const CPoint3d& anchor = points.back();
    return ScreenToViewPlane(point,
                             {static_cast<float>(anchor.x), static_cast<float>(anchor.y), static_cast<float>(anchor.z)},
                             result);
}

bool OpenGLViewport::ScreenToPlaneY(const QPoint& point, double y, CPoint3d& result) const {
    const int viewport_width = std::max(1, width());
    const int viewport_height = std::max(1, height());
    const float ndc_x = 2.0f * static_cast<float>(point.x()) / static_cast<float>(viewport_width) - 1.0f;
    const float ndc_y = 1.0f - 2.0f * static_cast<float>(point.y()) / static_cast<float>(viewport_height);
    const float aspect = static_cast<float>(viewport_width) / static_cast<float>(viewport_height);

    Vec3 forward{};
    Vec3 right{};
    Vec3 up{};
    viewport_camera_basis(camera_, forward, right, up);

    Vec3 ray_origin = camera_position(camera_, orthographic_projection_);
    Vec3 ray_direction{};
    if (orthographic_projection_) {
        const float half_height = std::max(
            kMinimumOrthographicHalfHeight, camera_.distance * 0.42f);
        const float half_width = half_height * aspect;
        ray_origin = camera_position(camera_, orthographic_projection_) + right * (ndc_x * half_width) + up * (ndc_y * half_height);
        ray_direction = forward;
    } else {
        const float tan_half_fov = std::tan(
            deg_to_rad(camera_.vertical_fov_degrees) * 0.5f);
        ray_direction = normalize(forward + right * (ndc_x * aspect * tan_half_fov) + up * (ndc_y * tan_half_fov));
    }

    if (std::fabs(ray_direction.y) <= 0.000001f) {
        return false;
    }

    const float t = (static_cast<float>(y) - ray_origin.y) / ray_direction.y;
    if (t <= 0.0f) {
        return false;
    }

    const Vec3 hit = ray_origin + ray_direction * t;
    result = CPoint3d(hit.x, y, hit.z);
    return true;
}

bool OpenGLViewport::ScreenToViewPlane(const QPoint& point, Vec3 plane_point, CPoint3d& result) const {
    const int viewport_width = std::max(1, width());
    const int viewport_height = std::max(1, height());
    const float ndc_x = 2.0f * static_cast<float>(point.x()) / static_cast<float>(viewport_width) - 1.0f;
    const float ndc_y = 1.0f - 2.0f * static_cast<float>(point.y()) / static_cast<float>(viewport_height);
    const float aspect = static_cast<float>(viewport_width) / static_cast<float>(viewport_height);

    Vec3 forward{};
    Vec3 right{};
    Vec3 up{};
    viewport_camera_basis(camera_, forward, right, up);

    Vec3 ray_origin = camera_position(camera_, orthographic_projection_);
    Vec3 ray_direction{};
    if (orthographic_projection_) {
        const float half_height = std::max(
            kMinimumOrthographicHalfHeight, camera_.distance * 0.42f);
        const float half_width = half_height * aspect;
        ray_origin = camera_position(camera_, orthographic_projection_) + right * (ndc_x * half_width) + up * (ndc_y * half_height);
        ray_direction = forward;
    } else {
        const float tan_half_fov = std::tan(
            deg_to_rad(camera_.vertical_fov_degrees) * 0.5f);
        ray_direction = normalize(forward + right * (ndc_x * aspect * tan_half_fov) + up * (ndc_y * tan_half_fov));
    }

    const float denominator = dot(ray_direction, forward);
    if (std::fabs(denominator) <= 0.000001f) {
        return false;
    }

    const float t = dot(plane_point - ray_origin, forward) / denominator;
    if (t <= 0.0f) {
        return false;
    }

    const Vec3 hit = ray_origin + ray_direction * t;
    result = CPoint3d(hit.x, hit.y, hit.z);
    return true;
}

void OpenGLViewport::UpdateHoveredSolidEdge(const QPoint& point) {
    if (!document_) {
        ClearHoveredSolidEdge();
        return;
    }

    const DomPoint screen_point{point.x(), point.y()};
    auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
        return renderer_.WorldToScreen(
            world, camera_, orthographic_projection_, width(), height(), screen);
    };

    size_t object_index = static_cast<size_t>(-1);
    int surface_index = -1;
    int edge_index = -1;
    document_->FindSolidEdgeAtScreen(
        screen_point,
        world_to_screen,
        static_cast<float>(capture_distance_pixels_),
        object_index,
        surface_index,
        edge_index);

    if (object_index == hovered_edge_object_index_
        && surface_index == hovered_edge_surface_index_
        && edge_index == hovered_edge_index_) {
        return;
    }
    hovered_edge_object_index_ = object_index;
    hovered_edge_surface_index_ = surface_index;
    hovered_edge_index_ = edge_index;
    update();
}

void OpenGLViewport::ClearHoveredSolidEdge() {
    if (hovered_edge_object_index_ == static_cast<size_t>(-1)
        && hovered_edge_surface_index_ < 0
        && hovered_edge_index_ < 0) {
        return;
    }
    hovered_edge_object_index_ = static_cast<size_t>(-1);
    hovered_edge_surface_index_ = -1;
    hovered_edge_index_ = -1;
    update();
}

void OpenGLViewport::DrawHoveredSolidEdge() {
    if (!document_
        || tool_ != ToolMode::Select
        || selection_mode_ != SelectionMode::Edge
        || hovered_edge_object_index_ >= document_->GetObjects().size()) {
        return;
    }

    const auto* solid = dynamic_cast<const CSolid*>(
        document_->GetObjects()[hovered_edge_object_index_].get());
    if (!solid) {
        return;
    }
    const auto edge_ref = std::make_pair(
        hovered_edge_surface_index_, hovered_edge_index_);
    const auto& selected_edges = solid->GetSelectedEdgeRefs();
    if (std::find(selected_edges.begin(), selected_edges.end(), edge_ref)
        != selected_edges.end()) {
        return;
    }

    const CSurfaceFace* surface = solid->GetSurfaceFace(
        hovered_edge_surface_index_);
    std::vector<Vec3> points;
    if (!surface
        || !surface->GetEdgePolylinePoints(hovered_edge_index_, points)
        || points.size() < 2) {
        return;
    }

    const GLboolean depth_test_enabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean lighting_enabled = glIsEnabled(GL_LIGHTING);
    const GLboolean blend_enabled = glIsEnabled(GL_BLEND);
    const GLboolean line_smooth_enabled = glIsEnabled(GL_LINE_SMOOTH);
    GLfloat previous_line_width = 1.0f;
    glGetFloatv(GL_LINE_WIDTH, &previous_line_width);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    const auto draw_edge = [&points](float width, Color color, float alpha) {
        glLineWidth(width);
        glColor4f(color.r, color.g, color.b, alpha);
        glBegin(GL_LINE_STRIP);
        for (const Vec3& point : points) {
            glVertex3f(point.x, point.y, point.z);
        }
        glEnd();
    };
    draw_edge(6.0f, {0.08f, 0.02f, 0.08f}, 0.90f);
    draw_edge(3.4f, {1.0f, 0.05f, 0.92f}, 1.0f);

    glLineWidth(previous_line_width);
    if (!line_smooth_enabled) {
        glDisable(GL_LINE_SMOOTH);
    }
    if (!blend_enabled) {
        glDisable(GL_BLEND);
    }
    if (lighting_enabled) {
        glEnable(GL_LIGHTING);
    }
    if (depth_test_enabled) {
        glEnable(GL_DEPTH_TEST);
    }
}

void OpenGLViewport::DrawCurveRubberBand() {
    if (!document_ || spatial_curve_preview_kind_
            == SpatialCurvePreviewKind::None
        || spatial_curve_preview_points_.empty()) {
        return;
    }

    std::vector<CPoint3d> points = spatial_curve_preview_points_;
    points.push_back(curve_preview_point_);
    if (points.size() < 2) return;

    const GLboolean depth_enabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean lighting_enabled = glIsEnabled(GL_LIGHTING);
    const GLboolean blend_enabled = glIsEnabled(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(2.5f);
    glColor4f(1.0f, 0.12f, 0.35f, 0.96f);
    glBegin(GL_LINE_STRIP);
    if (spatial_curve_preview_kind_
        == SpatialCurvePreviewKind::Polyline) {
        for (const CPoint3d& point : points) {
            glVertex3f(static_cast<float>(point.x),
                       static_cast<float>(point.y),
                       static_cast<float>(point.z));
        }
    } else {
        CBSpline preview("Curve Preview");
        if (spatial_curve_preview_kind_
            == SpatialCurvePreviewKind::Bezier) {
            preview.SetBezierInterpolationPoints(points);
        } else {
            preview.SetCurveType(spatial_curve_preview_kind_
                    == SpatialCurvePreviewKind::Nurbs
                ? SplineCurveType::Nurbs
                : SplineCurveType::BSpline);
            for (const CPoint3d& point : points) {
                preview.AddPoint(point);
            }
            preview.SetDegree(static_cast<int>(
                std::min<size_t>(3, points.size() - 1)));
        }

        const int samples = std::max(
            32, static_cast<int>(points.size() - 1) * 32);
        for (int sample = 0; sample <= samples; ++sample) {
            const CPoint3d point = preview.Evaluate(
                static_cast<float>(sample)
                / static_cast<float>(samples));
            glVertex3f(static_cast<float>(point.x),
                       static_cast<float>(point.y),
                       static_cast<float>(point.z));
        }
    }
    glEnd();

    // Keep the provisional next node visible on top of the scene.
    glPointSize(6.0f);
    glColor4f(1.0f, 0.92f, 0.92f, 1.0f);
    glBegin(GL_POINTS);
    glVertex3f(static_cast<float>(curve_preview_point_.x),
               static_cast<float>(curve_preview_point_.y),
               static_cast<float>(curve_preview_point_.z));
    glEnd();
    glPointSize(1.0f);
    glLineWidth(1.0f);
    if (!blend_enabled) glDisable(GL_BLEND);
    if (lighting_enabled) glEnable(GL_LIGHTING);
    if (depth_enabled) glEnable(GL_DEPTH_TEST);
}

void OpenGLViewport::DrawSketchRectanglePreview() {
    if (sketch_shape_kind_ != 0 && tool_ == ToolMode::SketchRectangle) {
        if (const auto shape = BuildSketchShape(sketch_rectangle_preview_point_)) { shape->Render3d(true); draw_primitive_grips(*shape); }
        return;
    }
    const std::vector<CPoint3d> points = SketchRectanglePoints(sketch_rectangle_first_point_, sketch_rectangle_preview_point_);
    if (points.size() != 4) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (tool_ == ToolMode::SolidBoxRectangle) glColor4f(0.2f, 0.25f, 0.4f, 0.3f);
    else glColor4f(1.0f, 0.95f, 0.05f, 0.12f);
    glBegin(GL_QUADS);
    for (const CPoint3d& point : points) {
        glVertex3f(static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z));
    }
    glEnd();

    glLineWidth(2.0f);
    if (tool_ == ToolMode::SolidBoxRectangle) glColor4f(1.0f, 0.15f, 0.6f, 1.0f);
    else glColor4f(1.0f, 0.95f, 0.05f, 0.95f);
    glBegin(GL_LINE_LOOP);
    for (const CPoint3d& point : points) {
        glVertex3f(static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z));
    }
    glEnd();
    glLineWidth(1.0f);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void OpenGLViewport::DrawSplinePreview() {
    if (draw_spline_raw_points_.empty()) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glPointSize(4.0f);
    glColor4f(0.92f, 0.92f, 0.92f, 0.85f);
    glBegin(GL_POINTS);
    for (const CPoint3d& point : draw_spline_raw_points_) {
        glVertex3f(static_cast<float>(point.x), static_cast<float>(point.y),
                   static_cast<float>(point.z));
    }
    glEnd();
    glPointSize(1.0f);
    glEnable(GL_DEPTH_TEST);

    if (draw_spline_preview_points_.size() < 2) {
        return;
    }
    CBSpline preview("Draw Spline Preview");
    preview.SetCurveType(SplineCurveType::BSpline);
    preview.SetDegree(std::min(
        3, static_cast<int>(draw_spline_preview_points_.size()) - 1));
    for (const CPoint3d& point : draw_spline_preview_points_) {
        preview.AddPoint(point);
    }
    preview.Render3d(true);
}

void OpenGLViewport::DrawSolidCylinderCirclePreview() {
    const Vec3 center{
        static_cast<float>(sketch_rectangle_first_point_.x),
        static_cast<float>(sketch_rectangle_first_point_.y),
        static_cast<float>(sketch_rectangle_first_point_.z)};
    const Vec3 radius_point{
        static_cast<float>(sketch_rectangle_preview_point_.x),
        static_cast<float>(sketch_rectangle_preview_point_.y),
        static_cast<float>(sketch_rectangle_preview_point_.z)};
    const Vec3 delta = radius_point - center;
    const float du = dot(delta, sketch_u_);
    const float dv = dot(delta, sketch_v_);
    const float radius = std::sqrt(du * du + dv * dv);
    if (radius <= 0.0001f) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    constexpr int segments = 64;
    glColor4f(0.2f, 0.25f, 0.4f, 0.3f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(center.x, center.y, center.z);
    for (int i = 0; i <= segments; ++i) {
        const float angle = 2.0f * 3.14159265358979323846f
            * static_cast<float>(i) / static_cast<float>(segments);
        const Vec3 point = center
            + sketch_u_ * (std::cos(angle) * radius)
            + sketch_v_ * (std::sin(angle) * radius);
        glVertex3f(point.x, point.y, point.z);
    }
    glEnd();

    glLineWidth(2.0f);
    glColor4f(1.0f, 0.15f, 0.6f, 1.0f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        const float angle = 2.0f * 3.14159265358979323846f
            * static_cast<float>(i) / static_cast<float>(segments);
        const Vec3 point = center
            + sketch_u_ * (std::cos(angle) * radius)
            + sketch_v_ * (std::sin(angle) * radius);
        glVertex3f(point.x, point.y, point.z);
    }
    glEnd();
    glLineWidth(1.0f);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void OpenGLViewport::DrawSketchFilletRadiusPreview() {
    if (!sketch_fillet_preview_valid_ || sketch_fillet_radius_ <= 0.0) {
        return;
    }

    const Vec3 center{
        static_cast<float>(sketch_fillet_preview_center_.x),
        static_cast<float>(sketch_fillet_preview_center_.y),
        static_cast<float>(sketch_fillet_preview_center_.z)};
    const Vec3 u_axis = normalize(sketch_u_);
    const Vec3 v_axis = normalize(sketch_v_);
    const float radius = static_cast<float>(sketch_fillet_radius_);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(2.0f);
    glColor4f(0.10f, 0.85f, 1.0f, 0.95f);
    constexpr int segments = 64;
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        const float angle = 2.0f * 3.14159265358979323846f
            * static_cast<float>(i) / static_cast<float>(segments);
        const Vec3 point = center
            + u_axis * (std::cos(angle) * radius)
            + v_axis * (std::sin(angle) * radius);
        glVertex3f(point.x, point.y, point.z);
    }
    glEnd();

    const float marker = radius * 0.08f;
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    glVertex3f((center - u_axis * marker).x, (center - u_axis * marker).y, (center - u_axis * marker).z);
    glVertex3f((center + u_axis * marker).x, (center + u_axis * marker).y, (center + u_axis * marker).z);
    glVertex3f((center - v_axis * marker).x, (center - v_axis * marker).y, (center - v_axis * marker).z);
    glVertex3f((center + v_axis * marker).x, (center + v_axis * marker).y, (center + v_axis * marker).z);
    glEnd();
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void OpenGLViewport::DrawSketchPolylinePreview() {
    if (sketch_polyline_points_.empty()) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glLineWidth(2.5f);
    glColor4f(0.95f, 0.15f, 0.75f, 1.0f);
    glBegin(GL_LINE_STRIP);
    for (const CPoint3d& point : sketch_polyline_points_) {
        glVertex3f(static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z));
    }
    if (sketch_polyline_preview_valid_) {
        glVertex3f(
            static_cast<float>(sketch_polyline_preview_point_.x),
            static_cast<float>(sketch_polyline_preview_point_.y),
            static_cast<float>(sketch_polyline_preview_point_.z));
    }
    glEnd();

    glPointSize(8.0f);
    glColor4f(0.0f, 1.0f, 0.2f, 1.0f);
    glBegin(GL_POINTS);
    for (const CPoint3d& point : sketch_polyline_points_) {
        glVertex3f(static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z));
    }
    glEnd();

    glPointSize(1.0f);
    glLineWidth(1.0f);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void OpenGLViewport::DrawSketchBezierPreview() {
    if (sketch_bezier_points_.empty()) {
        return;
    }

    std::vector<CPoint3d> controls = sketch_bezier_points_;
    while (controls.size() < 4) {
        controls.push_back(
            sketch_bezier_preview_valid_
                ? sketch_bezier_preview_point_
                : controls.back());
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glLineWidth(1.0f);
    glColor4f(0.35f, 0.8f, 1.0f, 0.75f);
    glBegin(GL_LINE_STRIP);
    for (const CPoint3d& control : controls) {
        glVertex3d(control.x, control.y, control.z);
    }
    glEnd();

    glLineWidth(2.5f);
    glColor4f(0.95f, 0.15f, 0.75f, 1.0f);
    glBegin(GL_LINE_STRIP);
    for (int index = 0; index <= 32; ++index) {
        const double t = static_cast<double>(index) / 32.0;
        const double u = 1.0 - t;
        const double b0 = u * u * u;
        const double b1 = 3.0 * u * u * t;
        const double b2 = 3.0 * u * t * t;
        const double b3 = t * t * t;
        glVertex3d(
            b0 * controls[0].x + b1 * controls[1].x
                + b2 * controls[2].x + b3 * controls[3].x,
            b0 * controls[0].y + b1 * controls[1].y
                + b2 * controls[2].y + b3 * controls[3].y,
            b0 * controls[0].z + b1 * controls[1].z
                + b2 * controls[2].z + b3 * controls[3].z);
    }
    glEnd();

    glPointSize(8.0f);
    glColor4f(0.0f, 1.0f, 0.2f, 1.0f);
    glBegin(GL_POINTS);
    for (const CPoint3d& control : sketch_bezier_points_) {
        glVertex3d(control.x, control.y, control.z);
    }
    glEnd();
    glPointSize(1.0f);
    glLineWidth(1.0f);
    glEnable(GL_DEPTH_TEST);
}

void OpenGLViewport::DrawVisibleCurvePoints() {
    if (!document_) return;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glPointSize(7.0f);
    glColor3f(0.0f, 0.9f, 0.15f);
    glBegin(GL_POINTS);
    for (const auto& object : document_->GetObjects()) {
        if (!object || !document_->IsObjectSelectable(*object)) continue;
        if (const auto* polyline = dynamic_cast<const CPolyline*>(object.get())) {
            for (const CPoint3d& point : polyline->GetPoints()) {
                glVertex3d(point.x, point.y, point.z);
            }
        } else if (const auto* spline = dynamic_cast<const CBSpline*>(object.get())) {
            const auto& points = spline->GetPoints();
            for (size_t index = 0; index < points.size(); ++index) {
                if (spline->IsBezierChain() && index % 3 != 0)
                    glColor3f(0.15f, 0.75f, 1.0f);
                else glColor3f(0.0f, 0.9f, 0.15f);
                glVertex3d(points[index].x, points[index].y, points[index].z);
            }
        } else if (const auto* sketch = dynamic_cast<const CSmartLine*>(object.get())) {
            glColor3f(0.0f, 0.9f, 0.15f);
            for (size_t index = 0; index < sketch->GetNodeCount(); ++index) {
                const CPoint3d point = sketch->GetNodeWorld(index);
                glVertex3d(point.x, point.y, point.z);
            }
            glColor3f(0.15f, 0.75f, 1.0f);
            for (size_t index = 0; index < sketch->GetBezierControlPointCount(); ++index) {
                const CPoint3d point = sketch->GetBezierControlPointWorld(index);
                glVertex3d(point.x, point.y, point.z);
            }
        }
        glColor3f(0.0f, 0.9f, 0.15f);
    }
    glEnd();
    glPointSize(1.0f);
    glEnable(GL_DEPTH_TEST);
}

void OpenGLViewport::DrawSelectedCurvePointHandles() {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    const auto& objects = document_->GetObjects();

    // Bezier handles are much easier to understand when their control arms
    // remain visible together with the selected curve.
    glLineWidth(1.0f);
    glColor3f(0.2f, 0.65f, 0.95f);
    glBegin(GL_LINES);
    for (size_t object_index : document_->GetSelectedObjectIndices()) {
        if (object_index >= objects.size() || !objects[object_index]) continue;
        const auto* spline = dynamic_cast<const CBSpline*>(
            objects[object_index].get());
        if (!spline) continue;
        const auto& points = spline->GetPoints();
        if (!spline->IsBezierChain()) {
            for (size_t index = 1; index < points.size(); ++index) {
                glVertex3d(points[index - 1].x, points[index - 1].y,
                           points[index - 1].z);
                glVertex3d(points[index].x, points[index].y,
                           points[index].z);
            }
            if (spline->IsClosed() && points.size() > 2) {
                glVertex3d(points.back().x, points.back().y, points.back().z);
                glVertex3d(points.front().x, points.front().y, points.front().z);
            }
            continue;
        }
        for (size_t anchor = 0; anchor < points.size(); anchor += 3) {
            if (anchor > 0) {
                glVertex3d(points[anchor].x, points[anchor].y, points[anchor].z);
                glVertex3d(points[anchor - 1].x, points[anchor - 1].y,
                           points[anchor - 1].z);
            }
            if (anchor + 1 < points.size()) {
                glVertex3d(points[anchor].x, points[anchor].y, points[anchor].z);
                glVertex3d(points[anchor + 1].x, points[anchor + 1].y,
                           points[anchor + 1].z);
            }
        }
    }
    glEnd();

    // Show editable handles for selected curves without marking their nodes as
    // selected. Green means an ordinary node, blue means a Bezier control.
    glPointSize(8.0f);
    glBegin(GL_POINTS);
    for (size_t object_index : document_->GetSelectedObjectIndices()) {
        if (object_index >= objects.size() || !objects[object_index]) continue;
        if (const auto* polyline = dynamic_cast<const CPolyline*>(
                objects[object_index].get())) {
            if (!show_curve_points_) {
                glColor3f(0.0f, 0.9f, 0.15f);
                for (const CPoint3d& point : polyline->GetPoints()) {
                    glVertex3d(point.x, point.y, point.z);
                }
            }
        } else if (const auto* spline = dynamic_cast<const CBSpline*>(
                       objects[object_index].get())) {
            const auto& points = spline->GetPoints();
            for (size_t point_index = 0;
                 point_index < points.size(); ++point_index) {
                const bool bezier_control = spline->IsBezierChain()
                    && point_index % 3 != 0;
                if (show_curve_points_) continue;
                if (bezier_control) glColor3f(0.15f, 0.75f, 1.0f);
                else glColor3f(0.0f, 0.9f, 0.15f);
                const CPoint3d& point = points[point_index];
                glVertex3d(point.x, point.y, point.z);
            }
        }
    }
    glEnd();

    // Only explicitly selected points receive the yellow selection marker.
    glPointSize(13.0f);
    glColor3f(1.0f, 0.85f, 0.0f);
    glBegin(GL_POINTS);
    for (const auto& selected : document_->GetSelectedCurvePoints()) {
        if (selected.first >= objects.size() || !objects[selected.first]) {
            continue;
        }
        CPoint3d point{};
        bool found = false;
        if (const auto* polyline = dynamic_cast<const CPolyline*>(
                objects[selected.first].get())) {
            if (selected.second < polyline->GetPoints().size()) {
                point = polyline->GetPoints()[selected.second];
                found = true;
            }
        } else if (const auto* spline = dynamic_cast<const CBSpline*>(
                       objects[selected.first].get())) {
            if (selected.second < spline->GetPoints().size()) {
                point = spline->GetPoints()[selected.second];
                found = true;
            }
        } else if (const auto* sketch = dynamic_cast<const CSmartLine*>(
                       objects[selected.first].get())) {
            if (selected.second < sketch->GetNodeCount()) {
                point = sketch->GetNodeWorld(selected.second);
                found = true;
            }
        }
        if (!found) continue;
        glVertex3d(point.x, point.y, point.z);
    }
    glEnd();
    glPointSize(1.0f);
    glEnable(GL_DEPTH_TEST);
}

void OpenGLViewport::DrawSketchEditHandles() {
    const CSmartLine* sketch = document_ ? EditableSketch() : nullptr;
    if (!sketch) {
        return;
    }

    if(sketch->GetPrimitive().kind) {
        draw_primitive_grips(*sketch,highlighted_sketch_handle_kind_==SketchHandleKind::Primitive
            ? int(highlighted_sketch_handle_index_) : -1);
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);

    glPointSize(10.0f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_POINTS);
    for (size_t index = 0; index < sketch->GetNodeCount(); ++index) {
        if (highlighted_sketch_handle_kind_ == SketchHandleKind::Node
            && highlighted_sketch_handle_index_ == index) {
            continue;
        }
        const CPoint3d point = sketch->GetNodeWorld(index);
        glVertex3f(static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z));
    }
    glEnd();

    glColor3f(0.2f, 0.75f, 1.0f);
    glBegin(GL_POINTS);
    for (size_t index = 0; index < sketch->GetBezierControlPointCount(); ++index) {
        if (highlighted_sketch_handle_kind_ == SketchHandleKind::BezierControl
            && highlighted_sketch_handle_index_ == index) {
            continue;
        }
        const CPoint3d point = sketch->GetBezierControlPointWorld(index);
        glVertex3d(point.x, point.y, point.z);
    }
    glEnd();

    glColor3f(0.1f, 0.45f, 1.0f);
    glBegin(GL_POINTS);
    for (size_t index = 0; index < sketch->GetArcGripCount(); ++index) {
        if (highlighted_sketch_handle_kind_ == SketchHandleKind::ArcControl
            && highlighted_sketch_handle_index_ == index) {
            continue;
        }
        const CPoint3d point = sketch->GetArcGripWorld(index);
        glVertex3d(point.x, point.y, point.z);
    }
    glEnd();

    glColor3f(1.0f, 0.05f, 0.05f);
    glBegin(GL_POINTS);
    for (size_t index = 0; index < sketch->GetNumFillets(); ++index) {
        if (highlighted_sketch_handle_kind_ == SketchHandleKind::Fillet
            && highlighted_sketch_handle_index_ == index) {
            continue;
        }
        const CFillet* fillet = sketch->GetFillet(index);
        if (!fillet) {
            continue;
        }
        const CLinkLine* first = sketch->GetLine(fillet->GetFirstLineIndex());
        const CLinkLine* second = sketch->GetLine(fillet->GetSecondLineIndex());
        if (!first || !second || !fillet->Calculate(*first, *second).valid) {
            continue;
        }
        const CPoint3d point = sketch->GetFilletGripWorld(index);
        glVertex3f(static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z));
    }
    glEnd();

    if (highlighted_sketch_handle_kind_ != SketchHandleKind::None) {
        CPoint3d point{};
        if (highlighted_sketch_handle_kind_ == SketchHandleKind::Node) {
            point = sketch->GetNodeWorld(highlighted_sketch_handle_index_);
        } else if (highlighted_sketch_handle_kind_ == SketchHandleKind::Fillet) {
            point = sketch->GetFilletGripWorld(highlighted_sketch_handle_index_);
        } else if (highlighted_sketch_handle_kind_ == SketchHandleKind::ArcControl) {
            point = sketch->GetArcGripWorld(highlighted_sketch_handle_index_);
        } else {
            point = sketch->GetBezierControlPointWorld(highlighted_sketch_handle_index_);
        }
        glPointSize(14.0f);
        glColor3f(1.0f, 0.9f, 0.0f);
        glBegin(GL_POINTS);
        glVertex3f(static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z));
        glEnd();
    }

    glPointSize(1.0f);
    glEnable(GL_DEPTH_TEST);
}

void OpenGLViewport::DrawEditPointSelectionRect() {
    const QRect rect = QRect(edit_point_selection_start_, edit_point_selection_current_).normalized();
    if (rect.width() <= 0 && rect.height() <= 0) {
        return;
    }

    const float left = static_cast<float>(rect.left());
    const float right = static_cast<float>(rect.right());
    const float top = static_cast<float>(rect.top());
    const float bottom = static_cast<float>(rect.bottom());

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, width(), height(), 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(1.0f, 0.95f, 0.05f, 0.12f);
    glBegin(GL_QUADS);
    glVertex2f(left, top);
    glVertex2f(right, top);
    glVertex2f(right, bottom);
    glVertex2f(left, bottom);
    glEnd();

    glLineWidth(0.75f);
    glColor4f(1.0f, 0.95f, 0.05f, 0.95f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(left, top);
    glVertex2f(right, top);
    glVertex2f(right, bottom);
    glVertex2f(left, bottom);
    glEnd();
    glLineWidth(1.0f);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void OpenGLViewport::DrawSelectRubberBandRect() {
    const QRect rect = QRect(rect_selection_start_, rect_selection_current_).normalized();
    if (rect.width() <= 0 && rect.height() <= 0) {
        return;
    }

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, width(), height(), 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const bool crossing = rect_selection_current_.x() >= rect_selection_start_.x();
    if (crossing) {
        glColor4f(0.20f, 0.90f, 0.34f, 0.10f);
    } else {
        glColor4f(0.20f, 0.55f, 1.00f, 0.10f);
    }
    glBegin(GL_QUADS);
    glVertex2i(rect.left(), rect.top());
    glVertex2i(rect.right(), rect.top());
    glVertex2i(rect.right(), rect.bottom());
    glVertex2i(rect.left(), rect.bottom());
    glEnd();

    if (crossing) {
        glColor4f(0.25f, 1.00f, 0.40f, 0.95f);
    } else {
        glColor4f(0.28f, 0.65f, 1.00f, 0.95f);
    }
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(rect.left(), rect.top());
    glVertex2i(rect.right(), rect.top());
    glVertex2i(rect.right(), rect.bottom());
    glVertex2i(rect.left(), rect.bottom());
    glEnd();
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void OpenGLViewport::DrawZoomRubberBandRect() {
    const QRect rect = QRect(zoom_rect_start_, zoom_rect_current_).normalized();
    if (rect.width() <= 0 && rect.height() <= 0) {
        return;
    }

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, width(), height(), 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(0.62f, 0.34f, 1.00f, 0.12f);
    glBegin(GL_QUADS);
    glVertex2i(rect.left(), rect.top());
    glVertex2i(rect.right(), rect.top());
    glVertex2i(rect.right(), rect.bottom());
    glVertex2i(rect.left(), rect.bottom());
    glEnd();

    glColor4f(0.72f, 0.48f, 1.00f, 1.00f);
    glLineWidth(1.4f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(rect.left(), rect.top());
    glVertex2i(rect.right(), rect.top());
    glVertex2i(rect.right(), rect.bottom());
    glVertex2i(rect.left(), rect.bottom());
    glEnd();

    glLineWidth(1.0f);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

bool OpenGLViewport::ApplyZoomRect() {
    const QRect rect = QRect(zoom_rect_start_, zoom_rect_current_).normalized();
    const int viewport_width = std::max(1, width());
    const int viewport_height = std::max(1, height());
    if (rect.width() < 8 || rect.height() < 8) {
        return false;
    }

    CPoint3d center_world{};
    if (!ScreenToViewPlane(rect.center(), camera_.target, center_world)) {
        return false;
    }

    const float width_factor =
        static_cast<float>(rect.width()) / static_cast<float>(viewport_width);
    const float height_factor =
        static_cast<float>(rect.height()) / static_cast<float>(viewport_height);
    const float fit_factor = std::clamp(
        std::max(width_factor, height_factor) * 1.05f,
        0.001f,
        1.0f);

    camera_.target = {
        static_cast<float>(center_world.x),
        static_cast<float>(center_world.y),
        static_cast<float>(center_world.z)};
    camera_.distance = std::clamp(
        camera_.distance * fit_factor,
        kMinimumCameraDistance,
        100000.0f);
    update();
    return true;
}

Vec3 OpenGLViewport::AxisVector(TransformAxis axis) const {
    if (axis == TransformAxis::X) {
        return {1.0f, 0.0f, 0.0f};
    }
    if (axis == TransformAxis::Y) {
        return {0.0f, 1.0f, 0.0f};
    }
    if (axis == TransformAxis::Z) {
        return {0.0f, 0.0f, 1.0f};
    }
    return {};
}

float OpenGLViewport::DistanceToScreenSegment(DomPoint point, DomPoint start, DomPoint end) const {
    const float dx = static_cast<float>(end.x - start.x);
    const float dy = static_cast<float>(end.y - start.y);
    const float length_sq = dx * dx + dy * dy;
    if (length_sq <= 0.0001f) {
        const float px = static_cast<float>(point.x - start.x);
        const float py = static_cast<float>(point.y - start.y);
        return std::sqrt(px * px + py * py);
    }

    const float t = std::clamp((static_cast<float>(point.x - start.x) * dx + static_cast<float>(point.y - start.y) * dy) / length_sq, 0.0f, 1.0f);
    const float closest_x = static_cast<float>(start.x) + t * dx;
    const float closest_y = static_cast<float>(start.y) + t * dy;
    const float px = static_cast<float>(point.x) - closest_x;
    const float py = static_cast<float>(point.y) - closest_y;
    return std::sqrt(px * px + py * py);
}

bool OpenGLViewport::HitTestRotationAxisLine(
    const QPoint& point, Vec3& start, Vec3& end) const {
    if (!document_) {
        return false;
    }

    constexpr float kTolerance = 12.0f;
    const DomPoint mouse{point.x(), point.y()};
    const auto world_to_screen = [this](Vec3 world, DomPoint& screen) {
        return renderer_.WorldToScreen(
            world, camera_, orthographic_projection_, width(), height(), screen);
    };

    float best_distance = kTolerance;
    bool found = !coordinate_axis_selection_ && document_->FindRotationAxisLineAtScreen(
        mouse, world_to_screen, kTolerance,
        start, end, &best_distance);

    if (show_coordinate_axes_ || coordinate_axis_selection_) {
        const float axis_length = xy_plane_view_enabled_
            ? grid_size_
            : grid_size_ * 0.5f;
        const Vec3 origin{};
        const Vec3 axis_ends[] = {
            {axis_length, 0.0f, 0.0f},
            {0.0f, axis_length, 0.0f},
            {0.0f, 0.0f, axis_length}
        };
        for (const Vec3& axis_end : axis_ends) {
            DomPoint screen_start{};
            DomPoint screen_end{};
            if (!world_to_screen(origin, screen_start)
                || !world_to_screen(axis_end, screen_end)) {
                continue;
            }
            const float distance = DistanceToScreenSegment(
                mouse, screen_start, screen_end);
            if (distance <= best_distance) {
                best_distance = distance;
                start = origin;
                end = axis_end;
                found = true;
            }
        }
    }
    return found;
}

void OpenGLViewport::DrawRotationAxisPickPreview() {
    if ((!picking_rotation_axis_ && !coordinate_axis_selection_) || !rotation_axis_hover_valid_) {
        return;
    }

    glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT | GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(5.0f);
    glColor4f(1.0f, 0.86f, 0.06f, 1.0f);
    glBegin(GL_LINES);
    glVertex3f(rotation_axis_hover_start_.x,
               rotation_axis_hover_start_.y,
               rotation_axis_hover_start_.z);
    glVertex3f(rotation_axis_hover_end_.x,
               rotation_axis_hover_end_.y,
               rotation_axis_hover_end_.z);
    glEnd();
    glPopAttrib();
}

void OpenGLViewport::DrawPointToPointMeasurement() {
    const CPoint3d end = measurement_waiting_for_second_point_
        ? measurement_preview_ : measurement_end_;
    DomPoint start_screen{};
    if (!renderer_.WorldToScreen(
            point_to_vec3(measurement_start_), camera_,
            orthographic_projection_, width(), height(), start_screen)) {
        return;
    }

    // The scene renderer may leave GL in wireframe polygon mode. Filled
    // QPainter primitives then disappear on some drivers, while text remains.
    // Force a predictable overlay state before drawing the dimension.
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    const QColor dimension_color(0, 235, 245);
    const QColor outline_color(2, 12, 16, 235);
    painter.setPen(QPen(dimension_color, 2.2));
    painter.setBrush(Qt::NoBrush);
    const QPointF start(start_screen.x, start_screen.y);

    if (measurement_waiting_for_second_point_
        && !measurement_preview_valid_) {
        painter.drawEllipse(start, 4.0, 4.0);
        return;
    }

    DomPoint end_screen{};
    if (!renderer_.WorldToScreen(
            point_to_vec3(end), camera_, orthographic_projection_,
            width(), height(), end_screen)) {
        return;
    }
    const QPointF finish(end_screen.x, end_screen.y);
    const QLineF dimension_line(start, finish);
    if (dimension_line.length() < 0.5) {
        painter.drawEllipse(start, 4.0, 4.0);
        return;
    }

    const QPointF direction = (finish - start) / dimension_line.length();
    const QPointF normal(-direction.y(), direction.x());
    const bool short_dimension = dimension_line.length() < 58.0;
    const QPointF visible_start = short_dimension
        ? start - direction * 20.0 : start;
    const QPointF visible_finish = short_dimension
        ? finish + direction * 20.0 : finish;
    constexpr qreal arrow_length = 14.0;
    constexpr qreal arrow_width = 5.0;
    const auto draw_arrow = [&](const QPointF& tip,
                                const QPointF& inward) {
        const QPointF base = tip + inward * arrow_length;
        painter.drawLine(tip, base + normal * arrow_width);
        painter.drawLine(tip, base - normal * arrow_width);
    };
    const auto draw_geometry = [&](const QPen& pen) {
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(visible_start, visible_finish);
        draw_arrow(start, short_dimension ? -direction : direction);
        draw_arrow(finish, short_dimension ? direction : -direction);
        painter.drawEllipse(start, 3.2, 3.2);
        painter.drawEllipse(finish, 3.2, 3.2);
    };
    draw_geometry(QPen(outline_color, 4.6,
                       Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    draw_geometry(QPen(dimension_color, 2.2,
                       Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    const double dx = end.x - measurement_start_.x;
    const double dy = end.y - measurement_start_.y;
    const double dz = end.z - measurement_start_.z;
    const double distance_mm = std::sqrt(dx * dx + dy * dy + dz * dz);
    const DisplayLengthUnit unit = LoadDisplayLengthUnit();
    const QString text = QString("%1 %2")
        .arg(MillimetersToDisplay(distance_mm, unit), 0, 'f', 2)
        .arg(DisplayLengthUnitSuffix(unit));

    QFont font = painter.font();
    font.setPointSize(9);
    font.setBold(true);
    painter.setFont(font);
    const QFontMetrics metrics(font);
    const QSize text_size = metrics.size(Qt::TextSingleLine, text);
    const qreal label_offset = text_size.height() * 0.5 + 11.0;
    QPointF text_center = (start + finish) * 0.5 + normal * label_offset;
    if (text_center.x() - text_size.width() * 0.5 < 4.0
        || text_center.x() + text_size.width() * 0.5 > width() - 4.0
        || text_center.y() - text_size.height() * 0.5 < 4.0
        || text_center.y() + text_size.height() * 0.5 > height() - 4.0) {
        text_center = (start + finish) * 0.5 - normal * label_offset;
    }
    QRectF text_rect(
        text_center.x() - text_size.width() * 0.5 - 4.0,
        text_center.y() - text_size.height() * 0.5 - 2.0,
        text_size.width() + 8.0,
        text_size.height() + 4.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(4, 10, 13, 225));
    painter.drawRoundedRect(text_rect, 3.0, 3.0);
    painter.setPen(dimension_color);
    painter.drawText(text_rect, Qt::AlignCenter, text);
}

void OpenGLViewport::DrawSheetBendGuide() {
    QPolygonF arc;
    for (const auto& point : sheet_bend_guide_) {
        DomPoint screen{};
        if (!renderer_.WorldToScreen(point_to_vec3(point), camera_, orthographic_projection_,
                                      width(), height(), screen)) return;
        arc << QPointF(screen.x, screen.y);
    }
    if (arc.size() < 2) return;
    // This is a screen overlay, not geometry hidden by the sheet. Inherited
    // depth/stipple state otherwise clips the stroke into almost invisible dots.
    glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_POLYGON_BIT | GL_LINE_BIT);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LINE_STIPPLE);
    glDisable(GL_POLYGON_STIPPLE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::white, 10, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPolyline(arc);
    painter.setPen(QPen(Qt::black, 8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPolyline(arc);
    painter.setPen(QPen(QColor(255, 220, 0), 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPolyline(arc);
    const QPointF tip = arc.back();
    QPointF tangent;
    for (int i = arc.size() - 2; i >= 0; --i) {
        tangent = tip - arc[i];
        if (std::hypot(tangent.x(), tangent.y()) >= 8) break;
    }
    const double length = std::hypot(tangent.x(), tangent.y());
    if (length >= 1) {
    tangent /= length;
    const QPointF sideways(-tangent.y(), tangent.x());
    QPolygonF head;
    head << tip << tip - tangent * 22 + sideways * 11 << tip - tangent * 22 - sideways * 11;
    painter.setPen(QPen(Qt::white, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::black);
    painter.drawPolygon(head);
    painter.setPen(QPen(Qt::black, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor(255, 220, 0));
    painter.drawPolygon(head);
    }
    }
    glPopAttrib();
}

void OpenGLViewport::DrawSurfaceSplitPreview() {
    QPainterPath path;
    auto addLine=[&](const std::vector<CPoint3d>& line) {
    bool connected=false;
    for(const auto& point:line) {
        DomPoint screen{};
        if(!renderer_.WorldToScreen(point_to_vec3(point),camera_,orthographic_projection_,width(),height(),screen)) {
            connected=false;continue;
        }
        if(connected)path.lineTo(screen.x,screen.y);else path.moveTo(screen.x,screen.y);
        connected=true;
    }
    };
    addLine(surface_split_preview_);
    QPainterPath reference;
    if(!surface_alignment_preview_.empty()){reference=path;path=QPainterPath();}
    for(const auto& line:surface_alignment_preview_)addLine(line);
    // A construction overlay stays legible on shaded surfaces and zebra stripes,
    // without depth fighting against the coincident surface tessellation.
    glPushAttrib(GL_ENABLE_BIT|GL_DEPTH_BUFFER_BIT|GL_POLYGON_BIT|GL_LINE_BIT);
    glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_LIGHTING);
    glDisable(GL_LINE_STIPPLE);glDisable(GL_POLYGON_STIPPLE);glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    {
        QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
        if(!reference.isEmpty()) {
            painter.setPen(QPen(QColor(20,220,230),2,Qt::DashLine));painter.drawPath(reference);
        }
        painter.setPen(QPen(Qt::white,7,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));painter.drawPath(path);
        painter.setPen(QPen(Qt::black,5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));painter.drawPath(path);
        painter.setPen(QPen(QColor(255,205,0),3,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));painter.drawPath(path);
        for(size_t i=0;i<surface_boundary_handles_.size();++i) {
            QPoint screen;const auto& p=surface_boundary_handles_[i];
            if(!ProjectWorldPoint(point_to_vec3(p),screen))continue;
            painter.setPen(QPen(Qt::black,2));painter.setBrush(int(i)==surface_boundary_selected_?QColor(255,130,40):QColor(255,245,160));
            painter.drawEllipse(screen,6,6);
        }
        if(!reference.isEmpty()&&path.elementCount()>1) {
            for(int endpoint=0;endpoint<2;++endpoint){const auto element=path.elementAt(endpoint?path.elementCount()-1:0);QPointF point(element.x,element.y);
                if(!surface_boundary_handles_.empty()){QPoint screen;const auto& handle=endpoint?surface_boundary_handles_.back():surface_boundary_handles_.front();if(ProjectWorldPoint(point_to_vec3(handle),screen))point=screen;}
                QRectF label(point.x()+7,point.y()-22,42,20);painter.fillRect(label,QColor(0,0,0,180));painter.setPen(Qt::white);painter.drawText(label,Qt::AlignCenter,endpoint?"100%":"0%");}
        }
    }
    glPopAttrib();
}

bool OpenGLViewport::ProjectWorldPoint(Vec3 point, QPoint& screen) const {
    DomPoint projected{};
    if (!renderer_.WorldToScreen(point, camera_, orthographic_projection_, width(), height(), projected)) return false;
    screen = QPoint(projected.x, projected.y);
    return rect().contains(screen);
}

void OpenGLViewport::DrawPointPickMarkers() {
    std::vector<QPointF> screen_points;
    screen_points.reserve(point_pick_markers_.size());
    for (const CPoint3d& point : point_pick_markers_) {
        DomPoint screen{};
        if (renderer_.WorldToScreen(
                point_to_vec3(point), camera_, orthographic_projection_,
                width(), height(), screen)) {
            screen_points.emplace_back(screen.x, screen.y);
        }
    }
    if (screen_points.empty()) return;
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(10, 255, 70), 2.0));
    painter.setBrush(QColor(10, 255, 70, 210));
    for (size_t index = 1; index < screen_points.size(); ++index) {
        painter.drawLine(screen_points[index - 1], screen_points[index]);
    }
    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(11);
    painter.setFont(font);
    for (size_t index = 0; index < screen_points.size(); ++index) {
        const QPointF point = screen_points[index];
        painter.drawRect(QRectF(point.x() - 5.0, point.y() - 5.0, 10.0, 10.0));
        painter.drawText(point + QPointF(8.0, -7.0),
                         QString::number(index + 1));
    }
}

void OpenGLViewport::DrawCabinetPreview() {
    const std::string& tool_id = solid_dimension_object_.tool_id;
    if (tool_id != "cabinet"
        && tool_id != "cabinet_advanced"
        && tool_id != "cabinet_advanced_slx"
        && tool_id != "cabinet_showcase") {
        return;
    }

    const auto& parameters = solid_dimension_object_.parameters;
    const double width_value = parameter_value(parameters, "width", 600.0);
    const double depth_value = parameter_value(parameters, "depth", 560.0);
    const double height_value = parameter_value(parameters, "height", 800.0);
    const double mounting_height =
        parameter_value(parameters, "overhead", 0.0) >= 0.5
        ? std::max(0.0, parameter_value(
            parameters, "mounting_height", 1300.0))
        : 0.0;
    if (width_value <= 0.0 || depth_value <= 0.0 || height_value <= 0.0) {
        return;
    }

    const double left = -width_value * 0.5;
    const double right = width_value * 0.5;
    const double front = -depth_value * 0.5;
    const double back = depth_value * 0.5;
    const CPoint3d corners[8] = {
        {left, front, mounting_height}, {right, front, mounting_height},
        {right, back, mounting_height}, {left, back, mounting_height},
        {left, front, mounting_height + height_value},
        {right, front, mounting_height + height_value},
        {right, back, mounting_height + height_value},
        {left, back, mounting_height + height_value}
    };
    const auto vertex = [&corners](int index) {
        const CPoint3d& point = corners[index];
        glVertex3d(point.x, point.y, point.z);
    };

    const GLboolean lighting_enabled = glIsEnabled(GL_LIGHTING);
    const GLboolean blend_enabled = glIsEnabled(GL_BLEND);
    const GLboolean depth_test_enabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean cull_enabled = glIsEnabled(GL_CULL_FACE);
    GLboolean depth_mask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
    GLint blend_source = GL_SRC_ALPHA;
    GLint blend_destination = GL_ONE_MINUS_SRC_ALPHA;
    glGetIntegerv(GL_BLEND_SRC, &blend_source);
    glGetIntegerv(GL_BLEND_DST, &blend_destination);
    GLint polygon_mode[2] = {GL_FILL, GL_FILL};
    glGetIntegerv(GL_POLYGON_MODE, polygon_mode);

    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glColor4f(0.08f, 0.58f, 1.0f, 0.10f);
    glBegin(GL_QUADS);
    for (int index : {0, 1, 2, 3}) vertex(index); // bottom
    for (int index : {4, 7, 6, 5}) vertex(index); // top
    for (int index : {0, 4, 5, 1}) vertex(index); // front
    for (int index : {3, 2, 6, 7}) vertex(index); // back
    for (int index : {0, 3, 7, 4}) vertex(index); // left
    for (int index : {1, 5, 6, 2}) vertex(index); // right
    glEnd();

    glColor4f(0.20f, 0.76f, 1.0f, 0.92f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    const int edges[][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    for (const auto& edge : edges) {
        vertex(edge[0]);
        vertex(edge[1]);
    }
    glEnd();
    glLineWidth(1.0f);

    glDepthMask(depth_mask);
    glPolygonMode(GL_FRONT, polygon_mode[0]);
    glPolygonMode(GL_BACK, polygon_mode[1]);
    glBlendFunc(blend_source, blend_destination);
    if (!blend_enabled) glDisable(GL_BLEND);
    if (!depth_test_enabled) glDisable(GL_DEPTH_TEST);
    if (cull_enabled) glEnable(GL_CULL_FACE);
    if (lighting_enabled) glEnable(GL_LIGHTING);
}

void OpenGLViewport::DrawSolidDimensions() {
    solid_dimension_hits_.clear();
    if (solid_dimensions_.empty()) {
        return;
    }

    const GLboolean lighting_enabled = glIsEnabled(GL_LIGHTING);
    const GLboolean depth_test_enabled = glIsEnabled(GL_DEPTH_TEST);
    GLint polygon_mode[2] = {GL_FILL, GL_FILL};
    glGetIntegerv(GL_POLYGON_MODE, polygon_mode);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    const auto vertex = [](const CPoint3d& point) {
        glVertex3d(point.x, point.y, point.z);
    };
    const auto line = [&vertex](const CPoint3d& start, const CPoint3d& end) {
        vertex(start);
        vertex(end);
    };
    const auto add_point = [](const CPoint3d& first, const CPoint3d& second) {
        return CPoint3d(
            first.x + second.x,
            first.y + second.y,
            first.z + second.z);
    };
    const auto subtract_point = [](const CPoint3d& first, const CPoint3d& second) {
        return CPoint3d(
            first.x - second.x,
            first.y - second.y,
            first.z - second.z);
    };
    const auto scale_point = [](const CPoint3d& point, double scale) {
        return CPoint3d(point.x * scale, point.y * scale, point.z * scale);
    };
    const auto normalized_point = [&scale_point](const CPoint3d& point) {
        const double length = std::sqrt(point.x * point.x + point.y * point.y + point.z * point.z);
        return length <= 1.0e-12 ? CPoint3d(0.0, 1.0, 0.0) : scale_point(point, 1.0 / length);
    };
    for (const CDimens3D& dimension : solid_dimensions_) {
        if (!dimension.IsVisible()) {
            continue;
        }
        const bool primary = dimension.IsActive();
        const bool radial_dimension = (dimension.GetParameterId() == "radius" || dimension.GetParameterId() == "distance"
            || (dimension.GetSourceIndex() < solid_dimension_objects_.size()
                && solid_dimension_objects_[dimension.GetSourceIndex()].tool_id == "SolidPrismTool"));
        if (primary) {
            glColor3f(0.05f, 0.62f, 1.0f);
            glLineWidth(2.5f);
        } else {
            glColor3f(0.76f, 0.80f, 0.84f);
            glLineWidth(1.2f);
        }
        glBegin(GL_LINES);
        const double measured_length = dimension.GetMeasuredLength();
        const DimensionGeometry3D geometry = dimension.GetGeometry(
            std::max(0.5, measured_length * 0.025));
        line(geometry.source_start, geometry.extension_start);
        line(geometry.source_end, geometry.extension_end);
        line(geometry.dimension_start, geometry.dimension_end);

        const CPoint3d direction = normalized_point(
            subtract_point(geometry.dimension_end, geometry.dimension_start));
        const CPoint3d wing = normalized_point(
            subtract_point(geometry.dimension_start, geometry.source_start));
        // Keep arrowheads at a constant visual size, like Dom-3D's
        // GetAbsoluteFromVisual(..., 15). Deriving world length from the
        // projected dimension also works for both perspective and ortho views.
        constexpr double kArrowLengthPixels = 15.0;
        double arrow_length = std::clamp(measured_length * 0.08, 0.6, 6.0);
        DomPoint arrow_start_screen{};
        DomPoint arrow_end_screen{};
        if (renderer_.WorldToScreen(
                point_to_vec3(geometry.dimension_start),
                camera_,
                orthographic_projection_,
                width(),
                height(),
                arrow_start_screen)
            && renderer_.WorldToScreen(
                point_to_vec3(geometry.dimension_end),
                camera_,
                orthographic_projection_,
                width(),
                height(),
                arrow_end_screen)) {
            const double projected_length = std::hypot(
                static_cast<double>(arrow_end_screen.x - arrow_start_screen.x),
                static_cast<double>(arrow_end_screen.y - arrow_start_screen.y));
            if (projected_length > 0.5) {
                arrow_length = measured_length
                    * kArrowLengthPixels / projected_length;
                // For a very short dimension turn the arrows outward, matching
                // the legacy CDimens::DrawArrow behavior.
                if (!radial_dimension
                    && projected_length < kArrowLengthPixels * 2.0 + 3.0) {
                    arrow_length = -arrow_length;
                }
            }
        }
        const double arrow_width = std::abs(arrow_length) * 0.30;
        const auto draw_arrow = [&](const CPoint3d& tip, const CPoint3d& inward) {
            if (radial_dimension || (dimension.GetSourceIndex() < solid_dimension_objects_.size()
                && (solid_dimension_objects_[dimension.GetSourceIndex()].tool_id == "SolidBox"
                || solid_dimension_objects_[dimension.GetSourceIndex()].tool_id == "SolidCylinder"))) return;
            const CPoint3d base = add_point(tip, scale_point(inward, arrow_length));
            line(tip, add_point(base, scale_point(wing, arrow_width)));
            line(tip, add_point(base, scale_point(wing, -arrow_width)));
        };
        if (!radial_dimension) {
            draw_arrow(geometry.dimension_start, direction);
        }
        glEnd();

        // The dimension end is the draggable handle. Draw its arrow directly
        // in the same native OpenGL layer as the visible dimension geometry;
        // QPainter filled primitives are not reliable on every GL driver.
        glColor3f(1.0f, 0.05f, 0.02f);
        glLineWidth(primary ? 4.0f : 3.0f);
        glBegin(GL_LINES);
        draw_arrow(geometry.dimension_end, scale_point(direction, -1.0));
        glEnd();
        if (radial_dimension || (dimension.GetSourceIndex() < solid_dimension_objects_.size()
            && (solid_dimension_objects_[dimension.GetSourceIndex()].tool_id == "SolidBox"
                || solid_dimension_objects_[dimension.GetSourceIndex()].tool_id == "SolidCylinder"))) {
            const double dx=arrow_end_screen.x-arrow_start_screen.x,dy=arrow_end_screen.y-arrow_start_screen.y;
            const double length=std::hypot(dx,dy);
            if(length>0.5) {
                // Native filled triangles: QPainter polygons disappear with
                // some OpenGL paint engines. Use the same pixel coordinates
                // as the mouse hit areas, independent of zoom and view angle.
                GLint matrix_mode;glGetIntegerv(GL_MATRIX_MODE,&matrix_mode);
                const GLboolean culling=glIsEnabled(GL_CULL_FACE);
                glDisable(GL_CULL_FACE);
                glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
                glOrtho(0,width(),height(),0,-1,1);
                glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
                glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
                glColor3f(1.0f,0.86f,0.0f);
                const double radial_length=length;
                if(radial_dimension) {
                    glLineWidth(2.0f);glBegin(GL_LINES);
                    glVertex2d(arrow_start_screen.x,arrow_start_screen.y);
                    glVertex2d(arrow_start_screen.x+dx/length*radial_length,arrow_start_screen.y+dy/length*radial_length);
                    glEnd();
                }
                glBegin(GL_TRIANGLES);
                for(int side=radial_dimension?1:0;side<2;++side) {
                    const double sign=side==0?-1.0:1.0,ux=sign*dx/length,uy=sign*dy/length;
                    auto anchor=side==0?arrow_start_screen:arrow_end_screen;
                    if(radial_dimension) anchor={qRound(arrow_start_screen.x+dx/length*radial_length),qRound(arrow_start_screen.y+dy/length*radial_length)};
                    // Tip at the dimension endpoint; the triangle lies inside
                    // the measured segment, matching the endpoint being moved.
                    const double half_head=radial_dimension?7.5:8.0;
                    const double head_width=5.5;
                    const double x=anchor.x-ux*half_head,y=anchor.y-uy*half_head;
                    glVertex2d(anchor.x,anchor.y);
                    glVertex2d(x-ux*half_head-uy*head_width,y-uy*half_head+ux*head_width);
                    glVertex2d(x-ux*half_head+uy*head_width,y-uy*half_head-ux*head_width);
                }
                glEnd();
                glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(matrix_mode);
                if(culling)glEnable(GL_CULL_FACE);
            }
        }
    }
    glLineWidth(1.0f);
    if (depth_test_enabled) {
        glEnable(GL_DEPTH_TEST);
    }
    if (lighting_enabled) {
        glEnable(GL_LIGHTING);
    }

    // The scene renderer can intentionally leave polygon mode at GL_LINE for
    // edge display. QPainter's OpenGL backend inherits that state, which makes
    // filled arrowheads and circular grips disappear. Screen-space overlays
    // must always be painted with filled polygons.
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    QFont font = painter.font();
    font.setPointSize(9);
    font.setBold(true);
    painter.setFont(font);

    const DisplayLengthUnit display_unit = LoadDisplayLengthUnit();
    const QString suffix = DisplayLengthUnitSuffix(display_unit);
    const QColor outline(4, 12, 22, 220);
    struct DimensionGripDrawInfo {
        QPointF position;
        QString parameter_id;
        int operation_index = -1;
        QPointF direction;
        bool box_arrow = false;
    };
    std::vector<DimensionGripDrawInfo> dimension_grips;
    const auto project = [this](const CPoint3d& point, QPointF& result) {
        DomPoint screen{};
        if (!renderer_.WorldToScreen(
                point_to_vec3(point),
                camera_,
                orthographic_projection_,
                width(),
                height(),
                screen)) {
            return false;
        }
        result = QPointF(screen.x, screen.y);
        return true;
    };

    for (const CDimens3D& dimension : solid_dimensions_) {
        if (!dimension.IsVisible()) {
            continue;
        }
        const bool primary = dimension.IsActive();
        const QColor color = primary
            ? QColor(20, 145, 255)
            : QColor(225, 229, 233);
        const DimensionGeometry3D geometry = dimension.GetGeometry(
            std::max(0.5, dimension.GetMeasuredLength() * 0.025));
        QPointF source_start;
        QPointF source_end;
        QPointF dimension_start;
        QPointF dimension_end;
        QPointF extension_start;
        QPointF extension_end;
        QPointF text_position;
        if (!project(geometry.source_start, source_start)
            || !project(geometry.source_end, source_end)
            || !project(geometry.dimension_start, dimension_start)
            || !project(geometry.dimension_end, dimension_end)
            || !project(geometry.extension_start, extension_start)
            || !project(geometry.extension_end, extension_end)
            || !project(geometry.text_position, text_position)) {
            continue;
        }

        if ((dimension.GetParameterId()=="radius" || dimension.GetParameterId()=="distance")) {
            const QPointF delta=dimension_end-dimension_start;
            const double len=std::hypot(delta.x(),delta.y());
            if(len>0.5) {
                const QPointF axis=delta/len,side(-axis.y(),axis.x());
                const double radial_length=len;
                source_start=dimension_start;source_end=dimension_end;
                extension_start=dimension_start;extension_end=dimension_end;
                text_position=dimension_start+axis*(radial_length*0.5)+side*24.0;
            }
        }
        QPointF direction = dimension_end - dimension_start;
        const double screen_length = std::hypot(direction.x(), direction.y());
        if (screen_length < 0.5) {
            continue;
        }
        direction /= screen_length;
        const QPointF perpendicular(-direction.y(), direction.x());

        const auto draw_thick_segment = [&painter](
                                            QPointF start,
                                            QPointF end,
                                            double thickness,
                                            const QColor& segment_color) {
            QPointF segment = end - start;
            const double length = std::hypot(segment.x(), segment.y());
            if (length <= 0.01) {
                return;
            }
            segment /= length;
            const QPointF normal(-segment.y(), segment.x());
            const QPointF half_width = normal * (thickness * 0.5);
            QPolygonF polygon;
            polygon << start + half_width
                    << end + half_width
                    << end - half_width
                    << start - half_width;
            painter.setPen(Qt::NoPen);
            painter.setBrush(segment_color);
            painter.drawPolygon(polygon);
        };
        const auto draw_dimension_lines = [&draw_thick_segment,
                                           &source_start,
                                           &source_end,
                                           &extension_start,
                                           &extension_end,
                                           &dimension_start,
                                           &dimension_end](
                                              double thickness,
                                              const QColor& segment_color) {
            draw_thick_segment(source_start, extension_start, thickness, segment_color);
            draw_thick_segment(source_end, extension_end, thickness, segment_color);
            draw_thick_segment(dimension_start, dimension_end, thickness, segment_color);
        };
        draw_dimension_lines(primary ? 3.5 : 2.5, outline);
        draw_dimension_lines(primary ? 2.0 : 1.25, color);

        const auto arrow_polygon = [&perpendicular](
                                    QPointF tip,
                                    QPointF inward) {
            QPolygonF arrow;
            arrow << tip
                  << tip + inward * 12.0 + perpendicular * 5.0
                  << tip + inward * 12.0 - perpendicular * 5.0;
            return arrow;
        };
        const bool box_handles = dimension.GetSourceIndex() < solid_dimension_objects_.size()
            && (solid_dimension_objects_[dimension.GetSourceIndex()].tool_id == "SolidBox"
                || solid_dimension_objects_[dimension.GetSourceIndex()].tool_id == "SolidCylinder");
        const bool prism_handle = dimension.GetSourceIndex() < solid_dimension_objects_.size()
            && solid_dimension_objects_[dimension.GetSourceIndex()].tool_id == "SolidPrismTool";
        const bool radial_dimension = (dimension.GetParameterId() == "radius" || dimension.GetParameterId() == "distance");
        if (!box_handles && !prism_handle && !radial_dimension) {
        const QPolygonF first_arrow = arrow_polygon(dimension_start, direction);
        const QPolygonF second_arrow = arrow_polygon(dimension_end, -direction);
        painter.setPen(QPen(outline, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(outline);
        if (!radial_dimension) painter.drawPolygon(first_arrow);
        painter.drawPolygon(second_arrow);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        if (!radial_dimension) painter.drawPolygon(first_arrow);
        painter.setBrush(QColor(255, 45, 25));
        painter.drawPolygon(second_arrow);

        }
        const QRect arrow_hit_rect = QRectF(dimension_start, dimension_end)
            .normalized()
            .adjusted(-9.0, -9.0, 9.0, 9.0)
            .toAlignedRect();
        solid_dimension_hits_.push_back(
            {arrow_hit_rect,
             QString::fromStdString(dimension.GetParameterId()),
             dimension.GetValue(),
             false,
             false,
             {qRound(dimension_start.x()), qRound(dimension_start.y())},
             {qRound(dimension_end.x()), qRound(dimension_end.y())},
             dimension.GetSourceIndex()});

        const bool supports_grips =
            dimension.GetSourceIndex() < solid_dimension_objects_.size();
        if (supports_grips) {
            const QString parameter_id =
                QString::fromStdString(dimension.GetParameterId());
            const int operation_index = static_cast<int>(
                solid_dimension_objects_[dimension.GetSourceIndex()].operation_index);
            for (int side = 0; side < (box_handles ? 2 : 1); ++side) {
                const bool move_base = side == 1;
                const QPointF outward = move_base ? -direction : direction;
                const QPointF anchor = move_base ? dimension_start : dimension_end;
                const bool filled_handle=box_handles || prism_handle || (dimension.GetParameterId()=="radius" || dimension.GetParameterId()=="distance");
                const double half_head=(prism_handle || dimension.GetParameterId()=="radius" || dimension.GetParameterId()=="distance")
                    ? 7.5:8.0;
                const QPointF position = filled_handle ? anchor - outward * half_head : anchor;
                dimension_grips.push_back({position, parameter_id, operation_index, outward, filled_handle});
                const double radius = box_handles ? 13.0 : 22.0;
                const QRect hit = QRectF(position-QPointF(radius,radius),QSizeF(2*radius,2*radius)).toAlignedRect();
                solid_dimension_hits_.push_back({hit, parameter_id, dimension.GetValue(), false, true,
                    {qRound(dimension_start.x()),qRound(dimension_start.y())},
                    {qRound(dimension_end.x()),qRound(dimension_end.y())},dimension.GetSourceIndex(),move_base});
            }
        }

        const QString text = QString("%1%2%3")
            .arg(dimension.GetParameterId() == "radius" ? QStringLiteral("R ") : QString())
            .arg(MillimetersToDisplay(dimension.GetValue(), display_unit), 0, 'f', 1)
            .arg(suffix);
        const QSize text_size = painter.fontMetrics().size(Qt::TextSingleLine, text);
        const QRect label_rect(
            qRound(text_position.x() - text_size.width() * 0.5 - 4.0),
            qRound(text_position.y() - text_size.height() * 0.5 - 2.0),
            text_size.width() + 8,
            text_size.height() + 4);
        painter.fillRect(label_rect.adjusted(-1, -1, 1, 1), outline);
        painter.fillRect(
            label_rect,
            primary ? QColor(235, 245, 255, 245)
                    : QColor(246, 246, 246, 238));
        painter.setPen(color);
        painter.drawText(label_rect, Qt::AlignCenter, text);
        solid_dimension_hits_.push_back(
            {label_rect,
             QString::fromStdString(dimension.GetParameterId()),
             dimension.GetValue(),
             true,
             false,
             {},
             {},
             dimension.GetSourceIndex()});
    }

    for(const auto& grip:cylinder_center_grips_) {
        QPointF position;
        if(!project(grip.center,position))continue;
        const auto source=std::find_if(solid_dimension_objects_.begin(),solid_dimension_objects_.end(),
            [&](const auto& object){return object.tool_id=="SolidCylinder" && int(object.operation_index)==grip.operation_index;});
        if(source==solid_dimension_objects_.end())continue;
        const bool hovered=(highlighted_solid_dimension_grip_=="origin.center" && highlighted_solid_dimension_operation_index_==grip.operation_index)
            || (active_solid_dimension_grip_=="origin.center" && active_solid_dimension_operation_index_==grip.operation_index);
        painter.setPen(QPen(hovered?QColor(0,220,255):QColor(255,80,110),2));
        painter.setBrush(QColor(20,20,20,180));painter.drawEllipse(position,12,12);
        painter.drawText(QRectF(position-QPointF(10,10),QSizeF(20,20)),Qt::AlignCenter,"C");
        solid_dimension_hits_.insert(solid_dimension_hits_.begin(),
            {QRectF(position-QPointF(14,14),QSizeF(28,28)).toAlignedRect(),"origin.center",0,false,true,{}, {},size_t(source-solid_dimension_objects_.begin()),false});
    }

    // Grips are a foreground UI element. Draw them only after every dimension
    // line, arrow and label so later dimensions cannot cover the handles.
    for (const DimensionGripDrawInfo& grip : dimension_grips) {
        if (grip.box_arrow) continue; // Drawn in the native GL pass.
        const QPointF& position = grip.position;
        const bool highlighted =
            (grip.parameter_id == highlighted_solid_dimension_grip_
             && grip.operation_index == highlighted_solid_dimension_operation_index_)
            || (grip.parameter_id == active_solid_dimension_grip_
                && grip.operation_index == active_solid_dimension_operation_index_);
        const double radius = highlighted ? 18.0 : 15.0;

        painter.save();
        painter.setOpacity(1.0);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setPen(Qt::NoPen);

        // Use opaque concentric fills instead of a QRadialGradient. Some
        // OpenGL paint engines displayed only the gradient's tiny highlight.
        painter.setBrush(QColor(0, 0, 0));
        painter.drawEllipse(position, radius + 3.0, radius + 3.0);
        painter.setBrush(QColor(245, 250, 255));
        painter.drawEllipse(position, radius + 1.0, radius + 1.0);
        painter.setBrush(highlighted
                             ? QColor(0, 190, 255)
                             : QColor(0, 92, 255));
        painter.drawEllipse(position, radius - 1.5, radius - 1.5);
        painter.setBrush(QColor(0, 42, 185));
        painter.drawEllipse(
            position + QPointF(radius * 0.22, radius * 0.24),
            radius * 0.58,
            radius * 0.58);
        painter.setBrush(QColor(225, 255, 255));
        painter.drawEllipse(
            position - QPointF(radius * 0.30, radius * 0.34),
            radius * 0.24,
            radius * 0.24);
        painter.restore();
    }
    painter.end();

    // Submit the QPainter/OpenGL overlay before restoring scene render state.
    // This is the OpenGL equivalent of the legacy rsFlush() call.
    glFlush();

    glPolygonMode(GL_FRONT, polygon_mode[0]);
    glPolygonMode(GL_BACK, polygon_mode[1]);
}

void OpenGLViewport::DrawCoordinateAxisLabels() {
    DomPoint x_screen{};
    DomPoint y_screen{};
    DomPoint z_screen{};
    const float lift = 0.02f;
    const float axis_length =
        xy_plane_view_enabled_ ? grid_size_ : grid_size_ * 0.5f;
    const Vec3 x_label{axis_length, 0.0f, lift};
    const Vec3 y_label{0.0f, axis_length, lift};
    const bool has_x = renderer_.WorldToScreen(x_label, camera_, orthographic_projection_, width(), height(), x_screen);
    const bool has_y = renderer_.WorldToScreen(y_label, camera_, orthographic_projection_, width(), height(), y_screen);
    const bool has_z = renderer_.WorldToScreen(
        {0.0f, 0.0f, axis_length},
        camera_,
        orthographic_projection_,
        width(),
        height(),
        z_screen);

    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    QFont label_font = painter.font();
    label_font.setBold(true);
    label_font.setPointSize(9);
    painter.setFont(label_font);

    const auto draw_label = [&painter](const DomPoint& point, const QColor& color, const QString& text) {
        painter.setPen(color);
        painter.drawText(QPoint(point.x + 5, point.y - 5), text);
    };

    if (has_x) {
        draw_label(x_screen, QColor(255, 20, 18), "X");
    }
    if (has_y) {
        draw_label(y_screen, QColor(40, 255, 45), "Y");
    }
    if (has_z) {
        draw_label(z_screen, QColor(55, 85, 255), "Z");
    }
}

std::vector<OpenGLViewport::WalkRoomFootprint>
OpenGLViewport::WalkRoomFootprints() const {
    std::vector<WalkRoomFootprint> result;
    if (!document_) return result;

    for (const auto& object : document_->GetObjects()) {
        const auto* room = object
            ? dynamic_cast<const CAssembled*>(object.get()) : nullptr;
        if (!room || room->GetParametricToolId() != "room"
            || !document_->IsObjectVisible(*room)
            || room->GetElementIds().empty()) {
            continue;
        }
        const CAlfaObject* floor = document_->FindObjectById(
            room->GetElementIds().front());
        Vec3 minimum{};
        Vec3 maximum{};
        if (!floor || !document_->IsObjectVisible(*floor)
            || !floor->GetBounds(minimum, maximum)) {
            continue;
        }
        if (maximum.x - minimum.x > 1.0f
            && maximum.y - minimum.y > 1.0f) {
            result.push_back({minimum, maximum});
        }
    }
    return result;
}

QRectF OpenGLViewport::WalkMiniMapRect() const {
    const qreal map_width = std::max(150, std::min(240, width() - 24));
    const qreal map_height = std::max(120, std::min(190, height() - 24));
    return QRectF(12.0, 12.0, map_width, map_height);
}

bool OpenGLViewport::WalkMiniMapTransform(
    QRectF& content_rect,
    Vec3& world_minimum,
    Vec3& world_maximum,
    std::vector<WalkRoomFootprint>& footprints) const {
    footprints = WalkRoomFootprints();
    if (footprints.empty()) return false;

    world_minimum = footprints.front().minimum;
    world_maximum = footprints.front().maximum;
    for (const WalkRoomFootprint& room : footprints) {
        world_minimum.x = std::min(world_minimum.x, room.minimum.x);
        world_minimum.y = std::min(world_minimum.y, room.minimum.y);
        world_maximum.x = std::max(world_maximum.x, room.maximum.x);
        world_maximum.y = std::max(world_maximum.y, room.maximum.y);
    }

    const QRectF panel = WalkMiniMapRect();
    const QRectF available = panel.adjusted(12.0, 30.0, -12.0, -12.0);
    const qreal world_width = std::max(
        1.0, static_cast<double>(world_maximum.x - world_minimum.x));
    const qreal world_height = std::max(
        1.0, static_cast<double>(world_maximum.y - world_minimum.y));
    const qreal scale = std::min(
        available.width() / world_width,
        available.height() / world_height);
    const qreal draw_width = world_width * scale;
    const qreal draw_height = world_height * scale;
    content_rect = QRectF(
        available.center().x() - draw_width * 0.5,
        available.center().y() - draw_height * 0.5,
        draw_width,
        draw_height);
    return true;
}

void OpenGLViewport::DrawWalkMiniMap() {
    const QRectF panel = WalkMiniMapRect();
    QImage overlay(panel.size().toSize(), QImage::Format_ARGB32_Premultiplied);
    overlay.fill(Qt::transparent);
    QPainter painter(&overlay);
    painter.translate(-panel.topLeft());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(panel, QColor(16, 18, 22));
    painter.setPen(QPen(QColor(186, 174, 196), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(panel, 5.0, 5.0);
    painter.setPen(QColor(235, 235, 238));
    QFont title_font = painter.font();
    title_font.setBold(true);
    painter.setFont(title_font);
    painter.drawText(panel.adjusted(10.0, 5.0, -8.0, 0.0),
                     Qt::AlignLeft | Qt::AlignTop,
                     "Walk - click to enter room");

    QRectF content;
    Vec3 world_minimum{};
    Vec3 world_maximum{};
    std::vector<WalkRoomFootprint> footprints;
    if (!WalkMiniMapTransform(
            content, world_minimum, world_maximum, footprints)) {
        painter.setFont(QFont());
        painter.setPen(QColor(180, 180, 185));
        painter.drawText(panel.adjusted(10.0, 35.0, -10.0, -10.0),
                         Qt::AlignCenter | Qt::TextWordWrap,
                         "No visible Room");
        painter.end();
        walk_mini_map_overlay_->setGeometry(panel.toAlignedRect());
        walk_mini_map_overlay_->setPixmap(QPixmap::fromImage(overlay));
        walk_mini_map_overlay_->show();
        walk_mini_map_overlay_->raise();
        return;
    }

    const auto world_to_map = [&](Vec3 world) {
        const double x_ratio = (world.x - world_minimum.x)
            / std::max(1.0f, world_maximum.x - world_minimum.x);
        const double y_ratio = (world.y - world_minimum.y)
            / std::max(1.0f, world_maximum.y - world_minimum.y);
        return QPointF(content.left() + x_ratio * content.width(),
                       content.bottom() - y_ratio * content.height());
    };

    painter.setFont(QFont());
    painter.setPen(QPen(QColor(229, 219, 202), 2.0));
    painter.setBrush(QColor(94, 82, 68));
    for (const WalkRoomFootprint& room : footprints) {
        QRectF room_rect = content;
        if (footprints.size() > 1) {
            const QPointF top_left = world_to_map(
                {room.minimum.x, room.maximum.y, 0.0f});
            const QPointF bottom_right = world_to_map(
                {room.maximum.x, room.minimum.y, 0.0f});
            room_rect = QRectF(top_left, bottom_right).normalized();
        }
        painter.drawRect(room_rect);
        const qreal wall_inset = std::min(
            7.0, std::min(room_rect.width(), room_rect.height()) * 0.08);
        painter.setBrush(QColor(35, 38, 42));
        painter.drawRect(room_rect.adjusted(
            wall_inset, wall_inset, -wall_inset, -wall_inset));
        painter.setBrush(QColor(94, 82, 68));
    }

    const Vec3 eye = camera_position(camera_);
    Vec3 forward = normalize(rotate(
        camera_.orientation, {0.0f, 0.0f, -1.0f}));
    forward.z = 0.0f;
    forward = normalize(forward);
    const QPointF camera_point = world_to_map(eye);
    const qreal arrow_length = 18.0;
    const QPointF arrow_end(
        camera_point.x() + forward.x * arrow_length,
        camera_point.y() - forward.y * arrow_length);
    painter.setPen(QPen(QColor(255, 72, 62), 2.5));
    painter.setBrush(QColor(255, 72, 62));
    painter.drawLine(camera_point, arrow_end);
    painter.drawEllipse(camera_point, 4.5, 4.5);
    painter.end();
    walk_mini_map_overlay_->setGeometry(panel.toAlignedRect());
    walk_mini_map_overlay_->setPixmap(QPixmap::fromImage(overlay));
    walk_mini_map_overlay_->show();
    walk_mini_map_overlay_->raise();
}

bool OpenGLViewport::PlaceWalkCameraFromMiniMap(const QPoint& point) {
    if (!WalkMiniMapRect().contains(point)) return false;

    QRectF content;
    Vec3 world_minimum{};
    Vec3 world_maximum{};
    std::vector<WalkRoomFootprint> footprints;
    if (!WalkMiniMapTransform(
            content, world_minimum, world_maximum, footprints)) {
        emit StatusTextChanged("Walk: create or show a Room first");
        return true;
    }
    if (!content.contains(point)) return true;

    const double x_ratio = (point.x() - content.left()) / content.width();
    const double y_ratio = (content.bottom() - point.y()) / content.height();
    Vec3 requested{
        static_cast<float>(world_minimum.x
            + x_ratio * (world_maximum.x - world_minimum.x)),
        static_cast<float>(world_minimum.y
            + y_ratio * (world_maximum.y - world_minimum.y)),
        0.0f};

    const WalkRoomFootprint* selected_room = nullptr;
    for (const WalkRoomFootprint& room : footprints) {
        if (requested.x >= room.minimum.x && requested.x <= room.maximum.x
            && requested.y >= room.minimum.y && requested.y <= room.maximum.y) {
            selected_room = &room;
            break;
        }
    }
    if (!selected_room) {
        emit StatusTextChanged("Walk: click inside a room on the map");
        return true;
    }

    const float inset_x = std::min(
        250.0f, (selected_room->maximum.x - selected_room->minimum.x) * 0.1f);
    const float inset_y = std::min(
        250.0f, (selected_room->maximum.y - selected_room->minimum.y) * 0.1f);
    requested.x = std::clamp(
        requested.x,
        selected_room->minimum.x + inset_x,
        selected_room->maximum.x - inset_x);
    requested.y = std::clamp(
        requested.y,
        selected_room->minimum.y + inset_y,
        selected_room->maximum.y - inset_y);
    requested.z = selected_room->maximum.z + 1600.0f;

    Vec3 forward = rotate(camera_.orientation, {0.0f, 0.0f, -1.0f});
    forward.z = 0.0f;
    forward = normalize(forward);
    if (dot(forward, forward) <= 0.00001f) {
        forward = {0.0f, -1.0f, 0.0f};
    }
    camera_.distance = 2500.0f;
    camera_.orientation = z_up_orientation_from_forward(
        forward, rotate(camera_.orientation, {1.0f, 0.0f, 0.0f}));
    camera_.target = requested + forward * camera_.distance;
    emit StatusTextChanged(
        "Walk: arrows move, left mouse rotates, click the map to relocate, Esc exits");
    update();
    return true;
}

void OpenGLViewport::MoveWalkCamera(
    int key, Qt::KeyboardModifiers modifiers) {
    Vec3 forward = rotate(camera_.orientation, {0.0f, 0.0f, -1.0f});
    forward.z = 0.0f;
    forward = normalize(forward);
    if (dot(forward, forward) <= 0.00001f) {
        forward = {0.0f, -1.0f, 0.0f};
    }
    Vec3 right = rotate(camera_.orientation, {1.0f, 0.0f, 0.0f});
    right.z = 0.0f;
    right = normalize(right);
    if (dot(right, right) <= 0.00001f) {
        right = normalize(cross(forward, {0.0f, 0.0f, 1.0f}));
    }

    float step = 150.0f;
    if (modifiers.testFlag(Qt::ShiftModifier)) step = 50.0f;
    if (modifiers.testFlag(Qt::ControlModifier)) step = 500.0f;
    Vec3 delta{};
    if (key == Qt::Key_Left) delta = right * -step;
    if (key == Qt::Key_Right) delta = right * step;
    if (key == Qt::Key_Up) delta = forward * step;
    if (key == Qt::Key_Down) delta = forward * -step;
    camera_.target = camera_.target + delta;
    update();
}

void OpenGLViewport::UpdateFPS()
{
    int now = static_cast<int>(GetTickCount64());

    if (m_lastFpsTime == 0)
        m_lastFpsTime = now;

    m_frameCounter++;

    int dt = now - m_lastFpsTime;
    if (dt >= 500)
    {
        m_fps = 1000.0f * m_frameCounter / float(dt);
        m_frameCounter = 0;
        m_lastFpsTime = now;
    }
}

void OpenGLViewport::DrawFPS()
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    QFont fps_font = painter.font();
    fps_font.setBold(true);
    fps_font.setPointSize(10);
    painter.setFont(fps_font);
    QString fps_text = DomTranslate("FPS: %1").arg(m_fps, 0, 'f', 1);
    painter.setPen(Qt::yellow);
    const int text_y = tool_ == ToolMode::Walk
        ? static_cast<int>(WalkMiniMapRect().bottom()) + 20 : 20;
	painter.drawText(QPoint(10, text_y), fps_text);
}

#include "BottleModelControls.inc"

void OpenGLViewport::ClearSubdivisionPreview() {
    subdivision_u_.clear();subdivision_v_.clear();update();
}
void OpenGLViewport::SetSubdivisionPreview(const TopoDS_Face& face,int u,int v) {
    subdivision_u_.clear();subdivision_v_.clear();
    if(face.IsNull()){update();return;}
    try {
        double u0,u1,v0,v1;BRepTools::UVBounds(face,u0,u1,v0,v1);
        BRepAdaptor_Surface surface(face);
        for(int axis=0;axis<2;++axis) {
            const int count=std::clamp(axis==0?u:v,1,32);
            auto& lines=axis==0?subdivision_u_:subdivision_v_;
            for(int i=1;i<count;++i) {
                const double fixed=axis==0?u0+(u1-u0)*i/count:v0+(v1-v0)*i/count;
                for(const auto& interval:SubdivisionIntervals(face,axis,fixed)) {
                    std::vector<Vec3> line;
                    for(int j=0;j<=128;++j) {
                        const double along=interval.first+(interval.second-interval.first)*j/128.;
                        const auto p=surface.Value(axis==0?fixed:along,axis==0?along:fixed);
                        line.push_back({float(p.X()),float(p.Y()),float(p.Z())});
                    }
                    lines.push_back(std::move(line));
                }
            }
        }
    }catch(const Standard_Failure&){subdivision_u_.clear();subdivision_v_.clear();}
    update();
}
void OpenGLViewport::DrawSubdivisionPreview() {
    if(subdivision_u_.empty()&&subdivision_v_.empty())return;
    glPushAttrib(GL_ALL_ATTRIB_BITS);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);
    glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);glDisable(GL_STENCIL_TEST);
    QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
    for(int axis=0;axis<2;++axis) {
        painter.setPen(QPen(axis==0?QColor(255,65,55):QColor(55,110,255),2));
        for(const auto& line:axis==0?subdivision_u_:subdivision_v_) {
            QPainterPath path;bool started=false;
            for(auto point:line){DomPoint screen{};
                if(!renderer_.WorldToScreen(point,camera_,orthographic_projection_,width(),height(),screen)){started=false;continue;}
                if(started)path.lineTo(screen.x,screen.y);else path.moveTo(screen.x,screen.y);started=true;
            }
            painter.drawPath(path);
        }
    }
    painter.end();glPopAttrib();
}

void OpenGLViewport::DrawFaceExtrudePreviewWalls() {
    if(!dragging_face_extrude_||tool_!=ToolMode::FaceExtrude||std::abs(face_extrude_distance_)<1.e-6f)return;
    // The planar tapered preview already contains its exact side walls.
    if(document_ && std::abs(face_extrude_taper_angle_degrees_)>1.e-7 && !document_->HasSelectedSolidFace())return;
    const auto delta=face_extrude_normal_*face_extrude_distance_;
    Vec3 scaleCenter{};double scale=1;
    if(document_&&std::abs(face_extrude_taper_angle_degrees_)>1.e-7) {
        if(const auto* solid=document_->GetSelectedFaceSolid()) {
            const auto face=solid->GetTopoFace(solid->GetSelectedFaceIndex());
            if(!face.IsNull()&&BRepAdaptor_Surface(face).GetType()!=GeomAbs_Plane)
                CurvedExtrudeScale(face,face_extrude_distance_,face_extrude_taper_angle_degrees_,scaleCenter,scale);
        }
    }
    const auto capPoint=[&](Vec3 p){return scaleCenter+(p-scaleCenter)*float(scale)+delta;};
    glPushAttrib(GL_ALL_ATTRIB_BITS);glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(.65f,.75f,.85f,.5f);
    for(const auto& line:face_extrude_boundary_) {
        glBegin(GL_QUAD_STRIP);
        for(auto p:line){glVertex3f(p.x,p.y,p.z);p=capPoint(p);glVertex3f(p.x,p.y,p.z);}
        glEnd();
    }
    glColor4f(1.f,.6f,.1f,1.f);glLineWidth(1.5f);
    for(const auto& line:face_extrude_boundary_) {
        glBegin(GL_LINE_STRIP);for(auto p:line){p=capPoint(p);glVertex3f(p.x,p.y,p.z);}glEnd();
        glBegin(GL_LINES);for(auto p:{line.front(),line.back()}){glVertex3f(p.x,p.y,p.z);p=capPoint(p);glVertex3f(p.x,p.y,p.z);}glEnd();
    }
    glPopAttrib();
}
