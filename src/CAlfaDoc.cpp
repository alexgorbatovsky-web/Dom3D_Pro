#include "CAlfaDoc.h"

#include "solid/Solid.h"
#include "solid/SurfaceSet.h"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <Geom_BSplineCurve.hxx>
#include <GeomAPI_PointsToBSpline.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GeomAbs_CurveType.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <gp_Dir.hxx>
#include <gp_Lin.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopTools_MapOfShape.hxx>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {
std::vector<TopoDS_Edge> unique_edges(const std::vector<TopoDS_Edge>& edges)
{
    std::vector<TopoDS_Edge> unique;
    TopTools_MapOfShape seen;
    for (const TopoDS_Edge& edge : edges) {
        if (!edge.IsNull() && seen.Add(edge)) {
            unique.push_back(edge);
        }
    }
    return unique;
}

bool rebuild_solid_from_shape(CAlfaDoc::ObjectList& objects, size_t solid_index, CSolid* source_solid, const TopoDS_Shape& result_shape)
{
    if (solid_index >= objects.size() || !source_solid || result_shape.IsNull()) {
        return false;
    }

    TopoDS_Shape shape_copy = result_shape;
    auto result = std::make_unique<CSolid>(shape_copy);
    result->SetName(source_solid->GetName());
    result->SetMaterial(source_solid->GetMaterial());
    result->SetMaterialId(source_solid->GetMaterialId());
    result->InitSurfaces();
    result->InitEdges();
    result->BuldMesh(0.1f);

    objects[solid_index] = std::move(result);
    return true;
}

TopoDS_Shape make_extrude_prism(const TopoDS_Face& face, Vec3 normal, float distance, double taper_angle_degrees)
{
    if (face.IsNull() || std::fabs(distance) <= 0.0001f) {
        return {};
    }

    const Vec3 vector = normal * distance;
    BRepPrimAPI_MakePrism prism_builder(face, gp_Vec(vector.x, vector.y, vector.z), false, true);
    prism_builder.Build();
    if (!prism_builder.IsDone()) {
        return {};
    }

    TopoDS_Shape prism_shape = prism_builder.Shape();
    if (prism_shape.IsNull() || std::fabs(taper_angle_degrees) <= 0.0001) {
        return prism_shape;
    }

    const double angle = taper_angle_degrees * 3.14159265358979323846 / 180.0;
    const double direction_sign = distance >= 0.0f ? 1.0 : -1.0;
    gp_Dir draft_direction(normal.x * direction_sign, normal.y * direction_sign, normal.z * direction_sign);

    BRepAdaptor_Surface base_surface(face, true);
    if (base_surface.GetType() != GeomAbs_Plane) {
        return {};
    }
    const gp_Pln neutral_plane = base_surface.Plane();

    BRepOffsetAPI_DraftAngle draft(prism_shape);
    for (TopExp_Explorer explorer(prism_shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        const TopoDS_Face current_face = TopoDS::Face(explorer.Current());
        BRepAdaptor_Surface surface(current_face, true);
        if (surface.GetType() != GeomAbs_Plane) {
            continue;
        }

        const gp_Dir face_normal = surface.Plane().Axis().Direction();
        const double alignment = std::fabs(face_normal.X() * normal.x + face_normal.Y() * normal.y + face_normal.Z() * normal.z);
        if (alignment > 0.98) {
            continue;
        }

        draft.Add(current_face, draft_direction, angle, neutral_plane);
        if (!draft.AddDone()) {
            draft.Remove(current_face);
            return {};
        }
    }

    draft.Build();
    if (!draft.IsDone()) {
        return {};
    }

    return draft.Shape();
}

TopoDS_Shape make_draft_face_shape(const TopoDS_Shape& base_shape,
                                   const TopoDS_Face& face,
                                   Vec3 draft_direction,
                                   const gp_Pln& neutral_plane,
                                   double angle_degrees)
{
    if (base_shape.IsNull() || face.IsNull()) {
        return {};
    }
    if (std::fabs(angle_degrees) <= 0.0001) {
        return base_shape;
    }

    try {
        const double angle = angle_degrees * 3.14159265358979323846 / 180.0;
        BRepOffsetAPI_DraftAngle draft(base_shape);
        draft.Add(face, gp_Dir(draft_direction.x, draft_direction.y, draft_direction.z), angle, neutral_plane);
        if (!draft.AddDone()) {
            return {};
        }
        draft.Build();
        if (!draft.IsDone()) {
            return {};
        }
        return draft.Shape();
    } catch (const Standard_Failure&) {
        return {};
    }
}

int count_faces(const TopoDS_Shape& shape)
{
    int count = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        ++count;
    }
    return count;
}

TopoDS_Face face_at_index(const TopoDS_Shape& shape, int face_index)
{
    if (shape.IsNull() || face_index < 0) {
        return {};
    }

    int index = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next(), ++index) {
        if (index == face_index) {
            return TopoDS::Face(explorer.Current());
        }
    }
    return {};
}

TopoDS_Shape make_thick_solid_shape(const TopoDS_Shape& base_shape,
                                    const std::vector<int>& face_indices,
                                    double thickness)
{
    if (base_shape.IsNull() || face_indices.empty()) {
        return {};
    }
    if (std::fabs(thickness) <= 0.0001) {
        return base_shape;
    }

    TopTools_ListOfShape closing_faces;
    for (int face_index : face_indices) {
        TopoDS_Face face = face_at_index(base_shape, face_index);
        if (!face.IsNull()) {
            closing_faces.Append(face);
        }
    }
    if (closing_faces.IsEmpty()) {
        return {};
    }

    try {
        BRepOffsetAPI_MakeThickSolid thick_solid;
        thick_solid.MakeThickSolidByJoin(base_shape,
                                         closing_faces,
                                         thickness,
                                         0.001,
                                         BRepOffset_Skin,
                                         Standard_False,
                                         Standard_False,
                                         GeomAbs_Arc,
                                         Standard_False);
        thick_solid.Build();
        if (!thick_solid.IsDone()) {
            return {};
        }
        return thick_solid.Shape();
    } catch (const Standard_Failure&) {
        return {};
    }
}

float distance_to_screen_segment(DomPoint point, DomPoint start, DomPoint end)
{
    const float dx = static_cast<float>(end.x - start.x);
    const float dy = static_cast<float>(end.y - start.y);
    const float length_sq = dx * dx + dy * dy;
    if (length_sq <= 0.0001f) {
        const float px = static_cast<float>(point.x - start.x);
        const float py = static_cast<float>(point.y - start.y);
        return std::sqrt(px * px + py * py);
    }

    const float t = std::clamp((static_cast<float>(point.x - start.x) * dx
        + static_cast<float>(point.y - start.y) * dy) / length_sq, 0.0f, 1.0f);
    const float closest_x = static_cast<float>(start.x) + t * dx;
    const float closest_y = static_cast<float>(start.y) + t * dy;
    const float px = static_cast<float>(point.x) - closest_x;
    const float py = static_cast<float>(point.y) - closest_y;
    return std::sqrt(px * px + py * py);
}

Vec3 to_vec3(const CPoint3d& point)
{
    return {static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z)};
}

Vec3 revolve_axis_direction(int axis_index);

bool build_profile_face_from_polyline(const CPolyline& polyline, TopoDS_Face& face, Vec3& normal)
{
    const std::vector<CPoint3d>& points = polyline.GetPoints();
    if (!polyline.IsClosed() || points.size() < 3) {
        return false;
    }

    Vec3 newell{};
    for (size_t i = 0; i < points.size(); ++i) {
        const CPoint3d& current = points[i];
        const CPoint3d& next = points[(i + 1) % points.size()];
        newell.x += static_cast<float>((current.y - next.y) * (current.z + next.z));
        newell.y += static_cast<float>((current.z - next.z) * (current.x + next.x));
        newell.z += static_cast<float>((current.x - next.x) * (current.y + next.y));
    }

    const float normal_length = std::sqrt(dot(newell, newell));
    if (normal_length <= 0.0001f) {
        return false;
    }
    normal = newell * (1.0f / normal_length);

    const Vec3 origin = to_vec3(points.front());
    for (const CPoint3d& point : points) {
        const Vec3 delta = to_vec3(point) - origin;
        if (std::fabs(dot(delta, normal)) > 0.001f) {
            return false;
        }
    }

    BRepBuilderAPI_MakePolygon polygon;
    for (const CPoint3d& point : points) {
        polygon.Add(gp_Pnt(point.x, point.y, point.z));
    }
    polygon.Close();
    if (!polygon.IsDone()) {
        return false;
    }

    const TopoDS_Wire wire = polygon.Wire();
    if (wire.IsNull()) {
        return false;
    }

    BRepBuilderAPI_MakeFace face_builder(wire, true);
    if (!face_builder.IsDone()) {
        return false;
    }

    face = face_builder.Face();
    return !face.IsNull();
}

TopoDS_Shape make_polyline_extrude_shape(const TopoDS_Face& profile_face,
                                         Vec3 normal,
                                         double distance,
                                         double taper_angle_degrees)
{
    if (profile_face.IsNull() || std::fabs(distance) <= 0.0001) {
        return {};
    }

    return make_extrude_prism(profile_face, normal, static_cast<float>(distance), taper_angle_degrees);
}

bool is_polyline_planar(const std::vector<CPoint3d>& points)
{
    if (points.size() < 4) {
        return true;
    }

    const Vec3 origin = to_vec3(points.front());
    Vec3 normal{};
    bool has_plane = false;
    for (size_t i = 1; i + 1 < points.size(); ++i) {
        const Vec3 a = to_vec3(points[i]) - origin;
        const Vec3 b = to_vec3(points[i + 1]) - origin;
        normal = cross(a, b);
        const float length = std::sqrt(dot(normal, normal));
        if (length > 0.0001f) {
            normal = normal * (1.0f / length);
            has_plane = true;
            break;
        }
    }

    if (!has_plane) {
        return true;
    }

    for (const CPoint3d& point : points) {
        if (std::fabs(dot(to_vec3(point) - origin, normal)) > 0.001f) {
            return false;
        }
    }
    return true;
}

bool is_polyline_on_xy_plane(const std::vector<CPoint3d>& points, double tolerance = 0.001)
{
    if (points.empty()) {
        return true;
    }

    const double plane_height = points.front().y;
    for (const CPoint3d& point : points) {
        if (std::fabs(point.y - plane_height) > tolerance) {
            return false;
        }
    }
    return true;
}

bool nearly_same(Vec3 a, Vec3 b, float tolerance = 0.0001f)
{
    const Vec3 delta = a - b;
    return dot(delta, delta) <= tolerance * tolerance;
}

Vec3 project_to_axis_through_origin(Vec3 point, Vec3 axis_direction)
{
    return axis_direction * dot(point, axis_direction);
}

Vec3 project_to_axis(Vec3 point, Vec3 axis_origin, Vec3 axis_direction)
{
    return axis_origin + axis_direction * dot(point - axis_origin, axis_direction);
}

bool build_face_from_points(const std::vector<Vec3>& points, TopoDS_Shape& profile_shape)
{
    if (points.size() < 3) {
        return false;
    }

    BRepBuilderAPI_MakePolygon polygon;
    for (Vec3 point : points) {
        polygon.Add(gp_Pnt(point.x, point.y, point.z));
    }
    polygon.Close();
    if (!polygon.IsDone()) {
        return false;
    }

    const TopoDS_Wire wire = polygon.Wire();
    if (wire.IsNull()) {
        return false;
    }

    BRepBuilderAPI_MakeFace face_builder(wire, true);
    if (!face_builder.IsDone() || face_builder.Face().IsNull()) {
        return false;
    }

    profile_shape = face_builder.Face();
    return true;
}

bool build_revolve_profile_from_polyline(const CPolyline& polyline, int axis_index, TopoDS_Shape& profile_shape, Vec3& axis_origin)
{
    const std::vector<CPoint3d>& points = polyline.GetPoints();
    if (points.size() < 2 || !is_polyline_on_xy_plane(points)) {
        return false;
    }

    axis_origin = {0.0f, static_cast<float>(points.front().y), 0.0f};

    if (polyline.IsClosed()) {
        BRepBuilderAPI_MakePolygon polygon;
        for (const CPoint3d& point : points) {
            polygon.Add(gp_Pnt(point.x, point.y, point.z));
        }
        polygon.Close();
        if (!polygon.IsDone()) {
            return false;
        }
        const TopoDS_Wire wire = polygon.Wire();
        if (wire.IsNull()) {
            return false;
        }
        BRepBuilderAPI_MakeFace face_builder(wire, true);
        if (face_builder.IsDone() && !face_builder.Face().IsNull()) {
            profile_shape = face_builder.Face();
            return true;
        }
        return false;
    }

    const Vec3 axis_direction = revolve_axis_direction(axis_index);
    std::vector<Vec3> face_points;
    face_points.reserve(points.size() + 2);
    for (const CPoint3d& point : points) {
        const Vec3 value = to_vec3(point);
        if (face_points.empty() || !nearly_same(face_points.back(), value)) {
            face_points.push_back(value);
        }
    }

    const Vec3 first = face_points.front();
    const Vec3 last = face_points.back();
    const Vec3 last_on_axis = project_to_axis(last, axis_origin, axis_direction);
    const Vec3 first_on_axis = project_to_axis(first, axis_origin, axis_direction);
    if (!nearly_same(face_points.back(), last_on_axis)) {
        face_points.push_back(last_on_axis);
    }
    if (!nearly_same(face_points.back(), first_on_axis) && !nearly_same(first, first_on_axis)) {
        face_points.push_back(first_on_axis);
    }

    return build_face_from_points(face_points, profile_shape);
}

Vec3 revolve_axis_direction(int axis_index)
{
    if (axis_index == 0) {
        return {1.0f, 0.0f, 0.0f};
    }
    if (axis_index == 1) {
        return {0.0f, 0.0f, 1.0f};
    }
    return {0.0f, 1.0f, 0.0f};
}

TopoDS_Shape make_polyline_revolve_shape(const TopoDS_Shape& profile_shape,
                                         Vec3 axis_origin,
                                         Vec3 axis_direction,
                                         double angle_degrees)
{
    if (profile_shape.IsNull() || std::fabs(angle_degrees) <= 0.0001) {
        return {};
    }

    const double angle = std::clamp(angle_degrees, 0.0, 360.0) * 3.14159265358979323846 / 180.0;
    if (angle <= 0.000001) {
        return {};
    }

    BRepPrimAPI_MakeRevol revol(profile_shape,
                                gp_Ax1(gp_Pnt(axis_origin.x, axis_origin.y, axis_origin.z),
                                       gp_Dir(axis_direction.x, axis_direction.y, axis_direction.z)),
                                angle,
                                Standard_False);
    revol.Build();
    if (!revol.IsDone()) {
        return {};
    }
    return revol.Shape();
}

TopoDS_Wire make_wire_from_bspline(const CBSpline& spline)
{
    const std::vector<CPoint3d>& points = spline.GetPoints();
    if (points.size() < 2) {
        return {};
    }

    const int samples = std::max(12, static_cast<int>(points.size()) * 12);
    const int array_count = spline.IsClosed() ? samples + 1 : samples + 1;
    TColgp_Array1OfPnt curve_points(1, array_count);
    for (int i = 0; i < array_count; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(array_count - 1);
        const CPoint3d point = spline.Evaluate(t);
        curve_points.SetValue(i + 1, gp_Pnt(point.x, point.y, point.z));
    }

    try {
        GeomAPI_PointsToBSpline curve_builder(curve_points);
        Handle(Geom_BSplineCurve) curve = curve_builder.Curve();
        if (curve.IsNull()) {
            return {};
        }

        BRepBuilderAPI_MakeEdge edge_builder(curve);
        if (!edge_builder.IsDone()) {
            return {};
        }

        BRepBuilderAPI_MakeWire wire_builder;
        wire_builder.Add(edge_builder.Edge());
        if (!wire_builder.IsDone()) {
            return {};
        }
        return wire_builder.Wire();
    } catch (const Standard_Failure&) {
        return {};
    }
}

TopoDS_Shape make_loft_surface_from_splines(const std::vector<const CBSpline*>& splines)
{
    if (splines.size() < 2) {
        return {};
    }

    try {
        BRepOffsetAPI_ThruSections loft(Standard_False, Standard_False, 0.001);
        loft.CheckCompatibility(Standard_True);
        for (const CBSpline* spline : splines) {
            if (!spline || spline->GetPointCount() < 2) {
                return {};
            }
            const TopoDS_Wire wire = make_wire_from_bspline(*spline);
            if (wire.IsNull()) {
                return {};
            }
            loft.AddWire(wire);
        }

        loft.Build();
        if (!loft.IsDone()) {
            return {};
        }
        return loft.Shape();
    } catch (const Standard_Failure&) {
        return {};
    }
}

bool hit_test_polyline_screen(const CPolyline& polyline,
                              DomPoint point,
                              const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                              float tolerance)
{
    const std::vector<CPoint3d>& points = polyline.GetPoints();
    if (points.empty()) {
        return false;
    }

    DomPoint previous{};
    if (!world_to_screen(to_vec3(points.front()), previous)) {
        return false;
    }

    if (points.size() == 1) {
        return distance_to_screen_segment(point, previous, previous) <= tolerance;
    }

    for (size_t i = 1; i < points.size(); ++i) {
        DomPoint current{};
        if (!world_to_screen(to_vec3(points[i]), current)) {
            return false;
        }
        if (distance_to_screen_segment(point, previous, current) <= tolerance) {
            return true;
        }
        previous = current;
    }

    if (polyline.IsClosed()) {
        DomPoint first{};
        if (world_to_screen(to_vec3(points.front()), first)
            && distance_to_screen_segment(point, previous, first) <= tolerance) {
            return true;
        }
    }

    return false;
}

bool hit_test_bspline_screen(const CBSpline& spline,
                             DomPoint point,
                             const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                             float tolerance)
{
    const std::vector<CPoint3d>& points = spline.GetPoints();
    if (points.empty()) {
        return false;
    }

    const int samples = std::max(2, static_cast<int>(points.size()) * 32);
    DomPoint previous{};
    if (!world_to_screen(to_vec3(spline.Evaluate(0.0f)), previous)) {
        return false;
    }

    for (int i = 1; i <= samples; ++i) {
        const CPoint3d curve_point = spline.Evaluate(static_cast<float>(i) / static_cast<float>(samples));
        DomPoint current{};
        if (!world_to_screen(to_vec3(curve_point), current)) {
            previous = current;
            continue;
        }
        if (distance_to_screen_segment(point, previous, current) <= tolerance) {
            return true;
        }
        previous = current;
    }

    return false;
}
}

CAlfaDoc::CAlfaDoc() {
    ResetDefaultMaterials();
    EnsureActivePolyline();
}

struct CAlfaDoc::LiveExtrudeData {
    TopoDS_Shape base_shape;
    TopoDS_Face base_face;
    Vec3 normal{};
    double taper_angle_degrees = 0.0;
    size_t object_index = 0;
    int face_index = -1;
};

struct CAlfaDoc::LivePolylineExtrudeData {
    TopoDS_Face profile_face;
    Vec3 normal{};
    size_t polyline_index = 0;
    size_t solid_index = 0;
    bool has_solid = false;
};

struct CAlfaDoc::LivePolylineRevolveData {
    TopoDS_Shape profile_shape;
    Vec3 axis_origin{};
    size_t polyline_index = 0;
    size_t solid_index = 0;
    bool has_solid = false;
};

struct CAlfaDoc::LiveFilletData {
    TopoDS_Shape base_shape;
    std::vector<TopoDS_Edge> edges;
    size_t object_index = 0;
};

struct CAlfaDoc::LiveChamferData {
    TopoDS_Shape base_shape;
    std::vector<TopoDS_Edge> edges;
    size_t object_index = 0;
};

struct CAlfaDoc::DraftFaceData {
    TopoDS_Shape base_shape;
    TopoDS_Face face;
    gp_Pln neutral_plane;
    Vec3 face_normal{};
    Vec3 draft_direction{};
    Vec3 axis_point{};
    Vec3 axis_dir{};
    size_t object_index = 0;
    int face_index = -1;
    bool has_axis = false;
    bool live_active = false;
};

struct CAlfaDoc::LiveThickSolidData {
    TopoDS_Shape base_shape;
    std::vector<int> face_indices;
    size_t object_index = 0;
    double thickness = 0.0;
    int base_face_count = 0;
    bool rebuild_on_update = false;
};

CAlfaDoc::~CAlfaDoc() = default;

void CAlfaDoc::Clear() {
    objects_.clear();
    ResetDefaultMaterials();
    active_object_index_ = 0;
    ClearSelection();
    EnsureActivePolyline();
}

void CAlfaDoc::ClearActivePolyline() {
    GetActivePolyline().Clear();
}

void CAlfaDoc::CreatePolyline() {
    const size_t next_number = objects_.size() + 1;
    auto polyline = std::make_unique<CPolyline>("Curve " + std::to_string(next_number));
    polyline->SetColor({0.98f, 0.77f, 0.30f});
    AssignDefaultMaterial(*polyline);
    objects_.push_back(std::move(polyline));
    active_object_index_ = objects_.size() - 1;
    ClearSelection();
}

void CAlfaDoc::CreateBSpline() {
    const size_t next_number = objects_.size() + 1;
    auto spline = std::make_unique<CBSpline>("B-Spline " + std::to_string(next_number));
    spline->SetColor({1.0f, 0.08f, 0.10f});
    AssignDefaultMaterial(*spline);
    objects_.push_back(std::move(spline));
    active_object_index_ = objects_.size() - 1;
    ClearSelection();
}

void CAlfaDoc::AddCurvePoint(CurvePoint point) {
    AddCurvePoint(CPoint3d(point.x, 0.08, point.z));
}

void CAlfaDoc::AddCurvePoint(CPoint3d point) {
    CPolyline& polyline = GetActivePolyline();
    if (!polyline.IsClosed() && polyline.GetPointCount() >= 3) {
        const CPoint3d& first = polyline.GetPoints().front();
        const float dx = static_cast<float>(point.x - first.x);
        const float dz = static_cast<float>(point.z - first.z);
        if (std::sqrt(dx * dx + dz * dz) <= 0.20f) {
            polyline.Close();
            return;
        }
    }

    if (!polyline.IsClosed()) {
        polyline.AddPoint(point);
    }
}

void CAlfaDoc::AddBSplinePoint(CPoint3d point) {
    CBSpline& spline = GetActiveBSpline();
    spline.AddPoint(point);
}

void CAlfaDoc::CreateSketchRectangle(const std::vector<CPoint3d>& points, const std::string& sketch_name) {
    if (points.size() != 4) {
        return;
    }

    auto polyline = std::make_unique<CPolyline>(sketch_name.empty() ? "Sketch Rectangle" : sketch_name + " Rectangle");
    polyline->SetColor({0.98f, 0.77f, 0.30f});
    for (const CPoint3d& point : points) {
        polyline->AddPoint(point);
    }
    polyline->Close();
    const Vec3 p0 = to_vec3(points[0]);
    const Vec3 p1 = to_vec3(points[1]);
    const Vec3 p2 = to_vec3(points[2]);
    polyline->SetLockedPlane(p0, cross(p1 - p0, p2 - p0));
    AssignDefaultMaterial(*polyline);
    objects_.push_back(std::move(polyline));
    active_object_index_ = objects_.size() - 1;
    selected_object_index_ = active_object_index_;
    selected_object_indices_ = {active_object_index_};
    has_selected_object_ = true;
    ClearPointSelection();
}

bool CAlfaDoc::CloseSelectedOrActivePolyline() {
    if (CPolyline* selected = GetSelectedPolyline()) {
        return selected->Close();
    }

    if (active_object_index_ < objects_.size()) {
        if (auto* active = dynamic_cast<CPolyline*>(objects_[active_object_index_].get())) {
            return active->Close();
        }
    }

    return false;
}

bool CAlfaDoc::CloseSelectedOrActiveBSpline() {
    if (CBSpline* selected = GetSelectedBSpline()) {
        return selected->Close();
    }

    if (active_object_index_ < objects_.size()) {
        if (auto* active = dynamic_cast<CBSpline*>(objects_[active_object_index_].get())) {
            return active->Close();
        }
    }

    return false;
}

bool CAlfaDoc::CreateMeshFromSelectedPolyline(CVector3d dir, float dist) {
    CPolyline* polyline = GetSelectedPolyline();
    if (!polyline) {
        return false;
    }

    auto mesh = std::make_unique<CMesh3D>();
    if (!mesh->Create(polyline, dir, dist)) {
        return false;
    }

    objects_.push_back(std::move(mesh));
    selected_object_index_ = objects_.size() - 1;
    selected_object_indices_ = {selected_object_index_};
    has_selected_object_ = true;
    ClearPointSelection();
    return true;
}

bool CAlfaDoc::BeginLiveExtrudeSelectedPolyline(double distance, bool reverse, double taper_angle_degrees) {
    CPolyline* polyline = GetSelectedPolyline();
    if (!polyline) {
        return false;
    }

    TopoDS_Face profile_face;
    Vec3 normal{};
    if (!build_profile_face_from_polyline(*polyline, profile_face, normal)) {
        return false;
    }

    live_polyline_extrude_ = std::make_unique<LivePolylineExtrudeData>();
    live_polyline_extrude_->profile_face = profile_face;
    live_polyline_extrude_->normal = normal;
    live_polyline_extrude_->polyline_index = selected_object_index_;
    live_polyline_extrude_->solid_index = objects_.size();
    live_polyline_extrude_->has_solid = false;
    return UpdateLiveExtrudeSelectedPolyline(distance, reverse, taper_angle_degrees);
}

bool CAlfaDoc::HasLivePolylineExtrude() const {
    return live_polyline_extrude_ != nullptr;
}

bool CAlfaDoc::UpdateLiveExtrudeSelectedPolyline(double distance, bool reverse, double taper_angle_degrees) {
    if (!live_polyline_extrude_) {
        return false;
    }

    if (std::fabs(distance) <= 0.0001) {
        if (live_polyline_extrude_->has_solid && live_polyline_extrude_->solid_index < objects_.size()) {
            objects_.erase(objects_.begin() + static_cast<ObjectList::difference_type>(live_polyline_extrude_->solid_index));
            live_polyline_extrude_->has_solid = false;
            live_polyline_extrude_->solid_index = objects_.size();
        }
        if (live_polyline_extrude_->polyline_index < objects_.size()) {
            selected_object_index_ = live_polyline_extrude_->polyline_index;
            active_object_index_ = selected_object_index_;
            selected_object_indices_ = {selected_object_index_};
            has_selected_object_ = true;
        }
        ClearPointSelection();
        return false;
    }

    const double signed_distance = reverse ? -distance : distance;
    try {
        TopoDS_Shape shape = make_polyline_extrude_shape(live_polyline_extrude_->profile_face,
                                                         live_polyline_extrude_->normal,
                                                         signed_distance,
                                                         taper_angle_degrees);
        if (shape.IsNull()) {
            return false;
        }

        auto solid = std::make_unique<CSolid>(shape);
        solid->SetName("Extrude Solid");
        solid->SetColor({0.64f, 0.70f, 0.58f});
        AssignDefaultMaterial(*solid);
        solid->InitSurfaces();
        solid->InitEdges();
        if (!solid->BuldMesh(0.1f)) {
            return false;
        }

        if (live_polyline_extrude_->has_solid && live_polyline_extrude_->solid_index < objects_.size()) {
            objects_[live_polyline_extrude_->solid_index] = std::move(solid);
        } else {
            objects_.push_back(std::move(solid));
            live_polyline_extrude_->solid_index = objects_.size() - 1;
            live_polyline_extrude_->has_solid = true;
        }

        selected_object_index_ = live_polyline_extrude_->solid_index;
        active_object_index_ = selected_object_index_;
        selected_object_indices_ = {selected_object_index_};
        has_selected_object_ = true;
        ClearPointSelection();
        return true;
    } catch (const Standard_Failure&) {
        return false;
    }
}

bool CAlfaDoc::FinishLiveExtrudeSelectedPolyline() {
    if (!live_polyline_extrude_ || !live_polyline_extrude_->has_solid) {
        return false;
    }

    const size_t solid_index = live_polyline_extrude_->solid_index;
    live_polyline_extrude_.reset();
    if (solid_index < objects_.size()) {
        selected_object_index_ = solid_index;
        active_object_index_ = solid_index;
        selected_object_indices_ = {solid_index};
        has_selected_object_ = true;
        ClearPointSelection();
        return true;
    }
    ClearSelection();
    return false;
}

void CAlfaDoc::CancelLiveExtrudeSelectedPolyline() {
    if (live_polyline_extrude_ && live_polyline_extrude_->has_solid && live_polyline_extrude_->solid_index < objects_.size()) {
        objects_.erase(objects_.begin() + static_cast<ObjectList::difference_type>(live_polyline_extrude_->solid_index));
    }

    const size_t polyline_index = live_polyline_extrude_ ? live_polyline_extrude_->polyline_index : objects_.size();
    live_polyline_extrude_.reset();
    if (polyline_index < objects_.size()) {
        selected_object_index_ = polyline_index;
        active_object_index_ = polyline_index;
        selected_object_indices_ = {polyline_index};
        has_selected_object_ = true;
        ClearPointSelection();
    } else {
        ClearSelection();
    }
}

bool CAlfaDoc::BeginLiveRevolveSelectedPolyline(double angle_degrees, int axis_index) {
    CPolyline* polyline = GetSelectedPolyline();
    if (!polyline) {
        return false;
    }

    live_polyline_revolve_ = std::make_unique<LivePolylineRevolveData>();
    live_polyline_revolve_->polyline_index = selected_object_index_;
    live_polyline_revolve_->solid_index = objects_.size();
    live_polyline_revolve_->has_solid = false;
    if (UpdateLiveRevolveSelectedPolyline(angle_degrees, axis_index)) {
        return true;
    }

    live_polyline_revolve_.reset();
    return false;
}

bool CAlfaDoc::HasLivePolylineRevolve() const {
    return live_polyline_revolve_ != nullptr;
}

bool CAlfaDoc::UpdateLiveRevolveSelectedPolyline(double angle_degrees, int axis_index) {
    if (!live_polyline_revolve_) {
        return false;
    }

    if (std::fabs(angle_degrees) <= 0.0001) {
        if (live_polyline_revolve_->has_solid && live_polyline_revolve_->solid_index < objects_.size()) {
            objects_.erase(objects_.begin() + static_cast<ObjectList::difference_type>(live_polyline_revolve_->solid_index));
            live_polyline_revolve_->has_solid = false;
            live_polyline_revolve_->solid_index = objects_.size();
        }
        if (live_polyline_revolve_->polyline_index < objects_.size()) {
            selected_object_index_ = live_polyline_revolve_->polyline_index;
            active_object_index_ = selected_object_index_;
            selected_object_indices_ = {selected_object_index_};
            has_selected_object_ = true;
        }
        ClearPointSelection();
        return false;
    }

    try {
        if (live_polyline_revolve_->polyline_index >= objects_.size()) {
            return false;
        }

        const auto* polyline = dynamic_cast<const CPolyline*>(objects_[live_polyline_revolve_->polyline_index].get());
        if (!polyline) {
            return false;
        }

        TopoDS_Shape profile_shape;
        Vec3 axis_origin{};
        if (!build_revolve_profile_from_polyline(*polyline, axis_index, profile_shape, axis_origin)) {
            return false;
        }

        live_polyline_revolve_->profile_shape = profile_shape;
        live_polyline_revolve_->axis_origin = axis_origin;

        TopoDS_Shape shape = make_polyline_revolve_shape(live_polyline_revolve_->profile_shape,
                                                         live_polyline_revolve_->axis_origin,
                                                         revolve_axis_direction(axis_index),
                                                         angle_degrees);
        if (shape.IsNull()) {
            return false;
        }

        auto solid = std::make_unique<CSolid>(shape);
        solid->SetName("Revolve Solid");
        solid->SetColor({0.64f, 0.70f, 0.58f});
        AssignDefaultMaterial(*solid);
        solid->InitSurfaces();
        solid->InitEdges();
        if (!solid->BuldMesh(0.1f)) {
            return false;
        }

        if (live_polyline_revolve_->has_solid && live_polyline_revolve_->solid_index < objects_.size()) {
            objects_[live_polyline_revolve_->solid_index] = std::move(solid);
        } else {
            objects_.push_back(std::move(solid));
            live_polyline_revolve_->solid_index = objects_.size() - 1;
            live_polyline_revolve_->has_solid = true;
        }

        selected_object_index_ = live_polyline_revolve_->solid_index;
        active_object_index_ = selected_object_index_;
        selected_object_indices_ = {selected_object_index_};
        has_selected_object_ = true;
        ClearPointSelection();
        return true;
    } catch (const Standard_Failure&) {
        return false;
    }
}

bool CAlfaDoc::FinishLiveRevolveSelectedPolyline() {
    if (!live_polyline_revolve_ || !live_polyline_revolve_->has_solid) {
        return false;
    }

    const size_t solid_index = live_polyline_revolve_->solid_index;
    live_polyline_revolve_.reset();
    if (solid_index < objects_.size()) {
        selected_object_index_ = solid_index;
        active_object_index_ = solid_index;
        selected_object_indices_ = {solid_index};
        has_selected_object_ = true;
        ClearPointSelection();
        return true;
    }
    ClearSelection();
    return false;
}

void CAlfaDoc::CancelLiveRevolveSelectedPolyline() {
    if (live_polyline_revolve_ && live_polyline_revolve_->has_solid && live_polyline_revolve_->solid_index < objects_.size()) {
        objects_.erase(objects_.begin() + static_cast<ObjectList::difference_type>(live_polyline_revolve_->solid_index));
    }

    const size_t polyline_index = live_polyline_revolve_ ? live_polyline_revolve_->polyline_index : objects_.size();
    live_polyline_revolve_.reset();
    if (polyline_index < objects_.size()) {
        selected_object_index_ = polyline_index;
        active_object_index_ = polyline_index;
        selected_object_indices_ = {polyline_index};
        has_selected_object_ = true;
        ClearPointSelection();
    } else {
        ClearSelection();
    }
}

bool CAlfaDoc::SelectObjectAt(CurvePoint point, float tolerance, bool include_mesh) {
    for (size_t i = objects_.size(); i > 0; --i) {
        const size_t index = i - 1;
        if (!include_mesh && dynamic_cast<CMesh3D*>(objects_[index].get())) {
            continue;
        }
        if (objects_[index]->IsVisible() && objects_[index]->HitTest(point, tolerance)) {
            for (ObjectPtr& object : objects_) {
                if (auto* solid = dynamic_cast<CSolid*>(object.get())) {
                    solid->ClearSelectedEdge();
                    solid->ClearSelectedFace();
                }
            }
            selected_object_index_ = index;
            active_object_index_ = index;
            selected_object_indices_ = {index};
            has_selected_object_ = true;
            ClearPointSelection();
            return true;
        }
    }

    ClearSelection();
    return false;
}

bool CAlfaDoc::AddObjectToSelectionAt(CurvePoint point, float tolerance, bool include_mesh) {
    for (size_t i = objects_.size(); i > 0; --i) {
        const size_t index = i - 1;
        if (!include_mesh && dynamic_cast<CMesh3D*>(objects_[index].get())) {
            continue;
        }
        if (objects_[index]->IsVisible() && objects_[index]->HitTest(point, tolerance)) {
            selected_object_index_ = index;
            active_object_index_ = index;
            has_selected_object_ = true;
            ClearPointSelection();
            if (!IsObjectSelected(index)) {
                selected_object_indices_.push_back(index);
            }
            return true;
        }
    }

    return false;
}

bool CAlfaDoc::RemoveObjectFromSelectionAt(CurvePoint point, float tolerance, bool include_mesh) {
    for (size_t i = objects_.size(); i > 0; --i) {
        const size_t index = i - 1;
        if (!include_mesh && dynamic_cast<CMesh3D*>(objects_[index].get())) {
            continue;
        }
        if (!objects_[index]->IsVisible() || !objects_[index]->HitTest(point, tolerance)) {
            continue;
        }

        auto existing = std::find(selected_object_indices_.begin(), selected_object_indices_.end(), index);
        if (existing == selected_object_indices_.end()) {
            return false;
        }

        selected_object_indices_.erase(existing);
        ClearPointSelection();
        if (selected_object_indices_.empty()) {
            ClearSelection();
        } else {
            selected_object_index_ = selected_object_indices_.back();
            active_object_index_ = selected_object_index_;
            has_selected_object_ = true;
        }
        return true;
    }

    return false;
}

bool CAlfaDoc::ToggleObjectSelectionAt(CurvePoint point, float tolerance, bool include_mesh) {
    for (size_t i = objects_.size(); i > 0; --i) {
        const size_t index = i - 1;
        if (!include_mesh && dynamic_cast<CMesh3D*>(objects_[index].get())) {
            continue;
        }
        if (!objects_[index]->IsVisible() || !objects_[index]->HitTest(point, tolerance)) {
            continue;
        }

        auto existing = std::find(selected_object_indices_.begin(), selected_object_indices_.end(), index);
        if (existing != selected_object_indices_.end()) {
            selected_object_indices_.erase(existing);
            ClearPointSelection();
            if (selected_object_indices_.empty()) {
                ClearSelection();
            } else {
                selected_object_index_ = selected_object_indices_.back();
                active_object_index_ = selected_object_index_;
                has_selected_object_ = true;
            }
            return true;
        }

        selected_object_indices_.push_back(index);
        selected_object_index_ = index;
        active_object_index_ = index;
        has_selected_object_ = true;
        ClearPointSelection();
        return true;
    }

    return false;
}

bool CAlfaDoc::SelectSolidEdgeAtScreen(DomPoint point,
                                       const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                       float tolerance,
                                       SelectionAction action) {
    for (size_t i = objects_.size(); i > 0; --i) {
        const size_t index = i - 1;
        CSolid* solid = dynamic_cast<CSolid*>(objects_[index].get());
        if (!solid || !solid->IsVisible()) {
            continue;
        }

        int surface_index = -1;
        int edge_index = -1;
        if (solid->HitTestEdgeScreen(point, world_to_screen, tolerance, surface_index, edge_index)) {
            if (action == SelectionAction::Replace || selected_object_index_ != index) {
                for (ObjectPtr& object : objects_) {
                    if (auto* other_solid = dynamic_cast<CSolid*>(object.get())) {
                        other_solid->ClearSelectedEdge();
                        other_solid->ClearSelectedFace();
                    }
                }
                if (action == SelectionAction::Remove) {
                    return false;
                }
                solid->SetSelectedEdge(surface_index, edge_index);
            } else if (action == SelectionAction::Add) {
                solid->AddSelectedEdge(surface_index, edge_index);
            } else {
                solid->RemoveSelectedEdge(surface_index, edge_index);
            }

            selected_object_index_ = index;
            active_object_index_ = index;
            selected_object_indices_ = {index};
            has_selected_object_ = true;
            ClearPointSelection();
            return true;
        }
    }

    return false;
}

bool CAlfaDoc::SelectSolidMeshAtScreen(DomPoint point,
                                       const std::function<bool(Vec3, DomPoint&, float&)>& project_world,
                                       SelectionAction action) {
    size_t best_index = objects_.size();
    float best_depth = std::numeric_limits<float>::max();

    for (size_t index = 0; index < objects_.size(); ++index) {
        CSolid* solid = dynamic_cast<CSolid*>(objects_[index].get());
        if (!solid || !solid->IsVisible()) {
            continue;
        }

        float depth = 0.0f;
        if (solid->HitTestMeshScreen(point, project_world, depth) && depth < best_depth) {
            best_index = index;
            best_depth = depth;
        }
    }

    if (best_index >= objects_.size()) {
        return false;
    }

    for (ObjectPtr& object : objects_) {
        if (auto* solid = dynamic_cast<CSolid*>(object.get())) {
            solid->ClearSelectedEdge();
            solid->ClearSelectedFace();
        }
    }

    if (action == SelectionAction::Add) {
        if (!IsObjectSelected(best_index)) {
            selected_object_indices_.push_back(best_index);
        }
    } else if (action == SelectionAction::Remove) {
        auto existing = std::find(selected_object_indices_.begin(), selected_object_indices_.end(), best_index);
        if (existing == selected_object_indices_.end()) {
            return false;
        }
        selected_object_indices_.erase(existing);
        ClearPointSelection();
        if (selected_object_indices_.empty()) {
            ClearSelection();
            return true;
        }
    } else {
        selected_object_indices_ = {best_index};
    }

    selected_object_index_ = best_index;
    active_object_index_ = best_index;
    has_selected_object_ = true;
    ClearPointSelection();
    return true;
}

bool CAlfaDoc::SelectMeshAtScreen(DomPoint point,
                                  const std::function<bool(Vec3, DomPoint&, float&)>& project_world,
                                  SelectionAction action) {
    size_t best_index = objects_.size();
    float best_depth = std::numeric_limits<float>::max();

    for (size_t index = 0; index < objects_.size(); ++index) {
        CMesh3D* mesh = dynamic_cast<CMesh3D*>(objects_[index].get());
        if (!mesh || !mesh->IsVisible()) {
            continue;
        }

        float depth = 0.0f;
        if (mesh->HitTestMeshScreen(point, project_world, depth) && depth < best_depth) {
            best_index = index;
            best_depth = depth;
        }
    }

    if (best_index >= objects_.size()) {
        return false;
    }

    for (ObjectPtr& object : objects_) {
        if (auto* solid = dynamic_cast<CSolid*>(object.get())) {
            solid->ClearSelectedEdge();
            solid->ClearSelectedFace();
        }
    }

    if (action == SelectionAction::Add) {
        if (!IsObjectSelected(best_index)) {
            selected_object_indices_.push_back(best_index);
        }
    } else if (action == SelectionAction::Remove) {
        auto existing = std::find(selected_object_indices_.begin(), selected_object_indices_.end(), best_index);
        if (existing == selected_object_indices_.end()) {
            return false;
        }
        selected_object_indices_.erase(existing);
        ClearPointSelection();
        if (selected_object_indices_.empty()) {
            ClearSelection();
            return true;
        }
    } else {
        selected_object_indices_ = {best_index};
    }

    selected_object_index_ = best_index;
    active_object_index_ = best_index;
    has_selected_object_ = true;
    ClearPointSelection();
    return true;
}

bool CAlfaDoc::SelectSolidPlanarFaceAtScreen(DomPoint point,
                                             const std::function<bool(Vec3, DomPoint&, float&)>& project_world,
                                             SelectionAction action) {
    return SelectSolidFaceAtScreen(point, project_world, true, action);
}

bool CAlfaDoc::SelectSolidFaceAtScreen(DomPoint point,
                                       const std::function<bool(Vec3, DomPoint&, float&)>& project_world,
                                       bool planar_only,
                                       SelectionAction action) {
    size_t best_index = objects_.size();
    int best_surface = -1;
    float best_depth = std::numeric_limits<float>::max();

    for (size_t index = 0; index < objects_.size(); ++index) {
        CSolid* solid = dynamic_cast<CSolid*>(objects_[index].get());
        if (!solid || !solid->IsVisible()) {
            continue;
        }

        int surface_index = -1;
        float depth = 0.0f;
        if (solid->HitTestFaceScreen(point, project_world, planar_only, surface_index, depth) && depth < best_depth) {
            best_index = index;
            best_surface = surface_index;
            best_depth = depth;
        }
    }

    if (best_index >= objects_.size() || best_surface < 0) {
        return false;
    }

    auto* solid = dynamic_cast<CSolid*>(objects_[best_index].get());
    if (!solid) {
        return false;
    }

    if (action == SelectionAction::Add && has_selected_solid_face_ && selected_face_object_index_ == best_index) {
        solid->AddSelectedFace(best_surface);
        if (std::find(selected_solid_face_indices_.begin(), selected_solid_face_indices_.end(), best_surface)
            == selected_solid_face_indices_.end()) {
            selected_solid_face_indices_.push_back(best_surface);
        }
    } else if (action == SelectionAction::Remove && has_selected_solid_face_ && selected_face_object_index_ == best_index) {
        solid->RemoveSelectedFace(best_surface);
        auto existing = std::find(selected_solid_face_indices_.begin(), selected_solid_face_indices_.end(), best_surface);
        if (existing != selected_solid_face_indices_.end()) {
            selected_solid_face_indices_.erase(existing);
        }
    } else {
        for (ObjectPtr& object : objects_) {
            if (auto* other_solid = dynamic_cast<CSolid*>(object.get())) {
                other_solid->ClearSelectedEdge();
                other_solid->ClearSelectedFace();
            }
        }
        if (action == SelectionAction::Remove) {
            return false;
        }
        solid->SetSelectedFace(best_surface);
        selected_solid_face_indices_ = {best_surface};
    }

    selected_object_index_ = 0;
    selected_face_object_index_ = best_index;
    active_object_index_ = best_index;
    selected_object_indices_.clear();
    has_selected_object_ = false;
    has_selected_solid_face_ = solid->HasSelectedFace();
    if (!has_selected_solid_face_) {
        selected_solid_face_indices_.clear();
    }
    ClearPointSelection();
    return true;
}

bool CAlfaDoc::HasSelectedSolidFace() const {
    if (!has_selected_solid_face_ || selected_face_object_index_ >= objects_.size()) {
        return false;
    }
    const CSolid* solid = dynamic_cast<const CSolid*>(objects_[selected_face_object_index_].get());
    return solid && solid->HasSelectedFace();
}

bool CAlfaDoc::GetSelectedSolidFaceCenterAndNormal(Vec3& center, Vec3& normal) const {
    if (!has_selected_solid_face_ || selected_face_object_index_ >= objects_.size()) {
        return false;
    }
    const CSolid* solid = dynamic_cast<const CSolid*>(objects_[selected_face_object_index_].get());
    return solid && solid->HasSelectedFace() && solid->GetFaceCenterAndNormal(solid->GetSelectedFaceIndex(), center, normal);
}

bool CAlfaDoc::PreviewExtrudeSelectedSolidFace(Vec3 delta) {
    if (!has_selected_solid_face_ || selected_face_object_index_ >= objects_.size()) {
        return false;
    }
    CSolid* solid = dynamic_cast<CSolid*>(objects_[selected_face_object_index_].get());
    if (!solid || !solid->HasSelectedFace()) {
        return false;
    }
    CSurfaceFace* face = solid->GetSurfaceFace(solid->GetSelectedFaceIndex());
    if (!face) {
        return false;
    }
    face->PreviewTranslate(delta);
    return true;
}

bool CAlfaDoc::BeginLiveExtrudeSelectedSolidFace(double taper_angle_degrees) {
    if (!has_selected_solid_face_ || selected_face_object_index_ >= objects_.size()) {
        return false;
    }

    CSolid* solid = dynamic_cast<CSolid*>(objects_[selected_face_object_index_].get());
    if (!solid || !solid->HasSelectedFace() || solid->m_Shape.IsNull()) {
        return false;
    }

    TopoDS_Face face = solid->GetTopoFace(solid->GetSelectedFaceIndex());
    if (face.IsNull()) {
        return false;
    }
    Vec3 center{};
    Vec3 normal{};
    if (!solid->GetFaceCenterAndNormal(solid->GetSelectedFaceIndex(), center, normal)) {
        return false;
    }

    live_extrude_ = std::make_unique<LiveExtrudeData>();
    live_extrude_->base_shape = solid->m_Shape;
    live_extrude_->base_face = face;
    live_extrude_->normal = normal;
    live_extrude_->taper_angle_degrees = taper_angle_degrees;
    live_extrude_->object_index = selected_face_object_index_;
    live_extrude_->face_index = solid->GetSelectedFaceIndex();
    return true;
}

bool CAlfaDoc::IsLiveExtrudeSelectedSolidFaceActive() const {
    return live_extrude_ && live_extrude_->object_index < objects_.size();
}

bool CAlfaDoc::UpdateLiveExtrudeSelectedSolidFace(float distance) {
    if (!live_extrude_ || live_extrude_->object_index >= objects_.size()) {
        return false;
    }

    CSolid* solid = dynamic_cast<CSolid*>(objects_[live_extrude_->object_index].get());
    if (!solid || live_extrude_->base_shape.IsNull() || live_extrude_->base_face.IsNull()) {
        return false;
    }

    if (std::fabs(distance) <= 0.0001f) {
        solid->m_Shape = live_extrude_->base_shape;
    } else {
        try {
            TopoDS_Shape prism_shape = make_extrude_prism(live_extrude_->base_face,
                                                          live_extrude_->normal,
                                                          distance,
                                                          live_extrude_->taper_angle_degrees);
            if (prism_shape.IsNull()) {
                return false;
            }

            TopoDS_Shape result_shape;
            if (distance >= 0.0f) {
                BRepAlgoAPI_Fuse operation(live_extrude_->base_shape, prism_shape);
                operation.Build();
                if (!operation.IsDone()) {
                    return false;
                }
                result_shape = operation.Shape();
            } else {
                BRepAlgoAPI_Cut operation(live_extrude_->base_shape, prism_shape);
                operation.Build();
                if (!operation.IsDone()) {
                    return false;
                }
                result_shape = operation.Shape();
            }

            if (result_shape.IsNull()) {
                return false;
            }
            solid->m_Shape = result_shape;
        } catch (const Standard_Failure&) {
            return false;
        }
    }

    solid->ClearSelectedEdge();
    solid->ClearSelectedFace();
    solid->InitSurfaces();
    solid->InitEdges();
    solid->BuldMesh(0.1f);
    active_object_index_ = live_extrude_->object_index;
    selected_face_object_index_ = 0;
    has_selected_solid_face_ = false;
    return true;
}

void CAlfaDoc::FinishLiveExtrudeSelectedSolidFace() {
    live_extrude_.reset();
    ClearSelection();
}

void CAlfaDoc::CancelLiveExtrudeSelectedSolidFace() {
    if (live_extrude_ && live_extrude_->object_index < objects_.size()) {
        if (auto* solid = dynamic_cast<CSolid*>(objects_[live_extrude_->object_index].get())) {
            solid->m_Shape = live_extrude_->base_shape;
            solid->InitSurfaces();
            solid->InitEdges();
            solid->BuldMesh(0.1f);
        }
    }
    live_extrude_.reset();
    ClearSelection();
}

bool CAlfaDoc::ApplyExtrudeSelectedSolidFace(float distance) {
    if (!has_selected_solid_face_ || selected_face_object_index_ >= objects_.size()) {
        return false;
    }
    const size_t solid_index = selected_face_object_index_;
    CSolid* solid = dynamic_cast<CSolid*>(objects_[solid_index].get());
    if (!solid || !solid->HasSelectedFace() || std::fabs(distance) <= 0.0001f) {
        return false;
    }

    Vec3 center{};
    Vec3 normal{};
    if (!solid->GetFaceCenterAndNormal(solid->GetSelectedFaceIndex(), center, normal)) {
        return false;
    }

    TopoDS_Face face = solid->GetTopoFace(solid->GetSelectedFaceIndex());
    if (face.IsNull() || solid->m_Shape.IsNull()) {
        return false;
    }

    try {
        TopoDS_Shape prism_shape = make_extrude_prism(face, normal, distance, 0.0);
        if (prism_shape.IsNull()) {
            return false;
        }

        TopoDS_Shape result_shape;
        if (distance >= 0.0f) {
            BRepAlgoAPI_Fuse operation(solid->m_Shape, prism_shape);
            operation.Build();
            if (!operation.IsDone()) {
                return false;
            }
            result_shape = operation.Shape();
        } else {
            BRepAlgoAPI_Cut operation(solid->m_Shape, prism_shape);
            operation.Build();
            if (!operation.IsDone()) {
                return false;
            }
            result_shape = operation.Shape();
        }
        if (result_shape.IsNull()) {
            return false;
        }

        solid->m_Shape = result_shape;
        solid->ClearSelectedFace();
        solid->ClearSelectedEdge();
        solid->InitSurfaces();
        solid->InitEdges();
        solid->BuldMesh(0.1f);
        has_selected_solid_face_ = false;
        selected_face_object_index_ = 0;
        return true;
    } catch (const Standard_Failure&) {
        return false;
    }
}

bool CAlfaDoc::BeginDraftFaceFromSelectedFace() {
    if (!has_selected_solid_face_ || selected_face_object_index_ >= objects_.size()) {
        return false;
    }

    CSolid* solid = dynamic_cast<CSolid*>(objects_[selected_face_object_index_].get());
    if (!solid || !solid->HasSelectedFace() || solid->m_Shape.IsNull()) {
        return false;
    }

    const int face_index = solid->GetSelectedFaceIndex();
    TopoDS_Face face = solid->GetTopoFace(face_index);
    if (face.IsNull()) {
        return false;
    }

    Vec3 center{};
    Vec3 normal{};
    if (!solid->GetFaceCenterAndNormal(face_index, center, normal)) {
        return false;
    }

    draft_face_ = std::make_unique<DraftFaceData>();
    draft_face_->base_shape = solid->m_Shape;
    draft_face_->face = face;
    draft_face_->face_normal = normal;
    draft_face_->object_index = selected_face_object_index_;
    draft_face_->face_index = face_index;
    return true;
}

bool CAlfaDoc::HasDraftFace() const {
    return draft_face_ && draft_face_->object_index < objects_.size() && !draft_face_->face.IsNull();
}

bool CAlfaDoc::HasDraftFaceAxis() const {
    return HasDraftFace() && draft_face_->has_axis;
}

bool CAlfaDoc::GetDraftFaceAxis(Vec3& center, Vec3& axis) const {
    if (!HasDraftFaceAxis()) {
        return false;
    }
    center = draft_face_->axis_point;
    axis = draft_face_->axis_dir;
    return true;
}

bool CAlfaDoc::SelectDraftFaceAxisEdgeAtScreen(DomPoint point,
                                               const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                               float tolerance) {
    if (!HasDraftFace()) {
        return false;
    }

    CSolid* solid = dynamic_cast<CSolid*>(objects_[draft_face_->object_index].get());
    if (!solid || !solid->IsVisible()) {
        return false;
    }

    int surface_index = -1;
    int edge_index = -1;
    if (!solid->HitTestEdgeScreen(point, world_to_screen, tolerance, surface_index, edge_index)) {
        return false;
    }

    CSurfaceFace* surface = solid->GetSurfaceFace(surface_index);
    const TopoDS_Edge* edge = surface ? surface->GetTopoEdge(edge_index) : nullptr;
    if (!edge || edge->IsNull()) {
        return false;
    }

    bool edge_belongs_to_face = false;
    for (TopExp_Explorer explorer(draft_face_->face, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        if (explorer.Current().IsSame(*edge)) {
            edge_belongs_to_face = true;
            break;
        }
    }
    if (!edge_belongs_to_face) {
        return false;
    }

    try {
        BRepAdaptor_Curve curve(*edge);
        if (curve.GetType() != GeomAbs_Line) {
            return false;
        }

        const Standard_Real first = curve.FirstParameter();
        const Standard_Real last = curve.LastParameter();
        if (last <= first) {
            return false;
        }

        const gp_Pnt p0 = curve.Value(first);
        const gp_Pnt p1 = curve.Value(last);
        Vec3 axis_point{static_cast<float>((p0.X() + p1.X()) * 0.5),
                        static_cast<float>((p0.Y() + p1.Y()) * 0.5),
                        static_cast<float>((p0.Z() + p1.Z()) * 0.5)};
        Vec3 axis_dir{static_cast<float>(p1.X() - p0.X()),
                      static_cast<float>(p1.Y() - p0.Y()),
                      static_cast<float>(p1.Z() - p0.Z())};
        axis_dir = normalize(axis_dir);
        if (dot(axis_dir, axis_dir) <= 0.000001f) {
            return false;
        }

        Vec3 plane_normal = cross(axis_dir, draft_face_->face_normal);
        plane_normal = normalize(plane_normal);
        if (dot(plane_normal, plane_normal) <= 0.000001f) {
            return false;
        }

        draft_face_->axis_point = axis_point;
        draft_face_->axis_dir = axis_dir;
        draft_face_->draft_direction = plane_normal;
        draft_face_->neutral_plane = gp_Pln(gp_Pnt(axis_point.x, axis_point.y, axis_point.z),
                                            gp_Dir(plane_normal.x, plane_normal.y, plane_normal.z));
        draft_face_->has_axis = true;
        solid->SetSelectedEdge(surface_index, edge_index);
        selected_object_index_ = draft_face_->object_index;
        selected_object_indices_ = {draft_face_->object_index};
        active_object_index_ = draft_face_->object_index;
        has_selected_object_ = true;
        has_selected_solid_face_ = false;
        selected_face_object_index_ = 0;
        ClearPointSelection();
        return true;
    } catch (const Standard_Failure&) {
        return false;
    }
}

bool CAlfaDoc::BeginLiveDraftFace() {
    if (!HasDraftFaceAxis()) {
        return false;
    }
    draft_face_->live_active = true;
    return true;
}

bool CAlfaDoc::IsLiveDraftFaceActive() const {
    return HasDraftFaceAxis() && draft_face_->live_active;
}

bool CAlfaDoc::UpdateLiveDraftFace(double angle_degrees) {
    if (!IsLiveDraftFaceActive() || draft_face_->object_index >= objects_.size()) {
        return false;
    }

    CSolid* solid = dynamic_cast<CSolid*>(objects_[draft_face_->object_index].get());
    if (!solid) {
        return false;
    }

    TopoDS_Shape result_shape = make_draft_face_shape(draft_face_->base_shape,
                                                      draft_face_->face,
                                                      draft_face_->draft_direction,
                                                      draft_face_->neutral_plane,
                                                      angle_degrees);
    if (result_shape.IsNull()) {
        return false;
    }

    solid->m_Shape = result_shape;
    solid->ClearSelectedEdge();
    solid->ClearSelectedFace();
    solid->InitSurfaces();
    solid->InitEdges();
    solid->BuldMesh(0.1f);
    active_object_index_ = draft_face_->object_index;
    return true;
}

void CAlfaDoc::FinishLiveDraftFace() {
    draft_face_.reset();
    ClearSelection();
}

void CAlfaDoc::CancelLiveDraftFace() {
    if (draft_face_ && draft_face_->object_index < objects_.size()) {
        if (auto* solid = dynamic_cast<CSolid*>(objects_[draft_face_->object_index].get())) {
            solid->m_Shape = draft_face_->base_shape;
            solid->InitSurfaces();
            solid->InitEdges();
            solid->BuldMesh(0.1f);
        }
    }
    draft_face_.reset();
    ClearSelection();
}

bool CAlfaDoc::BeginLiveThickSolidFromSelectedSolid(double thickness) {
    if (!HasSelection()) {
        return false;
    }

    const size_t solid_index = selected_object_index_;
    if (solid_index >= objects_.size()) {
        return false;
    }

    auto* solid = dynamic_cast<CSolid*>(objects_[solid_index].get());
    if (!solid || solid->m_Shape.IsNull()) {
        return false;
    }

    live_thick_solid_ = std::make_unique<LiveThickSolidData>();
    live_thick_solid_->base_shape = solid->m_Shape;
    live_thick_solid_->object_index = solid_index;
    live_thick_solid_->thickness = thickness;
    live_thick_solid_->base_face_count = count_faces(solid->m_Shape);
    live_thick_solid_->rebuild_on_update = false;
    selected_object_index_ = solid_index;
    selected_object_indices_.clear();
    active_object_index_ = solid_index;
    has_selected_object_ = false;
    has_selected_solid_face_ = false;
    ClearPointSelection();
    return true;
}

bool CAlfaDoc::BeginLiveThickSolidFromSelectedFaces(double thickness) {
    if (!has_selected_solid_face_ || selected_face_object_index_ >= objects_.size()) {
        return false;
    }

    auto* solid = dynamic_cast<CSolid*>(objects_[selected_face_object_index_].get());
    if (!solid || solid->m_Shape.IsNull() || selected_solid_face_indices_.empty()) {
        return false;
    }

    live_thick_solid_ = std::make_unique<LiveThickSolidData>();
    live_thick_solid_->base_shape = solid->m_Shape;
    live_thick_solid_->face_indices = selected_solid_face_indices_;
    live_thick_solid_->object_index = selected_face_object_index_;
    live_thick_solid_->thickness = thickness;
    live_thick_solid_->base_face_count = count_faces(solid->m_Shape);
    live_thick_solid_->rebuild_on_update = true;
    if (!UpdateLiveThickSolid(thickness)) {
        live_thick_solid_.reset();
        return false;
    }
    return true;
}

bool CAlfaDoc::HasLiveThickSolid() const {
    return live_thick_solid_ && live_thick_solid_->object_index < objects_.size();
}

bool CAlfaDoc::SelectLiveThickSolidFaceAtScreen(DomPoint point,
                                                const std::function<bool(Vec3, DomPoint&, float&)>& project_world) {
    if (!HasLiveThickSolid()) {
        return false;
    }

    auto* solid = dynamic_cast<CSolid*>(objects_[live_thick_solid_->object_index].get());
    if (!solid || !solid->IsVisible()) {
        return false;
    }

    int surface_index = -1;
    float depth = 0.0f;
    if (!solid->HitTestFaceScreen(point, project_world, false, surface_index, depth)) {
        return false;
    }
    if (surface_index < 0 || surface_index >= live_thick_solid_->base_face_count) {
        return false;
    }

    if (std::find(live_thick_solid_->face_indices.begin(), live_thick_solid_->face_indices.end(), surface_index)
        == live_thick_solid_->face_indices.end()) {
        live_thick_solid_->face_indices.push_back(surface_index);
    }

    auto* selected_solid = dynamic_cast<CSolid*>(objects_[live_thick_solid_->object_index].get());
    if (selected_solid) {
        selected_solid->ClearSelectedEdge();
        selected_solid->AddSelectedFace(surface_index);
    }
    selected_object_index_ = live_thick_solid_->object_index;
    selected_object_indices_.clear();
    active_object_index_ = live_thick_solid_->object_index;
    selected_face_object_index_ = 0;
    selected_solid_face_indices_.clear();
    has_selected_object_ = false;
    has_selected_solid_face_ = false;
    ClearPointSelection();
    return true;
}

bool CAlfaDoc::UpdateLiveThickSolid(double thickness) {
    if (!HasLiveThickSolid()) {
        return false;
    }

    live_thick_solid_->thickness = thickness;
    if (!live_thick_solid_->rebuild_on_update) {
        return true;
    }

    auto* solid = dynamic_cast<CSolid*>(objects_[live_thick_solid_->object_index].get());
    if (!solid || live_thick_solid_->base_shape.IsNull()) {
        return false;
    }
    if (live_thick_solid_->face_indices.empty() || std::fabs(thickness) <= 0.0001) {
        return true;
    }

    TopoDS_Shape result_shape = make_thick_solid_shape(live_thick_solid_->base_shape,
                                                       live_thick_solid_->face_indices,
                                                       thickness);
    if (result_shape.IsNull()
        || !rebuild_solid_from_shape(objects_, live_thick_solid_->object_index, solid, result_shape)) {
        return false;
    }

    return true;
}

bool CAlfaDoc::FinishLiveThickSolid() {
    if (!HasLiveThickSolid()) {
        return false;
    }

    if (live_thick_solid_->base_shape.IsNull()
        || live_thick_solid_->face_indices.empty()
        || std::fabs(live_thick_solid_->thickness) <= 0.0001) {
        return false;
    }

    if (!live_thick_solid_->rebuild_on_update) {
        auto* solid = dynamic_cast<CSolid*>(objects_[live_thick_solid_->object_index].get());
        if (!solid) {
            return false;
        }

        TopoDS_Shape result_shape = make_thick_solid_shape(live_thick_solid_->base_shape,
                                                           live_thick_solid_->face_indices,
                                                           live_thick_solid_->thickness);
        if (result_shape.IsNull()
            || !rebuild_solid_from_shape(objects_, live_thick_solid_->object_index, solid, result_shape)) {
            return false;
        }
    }

    selected_object_index_ = live_thick_solid_->object_index;
    selected_object_indices_.clear();
    active_object_index_ = live_thick_solid_->object_index;
    selected_face_object_index_ = 0;
    has_selected_object_ = false;
    has_selected_solid_face_ = false;
    ClearPointSelection();
    live_thick_solid_.reset();
    return true;
}

void CAlfaDoc::CancelLiveThickSolid() {
    if (HasLiveThickSolid()) {
        if (auto* solid = dynamic_cast<CSolid*>(objects_[live_thick_solid_->object_index].get())) {
            TopoDS_Shape base_shape = live_thick_solid_->base_shape;
            rebuild_solid_from_shape(objects_, live_thick_solid_->object_index, solid, base_shape);
            selected_object_index_ = live_thick_solid_->object_index;
            selected_object_indices_ = {live_thick_solid_->object_index};
            active_object_index_ = live_thick_solid_->object_index;
            has_selected_object_ = true;
        }
    }
    live_thick_solid_.reset();
    selected_solid_face_indices_.clear();
    has_selected_solid_face_ = false;
}

size_t CAlfaDoc::GetLiveThickSolidFaceCount() const {
    return live_thick_solid_ ? live_thick_solid_->face_indices.size() : 0;
}

bool CAlfaDoc::SelectPolylineAt(CurvePoint point, float tolerance) {
    for (size_t i = objects_.size(); i > 0; --i) {
        const size_t index = i - 1;
        CPolyline* polyline = dynamic_cast<CPolyline*>(objects_[index].get());
        if (polyline && polyline->IsVisible() && polyline->HitTest(point, tolerance)) {
            selected_object_index_ = index;
            active_object_index_ = index;
            selected_object_indices_ = {index};
            has_selected_object_ = true;
            ClearPointSelection();
            return true;
        }
    }

    ClearSelection();
    return false;
}

bool CAlfaDoc::SelectPolylineAtScreen(DomPoint point,
                                      const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                      float tolerance,
                                      SelectionAction action) {
    for (size_t i = objects_.size(); i > 0; --i) {
        const size_t index = i - 1;
        CAlfaObject* object = objects_[index].get();
        if (!object || !object->IsVisible()) {
            continue;
        }

        bool hit = false;
        if (const auto* polyline = dynamic_cast<const CPolyline*>(object)) {
            hit = hit_test_polyline_screen(*polyline, point, world_to_screen, tolerance);
        } else if (const auto* spline = dynamic_cast<const CBSpline*>(object)) {
            hit = hit_test_bspline_screen(*spline, point, world_to_screen, tolerance);
        }

        if (!hit) {
            continue;
        }

        if (action == SelectionAction::Remove) {
            auto existing = std::find(selected_object_indices_.begin(), selected_object_indices_.end(), index);
            if (existing == selected_object_indices_.end()) {
                return false;
            }
            selected_object_indices_.erase(existing);
            ClearPointSelection();
            if (selected_object_indices_.empty()) {
                ClearSelection();
                return true;
            }
            selected_object_index_ = selected_object_indices_.back();
            active_object_index_ = selected_object_index_;
            has_selected_object_ = true;
            return true;
        }

        if (action == SelectionAction::Add && has_selected_object_) {
            if (!IsObjectSelected(index)) {
                selected_object_indices_.push_back(index);
            }
        } else {
            selected_object_indices_ = {index};
        }

        for (ObjectPtr& object : objects_) {
            if (auto* solid = dynamic_cast<CSolid*>(object.get())) {
                solid->ClearSelectedEdge();
                solid->ClearSelectedFace();
            }
        }
        selected_object_index_ = index;
        active_object_index_ = index;
        has_selected_object_ = true;
        ClearPointSelection();
        return true;
    }

    if (action == SelectionAction::Replace) {
        ClearSelection();
    }
    return false;
}

bool CAlfaDoc::SelectPointAt(CurvePoint point, float tolerance) {
    if (!HasSelection()) {
        return false;
    }

    CPolyline* polyline = GetSelectedPolyline();
    if (!polyline) {
        ClearPointSelection();
        return false;
    }

    size_t point_index = 0;
    if (polyline->HitTestPoint(point, tolerance, point_index)) {
        selected_point_index_ = point_index;
        has_selected_point_ = true;
        return true;
    }

    ClearPointSelection();
    return false;
}

bool CAlfaDoc::SelectPolylinePointAtScreen(DomPoint point,
                                           const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                           float tolerance) {
    if (!HasSelection()) {
        return false;
    }

    CPolyline* polyline = GetSelectedPolyline();
    if (!polyline) {
        ClearPointSelection();
        return false;
    }

    float best_distance = tolerance;
    bool found = false;
    size_t found_index = 0;
    const std::vector<CPoint3d>& points = polyline->GetPoints();
    for (size_t i = 0; i < points.size(); ++i) {
        DomPoint screen{};
        if (!world_to_screen(to_vec3(points[i]), screen)) {
            continue;
        }
        const float dx = static_cast<float>(point.x - screen.x);
        const float dy = static_cast<float>(point.y - screen.y);
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance <= best_distance) {
            best_distance = distance;
            found_index = i;
            found = true;
        }
    }

    if (!found) {
        ClearPointSelection();
        return false;
    }

    selected_point_index_ = found_index;
    has_selected_point_ = true;
    return true;
}

bool CAlfaDoc::SelectCurvePointAtScreen(DomPoint point,
                                        const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                        float tolerance,
                                        SelectionAction action) {
    float best_distance = tolerance;
    bool found = false;
    size_t found_object_index = 0;
    size_t found_point_index = 0;

    const auto scan_points = [&](size_t object_index, const std::vector<CPoint3d>& points) {
        for (size_t point_index = 0; point_index < points.size(); ++point_index) {
            DomPoint screen{};
            if (!world_to_screen(to_vec3(points[point_index]), screen)) {
                continue;
            }
            const float dx = static_cast<float>(point.x - screen.x);
            const float dy = static_cast<float>(point.y - screen.y);
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance <= best_distance) {
                best_distance = distance;
                found_object_index = object_index;
                found_point_index = point_index;
                found = true;
            }
        }
    };

    for (size_t i = objects_.size(); i > 0; --i) {
        const size_t object_index = i - 1;
        if (!objects_[object_index] || !objects_[object_index]->IsVisible()) {
            continue;
        }
        if (const auto* polyline = dynamic_cast<const CPolyline*>(objects_[object_index].get())) {
            scan_points(object_index, polyline->GetPoints());
        } else if (const auto* spline = dynamic_cast<const CBSpline*>(objects_[object_index].get())) {
            scan_points(object_index, spline->GetPoints());
        }
    }

    if (!found) {
        if (action == SelectionAction::Replace) {
            ClearPointSelection();
        }
        return false;
    }

    selected_object_index_ = found_object_index;
    active_object_index_ = found_object_index;
    selected_object_indices_ = {found_object_index};
    has_selected_object_ = true;
    selected_point_index_ = found_point_index;
    selected_curve_points_ = {{found_object_index, found_point_index}};
    has_selected_point_ = true;
    return true;
}

bool CAlfaDoc::PickSelectedCurvePointAtScreen(DomPoint point,
                                              const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                              float tolerance,
                                              CPoint3d& selected_point) const {
    float best_distance = tolerance;
    bool found = false;

    for (const auto& selected : selected_curve_points_) {
        const size_t object_index = selected.first;
        const size_t point_index = selected.second;
        if (object_index >= objects_.size() || !objects_[object_index] || !objects_[object_index]->IsVisible()) {
            continue;
        }

        const std::vector<CPoint3d>* points = nullptr;
        if (const auto* polyline = dynamic_cast<const CPolyline*>(objects_[object_index].get())) {
            points = &polyline->GetPoints();
        } else if (const auto* spline = dynamic_cast<const CBSpline*>(objects_[object_index].get())) {
            points = &spline->GetPoints();
        }

        if (!points || point_index >= points->size()) {
            continue;
        }

        DomPoint screen{};
        if (!world_to_screen(to_vec3((*points)[point_index]), screen)) {
            continue;
        }

        const float dx = static_cast<float>(point.x - screen.x);
        const float dy = static_cast<float>(point.y - screen.y);
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance <= best_distance) {
            best_distance = distance;
            selected_point = (*points)[point_index];
            found = true;
        }
    }

    return found;
}

bool CAlfaDoc::SelectCurvePointsInScreenRect(DomRect rect,
                                             const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                             SelectionAction action) {
    const int left = std::min(rect.left, rect.right);
    const int right = std::max(rect.left, rect.right);
    const int top = std::min(rect.top, rect.bottom);
    const int bottom = std::max(rect.top, rect.bottom);

    std::vector<std::pair<size_t, size_t>> found_points;
    const auto scan_points = [&](size_t object_index, const std::vector<CPoint3d>& points) {
        for (size_t point_index = 0; point_index < points.size(); ++point_index) {
            DomPoint screen{};
            if (!world_to_screen(to_vec3(points[point_index]), screen)) {
                continue;
            }
            if (screen.x >= left && screen.x <= right && screen.y >= top && screen.y <= bottom) {
                found_points.emplace_back(object_index, point_index);
            }
        }
    };

    for (size_t object_index = 0; object_index < objects_.size(); ++object_index) {
        if (!objects_[object_index] || !objects_[object_index]->IsVisible()) {
            continue;
        }
        if (const auto* polyline = dynamic_cast<const CPolyline*>(objects_[object_index].get())) {
            scan_points(object_index, polyline->GetPoints());
        } else if (const auto* spline = dynamic_cast<const CBSpline*>(objects_[object_index].get())) {
            scan_points(object_index, spline->GetPoints());
        }
    }

    if (found_points.empty()) {
        if (action == SelectionAction::Replace) {
            ClearPointSelection();
        }
        return false;
    }

    if (action == SelectionAction::Add) {
        for (const auto& point_ref : found_points) {
            if (std::find(selected_curve_points_.begin(), selected_curve_points_.end(), point_ref) == selected_curve_points_.end()) {
                selected_curve_points_.push_back(point_ref);
            }
        }
    } else {
        selected_curve_points_ = std::move(found_points);
    }
    selected_object_index_ = selected_curve_points_.front().first;
    active_object_index_ = selected_object_index_;
    selected_object_indices_.clear();
    for (const auto& selected : selected_curve_points_) {
        if (!IsObjectSelected(selected.first)) {
            selected_object_indices_.push_back(selected.first);
        }
    }
    has_selected_object_ = !selected_object_indices_.empty();
    selected_point_index_ = selected_curve_points_.front().second;
    has_selected_point_ = true;
    return true;
}

bool CAlfaDoc::FindPolylinePointAtScreen(DomPoint point,
                                         const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                         float tolerance,
                                         size_t& object_index,
                                         size_t& point_index) const {
    float best_distance = tolerance;
    bool found = false;

    const auto scan_polyline = [&](size_t current_object_index, const CPolyline& polyline) {
        const std::vector<CPoint3d>& points = polyline.GetPoints();
        for (size_t current_point_index = 0; current_point_index < points.size(); ++current_point_index) {
            DomPoint screen{};
            if (!world_to_screen(to_vec3(points[current_point_index]), screen)) {
                continue;
            }

            const float dx = static_cast<float>(point.x - screen.x);
            const float dy = static_cast<float>(point.y - screen.y);
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance <= best_distance) {
                best_distance = distance;
                object_index = current_object_index;
                point_index = current_point_index;
                found = true;
            }
        }
    };

    if (has_selected_object_ && selected_object_index_ < objects_.size()) {
        if (const auto* polyline = dynamic_cast<const CPolyline*>(objects_[selected_object_index_].get())) {
            scan_polyline(selected_object_index_, *polyline);
        }
    }

    if (!found) {
        for (size_t i = objects_.size(); i > 0; --i) {
            const size_t current_object_index = i - 1;
            if (!objects_[current_object_index] || !objects_[current_object_index]->IsVisible()) {
                continue;
            }
            if (const auto* polyline = dynamic_cast<const CPolyline*>(objects_[current_object_index].get())) {
                scan_polyline(current_object_index, *polyline);
            }
        }
    }

    return found;
}

void CAlfaDoc::ClearSelection() {
    for (ObjectPtr& object : objects_) {
        if (auto* solid = dynamic_cast<CSolid*>(object.get())) {
            solid->ClearSelectedEdge();
            solid->ClearSelectedFace();
        }
    }
    selected_object_index_ = 0;
    selected_face_object_index_ = 0;
    selected_object_indices_.clear();
    selected_solid_face_indices_.clear();
    draft_face_.reset();
    has_selected_solid_face_ = false;
    has_selected_object_ = false;
    ClearPointSelection();
}

void CAlfaDoc::ClearPointSelection() {
    selected_point_index_ = 0;
    has_selected_point_ = false;
    selected_curve_points_.clear();
}

bool CAlfaDoc::HasSelection() const {
    return has_selected_object_ && !selected_object_indices_.empty() && selected_object_index_ < objects_.size();
}

bool CAlfaDoc::HasSelectedPoint() const {
    const CPolyline* polyline = GetSelectedPolyline();
    if (has_selected_point_ && polyline && selected_point_index_ < polyline->GetPointCount()) {
        return true;
    }
    const CBSpline* spline = GetSelectedBSpline();
    return has_selected_point_ && spline && selected_point_index_ < spline->GetPointCount();
}

bool CAlfaDoc::HasSelectedSolidEdge() const {
    const CSolid* solid = GetSelectedSolid();
    return solid && solid->HasSelectedEdge();
}

size_t CAlfaDoc::GetSelectedObjectIndex() const {
    return selected_object_index_;
}

size_t CAlfaDoc::GetSelectedPointIndex() const {
    return selected_point_index_;
}

bool CAlfaDoc::IsObjectSelected(size_t index) const {
    return std::find(selected_object_indices_.begin(), selected_object_indices_.end(), index) != selected_object_indices_.end();
}

size_t CAlfaDoc::GetSelectedObjectCount() const {
    return selected_object_indices_.size();
}

const std::vector<size_t>& CAlfaDoc::GetSelectedObjectIndices() const {
    return selected_object_indices_;
}

CAlfaObject* CAlfaDoc::GetSelectedObject() {
    if (!HasSelection()) {
        return nullptr;
    }

    return objects_[selected_object_index_].get();
}

const CAlfaObject* CAlfaDoc::GetSelectedObject() const {
    if (!HasSelection()) {
        return nullptr;
    }

    return objects_[selected_object_index_].get();
}

CMesh3D* CAlfaDoc::GetSelectedMesh() {
    return dynamic_cast<CMesh3D*>(GetSelectedObject());
}

const CMesh3D* CAlfaDoc::GetSelectedMesh() const {
    return dynamic_cast<const CMesh3D*>(GetSelectedObject());
}

CPolyline* CAlfaDoc::GetSelectedPolyline() {
    return dynamic_cast<CPolyline*>(GetSelectedObject());
}

const CPolyline* CAlfaDoc::GetSelectedPolyline() const {
    return dynamic_cast<const CPolyline*>(GetSelectedObject());
}

CBSpline* CAlfaDoc::GetSelectedBSpline() {
    return dynamic_cast<CBSpline*>(GetSelectedObject());
}

const CBSpline* CAlfaDoc::GetSelectedBSpline() const {
    return dynamic_cast<const CBSpline*>(GetSelectedObject());
}

CSolid* CAlfaDoc::GetSelectedSolid() {
    return dynamic_cast<CSolid*>(GetSelectedObject());
}

const CSolid* CAlfaDoc::GetSelectedSolid() const {
    return dynamic_cast<const CSolid*>(GetSelectedObject());
}

void CAlfaDoc::AddObject(std::unique_ptr<CAlfaObject> object) {
    if (!object) {
        return;
    }

    AssignDefaultMaterial(*object);
    objects_.push_back(std::move(object));
    selected_object_index_ = objects_.size() - 1;
    selected_object_indices_ = {selected_object_index_};
    has_selected_object_ = true;
    ClearPointSelection();
}

void CAlfaDoc::AddMesh(std::unique_ptr<CMesh3D> mesh) {
    AddObject(std::move(mesh));
}

bool CAlfaDoc::DuplicateSelectedObject() {
    const CAlfaObject* selected = GetSelectedObject();
    if (!selected) {
        return false;
    }

    std::unique_ptr<CAlfaObject> copy = selected->Clone();
    if (!copy) {
        return false;
    }

    const size_t copy_index = objects_.size();
    AssignDefaultMaterial(*copy);
    objects_.push_back(std::move(copy));
    selected_object_index_ = copy_index;
    active_object_index_ = copy_index;
    selected_object_indices_ = {copy_index};
    has_selected_object_ = true;
    ClearPointSelection();
    selected_solid_face_indices_.clear();
    has_selected_solid_face_ = false;
    return true;
}

bool CAlfaDoc::CreateLoftSurfaceFromSelectedBSplines() {
    std::vector<const CBSpline*> splines;
    splines.reserve(selected_object_indices_.size());
    for (size_t index : selected_object_indices_) {
        if (index >= objects_.size() || !objects_[index] || !objects_[index]->IsVisible()) {
            continue;
        }
        if (const auto* spline = dynamic_cast<const CBSpline*>(objects_[index].get())) {
            if (spline->GetPointCount() >= 2) {
                splines.push_back(spline);
            }
        }
    }

    if (splines.size() < 2) {
        return false;
    }

    TopoDS_Shape loft_shape = make_loft_surface_from_splines(splines);
    if (loft_shape.IsNull()) {
        return false;
    }

    auto surface = std::make_unique<CSurfaceSet>(loft_shape);
    surface->SetName("Loft Surface");
    surface->SetColor({0.70f, 0.72f, 0.68f});
    surface->InitSurfaces();
    surface->InitEdges();
    surface->BuldMesh(0.1f);
    AddObject(std::move(surface));
    return true;
}

bool CAlfaDoc::ReverseSelectedSurfaceNormals() {
    CSolid* solid = GetSelectedSolid();
    if (!solid) {
        return false;
    }
    const bool reversed = solid->ReverseNormals();
    if (reversed) {
        ClearPointSelection();
        selected_solid_face_indices_.clear();
        has_selected_solid_face_ = false;
    }
    return reversed;
}

bool CAlfaDoc::DeleteSelectedObject() {
    if (!HasSelection()) {
        return false;
    }

    std::vector<size_t> indices = selected_object_indices_;
    std::sort(indices.begin(), indices.end(), std::greater<size_t>());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    for (size_t index : indices) {
        if (index < objects_.size()) {
            objects_.erase(objects_.begin() + static_cast<ObjectList::difference_type>(index));
        }
    }

    ClearSelection();
    active_object_index_ = 0;
    EnsureActivePolyline();
    return true;
}

bool CAlfaDoc::DeleteSelectedPoint() {
    if (!HasSelectedPoint()) {
        return false;
    }

    CPolyline* polyline = GetSelectedPolyline();
    if (polyline && polyline->RemovePoint(selected_point_index_)) {
        ClearPointSelection();
        return true;
    }

    CBSpline* spline = GetSelectedBSpline();
    if (spline && spline->RemovePoint(selected_point_index_)) {
        ClearPointSelection();
        return true;
    }

    if (!polyline && !spline) {
        return false;
    }

    return false;
}

bool CAlfaDoc::MoveSelectedPoint(CurvePoint point) {
    if (!HasSelectedPoint()) {
        return false;
    }

    CPolyline* polyline = GetSelectedPolyline();
    return polyline && polyline->SetPoint(selected_point_index_, point);
}

bool CAlfaDoc::MoveSelectedPoint(CPoint3d point) {
    if (!HasSelectedPoint()) {
        return false;
    }

    CPolyline* polyline = GetSelectedPolyline();
    if (polyline) {
        return polyline->SetPoint(selected_point_index_, point);
    }

    CBSpline* spline = GetSelectedBSpline();
    return spline && spline->SetPoint(selected_point_index_, point);
}

bool CAlfaDoc::MoveSelectedCurvePoints(Vec3 delta) {
    if (selected_curve_points_.empty()) {
        return false;
    }

    bool moved = false;
    for (const auto& selected : selected_curve_points_) {
        if (selected.first >= objects_.size()) {
            continue;
        }

        if (auto* polyline = dynamic_cast<CPolyline*>(objects_[selected.first].get())) {
            if (selected.second < polyline->GetPoints().size()) {
                CPoint3d point = polyline->GetPoints()[selected.second];
                point.x += delta.x;
                point.y += delta.y;
                point.z += delta.z;
                moved = polyline->SetPoint(selected.second, point) || moved;
            }
        } else if (auto* spline = dynamic_cast<CBSpline*>(objects_[selected.first].get())) {
            if (selected.second < spline->GetPoints().size()) {
                CPoint3d point = spline->GetPoints()[selected.second];
                point.x += delta.x;
                point.y += delta.y;
                point.z += delta.z;
                moved = spline->SetPoint(selected.second, point) || moved;
            }
        }
    }
    return moved;
}

bool CAlfaDoc::ApplyFilletToSelectedPolylinePoint(double radius) {
    if (!HasSelectedPoint()) {
        return false;
    }

    CPolyline* polyline = GetSelectedPolyline();
    if (!polyline || selected_point_index_ >= polyline->GetPoints().size()) {
        return false;
    }

    const bool applied = polyline->ApplyFillet(selected_point_index_, radius);
    if (applied) {
        ClearPointSelection();
    }
    return applied;
}

bool CAlfaDoc::ApplyFilletToPolylinePointAtScreen(DomPoint point,
                                                  const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                                  float tolerance,
                                                  double radius) {
    size_t found_object_index = 0;
    size_t found_point_index = 0;
    if (!FindPolylinePointAtScreen(point, world_to_screen, tolerance, found_object_index, found_point_index)
        || found_object_index >= objects_.size()) {
        return false;
    }

    auto* polyline = dynamic_cast<CPolyline*>(objects_[found_object_index].get());
    if (!polyline || !polyline->ApplyFillet(found_point_index, radius)) {
        return false;
    }

    selected_object_index_ = found_object_index;
    active_object_index_ = found_object_index;
    selected_object_indices_ = {found_object_index};
    has_selected_object_ = true;
    ClearPointSelection();
    return true;
}

bool CAlfaDoc::GetSelectedPointPosition(CPoint3d& point) const {
    if (!HasSelectedPoint()) {
        return false;
    }

    const CPolyline* polyline = GetSelectedPolyline();
    if (polyline && selected_point_index_ < polyline->GetPoints().size()) {
        point = polyline->GetPoints()[selected_point_index_];
        return true;
    }

    const CBSpline* spline = GetSelectedBSpline();
    if (spline && selected_point_index_ < spline->GetPoints().size()) {
        point = spline->GetPoints()[selected_point_index_];
        return true;
    }
    return false;
}

std::vector<CPoint3d> CAlfaDoc::GetSelectedCurvePointPositions() const {
    std::vector<CPoint3d> points;
    points.reserve(selected_curve_points_.size());
    for (const auto& selected : selected_curve_points_) {
        if (selected.first >= objects_.size()) {
            continue;
        }
        if (const auto* polyline = dynamic_cast<const CPolyline*>(objects_[selected.first].get())) {
            if (selected.second < polyline->GetPoints().size()) {
                points.push_back(polyline->GetPoints()[selected.second]);
            }
        } else if (const auto* spline = dynamic_cast<const CBSpline*>(objects_[selected.first].get())) {
            if (selected.second < spline->GetPoints().size()) {
                points.push_back(spline->GetPoints()[selected.second]);
            }
        }
    }
    return points;
}

bool CAlfaDoc::MoveSelectedObjects(Vec3 delta) {
    if (!HasSelection()) {
        return false;
    }

    bool moved = false;
    for (size_t index : selected_object_indices_) {
        if (index < objects_.size()) {
            objects_[index]->Translate(delta);
            moved = true;
        }
    }

    return moved;
}

bool CAlfaDoc::RotateSelectedObjects(Vec3 center, Vec3 axis, float angle) {
    if (!HasSelection()) {
        return false;
    }

    bool rotated = false;
    for (size_t index : selected_object_indices_) {
        if (index < objects_.size()) {
            objects_[index]->Rotate(center, axis, angle);
            rotated = true;
        }
    }

    return rotated;
}

bool CAlfaDoc::ScaleSelectedObjects(Vec3 center, Vec3 axis, float factor) {
    if (!HasSelection() || factor <= 0.0001f) {
        return false;
    }

    bool scaled = false;
    for (size_t index : selected_object_indices_) {
        if (index < objects_.size()) {
            objects_[index]->Scale(center, axis, factor);
            scaled = true;
        }
    }

    return scaled;
}

bool CAlfaDoc::UniformScaleSelectedObjects(Vec3 center, float factor) {
    if (!HasSelection() || factor <= 0.0001f) {
        return false;
    }

    bool scaled = false;
    for (size_t index : selected_object_indices_) {
        if (index < objects_.size()) {
            objects_[index]->Scale(center, {}, factor);
            scaled = true;
        }
    }

    return scaled;
}

bool CAlfaDoc::PreviewMoveSelectedObjects(Vec3 delta) {
    if (!HasSelection()) {
        return false;
    }

    bool moved = false;
    for (size_t index : selected_object_indices_) {
        if (index >= objects_.size()) {
            continue;
        }
        if (auto* solid = dynamic_cast<CSolid*>(objects_[index].get())) {
            solid->PreviewTranslate(delta);
        } else {
            objects_[index]->Translate(delta);
        }
        moved = true;
    }
    return moved;
}

bool CAlfaDoc::PreviewRotateSelectedObjects(Vec3 center, Vec3 axis, float angle) {
    if (!HasSelection()) {
        return false;
    }

    bool rotated = false;
    for (size_t index : selected_object_indices_) {
        if (index >= objects_.size()) {
            continue;
        }
        if (auto* solid = dynamic_cast<CSolid*>(objects_[index].get())) {
            solid->PreviewRotate(center, axis, angle);
        } else {
            objects_[index]->Rotate(center, axis, angle);
        }
        rotated = true;
    }
    return rotated;
}

bool CAlfaDoc::PreviewScaleSelectedObjects(Vec3 center, Vec3 axis, float factor) {
    if (!HasSelection() || factor <= 0.0001f) {
        return false;
    }

    bool scaled = false;
    for (size_t index : selected_object_indices_) {
        if (index >= objects_.size()) {
            continue;
        }
        if (auto* solid = dynamic_cast<CSolid*>(objects_[index].get())) {
            solid->PreviewScale(center, axis, factor);
        } else {
            objects_[index]->Scale(center, axis, factor);
        }
        scaled = true;
    }
    return scaled;
}

bool CAlfaDoc::PreviewUniformScaleSelectedObjects(Vec3 center, float factor) {
    return PreviewScaleSelectedObjects(center, {}, factor);
}

bool CAlfaDoc::CommitMoveSelectedSolids(Vec3 delta) {
    bool moved = false;
    for (size_t index : selected_object_indices_) {
        if (index < objects_.size()) {
            if (auto* solid = dynamic_cast<CSolid*>(objects_[index].get())) {
                solid->Translate(delta);
                moved = true;
            }
        }
    }
    return moved;
}

bool CAlfaDoc::CommitRotateSelectedSolids(Vec3 center, Vec3 axis, float angle) {
    bool rotated = false;
    for (size_t index : selected_object_indices_) {
        if (index < objects_.size()) {
            if (auto* solid = dynamic_cast<CSolid*>(objects_[index].get())) {
                solid->Rotate(center, axis, angle);
                rotated = true;
            }
        }
    }
    return rotated;
}

bool CAlfaDoc::CommitScaleSelectedSolids(Vec3 center, Vec3 axis, float factor) {
    if (factor <= 0.0001f) {
        return false;
    }

    bool scaled = false;
    for (size_t index : selected_object_indices_) {
        if (index < objects_.size()) {
            if (auto* solid = dynamic_cast<CSolid*>(objects_[index].get())) {
                solid->Scale(center, axis, factor);
                scaled = true;
            }
        }
    }
    return scaled;
}

bool CAlfaDoc::CommitUniformScaleSelectedSolids(Vec3 center, float factor) {
    return CommitScaleSelectedSolids(center, {}, factor);
}

bool CAlfaDoc::ApplyBooleanToSolids(size_t body_index, size_t tool_index, BooleanOperation operation) {
    if (body_index >= objects_.size() || tool_index >= objects_.size() || body_index == tool_index) {
        return false;
    }

    auto* body = dynamic_cast<CSolid*>(objects_[body_index].get());
    auto* tool = dynamic_cast<CSolid*>(objects_[tool_index].get());
    if (!body || !tool || body->m_Shape.IsNull() || tool->m_Shape.IsNull()) {
        return false;
    }

    TopoDS_Shape result_shape;
    if (operation == BooleanOperation::Union) {
        BRepAlgoAPI_Fuse algo(body->m_Shape, tool->m_Shape);
        algo.Build();
        if (!algo.IsDone()) {
            return false;
        }
        result_shape = algo.Shape();
    } else if (operation == BooleanOperation::Cut) {
        BRepAlgoAPI_Cut algo(body->m_Shape, tool->m_Shape);
        algo.Build();
        if (!algo.IsDone()) {
            return false;
        }
        result_shape = algo.Shape();
    } else {
        BRepAlgoAPI_Common algo(body->m_Shape, tool->m_Shape);
        algo.Build();
        if (!algo.IsDone()) {
            return false;
        }
        result_shape = algo.Shape();
    }

    if (result_shape.IsNull()) {
        return false;
    }

    auto result = std::make_unique<CSolid>(result_shape);
    result->SetName(operation == BooleanOperation::Union ? "Boolean Union" : operation == BooleanOperation::Cut ? "Boolean Cut" : "Boolean Common");
    result->SetColor(body->GetColor());
    result->InitSurfaces();
    result->InitEdges();
    result->BuldMesh(0.1f);

    std::vector<size_t> erase_indices = {body_index, tool_index};
    std::sort(erase_indices.begin(), erase_indices.end(), std::greater<size_t>());
    erase_indices.erase(std::unique(erase_indices.begin(), erase_indices.end()), erase_indices.end());
    for (size_t index : erase_indices) {
        objects_.erase(objects_.begin() + static_cast<ObjectList::difference_type>(index));
    }

    objects_.push_back(std::move(result));
    selected_object_index_ = objects_.size() - 1;
    selected_object_indices_ = {selected_object_index_};
    active_object_index_ = selected_object_index_;
    has_selected_object_ = true;
    ClearPointSelection();
    return true;
}

bool CAlfaDoc::ApplyFilletToSelectedEdge(double radius) {
    if (radius <= 0.0 || !HasSelection()) {
        return false;
    }

    const size_t solid_index = selected_object_index_;
    if (solid_index >= objects_.size()) {
        return false;
    }

    auto* solid = dynamic_cast<CSolid*>(objects_[solid_index].get());
    if (!solid || solid->m_Shape.IsNull() || !solid->HasSelectedEdge()) {
        return false;
    }

    const std::vector<TopoDS_Edge> selected_edges = unique_edges(solid->GetSelectedTopoEdges());
    if (selected_edges.empty()) {
        return false;
    }

    TopoDS_Shape result_shape;
    try {
        BRepFilletAPI_MakeFillet fillet(solid->m_Shape);
        for (const TopoDS_Edge& edge : selected_edges) {
            fillet.Add(radius, edge);
        }
        fillet.Build();
        if (!fillet.IsDone()) {
            return false;
        }
        result_shape = fillet.Shape();
    } catch (const Standard_Failure&) {
        return false;
    }

    if (result_shape.IsNull()) {
        return false;
    }

    if (!rebuild_solid_from_shape(objects_, solid_index, solid, result_shape)) {
        return false;
    }
    selected_object_index_ = solid_index;
    selected_object_indices_ = {solid_index};
    active_object_index_ = solid_index;
    has_selected_object_ = true;
    ClearPointSelection();
    return true;
}

bool CAlfaDoc::ApplyFilletToAllSelectedSolidEdges(double radius) {
    if (radius <= 0.0 || !HasSelection()) {
        return false;
    }

    const size_t solid_index = selected_object_index_;
    if (solid_index >= objects_.size()) {
        return false;
    }

    auto* solid = dynamic_cast<CSolid*>(objects_[solid_index].get());
    if (!solid || solid->m_Shape.IsNull()) {
        return false;
    }

    const std::vector<TopoDS_Edge> edges = unique_edges(solid->GetAllTopoEdges());
    if (edges.empty()) {
        return false;
    }

    TopoDS_Shape result_shape;
    try {
        BRepFilletAPI_MakeFillet fillet(solid->m_Shape);
        for (const TopoDS_Edge& edge : edges) {
            fillet.Add(radius, edge);
        }
        fillet.Build();
        if (!fillet.IsDone()) {
            return false;
        }
        result_shape = fillet.Shape();
    } catch (const Standard_Failure&) {
        return false;
    }

    if (!rebuild_solid_from_shape(objects_, solid_index, solid, result_shape)) {
        return false;
    }

    selected_object_index_ = solid_index;
    selected_object_indices_ = {solid_index};
    active_object_index_ = solid_index;
    has_selected_object_ = true;
    ClearPointSelection();
    return true;
}

bool CAlfaDoc::BeginLiveFilletSelectedEdges(bool all_edges) {
    if (!HasSelection()) {
        return false;
    }

    const size_t solid_index = selected_object_index_;
    if (solid_index >= objects_.size()) {
        return false;
    }

    auto* solid = dynamic_cast<CSolid*>(objects_[solid_index].get());
    if (!solid || solid->m_Shape.IsNull()) {
        return false;
    }

    std::vector<TopoDS_Edge> edges = all_edges ? solid->GetAllTopoEdges() : solid->GetSelectedTopoEdges();
    edges = unique_edges(edges);
    if (edges.empty()) {
        return false;
    }

    live_fillet_ = std::make_unique<LiveFilletData>();
    live_fillet_->base_shape = solid->m_Shape;
    live_fillet_->edges = std::move(edges);
    live_fillet_->object_index = solid_index;
    return true;
}

bool CAlfaDoc::HasLiveFillet() const {
    return live_fillet_ && live_fillet_->object_index < objects_.size();
}

bool CAlfaDoc::UpdateLiveFillet(double radius) {
    if (!live_fillet_ || live_fillet_->object_index >= objects_.size()) {
        return false;
    }

    auto* solid = dynamic_cast<CSolid*>(objects_[live_fillet_->object_index].get());
    if (!solid || live_fillet_->base_shape.IsNull()) {
        return false;
    }

    TopoDS_Shape result_shape;
    if (radius <= 0.0001) {
        result_shape = live_fillet_->base_shape;
    } else {
        try {
            BRepFilletAPI_MakeFillet fillet(live_fillet_->base_shape);
            for (const TopoDS_Edge& edge : live_fillet_->edges) {
                fillet.Add(radius, edge);
            }
            fillet.Build();
            if (!fillet.IsDone()) {
                return false;
            }
            result_shape = fillet.Shape();
        } catch (const Standard_Failure&) {
            return false;
        }
    }

    if (result_shape.IsNull() || !rebuild_solid_from_shape(objects_, live_fillet_->object_index, solid, result_shape)) {
        return false;
    }

    selected_object_index_ = live_fillet_->object_index;
    selected_object_indices_ = {live_fillet_->object_index};
    active_object_index_ = live_fillet_->object_index;
    has_selected_object_ = true;
    ClearPointSelection();
    return true;
}

void CAlfaDoc::FinishLiveFillet() {
    live_fillet_.reset();
}

void CAlfaDoc::CancelLiveFillet() {
    if (live_fillet_ && live_fillet_->object_index < objects_.size()) {
        if (auto* solid = dynamic_cast<CSolid*>(objects_[live_fillet_->object_index].get())) {
            TopoDS_Shape base_shape = live_fillet_->base_shape;
            rebuild_solid_from_shape(objects_, live_fillet_->object_index, solid, base_shape);
            selected_object_index_ = live_fillet_->object_index;
            selected_object_indices_ = {live_fillet_->object_index};
            active_object_index_ = live_fillet_->object_index;
            has_selected_object_ = true;
        }
    }
    FinishLiveFillet();
}

bool CAlfaDoc::BeginLiveChamferSelectedEdges() {
    if (!HasSelection()) {
        return false;
    }

    const size_t solid_index = selected_object_index_;
    if (solid_index >= objects_.size()) {
        return false;
    }

    auto* solid = dynamic_cast<CSolid*>(objects_[solid_index].get());
    if (!solid || solid->m_Shape.IsNull() || !solid->HasSelectedEdge()) {
        return false;
    }

    std::vector<TopoDS_Edge> edges = unique_edges(solid->GetSelectedTopoEdges());
    if (edges.empty()) {
        return false;
    }

    live_chamfer_ = std::make_unique<LiveChamferData>();
    live_chamfer_->base_shape = solid->m_Shape;
    live_chamfer_->edges = std::move(edges);
    live_chamfer_->object_index = solid_index;
    return true;
}

bool CAlfaDoc::HasLiveChamfer() const {
    return live_chamfer_ && live_chamfer_->object_index < objects_.size();
}

bool CAlfaDoc::UpdateLiveChamfer(double distance) {
    if (!live_chamfer_ || live_chamfer_->object_index >= objects_.size()) {
        return false;
    }

    auto* solid = dynamic_cast<CSolid*>(objects_[live_chamfer_->object_index].get());
    if (!solid || live_chamfer_->base_shape.IsNull()) {
        return false;
    }

    TopoDS_Shape result_shape;
    if (distance <= 0.0001) {
        result_shape = live_chamfer_->base_shape;
    } else {
        try {
            BRepFilletAPI_MakeChamfer chamfer(live_chamfer_->base_shape);
            for (const TopoDS_Edge& edge : live_chamfer_->edges) {
                chamfer.Add(distance, edge);
            }
            chamfer.Build();
            if (!chamfer.IsDone()) {
                return false;
            }
            result_shape = chamfer.Shape();
        } catch (const Standard_Failure&) {
            return false;
        }
    }

    if (result_shape.IsNull() || !rebuild_solid_from_shape(objects_, live_chamfer_->object_index, solid, result_shape)) {
        return false;
    }

    selected_object_index_ = live_chamfer_->object_index;
    selected_object_indices_ = {live_chamfer_->object_index};
    active_object_index_ = live_chamfer_->object_index;
    has_selected_object_ = true;
    ClearPointSelection();
    return true;
}

void CAlfaDoc::FinishLiveChamfer() {
    live_chamfer_.reset();
}

void CAlfaDoc::CancelLiveChamfer() {
    if (live_chamfer_ && live_chamfer_->object_index < objects_.size()) {
        if (auto* solid = dynamic_cast<CSolid*>(objects_[live_chamfer_->object_index].get())) {
            TopoDS_Shape base_shape = live_chamfer_->base_shape;
            rebuild_solid_from_shape(objects_, live_chamfer_->object_index, solid, base_shape);
            selected_object_index_ = live_chamfer_->object_index;
            selected_object_indices_ = {live_chamfer_->object_index};
            active_object_index_ = live_chamfer_->object_index;
            has_selected_object_ = true;
        }
    }
    FinishLiveChamfer();
}

bool CAlfaDoc::GetSelectionBounds(Vec3& min_point, Vec3& max_point) const {
    if (!HasSelection()) {
        return false;
    }

    bool has_bounds = false;
    for (size_t index : selected_object_indices_) {
        if (index >= objects_.size() || !objects_[index]->IsVisible()) {
            continue;
        }

        Vec3 object_min{};
        Vec3 object_max{};
        if (!objects_[index]->GetBounds(object_min, object_max)) {
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

    return has_bounds;
}

bool CAlfaDoc::GetSelectionCenter(Vec3& center) const {
    Vec3 min_point{};
    Vec3 max_point{};
    if (!GetSelectionBounds(min_point, max_point)) {
        return false;
    }

    center = (min_point + max_point) * 0.5f;
    return true;
}

CPolyline& CAlfaDoc::GetActivePolyline() {
    EnsureActivePolyline();
    return *static_cast<CPolyline*>(objects_[active_object_index_].get());
}

const CPolyline& CAlfaDoc::GetActivePolyline() const {
    return *static_cast<const CPolyline*>(objects_[active_object_index_].get());
}

CBSpline& CAlfaDoc::GetActiveBSpline() {
    EnsureActiveBSpline();
    return *static_cast<CBSpline*>(objects_[active_object_index_].get());
}

const CBSpline& CAlfaDoc::GetActiveBSpline() const {
    return *static_cast<const CBSpline*>(objects_[active_object_index_].get());
}

CAlfaDoc::ObjectList& CAlfaDoc::GetObjects() {
    return objects_;
}

const CAlfaDoc::ObjectList& CAlfaDoc::GetObjects() const {
    return objects_;
}

std::vector<Material>& CAlfaDoc::GetMaterials() {
    return materials_;
}

const std::vector<Material>& CAlfaDoc::GetMaterials() const {
    return materials_;
}

void CAlfaDoc::ResetDefaultMaterials() {
    materials_ = Material::InitialDocumentMaterials();
}

Material* CAlfaDoc::FindMaterial(unsigned long id) {
    for (Material& material : materials_) {
        if (material.id == id) {
            return &material;
        }
    }
    return nullptr;
}

const Material* CAlfaDoc::FindMaterial(unsigned long id) const {
    for (const Material& material : materials_) {
        if (material.id == id) {
            return &material;
        }
    }
    return nullptr;
}

Material& CAlfaDoc::UpsertMaterial(Material material) {
    if (material.id == 0) {
        unsigned long next_id = 1;
        for (const Material& existing : materials_) {
            next_id = std::max(next_id, existing.id + 1);
        }
        material.id = next_id;
    }

    if (Material* existing = FindMaterial(material.id)) {
        *existing = material;
        return *existing;
    }

    materials_.push_back(std::move(material));
    return materials_.back();
}

bool CAlfaDoc::DeleteMaterial(unsigned long id) {
    if (id == 0 || materials_.size() <= 1) {
        return false;
    }

    const auto removed = std::remove_if(materials_.begin(), materials_.end(), [id](const Material& material) {
        return material.id == id;
    });
    if (removed == materials_.end()) {
        return false;
    }

    materials_.erase(removed, materials_.end());
    for (ObjectPtr& object : objects_) {
        if (object && object->GetMaterialId() == id) {
            object->SetMaterialId(0);
            AssignDefaultMaterial(*object);
        }
    }
    return true;
}

size_t CAlfaDoc::GetTotalPointCount() const {
    size_t count = 0;
    for (const ObjectPtr& object : objects_) {
        const auto* polyline = dynamic_cast<const CPolyline*>(object.get());
        if (polyline) {
            count += polyline->GetPointCount();
        }
    }
    return count;
}

void CAlfaDoc::EnsureActivePolyline() {
    if (objects_.empty()) {
        auto polyline = std::make_unique<CPolyline>("Curve 1");
        polyline->SetColor({0.98f, 0.77f, 0.30f});
        AssignDefaultMaterial(*polyline);
        objects_.push_back(std::move(polyline));
        active_object_index_ = 0;
    }

    if (active_object_index_ >= objects_.size() || dynamic_cast<CPolyline*>(objects_[active_object_index_].get()) == nullptr) {
        for (size_t i = 0; i < objects_.size(); ++i) {
            if (dynamic_cast<CPolyline*>(objects_[i].get()) != nullptr) {
                active_object_index_ = i;
                break;
            }
        }
    }

    if (dynamic_cast<CPolyline*>(objects_[active_object_index_].get()) == nullptr) {
        auto polyline = std::make_unique<CPolyline>("Curve " + std::to_string(objects_.size() + 1));
        polyline->SetColor({0.98f, 0.77f, 0.30f});
        AssignDefaultMaterial(*polyline);
        objects_.push_back(std::move(polyline));
        active_object_index_ = objects_.size() - 1;
    }

    if (selected_object_index_ >= objects_.size()) {
        ClearSelection();
    } else if (has_selected_object_ && selected_object_indices_.empty()) {
        selected_object_indices_ = {selected_object_index_};
    }
}

void CAlfaDoc::EnsureActiveBSpline() {
    if (objects_.empty()) {
        auto spline = std::make_unique<CBSpline>("B-Spline 1");
        spline->SetColor({1.0f, 0.08f, 0.10f});
        AssignDefaultMaterial(*spline);
        objects_.push_back(std::move(spline));
        active_object_index_ = 0;
        return;
    }

    if (active_object_index_ >= objects_.size() || dynamic_cast<CBSpline*>(objects_[active_object_index_].get()) == nullptr) {
        for (size_t i = objects_.size(); i > 0; --i) {
            if (dynamic_cast<CBSpline*>(objects_[i - 1].get()) != nullptr) {
                active_object_index_ = i - 1;
                break;
            }
        }
    }

    if (dynamic_cast<CBSpline*>(objects_[active_object_index_].get()) == nullptr) {
        auto spline = std::make_unique<CBSpline>("B-Spline " + std::to_string(objects_.size() + 1));
        spline->SetColor({1.0f, 0.08f, 0.10f});
        AssignDefaultMaterial(*spline);
        objects_.push_back(std::move(spline));
        active_object_index_ = objects_.size() - 1;
    }
}

void CAlfaDoc::AssignDefaultMaterial(CAlfaObject& object) {
    if (object.GetMaterialId() != 0) {
        if (const Material* material = FindMaterial(object.GetMaterialId())) {
            object.SetMaterial(*material);
        }
        return;
    }

    if (!materials_.empty()) {
        object.SetMaterial(materials_.front());
    }
}
