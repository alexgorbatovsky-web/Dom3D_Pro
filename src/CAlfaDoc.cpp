#include "CAlfaDoc.h"

#include "CGroup.h"
#include "CAssembled.h"
#include "CKitchenCabinet.h"
#include "SmartLine.h"
#include "SketchProfileBuilder.h"
#include "ExtrudeShapeBuilder.h"
#include "SweptSolidBuilder.h"
#include "TwoRailSweepSurfaceBuilder.h"
#include "FourSplineSurfaceBuilder.h"
#include "CadCurve3D.h"
#include "ReferenceImage.h"
#include "solid/Solid.h"
#include "solid/AssociativeClone.h"
#include "solid/TwoSketchSolidBuilder.h"
#include "solid/PolyhedronShapeBuilder.h"
#include "solid/SurfaceSet.h"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRep_Builder.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeShape.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRepOffsetAPI_MakeFilling.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepProj_Projection.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <GeomConvert.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <GeomAPI_PointsToBSpline.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GProp_GProps.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TColgp_HArray1OfPnt.hxx>
#include <gp_Dir.hxx>
#include <gp_Ax1.hxx>
#include <gp_Lin.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <ShapeFix_Solid.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopExp.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_MapOfShape.hxx>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {
CAlfaDoc* g_current_alfa_doc = nullptr;

struct ScreenRectBounds {
    int left = 0;
    int right = 0;
    int top = 0;
    int bottom = 0;
};

ScreenRectBounds normalize_screen_rect(DomRect rect)
{
    return {
        std::min(rect.left, rect.right),
        std::max(rect.left, rect.right),
        std::min(rect.top, rect.bottom),
        std::max(rect.top, rect.bottom)
    };
}

bool screen_point_inside(DomPoint point, const ScreenRectBounds& rect)
{
    return point.x >= rect.left && point.x <= rect.right
        && point.y >= rect.top && point.y <= rect.bottom;
}

double screen_orientation(DomPoint a, DomPoint b, DomPoint c)
{
    return static_cast<double>(b.x - a.x) * static_cast<double>(c.y - a.y)
        - static_cast<double>(b.y - a.y) * static_cast<double>(c.x - a.x);
}

bool screen_point_on_segment(DomPoint point, DomPoint a, DomPoint b)
{
    constexpr double epsilon = 0.001;
    return std::fabs(screen_orientation(a, b, point)) <= epsilon
        && point.x >= std::min(a.x, b.x) && point.x <= std::max(a.x, b.x)
        && point.y >= std::min(a.y, b.y) && point.y <= std::max(a.y, b.y);
}

bool screen_segments_intersect(DomPoint a, DomPoint b, DomPoint c, DomPoint d)
{
    const double o1 = screen_orientation(a, b, c);
    const double o2 = screen_orientation(a, b, d);
    const double o3 = screen_orientation(c, d, a);
    const double o4 = screen_orientation(c, d, b);
    if (((o1 > 0.0 && o2 < 0.0) || (o1 < 0.0 && o2 > 0.0))
        && ((o3 > 0.0 && o4 < 0.0) || (o3 < 0.0 && o4 > 0.0))) {
        return true;
    }
    return (std::fabs(o1) <= 0.001 && screen_point_on_segment(c, a, b))
        || (std::fabs(o2) <= 0.001 && screen_point_on_segment(d, a, b))
        || (std::fabs(o3) <= 0.001 && screen_point_on_segment(a, c, d))
        || (std::fabs(o4) <= 0.001 && screen_point_on_segment(b, c, d));
}

bool screen_segment_intersects_rect(DomPoint a, DomPoint b, const ScreenRectBounds& rect)
{
    if (screen_point_inside(a, rect) || screen_point_inside(b, rect)) {
        return true;
    }
    const DomPoint top_left{rect.left, rect.top};
    const DomPoint top_right{rect.right, rect.top};
    const DomPoint bottom_right{rect.right, rect.bottom};
    const DomPoint bottom_left{rect.left, rect.bottom};
    return screen_segments_intersect(a, b, top_left, top_right)
        || screen_segments_intersect(a, b, top_right, bottom_right)
        || screen_segments_intersect(a, b, bottom_right, bottom_left)
        || screen_segments_intersect(a, b, bottom_left, top_left);
}

bool screen_point_in_polygon(DomPoint point, const std::vector<DomPoint>& polygon)
{
    if (polygon.size() < 3) {
        return false;
    }
    bool inside = false;
    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const DomPoint a = polygon[j];
        const DomPoint b = polygon[i];
        if (screen_point_on_segment(point, a, b)) {
            return true;
        }
        const bool crosses = (a.y > point.y) != (b.y > point.y);
        if (crosses) {
            const double x = static_cast<double>(b.x - a.x)
                    * static_cast<double>(point.y - a.y)
                    / static_cast<double>(b.y - a.y)
                + static_cast<double>(a.x);
            if (static_cast<double>(point.x) < x) {
                inside = !inside;
            }
        }
    }
    return inside;
}

bool screen_polygon_intersects_rect(
    const std::vector<DomPoint>& polygon,
    const ScreenRectBounds& rect)
{
    if (polygon.empty()) {
        return false;
    }
    if (std::any_of(polygon.begin(), polygon.end(), [&](DomPoint point) {
            return screen_point_inside(point, rect);
        })) {
        return true;
    }
    for (size_t i = 0; i < polygon.size(); ++i) {
        if (screen_segment_intersects_rect(
                polygon[i], polygon[(i + 1) % polygon.size()], rect)) {
            return true;
        }
    }
    const DomPoint corners[] = {
        {rect.left, rect.top},
        {rect.right, rect.top},
        {rect.right, rect.bottom},
        {rect.left, rect.bottom}
    };
    return std::any_of(std::begin(corners), std::end(corners), [&](DomPoint corner) {
        return screen_point_in_polygon(corner, polygon);
    });
}

bool projected_points_match_rect(
    const std::vector<Vec3>& points,
    const ScreenRectBounds& rect,
    bool crossing,
    const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
    bool closed = false)
{
    if (points.empty()) {
        return false;
    }
    std::vector<DomPoint> projected(points.size());
    std::vector<bool> valid(points.size(), false);
    bool any_inside = false;
    bool all_inside = true;
    for (size_t i = 0; i < points.size(); ++i) {
        valid[i] = world_to_screen(points[i], projected[i]);
        const bool inside = valid[i] && screen_point_inside(projected[i], rect);
        any_inside = any_inside || inside;
        all_inside = all_inside && inside;
    }
    if (!crossing) {
        return all_inside;
    }
    if (any_inside) {
        return true;
    }
    const size_t segment_count = closed ? points.size() : points.size() - 1;
    for (size_t i = 0; i < segment_count; ++i) {
        const size_t next = (i + 1) % points.size();
        if (valid[i] && valid[next]
            && screen_segment_intersects_rect(projected[i], projected[next], rect)) {
            return true;
        }
    }
    return false;
}

bool mesh_matches_screen_rect(
    const CMesh3D& mesh,
    const ScreenRectBounds& rect,
    bool crossing,
    const std::function<bool(Vec3, DomPoint&)>& world_to_screen)
{
    const std::vector<Vec3>& vertices = mesh.GetVertices();
    if (vertices.empty()) {
        return false;
    }
    std::vector<DomPoint> projected(vertices.size());
    std::vector<bool> valid(vertices.size(), false);
    bool all_inside = true;
    for (size_t i = 0; i < vertices.size(); ++i) {
        valid[i] = world_to_screen(vertices[i], projected[i]);
        all_inside = all_inside
            && valid[i] && screen_point_inside(projected[i], rect);
    }
    if (!crossing) {
        return all_inside;
    }
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (valid[i] && screen_point_inside(projected[i], rect)) {
            return true;
        }
    }
    for (const CMesh3D::Face& face : mesh.GetFaces()) {
        std::vector<DomPoint> polygon;
        polygon.reserve(CMesh3D::FaceVertexCount(face));
        bool face_valid = true;
        for (size_t i = 0; i < CMesh3D::FaceVertexCount(face); ++i) {
            const size_t vertex_index = CMesh3D::GetFaceVertexIndex(face, i);
            if (vertex_index >= projected.size() || !valid[vertex_index]) {
                face_valid = false;
                break;
            }
            polygon.push_back(projected[vertex_index]);
        }
        if (face_valid && screen_polygon_intersects_rect(polygon, rect)) {
            return true;
        }
    }
    return false;
}

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
    result->m_id = source_solid->m_id;
    result->SetName(source_solid->GetName());
    result->SetColor(source_solid->GetColor());
    result->SetMaterial(source_solid->GetMaterial());
    result->SetMaterialId(source_solid->GetMaterialId());
    result->SetGroupName(source_solid->GetGroupName());
    result->SetVisible(source_solid->IsVisible());
    result->m_LayerID = source_solid->m_LayerID;
    if (source_solid->GetNumOperations() > 0) {
        result->CopyOperationTreeFrom(*source_solid);
    }
    result->ReBuldMesh();

    objects[solid_index] = std::move(result);
    return true;
}

std::vector<int> generated_face_indices(BRepBuilderAPI_MakeShape& builder,
                                        const std::vector<TopoDS_Edge>& source_edges,
                                        const TopoDS_Shape& result_shape)
{
    std::vector<TopoDS_Face> generated_faces;
    for (const TopoDS_Edge& edge : source_edges) {
        const TopTools_ListOfShape& generated = builder.Generated(edge);
        for (TopTools_ListIteratorOfListOfShape it(generated); it.More(); it.Next()) {
            if (it.Value().ShapeType() == TopAbs_FACE) {
                generated_faces.push_back(TopoDS::Face(it.Value()));
            }
        }
    }

    std::vector<int> indices;
    int index = 0;
    for (TopExp_Explorer explorer(result_shape, TopAbs_FACE); explorer.More(); explorer.Next(), ++index) {
        const TopoDS_Face face = TopoDS::Face(explorer.Current());
        if (std::any_of(generated_faces.begin(), generated_faces.end(), [&face](const TopoDS_Face& generated) {
                return face.IsSame(generated);
            })) {
            indices.push_back(index);
        }
    }
    return indices;
}

bool build_boolean_shape(BooleanOperation operation, const TopoDS_Shape& body, const TopoDS_Shape& tool, TopoDS_Shape& result)
{
    if (body.IsNull() || tool.IsNull()) {
        return false;
    }

    try {
        if (operation == BooleanOperation::Union) {
            BRepAlgoAPI_Fuse algo(body, tool);
            algo.Build();
            if (!algo.IsDone()) {
                return false;
            }
            result = algo.Shape();
        } else if (operation == BooleanOperation::Cut) {
            BRepAlgoAPI_Cut algo(body, tool);
            algo.Build();
            if (!algo.IsDone()) {
                return false;
            }
            result = algo.Shape();
        } else {
            BRepAlgoAPI_Common algo(body, tool);
            algo.Build();
            if (!algo.IsDone()) {
                return false;
            }
            result = algo.Shape();
        }
    } catch (const Standard_Failure&) {
        return false;
    }

    return !result.IsNull();
}

const char* boolean_operation_name(BooleanOperation operation)
{
    if (operation == BooleanOperation::Union) {
        return "Boolean Union";
    }
    if (operation == BooleanOperation::Cut) {
        return "Boolean Cut";
    }
    return "Boolean Common";
}

TopoDS_Shape make_extrude_prism(const TopoDS_Face& face, Vec3 normal, float distance, double taper_angle_degrees)
{
    return BuildExtrudeShape(face, normal, distance, taper_angle_degrees);
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

    const Vec3 unit_draft = normalize(draft_direction);
    if (dot(unit_draft, unit_draft) <= 0.000001f) {
        return {};
    }

    try {
        BRepOffsetAPI_DraftAngle draft(base_shape);
        draft.Add(face,
                  gp_Dir(unit_draft.x, unit_draft.y, unit_draft.z),
                  angle_degrees * 3.14159265358979323846 / 180.0,
                  neutral_plane);
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

double parameter_value(const std::vector<ParametricParameterValue>& parameters,
                       const char* id,
                       double fallback)
{
    for (const ParametricParameterValue& parameter : parameters) {
        if (parameter.id == id) {
            return parameter.value;
        }
    }
    return fallback;
}

bool has_non_uniform_scale_operation(const CSolid& solid)
{
    for (const ParametricFunction* operation : solid.GetOperationTree()) {
        if (!operation || operation->ToolId != "SolidTransform" || operation->Name != "Scale") {
            continue;
        }
        const double type = parameter_value(operation->Parameters, "type", -1.0);
        if (std::fabs(type - 2.0) > 0.001) {
            continue;
        }
        const Vec3 axis{
            static_cast<float>(parameter_value(operation->Parameters, "axis.x", 0.0)),
            static_cast<float>(parameter_value(operation->Parameters, "axis.y", 0.0)),
            static_cast<float>(parameter_value(operation->Parameters, "axis.z", 0.0))
        };
        if (dot(axis, axis) > 0.000001f) {
            return true;
        }
    }
    return false;
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

    const double plane_height = points.front().z;
    for (const CPoint3d& point : points) {
        if (std::fabs(point.z - plane_height) > tolerance) {
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

    axis_origin = {0.0f, 0.0f, static_cast<float>(points.front().z)};

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

bool build_revolve_profile_from_sketch(const CSmartLine& sketch,
                                       int axis_index,
                                       TopoDS_Shape& profile_shape,
                                       Vec3& axis_origin,
                                       Vec3& axis_direction)
{
    const SketchCoordinateSystem& system = sketch.GetCoordinateSystem();
    axis_origin = {
        static_cast<float>(system.origin.x),
        static_cast<float>(system.origin.y),
        static_cast<float>(system.origin.z)};
    axis_direction = revolve_axis_direction(axis_index);
    if (dot(axis_direction, axis_direction) <= 1.0e-12f) {
        return false;
    }
    TopoDS_Face profile_face;
    Vec3 profile_normal{};
    if (!BuildSketchRevolveProfileFace(
            sketch,
            axis_origin,
            axis_direction,
            profile_face,
            profile_normal)) {
        return false;
    }
    profile_shape = profile_face;
    return true;
}

Vec3 revolve_axis_direction(int axis_index)
{
    if (axis_index == 0) {
        return {1.0f, 0.0f, 0.0f};
    }
    if (axis_index == 1) {
        return {0.0f, 1.0f, 0.0f};
    }
    return {0.0f, 0.0f, 1.0f};
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

bool sweep_curve_samples(const CAlfaObject& object, SweepCurveSamples& samples)
{
    samples = {};
    if (const auto* curve = dynamic_cast<const CCadCurve3D*>(&object)) {
        samples.points.reserve(curve->GetPoints().size());
        for (const Vec3& point : curve->GetPoints()) {
            samples.points.emplace_back(point.x, point.y, point.z);
        }
        if (samples.points.size() >= 3) {
            double length = 0.0;
            for (std::size_t i = 1; i < samples.points.size(); ++i) {
                const CPoint3d delta(samples.points[i].x - samples.points[i - 1].x,
                                     samples.points[i].y - samples.points[i - 1].y,
                                     samples.points[i].z - samples.points[i - 1].z);
                length += std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
            }
            const CPoint3d end_delta(samples.points.back().x - samples.points.front().x,
                                     samples.points.back().y - samples.points.front().y,
                                     samples.points.back().z - samples.points.front().z);
            const double end_distance = std::sqrt(end_delta.x * end_delta.x
                + end_delta.y * end_delta.y + end_delta.z * end_delta.z);
            samples.closed = end_distance <= std::max(1.0e-6, length * 1.0e-4);
        }
        return samples.points.size() >= 2;
    }
    if (const auto* spline = dynamic_cast<const CBSpline*>(&object)) {
        if (spline->GetPointCount() < 2) {
            return false;
        }
        samples.closed = spline->IsClosed();
        const int count = std::max(24, static_cast<int>(spline->GetPointCount()) * 16);
        samples.points.reserve(static_cast<std::size_t>(count + 1));
        for (int i = 0; i <= count; ++i) {
            const float parameter = static_cast<float>(i) / static_cast<float>(count);
            samples.points.push_back(spline->Evaluate(parameter));
        }
        return true;
    }
    if (const auto* polyline = dynamic_cast<const CPolyline*>(&object)) {
        samples.points = polyline->GetPoints();
        samples.closed = polyline->IsClosed();
        return samples.points.size() >= 2;
    }
    if (const auto* sketch = dynamic_cast<const CSmartLine*>(&object)) {
        samples.points = sketch->GetProfilePointsWorld();
        samples.closed = sketch->IsClosed();
        return samples.points.size() >= 2;
    }
    return false;
}

TopoDS_Shape make_two_rail_sweep_surface(const CAlfaObject& profile,
                                         const CAlfaObject& first_rail,
                                         const CAlfaObject& second_rail)
{
    SweepCurveSamples profile_samples;
    SweepCurveSamples first_samples;
    SweepCurveSamples second_samples;
    if (!sweep_curve_samples(profile, profile_samples)
        || !sweep_curve_samples(first_rail, first_samples)
        || !sweep_curve_samples(second_rail, second_samples)) {
        return {};
    }
    return BuildTwoRailSweepSurfaceShape(profile_samples, first_samples, second_samples);
}

double point_distance(const CPoint3d& first, const CPoint3d& second)
{
    const double x = first.x - second.x;
    const double y = first.y - second.y;
    const double z = first.z - second.z;
    return std::sqrt(x * x + y * y + z * z);
}

TopoDS_Wire make_wire_from_curve_object(const CAlfaObject& object)
{
    if (const auto* spline = dynamic_cast<const CBSpline*>(&object)) {
        return make_wire_from_bspline(*spline);
    }

    SweepCurveSamples samples;
    if (!sweep_curve_samples(object, samples) || samples.points.size() < 2) {
        return {};
    }
    try {
        BRepBuilderAPI_MakeWire wire;
        for (size_t index = 1; index < samples.points.size(); ++index) {
            const CPoint3d& first = samples.points[index - 1];
            const CPoint3d& second = samples.points[index];
            if (point_distance(first, second) <= 1.0e-9) continue;
            wire.Add(BRepBuilderAPI_MakeEdge(
                gp_Pnt(first.x, first.y, first.z),
                gp_Pnt(second.x, second.y, second.z)));
        }
        if (samples.closed
            && point_distance(samples.points.front(), samples.points.back()) > 1.0e-9) {
            const CPoint3d& first = samples.points.back();
            const CPoint3d& second = samples.points.front();
            wire.Add(BRepBuilderAPI_MakeEdge(
                gp_Pnt(first.x, first.y, first.z),
                gp_Pnt(second.x, second.y, second.z)));
        }
        return wire.IsDone() ? wire.Wire() : TopoDS_Wire{};
    } catch (const Standard_Failure&) {
        return {};
    }
}

std::unique_ptr<CBSpline> editable_nurbs_from_edge(
    const TopoDS_Edge& edge, const std::string& name)
{
    if (edge.IsNull()) return {};
    try {
        Standard_Real first = 0.0;
        Standard_Real last = 0.0;
        TopLoc_Location location;
        const Handle(Geom_Curve) source =
            BRep_Tool::Curve(edge, location, first, last);
        if (source.IsNull()
            || Precision::IsNegativeInfinite(first)
            || Precision::IsPositiveInfinite(last)
            || last - first <= Precision::PConfusion()) {
            return {};
        }

        Handle(Geom_Curve) world =
            Handle(Geom_Curve)::DownCast(source->Copy());
        if (world.IsNull()) return {};
        if (!location.IsIdentity()) world->Transform(location.Transformation());
        Handle(Geom_BSplineCurve) nurbs = GeomConvert::CurveToBSplineCurve(
            new Geom_TrimmedCurve(world, first, last));
        if (nurbs.IsNull() || nurbs->NbPoles() < 2) return {};

        const bool preserve_closed = nurbs->IsClosed() || nurbs->IsPeriodic();
        if (nurbs->IsPeriodic()) nurbs->SetNotPeriodic();
        auto result = std::make_unique<CBSpline>(name);
        result->SetCurveType(SplineCurveType::Nurbs);
        result->SetDegree(nurbs->Degree());
        std::vector<double> weights;
        weights.reserve(static_cast<size_t>(nurbs->NbPoles()));
        for (Standard_Integer index = 1; index <= nurbs->NbPoles(); ++index) {
            const gp_Pnt pole = nurbs->Pole(index);
            result->AddPoint(CPoint3d(pole.X(), pole.Y(), pole.Z()));
            weights.push_back(nurbs->Weight(index));
        }
        result->SetCurveType(SplineCurveType::Nurbs);
        result->SetDegree(nurbs->Degree());
        result->SetWeights(std::move(weights));
        const TColStd_Array1OfReal& sequence = nurbs->KnotSequence();
        std::vector<double> knots;
        knots.reserve(static_cast<size_t>(sequence.Length()));
        for (Standard_Integer index = sequence.Lower();
             index <= sequence.Upper(); ++index) {
            knots.push_back(sequence.Value(index));
        }
        if (!result->SetKnots(std::move(knots))) return {};
        result->SetClosed(preserve_closed);
        result->SetColor(kDefaultCurveColor);
        return result;
    } catch (const Standard_Failure&) {
        return {};
    }
}

std::vector<TopoDS_Edge> shape_edges(const TopoDS_Shape& shape)
{
    std::vector<TopoDS_Edge> edges;
    TopTools_MapOfShape seen;
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE);
         explorer.More(); explorer.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
        if (!edge.IsNull() && seen.Add(edge)) edges.push_back(edge);
    }
    return edges;
}

std::vector<std::unique_ptr<CBSpline>> nurbs_curves_from_shape(
    const TopoDS_Shape& shape, const std::string& name)
{
    std::vector<std::unique_ptr<CBSpline>> curves;
    int number = 1;
    for (const TopoDS_Edge& edge : shape_edges(shape)) {
        std::unique_ptr<CBSpline> curve = editable_nurbs_from_edge(
            edge, name + (number > 1 ? " " + std::to_string(number) : ""));
        if (curve) {
            curves.push_back(std::move(curve));
            ++number;
        }
    }
    return curves;
}

bool selected_plane(const CAlfaObject& object, gp_Pln& plane)
{
    if (object.GetParametricToolId() != "PlaneTool") return false;
    const auto* surface = dynamic_cast<const CSurfaceSet*>(&object);
    if (!surface || surface->m_Shape.IsNull()) return false;
    for (TopExp_Explorer explorer(surface->m_Shape, TopAbs_FACE);
         explorer.More(); explorer.Next()) {
        BRepAdaptor_Surface adaptor(TopoDS::Face(explorer.Current()),
                                    Standard_True);
        if (adaptor.GetType() == GeomAbs_Plane) {
            plane = adaptor.Plane();
            return true;
        }
    }
    return false;
}

std::vector<std::vector<CPoint3d>> intersect_mesh_with_plane(
    const CMesh3D& mesh, const gp_Pln& plane)
{
    struct Segment { CPoint3d first; CPoint3d second; };
    constexpr double tolerance = 1.0e-6;
    const gp_Pnt origin = plane.Location();
    const gp_Dir normal = plane.Axis().Direction();
    const auto signed_distance = [&](const Vec3& point) {
        return gp_Vec(origin, gp_Pnt(point.x, point.y, point.z)).Dot(
            gp_Vec(normal));
    };
    const auto interpolate = [](const Vec3& a, const Vec3& b,
                                double da, double db) {
        const double divisor = da - db;
        const double t = std::fabs(divisor) <= 1.0e-18 ? 0.0 : da / divisor;
        return CPoint3d(a.x + (b.x - a.x) * t,
                        a.y + (b.y - a.y) * t,
                        a.z + (b.z - a.z) * t);
    };
    const auto same_point = [](const CPoint3d& a, const CPoint3d& b) {
        return point_distance(a, b) <= tolerance;
    };

    std::vector<Segment> segments;
    const auto& vertices = mesh.GetVertices();
    for (const CMesh3D::Face& face : mesh.GetFaces()) {
        if (face.deleted || face.corners.size() < 3) continue;
        const size_t root = CMesh3D::GetFaceVertexIndex(face, 0);
        if (root >= vertices.size()) continue;
        for (size_t corner = 1; corner + 1 < face.corners.size(); ++corner) {
            const size_t second_index = CMesh3D::GetFaceVertexIndex(face, corner);
            const size_t third_index = CMesh3D::GetFaceVertexIndex(face, corner + 1);
            if (second_index >= vertices.size() || third_index >= vertices.size()) continue;
            const std::array<Vec3, 3> triangle{
                vertices[root], vertices[second_index], vertices[third_index]};
            const std::array<double, 3> distances{
                signed_distance(triangle[0]), signed_distance(triangle[1]),
                signed_distance(triangle[2])};
            if (std::all_of(distances.begin(), distances.end(),
                            [](double value) { return std::fabs(value) <= tolerance; })) {
                continue;
            }
            std::vector<CPoint3d> hits;
            for (int edge_index = 0; edge_index < 3; ++edge_index) {
                const int next = (edge_index + 1) % 3;
                const double a = distances[edge_index];
                const double b = distances[next];
                CPoint3d hit;
                bool has_hit = false;
                if (std::fabs(a) <= tolerance) {
                    const Vec3& value = triangle[edge_index];
                    hit = CPoint3d(value.x, value.y, value.z);
                    has_hit = true;
                } else if ((a < 0.0) != (b < 0.0)) {
                    hit = interpolate(triangle[edge_index], triangle[next], a, b);
                    has_hit = true;
                }
                if (has_hit && std::none_of(hits.begin(), hits.end(),
                        [&](const CPoint3d& point) { return same_point(point, hit); })) {
                    hits.push_back(hit);
                }
            }
            if (hits.size() >= 2) {
                size_t first_hit = 0;
                size_t second_hit = 1;
                double longest = point_distance(hits[0], hits[1]);
                for (size_t a = 0; a < hits.size(); ++a) {
                    for (size_t b = a + 1; b < hits.size(); ++b) {
                        const double length = point_distance(hits[a], hits[b]);
                        if (length > longest) {
                            longest = length;
                            first_hit = a;
                            second_hit = b;
                        }
                    }
                }
                if (longest > tolerance) {
                    segments.push_back({hits[first_hit], hits[second_hit]});
                }
            }
        }
    }

    std::vector<std::vector<CPoint3d>> polylines;
    while (!segments.empty()) {
        std::vector<CPoint3d> points{
            segments.back().first, segments.back().second};
        segments.pop_back();
        bool extended = true;
        while (extended) {
            extended = false;
            for (size_t index = 0; index < segments.size(); ++index) {
                const Segment segment = segments[index];
                if (same_point(points.back(), segment.first)) {
                    points.push_back(segment.second);
                } else if (same_point(points.back(), segment.second)) {
                    points.push_back(segment.first);
                } else if (same_point(points.front(), segment.second)) {
                    points.insert(points.begin(), segment.first);
                } else if (same_point(points.front(), segment.first)) {
                    points.insert(points.begin(), segment.second);
                } else {
                    continue;
                }
                segments.erase(segments.begin()
                    + static_cast<std::vector<Segment>::difference_type>(index));
                extended = true;
                break;
            }
        }
        if (points.size() >= 2) polylines.push_back(std::move(points));
    }
    return polylines;
}

TopoDS_Shape make_ruled_surface_from_spline(const CBSpline& spline,
                                            double length,
                                            int direction_axis,
                                            bool reverse_normal)
{
    if (spline.GetPointCount() < 2 || std::fabs(length) <= 1.0e-9) {
        return {};
    }

    direction_axis = std::clamp(direction_axis, 0, 2);
    gp_Vec direction(0.0, 0.0, 0.0);
    if (direction_axis == 0) direction.SetX(length);
    else if (direction_axis == 1) direction.SetY(length);
    else direction.SetZ(length);

    try {
        // Interpolate a dense set of points from Dom-3D's own evaluator.  It
        // keeps Bezier chains, B-Splines and rational NURBS on the exact same
        // visible locus while giving OpenCascade one smooth curve/one face.
        const bool periodic = spline.IsClosed();
        const int sample_count = std::max(
            64, static_cast<int>(spline.GetPointCount()) * 24);
        Handle(TColgp_HArray1OfPnt) samples =
            new TColgp_HArray1OfPnt(1, sample_count);
        for (int index = 0; index < sample_count; ++index) {
            const double denominator = periodic
                ? static_cast<double>(sample_count)
                : static_cast<double>(sample_count - 1);
            const CPoint3d point = spline.Evaluate(
                static_cast<float>(index / denominator));
            samples->SetValue(index + 1, gp_Pnt(point.x, point.y, point.z));
        }

        GeomAPI_Interpolate interpolation(samples, periodic, 1.0e-7);
        interpolation.Perform();
        if (!interpolation.IsDone()) return {};

        BRepBuilderAPI_MakeEdge edge_builder(interpolation.Curve());
        if (!edge_builder.IsDone()) return {};
        BRepPrimAPI_MakePrism prism(
            edge_builder.Edge(), direction, Standard_False, Standard_True);
        prism.Build();
        if (!prism.IsDone() || prism.Shape().IsNull()) return {};

        TopoDS_Shape shape = prism.Shape();
        if (reverse_normal) shape.Reverse();
        return shape;
    } catch (const Standard_Failure&) {
        return {};
    }
}

struct SurfaceEdgePair {
    TopoDS_Edge first;
    TopoDS_Edge second;
    TopoDS_Face first_support;
    TopoDS_Face second_support;
    double distance = std::numeric_limits<double>::max();
};

struct SupportedBoundaryEdge {
    TopoDS_Edge edge;
    TopoDS_Face face;
};

std::vector<SupportedBoundaryEdge> surface_boundary_edges(
    const TopoDS_Shape& shape)
{
    TopTools_IndexedDataMapOfShapeListOfShape edge_faces;
    TopExp::MapShapesAndAncestors(
        shape, TopAbs_EDGE, TopAbs_FACE, edge_faces);
    std::vector<SupportedBoundaryEdge> result;
    for (Standard_Integer index = 1; index <= edge_faces.Extent(); ++index) {
        const TopTools_ListOfShape& faces = edge_faces.FindFromIndex(index);
        if (faces.Extent() != 1) continue;
        result.push_back({
            TopoDS::Edge(edge_faces.FindKey(index)),
            TopoDS::Face(faces.First())});
    }
    return result;
}

SurfaceEdgePair closest_surface_edges(const TopoDS_Shape& first,
                                      const TopoDS_Shape& second)
{
    SurfaceEdgePair best;
    const std::vector<SupportedBoundaryEdge> first_edges =
        surface_boundary_edges(first);
    const std::vector<SupportedBoundaryEdge> second_edges =
        surface_boundary_edges(second);
    for (const SupportedBoundaryEdge& first_boundary : first_edges) {
        const TopoDS_Edge first_edge = first_boundary.edge;
        TopoDS_Vertex a0, a1;
        TopExp::Vertices(first_edge, a0, a1, Standard_True);
        if (a0.IsNull() || a1.IsNull()) continue;
        const gp_Pnt p0 = BRep_Tool::Pnt(a0);
        const gp_Pnt p1 = BRep_Tool::Pnt(a1);

        for (const SupportedBoundaryEdge& second_boundary : second_edges) {
            TopoDS_Edge second_edge = second_boundary.edge;
            TopoDS_Vertex b0, b1;
            TopExp::Vertices(second_edge, b0, b1, Standard_True);
            if (b0.IsNull() || b1.IsNull()) continue;
            const gp_Pnt q0 = BRep_Tool::Pnt(b0);
            const gp_Pnt q1 = BRep_Tool::Pnt(b1);
            const double same = p0.SquareDistance(q0) + p1.SquareDistance(q1);
            const double reversed =
                p0.SquareDistance(q1) + p1.SquareDistance(q0);
            const double distance = std::min(same, reversed);
            if (distance >= best.distance) continue;
            if (reversed < same) second_edge.Reverse();
            best = {first_edge, second_edge,
                    first_boundary.face, second_boundary.face, distance};
        }
    }
    return best;
}

bool surface_edges_coincide(const TopoDS_Edge& first,
                            const TopoDS_Edge& second,
                            double tolerance)
{
    if (first.IsNull() || second.IsNull()) return false;
    try {
        BRepAdaptor_Curve first_curve(first);
        BRepAdaptor_Curve second_curve(second);
        const double first_begin = first_curve.FirstParameter();
        const double first_end = first_curve.LastParameter();
        const double second_begin = second_curve.FirstParameter();
        const double second_end = second_curve.LastParameter();
        if (!std::isfinite(first_begin) || !std::isfinite(first_end)
            || !std::isfinite(second_begin) || !std::isfinite(second_end)) {
            return false;
        }

        double same_max = 0.0;
        double reversed_max = 0.0;
        constexpr int sample_count = 16;
        for (int sample = 0; sample <= sample_count; ++sample) {
            const double t = static_cast<double>(sample) / sample_count;
            const gp_Pnt first_point = first_curve.Value(
                first_begin + (first_end - first_begin) * t);
            const gp_Pnt same_point = second_curve.Value(
                second_begin + (second_end - second_begin) * t);
            const gp_Pnt reversed_point = second_curve.Value(
                second_end + (second_begin - second_end) * t);
            same_max = std::max(same_max,
                                first_point.Distance(same_point));
            reversed_max = std::max(reversed_max,
                                    first_point.Distance(reversed_point));
        }
        return std::min(same_max, reversed_max) <= tolerance;
    } catch (const Standard_Failure&) {
        return false;
    }
}

TopoDS_Shape make_four_spline_surface(const CAlfaObject& first,
                                      const CAlfaObject& second,
                                      const CAlfaObject& third,
                                      const CAlfaObject& fourth)
{
    SweepCurveSamples first_samples;
    SweepCurveSamples second_samples;
    SweepCurveSamples third_samples;
    SweepCurveSamples fourth_samples;
    if (!sweep_curve_samples(first, first_samples)
        || !sweep_curve_samples(second, second_samples)
        || !sweep_curve_samples(third, third_samples)
        || !sweep_curve_samples(fourth, fourth_samples)) {
        return {};
    }
    return BuildFourSplineSurfaceShape(
        first_samples, second_samples, third_samples, fourth_samples);
}

TopoDS_Shape make_two_rail_sweep_solid(const CAlfaObject& profile,
                                       const CAlfaObject& first_rail,
                                       const CAlfaObject& second_rail)
{
    SweepCurveSamples profile_samples;
    SweepCurveSamples first_samples;
    SweepCurveSamples second_samples;
    if (!sweep_curve_samples(profile, profile_samples)
        || !sweep_curve_samples(first_rail, first_samples)
        || !sweep_curve_samples(second_rail, second_samples)) {
        return {};
    }
    const auto* sketch = dynamic_cast<const CSmartLine*>(&profile);
    TopoDS_Face profile_face;
    Vec3 profile_normal{};
    if (!sketch
        || !BuildSketchProfileFace(*sketch, profile_face, profile_normal)) {
        return {};
    }
    TopExp_Explorer wires(profile_face, TopAbs_WIRE);
    if (!wires.More()) {
        return {};
    }
    return BuildTwoRailSweepSolidShape(
        profile_samples,
        first_samples,
        second_samples,
        TopoDS::Wire(wires.Current()));
}

bool hit_test_polyline_screen(const CPolyline& polyline,
                              DomPoint point,
                              const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                              float tolerance)
{
    const std::vector<CPoint3d> points = polyline.GetRoundedPathPoints();
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

    const CPoint3d& first_point = points.front();
    const CPoint3d& last_point = points.back();
    const double closure_distance_squared =
        (first_point.x - last_point.x) * (first_point.x - last_point.x)
        + (first_point.y - last_point.y) * (first_point.y - last_point.y)
        + (first_point.z - last_point.z) * (first_point.z - last_point.z);
    if (polyline.IsClosed() && closure_distance_squared > 1.0e-18) {
        DomPoint first{};
        if (world_to_screen(to_vec3(points.front()), first)
            && distance_to_screen_segment(point, previous, first) <= tolerance) {
            return true;
        }
    }

    return false;
}

bool hit_test_sketch_screen(const CSmartLine& sketch,
                            DomPoint point,
                            const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                            float tolerance)
{
    return sketch.HitTestScreen(point, world_to_screen, tolerance);
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

CAlfaDoc* GetAlfaDoc()
{
    return g_current_alfa_doc;
}

struct CAlfaDoc::Snapshot {
    struct LayerState {
        int id = 0;
        std::string name;
        bool visible = true;
        bool selectable = true;
    };

    ObjectList objects;
    std::vector<Material> materials;
    std::vector<LayerState> layers;
    unsigned long next_object_id = 1;
    size_t active_object_index = 0;
    size_t selected_object_index = 0;
    size_t selected_face_object_index = 0;
    size_t selected_point_index = 0;
    std::vector<size_t> selected_object_indices;
    std::vector<std::pair<size_t, size_t>> selected_curve_points;
    std::vector<int> selected_solid_face_indices;
    int work_layer = 0;
    bool has_selected_solid_face = false;
    bool has_selected_object = false;
    bool has_selected_point = false;
    bool has_transform_gizmo_origin = false;
    Vec3 transform_gizmo_origin{};
};

CLayer::CLayer(int id, std::string name)
    : Name(std::move(name)),
      m_ID(id) {
}

int CLayer::ID() const {
    return m_ID;
}

CAlfaDoc::CAlfaDoc() {
    g_current_alfa_doc = this;
    EnsureDefaultLayer();
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
    float distance = 0.0f;
    std::vector<int> created_surface_indices;
};

struct CAlfaDoc::LivePolylineExtrudeData {
    TopoDS_Face profile_face;
    Vec3 normal{};
    unsigned long profile_id = 0;
    double distance = 1.0;
    bool reverse = false;
    double taper_angle_degrees = 0.0;
    size_t polyline_index = 0;
    size_t solid_index = 0;
    bool has_solid = false;
};

struct CAlfaDoc::LivePolylineRevolveData {
    TopoDS_Shape profile_shape;
    Vec3 axis_origin{};
    Vec3 axis_direction{0.0f, 0.0f, 1.0f};
    unsigned long profile_id = 0;
    double angle_degrees = 360.0;
    int axis_index = 2;
    size_t polyline_index = 0;
    size_t solid_index = 0;
    bool has_solid = false;
};

struct CAlfaDoc::LiveFilletData {
    TopoDS_Shape base_shape;
    std::vector<TopoDS_Edge> edges;
    std::vector<std::pair<int, int>> edge_refs;
    std::vector<int> created_surface_indices;
    size_t object_index = 0;
};

struct CAlfaDoc::LiveChamferData {
    TopoDS_Shape base_shape;
    std::vector<TopoDS_Edge> edges;
    std::vector<std::pair<int, int>> edge_refs;
    std::vector<int> created_surface_indices;
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
    int axis_edge_index = -1;
    double angle_degrees = 0.0;
    std::vector<int> created_surface_indices;
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
    std::vector<int> created_surface_indices;
};

CAlfaDoc::~CAlfaDoc() {
    if (g_current_alfa_doc == this) {
        g_current_alfa_doc = nullptr;
    }
    for (CLayer* layer : m_Layers) {
        delete layer;
    }
    m_Layers.clear();
}

void CAlfaDoc::Clear() {
    objects_.clear();
    ResetDefaultMaterials();
    for (CLayer* layer : m_Layers) {
        delete layer;
    }
    m_Layers.clear();
    Work_layer = 0;
    EnsureDefaultLayer();
    next_object_id_ = 1;
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
    EnsureObjectId(*polyline);
    AssignDefaultMaterial(*polyline);
    AssignObjectToWorkLayer(*polyline);
    objects_.push_back(std::move(polyline));
    active_object_index_ = objects_.size() - 1;
    ClearSelection();
}

void CAlfaDoc::CreateBSpline() {
    const size_t next_number = objects_.size() + 1;
    auto spline = std::make_unique<CBSpline>("B-Spline " + std::to_string(next_number));
    EnsureObjectId(*spline);
    AssignDefaultMaterial(*spline);
    AssignObjectToWorkLayer(*spline);
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

    CPolyline polyline(sketch_name.empty() ? "Sketch Rectangle" : sketch_name + " Rectangle");
    for (const CPoint3d& point : points) {
        polyline.AddPoint(point);
    }
    polyline.Close();

    auto sketch = std::make_unique<CSmartLine>(polyline.GetName());
    if (!sketch->Create(polyline)) {
        return;
    }
    AddObject(std::move(sketch));
    active_object_index_ = objects_.size() - 1;
}

bool CAlfaDoc::CreateSketchPolyline(const std::vector<CPoint3d>& points,
                                    bool closed,
                                    const std::string& sketch_name,
                                    CPoint3d origin,
                                    CPoint3d x_axis,
                                    CPoint3d y_axis) {
    auto sketch = std::make_unique<CSmartLine>(
        sketch_name.empty() ? "Sketch Polyline" : sketch_name + " Polyline");
    if (!sketch->CreateFromWorldPoints(points, closed, origin, x_axis, y_axis)) {
        return false;
    }
    AddObject(std::move(sketch));
    active_object_index_ = objects_.size() - 1;
    return true;
}

bool CAlfaDoc::CreateSketchBezier(const std::vector<CPoint3d>& control_points,
                                  const std::string& sketch_name,
                                  CPoint3d origin,
                                  CPoint3d x_axis,
                                  CPoint3d y_axis) {
    if (control_points.size() != 4) {
        return false;
    }
    CPoint3d normal(
        x_axis.y * y_axis.z - x_axis.z * y_axis.y,
        x_axis.z * y_axis.x - x_axis.x * y_axis.z,
        x_axis.x * y_axis.y - x_axis.y * y_axis.x);
    auto sketch = std::make_unique<CSmartLine>(
        sketch_name.empty() ? "Sketch Bezier" : sketch_name + " Bezier");
    if (!sketch->SetCoordinateSystem(origin, x_axis, normal)
        || !sketch->AddBezierWorld(
            control_points[0],
            control_points[1],
            control_points[2],
            control_points[3])) {
        return false;
    }
    AddObject(std::move(sketch));
    active_object_index_ = objects_.size() - 1;
    return true;
}

bool CAlfaDoc::CreateSweptSolid(unsigned long section_id,
                                unsigned long guide_id,
                                int transition_mode,
                                double delta_x,
                                double delta_y,
                                double angle_degrees) {
    const auto* section = dynamic_cast<const CSmartLine*>(FindObjectById(section_id));
    const CAlfaObject* guide = FindObjectById(guide_id);
    if (!section || !section->IsClosed() || !guide || section == guide) {
        return false;
    }
    if (const auto* guide_sketch = dynamic_cast<const CSmartLine*>(guide)) {
        if (guide_sketch->IsClosed()) {
            return false;
        }
    } else if (const auto* guide_spline = dynamic_cast<const CBSpline*>(guide)) {
        if (guide_spline->IsClosed()) {
            return false;
        }
    } else {
        return false;
    }

    TopoDS_Shape shape = BuildSweptSolidShape(
        *section, *guide, transition_mode, delta_x, delta_y, angle_degrees);
    if (shape.IsNull()) {
        return false;
    }

    auto solid = std::make_unique<CSolid>(shape);
    solid->SetName("Swept Solid");
    solid->SetParametricOperation(
        0,
        "SolidSweptTool",
        "Swept",
        {
            {"transition", static_cast<double>(transition_mode)},
            {"dx", delta_x},
            {"dy", delta_y},
            {"angle", angle_degrees},
            {"section.id", static_cast<double>(section_id)},
            {"guide.id", static_cast<double>(guide_id)}
        });
    if (!solid->ReBuldMesh()) {
        return false;
    }
    AddObject(std::move(solid));
    return true;
}

bool CAlfaDoc::CreateFrameSolid(unsigned long profile_id,
                                double width,
                                double height) {
    const auto* profile = dynamic_cast<const CSmartLine*>(FindObjectById(profile_id));
    if (!profile || !profile->IsClosed()) {
        return false;
    }

    TopoDS_Shape shape = BuildFrameSolidShape(*profile, width, height);
    if (shape.IsNull()) {
        return false;
    }

    auto solid = std::make_unique<CSolid>(shape);
    solid->SetName("Frame");
    solid->SetParametricOperation(
        0,
        "SolidFrameTool",
        "Frame",
        {
            {"width", width},
            {"height", height},
            {"profile.id", static_cast<double>(profile_id)}
        });
    if (!solid->ReBuldMesh()) {
        return false;
    }
    AddObject(std::move(solid));
    return true;
}

bool CAlfaDoc::CreateWireSolid(unsigned long path_id, double radius) {
    const CAlfaObject* path = FindObjectById(path_id);
    const bool supported = dynamic_cast<const CPolyline*>(path)
        || dynamic_cast<const CSmartLine*>(path)
        || dynamic_cast<const CBSpline*>(path);
    if (!path || !supported || radius <= 0.0) return false;
    TopoDS_Shape shape = BuildWireSolidShape(*path, radius);
    if (shape.IsNull()) return false;
    auto solid = std::make_unique<CSolid>(shape);
    solid->SetName("Wire");
    solid->SetParametricOperation(0, "SolidWireTool", "Wire",
                                  {{"radius", radius}, {"profile.id", static_cast<double>(path_id)}});
    if (!solid->ReBuldMesh()) return false;
    AddObject(std::move(solid));
    return true;
}

bool CAlfaDoc::CreatePolyhedronSolid(unsigned long profile_id,
                                     int axis_index,
                                     int turns)
{
    const auto* profile =
        dynamic_cast<const CSmartLine*>(FindObjectById(profile_id));
    if (!profile) {
        return false;
    }

    const std::vector<CPoint3d> sketch_points =
        profile->GetProfilePointsWorld();
    std::vector<Vec3> profile_points;
    profile_points.reserve(sketch_points.size());
    for (const CPoint3d& point : sketch_points) {
        profile_points.push_back({
            static_cast<float>(point.x),
            static_cast<float>(point.y),
            static_cast<float>(point.z)});
    }
    const SketchCoordinateSystem& system = profile->GetCoordinateSystem();
    const Vec3 axis_origin{
        static_cast<float>(system.origin.x),
        static_cast<float>(system.origin.y),
        static_cast<float>(system.origin.z)};
    axis_index = std::clamp(axis_index, 0, 2);
    turns = std::max(3, turns);

    const Vec3 axis_direction = revolve_axis_direction(axis_index);
    TopoDS_Shape shape;
    bool shape_built = false;
    if (profile->IsClosed()) {
        TopoDS_Face profile_face;
        Vec3 profile_normal{};
        shape_built = BuildSketchProfileFace(
                *profile, profile_face, profile_normal)
            && BuildPolyhedronShapeFromProfileFace(
                profile_face,
                axis_origin,
                axis_direction,
                turns,
                shape);
    } else {
        TopoDS_Wire profile_wire;
        shape_built = BuildOpenSketchProfileWire(
                *profile, profile_wire)
            && BuildPolyhedronShapeFromOpenProfileWire(
                profile_wire,
                axis_origin,
                axis_direction,
                turns,
                shape);
        if (!shape_built) {
            TopoDS_Face axis_closed_face;
            Vec3 profile_normal{};
            shape_built = BuildSketchRevolveProfileFace(
                    *profile,
                    axis_origin,
                    axis_direction,
                    axis_closed_face,
                    profile_normal)
                && BuildPolyhedronShapeFromProfileFace(
                    axis_closed_face,
                    axis_origin,
                    axis_direction,
                    turns,
                    shape);
        }
    }
    bool has_exact_curves = profile->GetNumFillets() > 0;
    for (std::size_t line_index = 0;
         !has_exact_curves && line_index < profile->GetNumLines();
         ++line_index) {
        const CLinkLine* line = profile->GetLine(line_index);
        has_exact_curves = line
            && (line->GetType() == LinkLineType::Bezier
                || line->GetType() == LinkLineType::Arc);
    }
    if (!shape_built && has_exact_curves) {
        return false;
    }
    if (!shape_built
        && !BuildPolyhedronShape(
            profile_points,
            profile->IsClosed(),
            axis_origin,
            axis_direction,
            turns,
            shape)) {
        return false;
    }

    auto solid = std::make_unique<CSolid>(shape);
    solid->SetName("Polyhedron");
    AssignDefaultMaterial(*solid);
    AssignObjectToWorkLayer(*solid);
    if (!solid->ReBuldMesh()) {
        return false;
    }
    solid->SetParametricOperation(
        0,
        "SolidPolyhedronTool",
        "Polyhedron",
        {
            {"turns", static_cast<double>(turns)},
            {"axis", static_cast<double>(axis_index)},
            {"profile.id", static_cast<double>(profile_id)}
        });
    AddObject(std::move(solid));
    return true;
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

    EnsureObjectId(*mesh);
    AssignDefaultMaterial(*mesh);
    AssignObjectToWorkLayer(*mesh);
    objects_.push_back(std::move(mesh));
    selected_object_index_ = objects_.size() - 1;
    selected_object_indices_ = {selected_object_index_};
    has_selected_object_ = true;
    ClearPointSelection();
    return true;
}

bool CAlfaDoc::BeginLiveExtrudeSelectedPolyline(double distance, bool reverse, double taper_angle_degrees) {
    CPolyline* polyline = GetSelectedPolyline();
    CSmartLine* sketch = GetSelectedSketch();
    CAlfaObject* profile_object = sketch ? static_cast<CAlfaObject*>(sketch) : polyline;
    if ((!polyline && !sketch) || !profile_object) {
        return false;
    }
    EnsureObjectId(*profile_object);

    TopoDS_Face profile_face;
    Vec3 normal{};
    const bool profile_built = sketch
        ? BuildSketchProfileFace(*sketch, profile_face, normal)
        : build_profile_face_from_polyline(*polyline, profile_face, normal);
    if (!profile_built) {
        return false;
    }

    live_polyline_extrude_ = std::make_unique<LivePolylineExtrudeData>();
    live_polyline_extrude_->profile_face = profile_face;
    live_polyline_extrude_->normal = normal;
    live_polyline_extrude_->profile_id = profile_object->m_id;
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
    live_polyline_extrude_->distance = distance;
    live_polyline_extrude_->reverse = reverse;
    live_polyline_extrude_->taper_angle_degrees = taper_angle_degrees;
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
        if (live_polyline_extrude_->has_solid && live_polyline_extrude_->solid_index < objects_.size() && objects_[live_polyline_extrude_->solid_index]) {
            solid->m_id = objects_[live_polyline_extrude_->solid_index]->m_id;
        }
        EnsureObjectId(*solid);
        AssignDefaultMaterial(*solid);
        if (live_polyline_extrude_->has_solid
            && live_polyline_extrude_->solid_index < objects_.size()
            && objects_[live_polyline_extrude_->solid_index]) {
            solid->m_LayerID = objects_[live_polyline_extrude_->solid_index]->m_LayerID;
        } else {
            AssignObjectToWorkLayer(*solid);
        }
        if (!solid->ReBuldMesh()) {
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
    const unsigned long profile_id = live_polyline_extrude_->profile_id;
    const double distance = live_polyline_extrude_->distance;
    const bool reverse = live_polyline_extrude_->reverse;
    const double taper_angle_degrees = live_polyline_extrude_->taper_angle_degrees;
    live_polyline_extrude_.reset();
    if (solid_index < objects_.size()) {
        if (auto* solid = dynamic_cast<CSolid*>(objects_[solid_index].get())) {
            solid->SetParametricOperation(0,
                                          "SolidExtrudeTool",
                                          "Extrude",
                                          {
                                              {"distance", distance},
                                              {"reverse", reverse ? 1.0 : 0.0},
                                              {"taper", taper_angle_degrees},
                                              {"profile.id", static_cast<double>(profile_id)}
                                          });
        }
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
    CSmartLine* sketch = GetSelectedSketch();
    CAlfaObject* profile = polyline
        ? static_cast<CAlfaObject*>(polyline)
        : static_cast<CAlfaObject*>(sketch);
    if (!profile) {
        return false;
    }
    EnsureObjectId(*profile);

    live_polyline_revolve_ = std::make_unique<LivePolylineRevolveData>();
    live_polyline_revolve_->profile_id = profile->m_id;
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

    live_polyline_revolve_->angle_degrees = angle_degrees;
    live_polyline_revolve_->axis_index = std::clamp(axis_index, 0, 2);
    try {
        if (live_polyline_revolve_->polyline_index >= objects_.size()) {
            return false;
        }

        const CAlfaObject* profile =
            objects_[live_polyline_revolve_->polyline_index].get();
        const auto* polyline = dynamic_cast<const CPolyline*>(profile);
        const auto* sketch = dynamic_cast<const CSmartLine*>(profile);
        if (!polyline && !sketch) {
            return false;
        }

        TopoDS_Shape profile_shape;
        Vec3 axis_origin{};
        Vec3 axis_direction = revolve_axis_direction(axis_index);
        const bool built = sketch
            ? build_revolve_profile_from_sketch(
                *sketch,
                axis_index,
                profile_shape,
                axis_origin,
                axis_direction)
            : build_revolve_profile_from_polyline(
                *polyline, axis_index, profile_shape, axis_origin);
        if (!built) {
            return false;
        }

        live_polyline_revolve_->profile_shape = profile_shape;
        live_polyline_revolve_->axis_origin = axis_origin;
        live_polyline_revolve_->axis_direction = axis_direction;

        TopoDS_Shape shape = make_polyline_revolve_shape(live_polyline_revolve_->profile_shape,
                                                         live_polyline_revolve_->axis_origin,
                                                         live_polyline_revolve_->axis_direction,
                                                         angle_degrees);
        if (shape.IsNull()) {
            return false;
        }

        auto solid = std::make_unique<CSolid>(shape);
        solid->SetName("Revolve Solid");
        if (live_polyline_revolve_->has_solid
            && live_polyline_revolve_->solid_index < objects_.size()
            && objects_[live_polyline_revolve_->solid_index]) {
            solid->m_id = objects_[live_polyline_revolve_->solid_index]->m_id;
        }
        EnsureObjectId(*solid);
        AssignDefaultMaterial(*solid);
        if (live_polyline_revolve_->has_solid
            && live_polyline_revolve_->solid_index < objects_.size()
            && objects_[live_polyline_revolve_->solid_index]) {
            solid->m_LayerID = objects_[live_polyline_revolve_->solid_index]->m_LayerID;
        } else {
            AssignObjectToWorkLayer(*solid);
        }
        if (!solid->ReBuldMesh()) {
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
    const unsigned long profile_id = live_polyline_revolve_->profile_id;
    const double angle_degrees = live_polyline_revolve_->angle_degrees;
    const int axis_index = live_polyline_revolve_->axis_index;
    live_polyline_revolve_.reset();
    if (solid_index < objects_.size()) {
        if (auto* solid = dynamic_cast<CSolid*>(objects_[solid_index].get())) {
            solid->SetParametricOperation(0,
                                          "SurfaceOfRevolution",
                                          "Revolve",
                                          {
                                              {"angle", angle_degrees},
                                              {"axis", static_cast<double>(axis_index)},
                                              {"profile.id", static_cast<double>(profile_id)}
                                          });
        }
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
        if (IsObjectSelectable(*objects_[index]) && objects_[index]->HitTest(point, tolerance)) {
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
        if (IsObjectSelectable(*objects_[index]) && objects_[index]->HitTest(point, tolerance)) {
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
        if (!IsObjectSelectable(*objects_[index]) || !objects_[index]->HitTest(point, tolerance)) {
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
        if (!IsObjectSelectable(*objects_[index]) || !objects_[index]->HitTest(point, tolerance)) {
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

bool CAlfaDoc::FindSolidEdgeAtScreen(
    DomPoint point,
    const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
    float tolerance,
    size_t& object_index,
    int& surface_index,
    int& edge_index,
    float* screen_distance) const {
    bool found = false;
    float best_distance = tolerance;
    for (size_t i = objects_.size(); i > 0; --i) {
        const size_t index = i - 1;
        const auto* solid = dynamic_cast<const CSolid*>(objects_[index].get());
        if (!solid || !IsObjectSelectable(*solid)) {
            continue;
        }

        int candidate_surface = -1;
        int candidate_edge = -1;
        float candidate_distance = best_distance;
        if (solid->HitTestEdgeScreen(
                point, world_to_screen, best_distance,
                candidate_surface, candidate_edge, &candidate_distance)) {
            found = true;
            best_distance = candidate_distance;
            object_index = index;
            surface_index = candidate_surface;
            edge_index = candidate_edge;
        }
    }

    if (found && screen_distance) {
        *screen_distance = best_distance;
    }
    return found;
}

bool CAlfaDoc::FindRotationAxisLineAtScreen(
    DomPoint point,
    const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
    float tolerance,
    Vec3& start,
    Vec3& end,
    float* screen_distance) const {
    bool found = false;
    float best_distance = tolerance;
    const auto consider_segment = [&](Vec3 candidate_start, Vec3 candidate_end) {
        const Vec3 direction = candidate_end - candidate_start;
        if (dot(direction, direction) <= 0.000001f) {
            return;
        }
        DomPoint screen_start{};
        DomPoint screen_end{};
        if (!world_to_screen(candidate_start, screen_start)
            || !world_to_screen(candidate_end, screen_end)) {
            return;
        }
        const float distance = distance_to_screen_segment(
            point, screen_start, screen_end);
        if (distance <= best_distance) {
            best_distance = distance;
            start = candidate_start;
            end = candidate_end;
            found = true;
        }
    };

    for (const ObjectPtr& object : objects_) {
        if (!object || !IsObjectSelectable(*object)) {
            continue;
        }

        if (const auto* polyline = dynamic_cast<const CPolyline*>(object.get())) {
            const auto& points = polyline->GetPoints();
            for (size_t i = 1; i < points.size(); ++i) {
                consider_segment(to_vec3(points[i - 1]), to_vec3(points[i]));
            }
            if (polyline->IsClosed() && points.size() > 2) {
                consider_segment(to_vec3(points.back()), to_vec3(points.front()));
            }
            continue;
        }

        if (const auto* sketch = dynamic_cast<const CSmartLine*>(object.get())) {
            for (size_t i = 0; i < sketch->GetNumLines(); ++i) {
                const CLinkLine* line = sketch->GetLine(i);
                if (!line || (line->GetType() != LinkLineType::Segment
                    && line->GetType() != LinkLineType::Horizontal
                    && line->GetType() != LinkLineType::Vertical)) {
                    continue;
                }
                consider_segment(
                    to_vec3(sketch->LocalToWorld(line->GetStart())),
                    to_vec3(sketch->LocalToWorld(line->GetEnd())));
            }
            continue;
        }

        const auto* solid = dynamic_cast<const CSolid*>(object.get());
        if (!solid) {
            continue;
        }
        for (int surface_index = 0;
             surface_index < solid->GetNumSurfaces();
             ++surface_index) {
            const CSurfaceFace* surface = solid->GetSurfaceFace(surface_index);
            if (!surface) {
                continue;
            }
            for (int edge_index = 0;
                 edge_index < surface->GetEdgeCount();
                 ++edge_index) {
                const TopoDS_Edge* edge = surface->GetTopoEdge(edge_index);
                if (!edge || edge->IsNull()) {
                    continue;
                }
                try {
                    const BRepAdaptor_Curve curve(*edge);
                    if (curve.GetType() != GeomAbs_Line) {
                        continue;
                    }
                } catch (const Standard_Failure&) {
                    continue;
                }
                Vec3 edge_start{};
                Vec3 edge_end{};
                if (surface->GetEdgeEndpoints(
                        edge_index, edge_start, edge_end)) {
                    consider_segment(edge_start, edge_end);
                }
            }
        }
    }

    if (found && screen_distance) {
        *screen_distance = best_distance;
    }
    return found;
}

bool CAlfaDoc::SelectSolidEdgeAtScreen(DomPoint point,
                                       const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                       float tolerance,
                                       SelectionAction action) {
    size_t index = objects_.size();
    int surface_index = -1;
    int edge_index = -1;
    if (!FindSolidEdgeAtScreen(
            point, world_to_screen, tolerance,
            index, surface_index, edge_index)) {
        return false;
    }

    auto* solid = dynamic_cast<CSolid*>(objects_[index].get());
    if (!solid) {
        return false;
    }
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

bool CAlfaDoc::SelectSolidMeshAtScreen(DomPoint point,
                                       const std::function<bool(Vec3, DomPoint&, float&)>& project_world,
                                       SelectionAction action) {
    CSolid* hit_solid = FindSolidAtScreen(point, project_world);
    if (!hit_solid) {
        return false;
    }
    size_t best_index = FindObjectIndexById(hit_solid->m_id);
    best_index = ResolveGroupSelectionIndex(best_index);

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
        if (!mesh || !IsObjectSelectable(*mesh)) {
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
    best_index = ResolveGroupSelectionIndex(best_index);

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
        if (!solid || !IsObjectSelectable(*solid)) {
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

bool CAlfaDoc::SelectEdgesOfSelectedFaces() {
    if (!HasSelectedSolidFace()) {
        return false;
    }

    const size_t solid_index = selected_face_object_index_;
    auto* solid = dynamic_cast<CSolid*>(objects_[solid_index].get());
    if (!solid) {
        return false;
    }

    const std::vector<int> face_indices = solid->GetSelectedFaceIndices();
    TopTools_MapOfShape selected_topology;
    solid->ClearSelectedEdge();

    for (int face_index : face_indices) {
        const CSurfaceFace* surface = solid->GetSurfaceFace(face_index);
        if (!surface) {
            continue;
        }
        for (int edge_index = 0; edge_index < surface->GetEdgeCount(); ++edge_index) {
            const TopoDS_Edge* edge = surface->GetTopoEdge(edge_index);
            if (!edge || edge->IsNull() || !selected_topology.Add(*edge)) {
                continue;
            }
            solid->AddSelectedEdge(face_index, edge_index);
        }
    }

    solid->ClearSelectedFace();
    selected_solid_face_indices_.clear();
    has_selected_solid_face_ = false;
    selected_face_object_index_ = 0;

    if (!solid->HasSelectedEdge()) {
        ClearSelection();
        return false;
    }

    selected_object_index_ = solid_index;
    active_object_index_ = solid_index;
    selected_object_indices_ = {solid_index};
    has_selected_object_ = true;
    ClearPointSelection();
    return true;
}

CSolid* CAlfaDoc::GetSelectedFaceSolid() {
    if (!HasSelectedSolidFace()) {
        return nullptr;
    }
    return dynamic_cast<CSolid*>(objects_[selected_face_object_index_].get());
}

const CSolid* CAlfaDoc::GetSelectedFaceSolid() const {
    if (!HasSelectedSolidFace()) {
        return nullptr;
    }
    return dynamic_cast<const CSolid*>(objects_[selected_face_object_index_].get());
}

bool CAlfaDoc::GetSelectedSolidFaceCenterAndNormal(Vec3& center, Vec3& normal) const {
    if (!has_selected_solid_face_ || selected_face_object_index_ >= objects_.size()) {
        return false;
    }
    const CSolid* solid = dynamic_cast<const CSolid*>(objects_[selected_face_object_index_].get());
    return solid && solid->HasSelectedFace() && solid->GetFaceCenterAndNormal(solid->GetSelectedFaceIndex(), center, normal);
}

bool CAlfaDoc::GetSelectedSolidFaceSketchPlane(Vec3& origin,
                                               Vec3& x_axis,
                                               Vec3& y_axis,
                                               Vec3& normal,
                                               unsigned long& body_id,
                                               int& face_index) const {
    if (!HasSelectedSolidFace() || selected_face_object_index_ >= objects_.size()) {
        return false;
    }
    const auto* solid = dynamic_cast<const CSolid*>(objects_[selected_face_object_index_].get());
    if (!solid || !solid->HasSelectedFace()) {
        return false;
    }
    face_index = solid->GetSelectedFaceIndex();
    body_id = solid->m_id;
    return GetSolidFaceSketchPlane(
        body_id,
        face_index,
        origin,
        x_axis,
        y_axis,
        normal);
}

bool CAlfaDoc::GetSolidFaceSketchPlane(unsigned long body_id,
                                       int face_index,
                                       Vec3& origin,
                                       Vec3& x_axis,
                                       Vec3& y_axis,
                                       Vec3& normal) const {
    const auto* solid = dynamic_cast<const CSolid*>(FindObjectById(body_id));
    if (!solid || face_index < 0) {
        return false;
    }
    const TopoDS_Face face = solid->GetTopoFace(face_index);
    if (face.IsNull()) {
        return false;
    }

    try {
        BRepAdaptor_Surface surface(face, true);
        if (surface.GetType() != GeomAbs_Plane) {
            return false;
        }
        if (!solid->GetFaceCenterAndNormal(face_index, origin, normal)) {
            return false;
        }
        normal = normalize(normal);
        if (dot(normal, normal) <= 0.000001f) {
            return false;
        }

        const gp_Dir plane_x = surface.Plane().Position().XDirection();
        x_axis = normalize(Vec3{
            static_cast<float>(plane_x.X()),
            static_cast<float>(plane_x.Y()),
            static_cast<float>(plane_x.Z())});
        x_axis = normalize(x_axis - normal * dot(x_axis, normal));
        if (dot(x_axis, x_axis) <= 0.000001f) {
            const Vec3 reference = std::abs(normal.z) < 0.9f
                ? Vec3{0.0f, 0.0f, 1.0f}
                : Vec3{1.0f, 0.0f, 0.0f};
            x_axis = normalize(cross(reference, normal));
        }
        y_axis = normalize(cross(normal, x_axis));
        if (dot(y_axis, y_axis) <= 0.000001f) {
            return false;
        }
        return body_id != 0;
    } catch (const Standard_Failure&) {
        return false;
    }
}

bool CAlfaDoc::UpdateAttachedSketches() {
    bool updated = false;
    for (const ObjectPtr& object : objects_) {
        auto* sketch = dynamic_cast<CSmartLine*>(object.get());
        if (!sketch || !sketch->HasFaceAttachment()) {
            continue;
        }
        const SketchFaceAttachment& attachment = sketch->GetFaceAttachment();
        const auto* solid = dynamic_cast<const CSolid*>(
            FindObjectById(attachment.body_id));
        if (!solid) {
            continue;
        }

        const SketchCoordinateSystem old_system =
            sketch->GetCoordinateSystem();
        Vec3 old_origin{
            static_cast<float>(old_system.origin.x),
            static_cast<float>(old_system.origin.y),
            static_cast<float>(old_system.origin.z)};
        Vec3 old_normal = normalize(Vec3{
            static_cast<float>(old_system.normal.x),
            static_cast<float>(old_system.normal.y),
            static_cast<float>(old_system.normal.z)});
        Vec3 old_x_axis = normalize(Vec3{
            static_cast<float>(old_system.x_axis.x),
            static_cast<float>(old_system.x_axis.y),
            static_cast<float>(old_system.x_axis.z)});
        if (dot(old_normal, old_normal) <= 0.000001f) {
            continue;
        }

        int best_face_index = -1;
        double best_score = std::numeric_limits<double>::max();
        Vec3 best_center{};
        Vec3 best_normal{};
        for (int face_index = 0;
             face_index < solid->GetNumSurfaces();
             ++face_index) {
            const TopoDS_Face face = solid->GetTopoFace(face_index);
            if (face.IsNull()) {
                continue;
            }
            try {
                BRepAdaptor_Surface surface(face, true);
                if (surface.GetType() != GeomAbs_Plane) {
                    continue;
                }
            } catch (const Standard_Failure&) {
                continue;
            }

            Vec3 center{};
            Vec3 normal{};
            if (!solid->GetFaceCenterAndNormal(
                    face_index, center, normal)) {
                continue;
            }
            normal = normalize(normal);
            double alignment = dot(normal, old_normal);
            if (std::fabs(alignment) < 0.999) {
                continue;
            }
            if (alignment < 0.0) {
                normal = normal * -1.0f;
            }

            const double plane_distance = std::fabs(
                static_cast<double>(dot(center - old_origin, normal)));
            double face_distance = plane_distance;
            try {
                const TopoDS_Vertex reference_vertex =
                    BRepBuilderAPI_MakeVertex(gp_Pnt(
                        old_origin.x,
                        old_origin.y,
                        old_origin.z)).Vertex();
                BRepExtrema_DistShapeShape distance(reference_vertex, face);
                distance.Perform();
                if (distance.IsDone() && distance.NbSolution() > 0) {
                    face_distance = distance.Value();
                }
            } catch (const Standard_Failure&) {
            }

            double score = plane_distance * 4.0 + face_distance;
            if (face_index == attachment.face_index) {
                score *= 0.999;
            }
            if (score < best_score) {
                best_score = score;
                best_face_index = face_index;
                best_center = center;
                best_normal = normal;
            }
        }
        if (best_face_index < 0) {
            continue;
        }

        const Vec3 origin = old_origin
            + best_normal * dot(best_center - old_origin, best_normal);
        Vec3 x_axis = old_x_axis
            - best_normal * dot(old_x_axis, best_normal);
        x_axis = normalize(x_axis);
        if (dot(x_axis, x_axis) <= 0.000001f) {
            const Vec3 reference = std::abs(best_normal.z) < 0.9f
                ? Vec3{0.0f, 0.0f, 1.0f}
                : Vec3{1.0f, 0.0f, 0.0f};
            x_axis = normalize(cross(reference, best_normal));
        }

        if (attachment.face_index != best_face_index) {
            sketch->SetFaceAttachment(
                attachment.body_id, best_face_index);
        }
        if (sketch->SetCoordinateSystem(
                CPoint3d(origin.x, origin.y, origin.z),
                CPoint3d(x_axis.x, x_axis.y, x_axis.z),
                CPoint3d(
                    best_normal.x,
                    best_normal.y,
                    best_normal.z))) {
            updated = true;
        }
    }
    return updated;
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
    solid->ReBuldMesh();
    live_extrude_->distance = distance;
    live_extrude_->created_surface_indices = solid->FindCreatedSurfaceIndices(live_extrude_->base_shape);
    active_object_index_ = live_extrude_->object_index;
    selected_face_object_index_ = 0;
    has_selected_solid_face_ = false;
    return true;
}

void CAlfaDoc::FinishLiveExtrudeSelectedSolidFace() {
    if (live_extrude_ && live_extrude_->object_index < objects_.size()
        && std::fabs(live_extrude_->distance) > 0.0001f) {
        if (auto* solid = dynamic_cast<CSolid*>(objects_[live_extrude_->object_index].get())) {
            solid->SetParametricOperation(
                solid->GetOperationTree().size(),
                "SolidExtrudeFace",
                "Extrude Face",
                {
                    {"distance", live_extrude_->distance},
                    {"taper", live_extrude_->taper_angle_degrees},
                    {"face.index", static_cast<double>(live_extrude_->face_index)}
                },
                live_extrude_->created_surface_indices);
        }
    }
    live_extrude_.reset();
    UpdateAttachedSketches();
    ClearSelection();
}

void CAlfaDoc::CancelLiveExtrudeSelectedSolidFace() {
    if (live_extrude_ && live_extrude_->object_index < objects_.size()) {
        if (auto* solid = dynamic_cast<CSolid*>(objects_[live_extrude_->object_index].get())) {
            solid->m_Shape = live_extrude_->base_shape;
            solid->ReBuldMesh();
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
        solid->ReBuldMesh();
        has_selected_solid_face_ = false;
        selected_face_object_index_ = 0;
        UpdateAttachedSketches();
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
    if (has_non_uniform_scale_operation(*solid)) {
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
    if (!solid || !IsObjectSelectable(*solid)) {
        return false;
    }

    int surface_index = draft_face_->face_index;
    int edge_index = -1;
    CSurfaceFace* surface = solid->GetSurfaceFace(surface_index);
    if (!surface || !surface->HitTestEdgeScreen(point, world_to_screen, tolerance, edge_index)) {
        return false;
    }

    const TopoDS_Edge* edge = surface->GetTopoEdge(edge_index);

    try {
        Vec3 edge_start{};
        Vec3 edge_end{};
        if (edge && !edge->IsNull()) {
            BRepAdaptor_Curve curve(*edge);
            const Standard_Real first = curve.FirstParameter();
            const Standard_Real last = curve.LastParameter();
            if (last <= first) {
                return false;
            }

            const gp_Pnt p0 = curve.Value(first);
            const gp_Pnt p1 = curve.Value(last);
            edge_start = {static_cast<float>(p0.X()), static_cast<float>(p0.Y()), static_cast<float>(p0.Z())};
            edge_end = {static_cast<float>(p1.X()), static_cast<float>(p1.Y()), static_cast<float>(p1.Z())};
            const Vec3 chord = edge_end - edge_start;
            const float chord_length = std::sqrt(dot(chord, chord));
            if (chord_length <= 0.000001f) {
                return false;
            }
            const Vec3 chord_dir = chord * (1.0f / chord_length);
            const float tolerance_3d = std::max(0.001f, chord_length * 0.001f);
            for (int sample = 1; sample < 6; ++sample) {
                const Standard_Real t = first + (last - first) * static_cast<Standard_Real>(sample) / 6.0;
                const gp_Pnt p = curve.Value(t);
                const Vec3 sample_point{static_cast<float>(p.X()), static_cast<float>(p.Y()), static_cast<float>(p.Z())};
                const Vec3 from_start = sample_point - edge_start;
                const Vec3 closest = edge_start + chord_dir * dot(from_start, chord_dir);
                if (std::sqrt(dot(sample_point - closest, sample_point - closest)) > tolerance_3d) {
                    return false;
                }
            }
        } else {
            if (!surface->GetEdgeEndpoints(edge_index, edge_start, edge_end)) {
                return false;
            }
        }

        Vec3 axis_point{(edge_start.x + edge_end.x) * 0.5f,
                        (edge_start.y + edge_end.y) * 0.5f,
                        (edge_start.z + edge_end.z) * 0.5f};
        Vec3 axis_dir{edge_end.x - edge_start.x,
                      edge_end.y - edge_start.y,
                      edge_end.z - edge_start.z};
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
        draft_face_->axis_edge_index = edge_index;
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
    solid->ReBuldMesh();
    draft_face_->angle_degrees = angle_degrees;
    draft_face_->created_surface_indices = solid->FindCreatedSurfaceIndices(draft_face_->base_shape);
    active_object_index_ = draft_face_->object_index;
    return true;
}

void CAlfaDoc::FinishLiveDraftFace() {
    if (draft_face_ && draft_face_->object_index < objects_.size()
        && draft_face_->face_index >= 0
        && draft_face_->axis_edge_index >= 0
        && std::fabs(draft_face_->angle_degrees) > 0.0001) {
        if (auto* solid = dynamic_cast<CSolid*>(objects_[draft_face_->object_index].get())) {
            solid->SetParametricOperation(
                solid->GetOperationTree().size(),
                "SolidDraft",
                "Draft Face",
                {
                    {"angle", draft_face_->angle_degrees},
                    {"face.index", static_cast<double>(draft_face_->face_index)},
                    {"axis.edge", static_cast<double>(draft_face_->axis_edge_index)}
                },
                draft_face_->created_surface_indices);
        }
    }
    draft_face_.reset();
    UpdateAttachedSketches();
    ClearSelection();
}

void CAlfaDoc::CancelLiveDraftFace() {
    if (draft_face_ && draft_face_->object_index < objects_.size()) {
        if (auto* solid = dynamic_cast<CSolid*>(objects_[draft_face_->object_index].get())) {
            solid->m_Shape = draft_face_->base_shape;
            solid->ReBuldMesh();
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
    if (!solid || !IsObjectSelectable(*solid)) {
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
    if (auto* rebuilt = dynamic_cast<CSolid*>(objects_[live_thick_solid_->object_index].get())) {
        live_thick_solid_->created_surface_indices =
            rebuilt->FindCreatedSurfaceIndices(live_thick_solid_->base_shape);
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
        if (auto* rebuilt = dynamic_cast<CSolid*>(objects_[live_thick_solid_->object_index].get())) {
            live_thick_solid_->created_surface_indices =
                rebuilt->FindCreatedSurfaceIndices(live_thick_solid_->base_shape);
        }
    }

    if (auto* solid = dynamic_cast<CSolid*>(objects_[live_thick_solid_->object_index].get())) {
        std::vector<ParametricParameterValue> parameters{
            {"thick", live_thick_solid_->thickness},
            {"face.count", static_cast<double>(live_thick_solid_->face_indices.size())}
        };
        for (size_t i = 0; i < live_thick_solid_->face_indices.size(); ++i) {
            parameters.push_back({
                "face." + std::to_string(i) + ".index",
                static_cast<double>(live_thick_solid_->face_indices[i])
            });
        }
        solid->SetParametricOperation(solid->GetOperationTree().size(),
                                      "ThickSolidTool",
                                      "Thick Solid",
                                      std::move(parameters),
                                      live_thick_solid_->created_surface_indices);
    }

    selected_object_index_ = live_thick_solid_->object_index;
    selected_object_indices_.clear();
    active_object_index_ = live_thick_solid_->object_index;
    selected_face_object_index_ = 0;
    has_selected_object_ = false;
    has_selected_solid_face_ = false;
    ClearPointSelection();
    live_thick_solid_.reset();
    UpdateAttachedSketches();
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
        if (polyline && IsObjectSelectable(*polyline) && polyline->HitTest(point, tolerance)) {
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
        if (!object || !IsObjectSelectable(*object)) {
            continue;
        }

        bool hit = false;
        if (const auto* polyline = dynamic_cast<const CPolyline*>(object)) {
            hit = hit_test_polyline_screen(*polyline, point, world_to_screen, tolerance);
        } else if (const auto* sketch = dynamic_cast<const CSmartLine*>(object)) {
            hit = hit_test_sketch_screen(*sketch, point, world_to_screen, tolerance);
        } else if (const auto* spline = dynamic_cast<const CBSpline*>(object)) {
            hit = hit_test_bspline_screen(*spline, point, world_to_screen, tolerance);
        }

        if (!hit) {
            continue;
        }
        const size_t selected_index = ResolveGroupSelectionIndex(index);

        if (action == SelectionAction::Remove) {
            auto existing = std::find(selected_object_indices_.begin(), selected_object_indices_.end(), selected_index);
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
            if (!IsObjectSelected(selected_index)) {
                selected_object_indices_.push_back(selected_index);
            }
        } else {
            selected_object_indices_ = {selected_index};
        }

        for (ObjectPtr& object : objects_) {
            if (auto* solid = dynamic_cast<CSolid*>(object.get())) {
                solid->ClearSelectedEdge();
                solid->ClearSelectedFace();
            }
        }
        selected_object_index_ = selected_index;
        active_object_index_ = selected_index;
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
        if (!objects_[object_index] || !IsObjectSelectable(*objects_[object_index])) {
            continue;
        }
        if (const auto* polyline = dynamic_cast<const CPolyline*>(objects_[object_index].get())) {
            scan_points(object_index, polyline->GetPoints());
        } else if (const auto* spline = dynamic_cast<const CBSpline*>(objects_[object_index].get())) {
            scan_points(object_index, spline->GetPoints());
        } else if (const auto* sketch = dynamic_cast<const CSmartLine*>(objects_[object_index].get())) {
            std::vector<CPoint3d> nodes;
            nodes.reserve(sketch->GetNodeCount());
            for (size_t node_index = 0; node_index < sketch->GetNodeCount(); ++node_index) {
                nodes.push_back(sketch->GetNodeWorld(node_index));
            }
            scan_points(object_index, nodes);
        }
    }

    if (!found) {
        if (action == SelectionAction::Replace) {
            ClearPointSelection();
        }
        return false;
    }

    const std::pair<size_t, size_t> point_ref{found_object_index, found_point_index};
    if (action == SelectionAction::Replace) {
        selected_curve_points_ = {point_ref};
    } else if (action == SelectionAction::Add) {
        if (std::find(selected_curve_points_.begin(), selected_curve_points_.end(), point_ref)
            == selected_curve_points_.end()) {
            selected_curve_points_.push_back(point_ref);
        }
    } else {
        const auto existing = std::find(selected_curve_points_.begin(), selected_curve_points_.end(), point_ref);
        if (existing != selected_curve_points_.end()) selected_curve_points_.erase(existing);
    }
    if (selected_curve_points_.empty()) {
        ClearSelection();
        return true;
    }
    selected_object_indices_.clear();
    for (const auto& selected : selected_curve_points_) {
        if (!IsObjectSelected(selected.first)) selected_object_indices_.push_back(selected.first);
    }
    selected_object_index_ = selected_curve_points_.front().first;
    active_object_index_ = selected_object_index_;
    has_selected_object_ = true;
    selected_point_index_ = selected_curve_points_.front().second;
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
        if (object_index >= objects_.size() || !objects_[object_index] || !IsObjectSelectable(*objects_[object_index])) {
            continue;
        }

        const std::vector<CPoint3d>* points = nullptr;
        if (const auto* polyline = dynamic_cast<const CPolyline*>(objects_[object_index].get())) {
            points = &polyline->GetPoints();
        } else if (const auto* spline = dynamic_cast<const CBSpline*>(objects_[object_index].get())) {
            points = &spline->GetPoints();
        } else if (const auto* sketch = dynamic_cast<const CSmartLine*>(objects_[object_index].get())) {
            if (point_index < sketch->GetNodeCount()) {
                const CPoint3d point_world = sketch->GetNodeWorld(point_index);
                DomPoint screen{};
                if (world_to_screen(to_vec3(point_world), screen)) {
                    const float dx = static_cast<float>(point.x - screen.x);
                    const float dy = static_cast<float>(point.y - screen.y);
                    const float distance = std::sqrt(dx * dx + dy * dy);
                    if (distance <= best_distance) {
                        best_distance = distance;
                        selected_point = point_world;
                        found = true;
                    }
                }
            }
            continue;
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
        if (!objects_[object_index] || !IsObjectSelectable(*objects_[object_index])) {
            continue;
        }
        if (const auto* polyline = dynamic_cast<const CPolyline*>(objects_[object_index].get())) {
            scan_points(object_index, polyline->GetPoints());
        } else if (const auto* spline = dynamic_cast<const CBSpline*>(objects_[object_index].get())) {
            scan_points(object_index, spline->GetPoints());
        } else if (const auto* sketch = dynamic_cast<const CSmartLine*>(objects_[object_index].get())) {
            std::vector<CPoint3d> nodes;
            nodes.reserve(sketch->GetNodeCount());
            for (size_t node_index = 0; node_index < sketch->GetNodeCount(); ++node_index) {
                nodes.push_back(sketch->GetNodeWorld(node_index));
            }
            scan_points(object_index, nodes);
        }
    }

    if (action == SelectionAction::Add) {
        for (const auto& point_ref : found_points) {
            if (std::find(selected_curve_points_.begin(), selected_curve_points_.end(), point_ref) == selected_curve_points_.end()) {
                selected_curve_points_.push_back(point_ref);
            }
        }
    } else if (action == SelectionAction::Remove) {
        for (const auto& point_ref : found_points) {
            const auto existing = std::find(selected_curve_points_.begin(), selected_curve_points_.end(), point_ref);
            if (existing != selected_curve_points_.end()) {
                selected_curve_points_.erase(existing);
            }
        }
    } else {
        selected_curve_points_ = std::move(found_points);
    }

    if (selected_curve_points_.empty()) {
        ClearPointSelection();
        return false;
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

bool CAlfaDoc::SelectAllPointsOfSelectedCurve() {
    if (!HasSelection() || selected_object_index_ >= objects_.size()
        || !objects_[selected_object_index_]) {
        return false;
    }

    size_t point_count = 0;
    if (const auto* polyline = dynamic_cast<const CPolyline*>(
            objects_[selected_object_index_].get())) {
        point_count = polyline->GetPointCount();
    } else if (const auto* spline = dynamic_cast<const CBSpline*>(
                   objects_[selected_object_index_].get())) {
        point_count = spline->GetPointCount();
    } else {
        return false;
    }
    if (point_count == 0) {
        return false;
    }

    selected_curve_points_.clear();
    selected_curve_points_.reserve(point_count);
    for (size_t point_index = 0; point_index < point_count; ++point_index) {
        selected_curve_points_.emplace_back(selected_object_index_, point_index);
    }
    selected_object_indices_ = {selected_object_index_};
    active_object_index_ = selected_object_index_;
    has_selected_object_ = true;
    selected_point_index_ = 0;
    has_selected_point_ = true;
    return true;
}

CSolid* CAlfaDoc::FindSolidAtScreen(
    DomPoint point,
    const std::function<bool(Vec3, DomPoint&, float&)>& project_world) {
    CSolid* best_solid = nullptr;
    float best_depth = std::numeric_limits<float>::max();
    for (const ObjectPtr& object : objects_) {
        auto* solid = dynamic_cast<CSolid*>(object.get());
        if (!solid || !IsObjectSelectable(*solid)) {
            continue;
        }
        float depth = 0.0f;
        if (solid->HitTestMeshScreen(point, project_world, depth) && depth < best_depth) {
            best_solid = solid;
            best_depth = depth;
        }
    }
    return best_solid;
}

bool CAlfaDoc::SelectObjectsInScreenRect(
    DomRect rect,
    const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
    SelectionAction action) {
    // Dom-3D convention requested for Select By Rectangle:
    // left-to-right is crossing, right-to-left is fully enclosed.
    const bool crossing = rect.right >= rect.left;
    const ScreenRectBounds screen_rect = normalize_screen_rect(rect);

    const auto bounds_match = [&](const CAlfaObject& object) {
        Vec3 min_point{};
        Vec3 max_point{};
        if (!object.GetBounds(min_point, max_point)) {
            return false;
        }
        bool all_inside = true;
        bool projected_any = false;
        int projected_left = std::numeric_limits<int>::max();
        int projected_right = std::numeric_limits<int>::min();
        int projected_top = std::numeric_limits<int>::max();
        int projected_bottom = std::numeric_limits<int>::min();
        for (int x = 0; x < 2; ++x) {
            for (int y = 0; y < 2; ++y) {
                for (int z = 0; z < 2; ++z) {
                    const Vec3 corner{
                        x == 0 ? min_point.x : max_point.x,
                        y == 0 ? min_point.y : max_point.y,
                        z == 0 ? min_point.z : max_point.z
                    };
                    DomPoint screen{};
                    const bool projected = world_to_screen(corner, screen);
                    projected_any = projected_any || projected;
                    all_inside = all_inside
                        && projected && screen_point_inside(screen, screen_rect);
                    if (projected) {
                        projected_left = std::min(projected_left, screen.x);
                        projected_right = std::max(projected_right, screen.x);
                        projected_top = std::min(projected_top, screen.y);
                        projected_bottom = std::max(projected_bottom, screen.y);
                    }
                }
            }
        }
        if (!crossing) {
            return all_inside;
        }
        return projected_any
            && projected_right >= screen_rect.left
            && projected_left <= screen_rect.right
            && projected_bottom >= screen_rect.top
            && projected_top <= screen_rect.bottom;
    };

    const auto object_matches = [&](const CAlfaObject& object) {
        if (const auto* solid = dynamic_cast<const CSolid*>(&object)) {
            bool has_mesh = false;
            bool matches = !crossing;
            for (int surface_index = 0; surface_index < solid->GetNumSurfaces(); ++surface_index) {
                const CSurfaceFace* surface = solid->GetSurfaceFace(surface_index);
                if (!surface || !surface->pMesh3D
                    || surface->pMesh3D->GetVertices().empty()) {
                    continue;
                }
                has_mesh = true;
                const bool surface_matches = mesh_matches_screen_rect(
                    *surface->pMesh3D, screen_rect, crossing, world_to_screen);
                matches = crossing
                    ? matches || surface_matches
                    : matches && surface_matches;
                if (crossing && matches) {
                    break;
                }
            }
            return has_mesh ? matches : bounds_match(object);
        }
        if (const auto* mesh = dynamic_cast<const CMesh3D*>(&object)) {
            return mesh_matches_screen_rect(
                *mesh, screen_rect, crossing, world_to_screen);
        }
        if (const auto* polyline = dynamic_cast<const CPolyline*>(&object)) {
            std::vector<Vec3> points;
            const std::vector<CPoint3d> rounded_points = polyline->GetRoundedPathPoints();
            points.reserve(rounded_points.size());
            for (const CPoint3d& point : rounded_points) {
                points.push_back(to_vec3(point));
            }
            return projected_points_match_rect(
                points, screen_rect, crossing, world_to_screen, false);
        }
        if (const auto* spline = dynamic_cast<const CBSpline*>(&object)) {
            std::vector<Vec3> points;
            constexpr int sample_count = 64;
            points.reserve(sample_count + 1);
            for (int i = 0; i <= sample_count; ++i) {
                points.push_back(to_vec3(spline->Evaluate(
                    static_cast<float>(i) / static_cast<float>(sample_count))));
            }
            return projected_points_match_rect(
                points, screen_rect, crossing, world_to_screen, spline->IsClosed());
        }
        return bounds_match(object);
    };

    std::vector<size_t> found_indices;
    for (size_t object_index = 0; object_index < objects_.size(); ++object_index) {
        const CAlfaObject* object = objects_[object_index].get();
        if (!object || !IsObjectSelectable(*object)) {
            continue;
        }

        if (object_matches(*object)) {
            found_indices.push_back(object_index);
        }
    }

    std::set<unsigned long> grouped_element_ids;
    std::set<unsigned long> visited_group_ids;
    std::function<void(const CGroup&)> collect_group_elements;
    collect_group_elements = [&](const CGroup& group) {
        if (!visited_group_ids.insert(group.m_id).second) {
            return;
        }
        for (unsigned long id : group.GetElementIds()) {
            grouped_element_ids.insert(id);
            if (const auto* child_group = dynamic_cast<const CGroup*>(FindObjectById(id))) {
                collect_group_elements(*child_group);
            }
        }
    };
    for (size_t index : found_indices) {
        if (const auto* group = dynamic_cast<const CGroup*>(objects_[index].get())) {
            collect_group_elements(*group);
        }
    }
    found_indices.erase(
        std::remove_if(found_indices.begin(), found_indices.end(), [&](size_t index) {
            return !dynamic_cast<const CGroup*>(objects_[index].get())
                && grouped_element_ids.count(objects_[index]->m_id) > 0;
        }),
        found_indices.end());

    const std::vector<size_t> previous_selection = selected_object_indices_;
    if (action == SelectionAction::Add) {
        for (size_t index : found_indices) {
            if (!IsObjectSelected(index)) {
                selected_object_indices_.push_back(index);
            }
        }
    } else if (action == SelectionAction::Remove) {
        for (size_t index : found_indices) {
            const auto existing = std::find(selected_object_indices_.begin(), selected_object_indices_.end(), index);
            if (existing != selected_object_indices_.end()) {
                selected_object_indices_.erase(existing);
            }
        }
    } else {
        selected_object_indices_ = found_indices;
    }

    for (ObjectPtr& object : objects_) {
        if (auto* solid = dynamic_cast<CSolid*>(object.get())) {
            solid->ClearSelectedEdge();
            solid->ClearSelectedFace();
        }
    }
    selected_solid_face_indices_.clear();
    has_selected_solid_face_ = false;
    ClearPointSelection();

    if (selected_object_indices_.empty()) {
        selected_object_index_ = 0;
        has_selected_object_ = false;
    } else {
        selected_object_index_ = selected_object_indices_.back();
        active_object_index_ = selected_object_index_;
        has_selected_object_ = true;
    }
    return selected_object_indices_ != previous_selection;
}

bool CAlfaDoc::SelectSolidFacesInScreenRect(
    DomRect rect,
    const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
    SelectionAction action) {
    const bool crossing = rect.right >= rect.left;
    const ScreenRectBounds screen_rect = normalize_screen_rect(rect);

    size_t found_solid_index = objects_.size();
    std::vector<int> found_faces;
    for (size_t object_index = 0; object_index < objects_.size(); ++object_index) {
        const auto* solid = dynamic_cast<const CSolid*>(objects_[object_index].get());
        if (!solid || !IsObjectSelectable(*solid)) {
            continue;
        }

        std::vector<int> solid_faces;
        for (int surface_index = 0; surface_index < solid->GetNumSurfaces(); ++surface_index) {
            const CSurfaceFace* surface = solid->GetSurfaceFace(surface_index);
            if (!surface || !surface->pMesh3D || surface->pMesh3D->GetVertices().empty()) {
                continue;
            }
            if (mesh_matches_screen_rect(
                    *surface->pMesh3D, screen_rect, crossing, world_to_screen)) {
                solid_faces.push_back(surface_index);
            }
        }
        if (!solid_faces.empty()) {
            found_solid_index = object_index;
            found_faces = std::move(solid_faces);
        }
    }

    if (found_solid_index >= objects_.size()) {
        if (action == SelectionAction::Replace) {
            ClearSelection();
        }
        return false;
    }

    auto* solid = dynamic_cast<CSolid*>(objects_[found_solid_index].get());
    if (!solid) {
        return false;
    }
    if (action == SelectionAction::Remove) {
        if (!has_selected_solid_face_ || selected_face_object_index_ != found_solid_index) {
            return false;
        }
        for (int face_index : found_faces) {
            solid->RemoveSelectedFace(face_index);
            const auto existing = std::find(selected_solid_face_indices_.begin(), selected_solid_face_indices_.end(), face_index);
            if (existing != selected_solid_face_indices_.end()) {
                selected_solid_face_indices_.erase(existing);
            }
        }
    } else {
        if (action == SelectionAction::Replace
            || !has_selected_solid_face_
            || selected_face_object_index_ != found_solid_index) {
            for (ObjectPtr& object : objects_) {
                if (auto* other_solid = dynamic_cast<CSolid*>(object.get())) {
                    other_solid->ClearSelectedEdge();
                    other_solid->ClearSelectedFace();
                }
            }
            selected_solid_face_indices_.clear();
        }
        for (int face_index : found_faces) {
            solid->AddSelectedFace(face_index);
            if (std::find(selected_solid_face_indices_.begin(), selected_solid_face_indices_.end(), face_index)
                == selected_solid_face_indices_.end()) {
                selected_solid_face_indices_.push_back(face_index);
            }
        }
    }

    selected_face_object_index_ = found_solid_index;
    active_object_index_ = found_solid_index;
    selected_object_index_ = 0;
    selected_object_indices_.clear();
    has_selected_object_ = false;
    has_selected_solid_face_ = solid->HasSelectedFace();
    if (!has_selected_solid_face_) {
        selected_solid_face_indices_.clear();
    }
    ClearPointSelection();
    return true;
}

bool CAlfaDoc::SelectSolidEdgesInScreenRect(
    DomRect rect,
    const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
    SelectionAction action) {
    const bool crossing = rect.right >= rect.left;
    const ScreenRectBounds screen_rect = normalize_screen_rect(rect);

    size_t found_solid_index = objects_.size();
    std::vector<std::pair<int, int>> found_edges;
    for (size_t object_index = 0; object_index < objects_.size(); ++object_index) {
        const auto* solid = dynamic_cast<const CSolid*>(objects_[object_index].get());
        if (!solid || !IsObjectSelectable(*solid)) {
            continue;
        }

        std::vector<std::pair<int, int>> solid_edges;
        for (int surface_index = 0; surface_index < solid->GetNumSurfaces(); ++surface_index) {
            const CSurfaceFace* surface = solid->GetSurfaceFace(surface_index);
            if (!surface) {
                continue;
            }
            for (int edge_index = 0; edge_index < surface->GetEdgeCount(); ++edge_index) {
                std::vector<Vec3> edge_points;
                bool inside = false;
                if (surface->GetEdgePolylinePoints(edge_index, edge_points)) {
                    inside = projected_points_match_rect(
                        edge_points, screen_rect, crossing, world_to_screen);
                } else {
                    Vec3 start{};
                    Vec3 end{};
                    if (surface->GetEdgeEndpoints(edge_index, start, end)) {
                        inside = projected_points_match_rect(
                            {start, end}, screen_rect, crossing, world_to_screen);
                    }
                }
                if (inside) {
                    solid_edges.emplace_back(surface_index, edge_index);
                }
            }
        }
        if (!solid_edges.empty()) {
            found_solid_index = object_index;
            found_edges = std::move(solid_edges);
        }
    }

    if (found_solid_index >= objects_.size()) {
        if (action == SelectionAction::Replace) {
            ClearSelection();
        }
        return false;
    }

    auto* solid = dynamic_cast<CSolid*>(objects_[found_solid_index].get());
    if (!solid) {
        return false;
    }
    if (action == SelectionAction::Remove) {
        if (!HasSelection() || selected_object_index_ != found_solid_index) {
            return false;
        }
        for (const auto& edge : found_edges) {
            solid->RemoveSelectedEdge(edge.first, edge.second);
        }
    } else {
        if (action == SelectionAction::Replace
            || !HasSelection()
            || selected_object_index_ != found_solid_index) {
            for (ObjectPtr& object : objects_) {
                if (auto* other_solid = dynamic_cast<CSolid*>(object.get())) {
                    other_solid->ClearSelectedEdge();
                    other_solid->ClearSelectedFace();
                }
            }
        }
        for (const auto& edge : found_edges) {
            solid->AddSelectedEdge(edge.first, edge.second);
        }
    }

    if (!solid->HasSelectedEdge()) {
        ClearSelection();
        return true;
    }
    selected_object_index_ = found_solid_index;
    active_object_index_ = found_solid_index;
    selected_object_indices_ = {found_solid_index};
    has_selected_object_ = true;
    selected_solid_face_indices_.clear();
    has_selected_solid_face_ = false;
    ClearPointSelection();
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

    const auto scan_sketch = [&](size_t current_object_index, const CSmartLine& sketch) {
        for (size_t line_index = 0; line_index < sketch.GetNumLines(); ++line_index) {
            const CLinkLine* line = sketch.GetLine(line_index);
            if (!line) {
                continue;
            }
            const CPoint3d world_point = sketch.LocalToWorld(line->GetEnd());
            DomPoint screen{};
            if (!world_to_screen(to_vec3(world_point), screen)) {
                continue;
            }
            const float dx = static_cast<float>(point.x - screen.x);
            const float dy = static_cast<float>(point.y - screen.y);
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance <= best_distance) {
                best_distance = distance;
                object_index = current_object_index;
                point_index = line_index;
                found = true;
            }
        }
    };

    if (has_selected_object_ && selected_object_index_ < objects_.size()) {
        if (const auto* polyline = dynamic_cast<const CPolyline*>(objects_[selected_object_index_].get())) {
            scan_polyline(selected_object_index_, *polyline);
        } else if (const auto* sketch = dynamic_cast<const CSmartLine*>(objects_[selected_object_index_].get())) {
            scan_sketch(selected_object_index_, *sketch);
        }
    }

    if (!found) {
        for (size_t i = objects_.size(); i > 0; --i) {
            const size_t current_object_index = i - 1;
            if (!objects_[current_object_index] || !IsObjectSelectable(*objects_[current_object_index])) {
                continue;
            }
            if (const auto* polyline = dynamic_cast<const CPolyline*>(objects_[current_object_index].get())) {
                scan_polyline(current_object_index, *polyline);
            } else if (const auto* sketch = dynamic_cast<const CSmartLine*>(objects_[current_object_index].get())) {
                scan_sketch(current_object_index, *sketch);
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
    ClearTransformGizmoOrigin();
    ClearPointSelection();
}

bool CAlfaDoc::SelectObjectById(unsigned long object_id, SelectionAction action) {
    const size_t index = FindObjectIndexById(object_id);
    if (index >= objects_.size() || !objects_[index]) {
        return false;
    }

    if (action == SelectionAction::Remove) {
        const auto found = std::find(
            selected_object_indices_.begin(), selected_object_indices_.end(), index);
        if (found == selected_object_indices_.end()) {
            return false;
        }
        selected_object_indices_.erase(found);
        if (selected_object_indices_.empty()) {
            ClearSelection();
        } else {
            selected_object_index_ = selected_object_indices_.back();
            active_object_index_ = selected_object_index_;
            has_selected_object_ = true;
        }
        return true;
    }

    if (action == SelectionAction::Replace) {
        ClearSelection();
    } else if (IsObjectSelected(index)) {
        return true;
    }
    selected_object_index_ = index;
    active_object_index_ = index;
    selected_object_indices_.push_back(index);
    has_selected_object_ = true;
    ClearPointSelection();
    return true;
}

void CAlfaDoc::ClearPointSelection() {
    selected_point_index_ = 0;
    has_selected_point_ = false;
    selected_curve_points_.clear();
}

bool CAlfaDoc::HasSelection() const {
    return has_selected_object_ && !selected_object_indices_.empty() && selected_object_index_ < objects_.size();
}

bool CAlfaDoc::ExpandSelectedGroups() {
    if (!HasSelection()) {
        return false;
    }

    std::set<std::string> selected_groups;
    for (size_t index : selected_object_indices_) {
        if (index < objects_.size() && objects_[index] && !objects_[index]->GetGroupName().empty()) {
            selected_groups.insert(objects_[index]->GetGroupName());
        }
    }

    if (selected_groups.empty()) {
        return false;
    }

    bool expanded = false;
    for (size_t index = 0; index < objects_.size(); ++index) {
        if (!objects_[index]
            || selected_groups.find(objects_[index]->GetGroupName()) == selected_groups.end()
            || std::find(selected_object_indices_.begin(), selected_object_indices_.end(), index)
                != selected_object_indices_.end()) {
            continue;
        }
        selected_object_indices_.push_back(index);
        expanded = true;
    }
    return expanded;
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

size_t CAlfaDoc::SelectAllVisibleObjects() {
    selected_object_indices_.clear();
    selected_solid_face_indices_.clear();
    ClearPointSelection();

    for (ObjectPtr& object : objects_) {
        if (auto* solid = dynamic_cast<CSolid*>(object.get())) {
            solid->ClearSelectedEdge();
            solid->ClearSelectedFace();
        }
    }

    for (size_t index = 0; index < objects_.size(); ++index) {
        if (objects_[index] && IsObjectVisible(*objects_[index])) {
            selected_object_indices_.push_back(index);
        }
    }

    if (selected_object_indices_.empty()) {
        has_selected_object_ = false;
        selected_object_index_ = 0;
        active_object_index_ = 0;
        return 0;
    }

    selected_object_index_ = selected_object_indices_.back();
    active_object_index_ = selected_object_index_;
    has_selected_object_ = true;
    return selected_object_indices_.size();
}

size_t CAlfaDoc::GetSelectedObjectIndex() const {
    return selected_object_index_;
}

size_t CAlfaDoc::GetSelectedPointIndex() const {
    return selected_point_index_;
}

bool CAlfaDoc::IsObjectSelected(size_t index) const {
    if (index == hidden_selection_highlight_index_) {
        return false;
    }
    return std::find(selected_object_indices_.begin(), selected_object_indices_.end(), index) != selected_object_indices_.end();
}

void CAlfaDoc::SetObjectSelectionHighlightHidden(size_t index, bool hidden) {
    hidden_selection_highlight_index_ = hidden ? index : static_cast<size_t>(-1);
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

CSmartLine* CAlfaDoc::GetSelectedSketch() {
    return dynamic_cast<CSmartLine*>(GetSelectedObject());
}

const CSmartLine* CAlfaDoc::GetSelectedSketch() const {
    return dynamic_cast<const CSmartLine*>(GetSelectedObject());
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

CAlfaObject* CAlfaDoc::FindObjectById(unsigned long id) {
    if (id == 0) {
        return nullptr;
    }
    for (const ObjectPtr& object : objects_) {
        if (object && object->m_id == id) {
            return object.get();
        }
    }
    return nullptr;
}

const CAlfaObject* CAlfaDoc::FindObjectById(unsigned long id) const {
    if (id == 0) {
        return nullptr;
    }
    for (const ObjectPtr& object : objects_) {
        if (object && object->m_id == id) {
            return object.get();
        }
    }
    return nullptr;
}

size_t CAlfaDoc::FindObjectIndexById(unsigned long id) const {
    if (id == 0) {
        return objects_.size();
    }
    for (size_t i = 0; i < objects_.size(); ++i) {
        if (objects_[i] && objects_[i]->m_id == id) {
            return i;
        }
    }
    return objects_.size();
}

void CAlfaDoc::EnsureObjectId(CAlfaObject& object) {
    const CAlfaObject* existing = FindObjectById(object.m_id);
    if (object.m_id == 0 || (existing != nullptr && existing != &object)) {
        while (FindObjectById(next_object_id_) != nullptr) {
            ++next_object_id_;
        }
        object.m_id = next_object_id_++;
        return;
    }
    next_object_id_ = std::max(next_object_id_, object.m_id + 1);
}

void CAlfaDoc::EnsureObjectIds() {
    next_object_id_ = 1;
    for (const ObjectPtr& object : objects_) {
        if (object) {
            EnsureObjectId(*object);
        }
    }
}

void CAlfaDoc::AddObject(std::unique_ptr<CAlfaObject> object) {
    if (!object) {
        return;
    }

    EnsureObjectId(*object);
    AssignDefaultMaterial(*object);
    AssignObjectToWorkLayer(*object);
    objects_.push_back(std::move(object));
    selected_object_index_ = objects_.size() - 1;
    selected_object_indices_ = {selected_object_index_};
    has_selected_object_ = true;
    ClearPointSelection();
}

void CAlfaDoc::AddMesh(std::unique_ptr<CMesh3D> mesh) {
    AddObject(std::move(mesh));
}

bool CAlfaDoc::IsObjectSelectionHighlighted(size_t index) const {
    if (index >= objects_.size() || !objects_[index]) {
        return false;
    }
    if (IsObjectSelected(index)) {
        return true;
    }

    const unsigned long target_id = objects_[index]->m_id;
    std::set<unsigned long> visited_groups;
    const auto contains_recursive =
        [this, target_id, &visited_groups](const auto& self,
                                           const CGroup& group) -> bool {
            if (!visited_groups.insert(group.m_id).second) {
                return false;
            }
            for (unsigned long element_id : group.GetElementIds()) {
                if (element_id == target_id) {
                    return true;
                }
                const CAlfaObject* child = FindObjectById(element_id);
                const auto* child_group = dynamic_cast<const CGroup*>(child);
                if (child_group && self(self, *child_group)) {
                    return true;
                }
            }
            return false;
        };

    for (size_t selected_index : selected_object_indices_) {
        if (selected_index >= objects_.size()
            || selected_index == hidden_selection_highlight_index_
            || !objects_[selected_index]) {
            continue;
        }
        const auto* group =
            dynamic_cast<const CGroup*>(objects_[selected_index].get());
        visited_groups.clear();
        if (group && contains_recursive(contains_recursive, *group)) {
            return true;
        }
    }
    return false;
}

bool CAlfaDoc::CreateGroupFromSelection() {
    if (!HasSelection()) {
        return false;
    }

    std::vector<unsigned long> element_ids;
    for (size_t index : selected_object_indices_) {
        if (index >= objects_.size() || !objects_[index]) {
            continue;
        }
        EnsureObjectId(*objects_[index]);
        element_ids.push_back(objects_[index]->m_id);
    }
    std::sort(element_ids.begin(), element_ids.end());
    element_ids.erase(std::unique(element_ids.begin(), element_ids.end()), element_ids.end());
    if (element_ids.size() < 2) {
        return false;
    }

    int suffix = 1;
    std::string name;
    do {
        name = "Group " + std::to_string(suffix++);
    } while (std::any_of(objects_.begin(), objects_.end(), [&name](const ObjectPtr& object) {
        return object && object->GetName() == name;
    }));

    int layer_id = GetWorkLayerID();
    if (CAlfaObject* first = FindObjectById(element_ids.front())) {
        layer_id = first->m_LayerID;
    }
    auto group = std::make_unique<CGroup>(name, std::move(element_ids));
    AddObject(std::move(group));
    if (auto* created = dynamic_cast<CGroup*>(GetSelectedObject())) {
        created->SetLayer(static_cast<unsigned long>(layer_id));
    }
    return true;
}

bool CAlfaDoc::CreateAssemblyFromSelection() {
    if (!HasSelection()) {
        return false;
    }
    std::vector<unsigned long> element_ids;
    for (size_t index : selected_object_indices_) {
        if (index < objects_.size() && objects_[index]) {
            EnsureObjectId(*objects_[index]);
            element_ids.push_back(objects_[index]->m_id);
        }
    }
    std::sort(element_ids.begin(), element_ids.end());
    element_ids.erase(std::unique(element_ids.begin(), element_ids.end()), element_ids.end());
    if (element_ids.size() < 2) {
        return false;
    }
    int suffix = 1;
    std::string name;
    do {
        name = "Assembly " + std::to_string(suffix++);
    } while (std::any_of(objects_.begin(), objects_.end(), [&name](const ObjectPtr& object) {
        return object && object->GetName() == name;
    }));
    int layer_id = GetWorkLayerID();
    if (CAlfaObject* first = FindObjectById(element_ids.front())) {
        layer_id = first->m_LayerID;
    }
    AddObject(std::make_unique<CAssembled>(name, std::move(element_ids)));
    if (auto* assembly = dynamic_cast<CAssembled*>(GetSelectedObject())) {
        assembly->SetLayer(static_cast<unsigned long>(layer_id));
    }
    return true;
}

bool CAlfaDoc::UngroupSelection() {
    if (!HasSelection()) {
        return false;
    }

    std::vector<unsigned long> released_ids;
    std::vector<size_t> group_indices;
    for (size_t index : selected_object_indices_) {
        if (index >= objects_.size()) {
            continue;
        }
        const auto* group = dynamic_cast<const CGroup*>(objects_[index].get());
        if (!group) {
            continue;
        }
        released_ids.insert(
            released_ids.end(),
            group->GetElementIds().begin(),
            group->GetElementIds().end());
        group_indices.push_back(index);
    }
    if (group_indices.empty()) {
        return false;
    }

    std::sort(group_indices.begin(), group_indices.end(), std::greater<size_t>());
    for (size_t index : group_indices) {
        objects_.erase(objects_.begin() + static_cast<ObjectList::difference_type>(index));
    }

    selected_object_indices_.clear();
    for (unsigned long id : released_ids) {
        const size_t index = FindObjectIndexById(id);
        if (index < objects_.size() && !IsObjectSelected(index)) {
            selected_object_indices_.push_back(index);
        }
    }
    if (selected_object_indices_.empty()) {
        ClearSelection();
        return true;
    }
    selected_object_index_ = selected_object_indices_.back();
    active_object_index_ = selected_object_index_;
    has_selected_object_ = true;
    ClearPointSelection();
    return true;
}

bool CAlfaDoc::DuplicateSelectedObject() {
    if (!HasSelection()) {
        return false;
    }

    if (selected_object_indices_.size() == 1
        && selected_object_indices_.front() < objects_.size()) {
        const auto* source_group = dynamic_cast<const CGroup*>(
            objects_[selected_object_indices_.front()].get());
        if (source_group) {
            std::map<unsigned long, unsigned long> copied_ids;
            std::set<unsigned long> visiting_ids;
            std::function<unsigned long(const CAlfaObject&)> copy_subtree;
            copy_subtree = [&](const CAlfaObject& source) -> unsigned long {
                const auto existing = copied_ids.find(source.m_id);
                if (existing != copied_ids.end()) {
                    return existing->second;
                }
                if (!visiting_ids.insert(source.m_id).second) {
                    return 0;
                }

                std::unique_ptr<CAlfaObject> copy;
                if (const auto* group = dynamic_cast<const CGroup*>(&source)) {
                    std::vector<unsigned long> child_copy_ids;
                    for (unsigned long child_id : group->GetElementIds()) {
                        const CAlfaObject* child = FindObjectById(child_id);
                        if (!child) {
                            continue;
                        }
                        const unsigned long child_copy_id = copy_subtree(*child);
                        if (child_copy_id != 0) {
                            child_copy_ids.push_back(child_copy_id);
                        }
                    }
                    std::unique_ptr<CGroup> group_copy;
                    if (const auto* cabinet = dynamic_cast<const CKitchenCabinet*>(group)) {
                        auto cabinet_copy = std::make_unique<CKitchenCabinet>(
                            cabinet->GetName() + " Copy", std::move(child_copy_ids),
                            cabinet->GetDefinition());
                        cabinet_copy->SetDrawParam(cabinet->GetDrawParam());
                        cabinet_copy->SetIdDim(cabinet->GetIdDim());
                        cabinet_copy->SetAssemblyTransform(
                            cabinet->GetAssemblyTransform());
                        for (const CDimens3D* dimension : cabinet->GetDimensions()) {
                            if (dimension) {
                                cabinet_copy->AddDimension(*dimension);
                            }
                        }
                        group_copy = std::move(cabinet_copy);
                    } else if (const auto* assembly = dynamic_cast<const CAssembled*>(group)) {
                        auto assembly_copy = std::make_unique<CAssembled>(
                            assembly->GetName() + " Copy", std::move(child_copy_ids));
                        assembly_copy->SetDrawParam(assembly->GetDrawParam());
                        assembly_copy->SetIdDim(assembly->GetIdDim());
                        assembly_copy->SetAssemblyTransform(
                            assembly->GetAssemblyTransform());
                        for (const CDimens3D* dimension : assembly->GetDimensions()) {
                            if (dimension) {
                                assembly_copy->AddDimension(*dimension);
                            }
                        }
                        group_copy = std::move(assembly_copy);
                    } else {
                        group_copy = std::make_unique<CGroup>(
                            group->GetName() + " Copy", std::move(child_copy_ids));
                    }
                    group_copy->SetGroupName(group->GetGroupName());
                    group_copy->CAlfaObject::SetVisible(group->IsVisible());
                    group_copy->CAlfaObject::SetColor(group->GetColor());
                    group_copy->SetMaterial(group->GetMaterial());
                    group_copy->SetMaterialId(group->GetMaterialId());
                    if (group->IsParametric()) {
                        group_copy->SetParametricDefinition(
                            group->GetParametricToolId(),
                            group->GetParametricParameters());
                    }
                    group_copy->m_LayerID = group->m_LayerID;
                    copy = std::move(group_copy);
                } else {
                    copy = source.Clone();
                }
                if (!copy) {
                    visiting_ids.erase(source.m_id);
                    return 0;
                }

                copy->m_id = 0;
                EnsureObjectId(*copy);
                AssignDefaultMaterial(*copy);
                const unsigned long copy_id = copy->m_id;
                objects_.push_back(std::move(copy));
                copied_ids[source.m_id] = copy_id;
                visiting_ids.erase(source.m_id);
                return copy_id;
            };

            const unsigned long root_copy_id = copy_subtree(*source_group);
            // Linked solids inside a copied assembly must depend on the
            // copied source solids, not on the originals.  Keeping the old
            // ids makes a multi-selection transform apply through both the
            // original and copied dependency trees.
            for (const auto& [source_id, copy_id] : copied_ids) {
                const auto* source_clone = dynamic_cast<const CAssociativeClone*>(
                    FindObjectById(source_id));
                auto* copied_clone = dynamic_cast<CAssociativeClone*>(
                    FindObjectById(copy_id));
                if (!source_clone || !copied_clone) {
                    continue;
                }
                const auto copied_source = copied_ids.find(
                    source_clone->GetSourceId());
                if (copied_source != copied_ids.end()) {
                    copied_clone->SetSourceId(copied_source->second);
                }
            }
            selected_object_index_ = FindObjectIndexById(root_copy_id);
            if (selected_object_index_ >= objects_.size()) {
                return false;
            }
            selected_object_indices_ = {selected_object_index_};
            active_object_index_ = selected_object_index_;
            has_selected_object_ = true;
            ClearPointSelection();
            return true;
        }
    }

    ExpandSelectedGroups();
    const std::vector<size_t> source_indices = selected_object_indices_;
    std::map<std::string, std::string> copied_group_names;

    const auto unique_group_name = [this, &copied_group_names](const std::string& source_name) {
        const auto existing_copy = copied_group_names.find(source_name);
        if (existing_copy != copied_group_names.end()) {
            return existing_copy->second;
        }

        const std::string base = source_name + " Copy";
        std::string candidate = base;
        int suffix = 2;
        const auto group_exists = [this, &copied_group_names](const std::string& name) {
            for (const ObjectPtr& object : objects_) {
                if (object && object->GetGroupName() == name) {
                    return true;
                }
            }
            for (const auto& entry : copied_group_names) {
                if (entry.second == name) {
                    return true;
                }
            }
            return false;
        };
        while (group_exists(candidate)) {
            candidate = base + " " + std::to_string(suffix++);
        }
        copied_group_names[source_name] = candidate;
        return candidate;
    };

    std::vector<std::unique_ptr<CAlfaObject>> copies;
    std::map<unsigned long, unsigned long> copied_ids;
    copies.reserve(source_indices.size());
    for (size_t source_index : source_indices) {
        if (source_index >= objects_.size() || !objects_[source_index]) {
            continue;
        }

        std::unique_ptr<CAlfaObject> copy = objects_[source_index]->Clone();
        if (!copy) {
            continue;
        }
        if (!copy->GetGroupName().empty()) {
            copy->SetGroupName(unique_group_name(copy->GetGroupName()));
        }
        copy->m_id = 0;
        EnsureObjectId(*copy);
        copied_ids[objects_[source_index]->m_id] = copy->m_id;
        AssignDefaultMaterial(*copy);
        copies.push_back(std::move(copy));
    }

    if (copies.empty()) {
        return false;
    }

    selected_object_indices_.clear();
    for (auto& copy : copies) {
        const size_t copy_index = objects_.size();
        objects_.push_back(std::move(copy));
        selected_object_indices_.push_back(copy_index);
    }
    for (const auto& [source_id, copy_id] : copied_ids) {
        const auto* source_clone = dynamic_cast<const CAssociativeClone*>(
            FindObjectById(source_id));
        auto* copied_clone = dynamic_cast<CAssociativeClone*>(
            FindObjectById(copy_id));
        if (!source_clone || !copied_clone) {
            continue;
        }
        const auto copied_source = copied_ids.find(source_clone->GetSourceId());
        if (copied_source != copied_ids.end()) {
            copied_clone->SetSourceId(copied_source->second);
        }
    }
    selected_object_index_ = selected_object_indices_.back();
    active_object_index_ = selected_object_index_;
    has_selected_object_ = true;
    ClearPointSelection();
    selected_solid_face_indices_.clear();
    has_selected_solid_face_ = false;
    return true;
}

bool CAlfaDoc::MirrorSelectedObjects(Vec3 plane_point, Vec3 plane_normal) {
    if (dot(plane_normal, plane_normal) <= 0.000001f || !DuplicateSelectedObject()) {
        return false;
    }

    for (size_t index : selected_object_indices_) {
        if (index < objects_.size() && objects_[index]) {
            objects_[index]->Mirror(plane_point, plane_normal);
        }
    }
    return true;
}

bool CAlfaDoc::CreateLoftSurfaceFromSelectedBSplines() {
    std::vector<const CBSpline*> splines;
    splines.reserve(selected_object_indices_.size());
    for (size_t index : selected_object_indices_) {
        if (index >= objects_.size() || !objects_[index] || !IsObjectVisible(*objects_[index])) {
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
    surface->ReBuldMesh();
    AddObject(std::move(surface));
    return true;
}

size_t CAlfaDoc::CreatePlaneIntersectionCurves() {
    const CAlfaObject* plane_object = nullptr;
    const CAlfaObject* target = nullptr;
    gp_Pln plane;
    for (size_t index : selected_object_indices_) {
        if (index >= objects_.size() || !objects_[index]
            || !IsObjectVisible(*objects_[index])) continue;
        gp_Pln candidate;
        if (selected_plane(*objects_[index], candidate)) {
            if (plane_object) return 0;
            plane_object = objects_[index].get();
            plane = candidate;
        } else {
            if (target) return 0;
            target = objects_[index].get();
        }
    }
    if (!plane_object || !target) return 0;

    std::vector<std::unique_ptr<CAlfaObject>> results;
    try {
        if (const auto* mesh = dynamic_cast<const CMesh3D*>(target)) {
            int number = 1;
            for (std::vector<CPoint3d>& points :
                 intersect_mesh_with_plane(*mesh, plane)) {
                if (points.size() < 2) continue;
                auto polyline = std::make_unique<CPolyline>(
                    "Plane-Mesh Intersection " + std::to_string(number++));
                const bool closed = point_distance(points.front(), points.back())
                    <= 1.0e-6;
                if (closed) points.pop_back();
                for (const CPoint3d& point : points) polyline->AddPoint(point);
                polyline->SetClosed(closed);
                polyline->SetColor(kDefaultCurveColor);
                results.push_back(std::move(polyline));
            }
        } else if (const auto* solid = dynamic_cast<const CSolid*>(target)) {
            const auto* plane_surface =
                dynamic_cast<const CSurfaceSet*>(plane_object);
            if (!plane_surface || plane_surface->m_Shape.IsNull()
                || solid->m_Shape.IsNull()) return 0;
            BRepAlgoAPI_Section section(
                solid->m_Shape, plane_surface->m_Shape, Standard_False);
            section.Approximation(Standard_True);
            section.Build();
            if (!section.IsDone()) return 0;
            std::vector<std::unique_ptr<CBSpline>> curves =
                nurbs_curves_from_shape(section.Shape(),
                                        "Plane Intersection");
            for (auto& curve : curves) results.push_back(std::move(curve));
        } else {
            return 0;
        }
    } catch (const Standard_Failure&) {
        return 0;
    }

    const size_t count = results.size();
    for (auto& result : results) AddObject(std::move(result));
    return count;
}

size_t CAlfaDoc::CreateSurfaceIntersectionCurves() {
    std::vector<const CSurfaceSet*> surfaces;
    for (size_t index : selected_object_indices_) {
        if (index >= objects_.size() || !objects_[index]
            || !IsObjectVisible(*objects_[index])) continue;
        const auto* surface =
            dynamic_cast<const CSurfaceSet*>(objects_[index].get());
        if (!surface || surface->GetParametricToolId() == "PlaneTool") return 0;
        surfaces.push_back(surface);
    }
    if (surfaces.size() != 2 || surfaces[0]->m_Shape.IsNull()
        || surfaces[1]->m_Shape.IsNull()) return 0;
    try {
        BRepAlgoAPI_Section section(
            surfaces[0]->m_Shape, surfaces[1]->m_Shape, Standard_False);
        section.Approximation(Standard_True);
        section.Build();
        if (!section.IsDone()) return 0;
        std::vector<std::unique_ptr<CBSpline>> curves =
            nurbs_curves_from_shape(section.Shape(), "Surface Intersection");
        const size_t count = curves.size();
        for (auto& curve : curves) AddObject(std::move(curve));
        return count;
    } catch (const Standard_Failure&) {
        return 0;
    }
}

size_t CAlfaDoc::ProjectSelectedCurveToSurface(Vec3 direction) {
    if (dot(direction, direction) <= 1.0e-12f) return 0;
    const CAlfaObject* curve = nullptr;
    const CSurfaceSet* surface = nullptr;
    for (size_t index : selected_object_indices_) {
        if (index >= objects_.size() || !objects_[index]
            || !IsObjectVisible(*objects_[index])) continue;
        CAlfaObject* object = objects_[index].get();
        if (const auto* candidate = dynamic_cast<const CSurfaceSet*>(object)) {
            if (surface) return 0;
            surface = candidate;
            continue;
        }
        SweepCurveSamples samples;
        if (sweep_curve_samples(*object, samples)) {
            if (curve) return 0;
            curve = object;
        } else {
            return 0;
        }
    }
    if (!curve || !surface || surface->m_Shape.IsNull()) return 0;
    const TopoDS_Wire source = make_wire_from_curve_object(*curve);
    if (source.IsNull()) return 0;
    try {
        BRepProj_Projection projection(
            source, surface->m_Shape,
            gp_Dir(direction.x, direction.y, direction.z));
        std::vector<std::unique_ptr<CBSpline>> results;
        while (projection.More()) {
            std::vector<std::unique_ptr<CBSpline>> curves =
                nurbs_curves_from_shape(projection.Current(),
                                        "Projected Curve");
            for (auto& projected : curves) results.push_back(std::move(projected));
            projection.Next();
        }
        const size_t count = results.size();
        for (auto& result : results) AddObject(std::move(result));
        return count;
    } catch (const Standard_Failure&) {
        return 0;
    }
}

size_t CAlfaDoc::ExtractSelectedSurfaceEdges() {
    CSolid* solid = GetSelectedSolid();
    if (!solid) return 0;
    const std::vector<TopoDS_Edge> edges =
        unique_edges(solid->GetSelectedTopoEdges());
    if (edges.empty()) return 0;
    std::vector<std::unique_ptr<CBSpline>> curves;
    int number = 1;
    for (const TopoDS_Edge& edge : edges) {
        std::unique_ptr<CBSpline> curve = editable_nurbs_from_edge(
            edge, "Extracted Edge " + std::to_string(number++));
        if (curve) curves.push_back(std::move(curve));
    }
    const size_t count = curves.size();
    for (auto& curve : curves) AddObject(std::move(curve));
    return count;
}

bool CAlfaDoc::JoinSelectedSurfaces() {
    if (selected_object_indices_.size() < 2) return false;

    std::vector<size_t> indices = selected_object_indices_;
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

    std::vector<CSurfaceSet*> surfaces;
    surfaces.reserve(indices.size());
    for (size_t index : indices) {
        if (index >= objects_.size() || !objects_[index]
            || !IsObjectVisible(*objects_[index])) {
            return false;
        }
        auto* surface = dynamic_cast<CSurfaceSet*>(objects_[index].get());
        if (!surface || surface->m_Shape.IsNull()) return false;
        surfaces.push_back(surface);
    }

    try {
        BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);
        for (const CSurfaceSet* surface : surfaces) {
            builder.Add(compound, surface->m_Shape);
        }

        // Connect all selected components with G2 filling patches. The bridge
        // passes through both nearest boundary curves and matches both the
        // tangent plane and the curvature of their supporting faces.
        std::vector<bool> connected(surfaces.size(), false);
        connected[0] = true;
        for (size_t link = 1; link < surfaces.size(); ++link) {
            SurfaceEdgePair closest;
            size_t next_surface = surfaces.size();
            for (size_t first = 0; first < surfaces.size(); ++first) {
                if (!connected[first]) continue;
                for (size_t second = 0; second < surfaces.size(); ++second) {
                    if (connected[second]) continue;
                    SurfaceEdgePair candidate = closest_surface_edges(
                        surfaces[first]->m_Shape, surfaces[second]->m_Shape);
                    if (!candidate.first.IsNull()
                        && candidate.distance < closest.distance) {
                        closest = candidate;
                        next_surface = second;
                    }
                }
            }
            if (next_surface >= surfaces.size()) return false;

            // A shared boundary needs only topological sewing. Building a
            // filling patch here would create a zero-width and unstable
            // "bridge" between already touching surfaces.
            constexpr double sewing_tolerance = 1.0e-4;
            if (surface_edges_coincide(closest.first, closest.second,
                                       sewing_tolerance)) {
                connected[next_surface] = true;
                continue;
            }

            BRepOffsetAPI_MakeFilling filling(
                4, 24, 3, Standard_True,
                1.0e-5, 1.0e-4, 0.005, 0.02, 10, 16);
            TopoDS_Vertex first_start, first_end;
            TopoDS_Vertex second_start, second_end;
            TopExp::Vertices(closest.first,
                             first_start, first_end, Standard_True);
            TopExp::Vertices(closest.second,
                             second_start, second_end, Standard_True);
            if (first_start.IsNull() || first_end.IsNull()
                || second_start.IsNull() || second_end.IsNull()) {
                return false;
            }
            const TopoDS_Edge end_connector = BRepBuilderAPI_MakeEdge(
                BRep_Tool::Pnt(first_end), BRep_Tool::Pnt(second_end));
            const TopoDS_Edge start_connector = BRepBuilderAPI_MakeEdge(
                BRep_Tool::Pnt(second_start), BRep_Tool::Pnt(first_start));
            if (end_connector.IsNull() || start_connector.IsNull()) {
                return false;
            }
            TopoDS_Edge reversed_second = closest.second;
            reversed_second.Reverse();
            filling.Add(closest.first, closest.first_support,
                        GeomAbs_G2, Standard_True);
            filling.Add(end_connector, GeomAbs_C0, Standard_True);
            filling.Add(reversed_second, closest.second_support,
                        GeomAbs_G2, Standard_True);
            filling.Add(start_connector, GeomAbs_C0, Standard_True);
            filling.Build();
            if (!filling.IsDone() || filling.Shape().IsNull()) return false;
            if (!std::isfinite(filling.G1Error())
                || !std::isfinite(filling.G2Error())
                || filling.G1Error() > 0.02
                || filling.G2Error() > 0.05) {
                return false;
            }
            const TopoDS_Face bridge = TopoDS::Face(filling.Shape());
            if (bridge.IsNull()) return false;
            builder.Add(compound, bridge);
            connected[next_surface] = true;
        }

        // A Surface Set may contain several faces without forcing them into a
        // shell. Sewing open faces can reverse or split valid input faces and
        // corrupt their UV triangulation. Keep their original geometry and
        // orientation; coincident boundaries remain coincident, while a gap
        // is occupied by the explicitly constructed bridge face above.
        TopoDS_Shape joined_shape = compound;
        if (!BRepCheck_Analyzer(joined_shape).IsValid()) return false;
        auto joined = std::make_unique<CSurfaceSet>(joined_shape);
        joined->SetName("Joined Surfaces");
        joined->SetColor(surfaces.front()->GetColor());
        joined->SetMaterial(surfaces.front()->GetMaterial());
        joined->SetMaterialId(surfaces.front()->GetMaterialId());
        if (!joined->ReBuldMesh()) return false;

        std::sort(indices.begin(), indices.end(), std::greater<size_t>());
        for (size_t index : indices) {
            objects_.erase(objects_.begin()
                + static_cast<ObjectList::difference_type>(index));
        }
        ClearSelection();
        AddObject(std::move(joined));
        return true;
    } catch (const Standard_Failure&) {
        return false;
    }
}

bool CAlfaDoc::CreateRuledSurfaceFromSpline(unsigned long curve_id,
                                            double length,
                                            int direction_axis,
                                            bool reverse_normal) {
    const auto* spline = dynamic_cast<const CBSpline*>(FindObjectById(curve_id));
    if (!spline || spline->GetPointCount() < 2) return false;

    TopoDS_Shape shape = make_ruled_surface_from_spline(
        *spline, length, direction_axis, reverse_normal);
    if (shape.IsNull()) return false;

    auto surface = std::make_unique<CSurfaceSet>(shape);
    surface->SetName("Ruled Surface");
    surface->SetColor({0.70f, 0.72f, 0.68f});
    if (!surface->ReBuldMesh()) return false;
    AddObject(std::move(surface));
    return true;
}

bool CAlfaDoc::RebuildRuledSurface(size_t object_index,
                                   unsigned long curve_id,
                                   double length,
                                   int direction_axis,
                                   bool reverse_normal) {
    if (object_index >= objects_.size()) return false;
    auto* surface = dynamic_cast<CSurfaceSet*>(objects_[object_index].get());
    const auto* spline = dynamic_cast<const CBSpline*>(FindObjectById(curve_id));
    if (!surface || !spline || spline->GetPointCount() < 2) return false;

    TopoDS_Shape shape = make_ruled_surface_from_spline(
        *spline, length, direction_axis, reverse_normal);
    if (shape.IsNull()) return false;
    surface->m_Shape = shape;
    return surface->ReBuldMesh();
}

bool CAlfaDoc::CreateFourSplineSurfaceFromSelection() {
    if (selected_object_indices_.size() != 4) {
        return false;
    }

    std::array<CAlfaObject*, 4> curves{};
    for (size_t index = 0; index < curves.size(); ++index) {
        const size_t object_index = selected_object_indices_[index];
        if (object_index >= objects_.size() || !objects_[object_index]
            || !IsObjectVisible(*objects_[object_index])) {
            return false;
        }
        SweepCurveSamples samples;
        if (!sweep_curve_samples(*objects_[object_index], samples)
            || samples.closed) {
            return false;
        }
        curves[index] = objects_[object_index].get();
    }

    TopoDS_Shape shape = make_four_spline_surface(
        *curves[0], *curves[1], *curves[2], *curves[3]);
    if (shape.IsNull()) {
        return false;
    }

    auto surface = std::make_unique<CSurfaceSet>(shape);
    surface->SetName("Surface by 4 Splines");
    surface->SetColor({0.70f, 0.72f, 0.68f});
    surface->SetParametricOperation(0,
        "SurfaceFourSplines", "Surface by 4 Splines", {
            {"curve1.id", static_cast<double>(curves[0]->m_id)},
            {"curve2.id", static_cast<double>(curves[1]->m_id)},
            {"curve3.id", static_cast<double>(curves[2]->m_id)},
            {"curve4.id", static_cast<double>(curves[3]->m_id)}
        });
    if (!surface->ReBuldMesh()) {
        return false;
    }
    AddObject(std::move(surface));
    return true;
}

bool CAlfaDoc::RebuildFourSplineSurface(size_t object_index,
                                        unsigned long first_id,
                                        unsigned long second_id,
                                        unsigned long third_id,
                                        unsigned long fourth_id) {
    if (object_index >= objects_.size()) {
        return false;
    }
    auto* surface = dynamic_cast<CSurfaceSet*>(objects_[object_index].get());
    const CAlfaObject* first = FindObjectById(first_id);
    const CAlfaObject* second = FindObjectById(second_id);
    const CAlfaObject* third = FindObjectById(third_id);
    const CAlfaObject* fourth = FindObjectById(fourth_id);
    if (!surface || !first || !second || !third || !fourth) {
        return false;
    }

    TopoDS_Shape shape = make_four_spline_surface(
        *first, *second, *third, *fourth);
    if (shape.IsNull()) {
        return false;
    }
    surface->m_Shape = shape;
    return surface->ReBuldMesh();
}

bool CAlfaDoc::CreateTwoRailSweepSurfaceFromSelection() {
    if (selected_object_indices_.size() != 3) {
        return false;
    }

    std::array<CAlfaObject*, 3> curves{};
    for (std::size_t i = 0; i < selected_object_indices_.size(); ++i) {
        const std::size_t index = selected_object_indices_[i];
        if (index >= objects_.size() || !objects_[index] || !IsObjectVisible(*objects_[index])) {
            return false;
        }
        SweepCurveSamples check;
        if (!sweep_curve_samples(*objects_[index], check)) {
            return false;
        }
        curves[i] = objects_[index].get();
    }

    std::size_t profile_index = 0;
    int closed_count = 0;
    for (std::size_t i = 0; i < curves.size(); ++i) {
        SweepCurveSamples samples;
        sweep_curve_samples(*curves[i], samples);
        if (samples.closed) {
            profile_index = i;
            ++closed_count;
        }
    }
    if (closed_count > 1) {
        return false;
    }

    std::array<std::size_t, 2> rail_indices{};
    std::size_t rail_count = 0;
    for (std::size_t i = 0; i < curves.size(); ++i) {
        if (i != profile_index) {
            rail_indices[rail_count++] = i;
        }
    }

    TopoDS_Shape shape = make_two_rail_sweep_surface(
        *curves[profile_index], *curves[rail_indices[0]], *curves[rail_indices[1]]);
    if (shape.IsNull()) {
        return false;
    }

    auto surface = std::make_unique<CSurfaceSet>(shape);
    surface->SetName("Two-Rail Sweep Surface");
    surface->SetColor({0.70f, 0.72f, 0.68f});
    surface->SetParametricOperation(0, "SurfaceSweepTwoRails", "Two-Rail Sweep Surface", {
        {"profile.id", static_cast<double>(curves[profile_index]->m_id)},
        {"guide1.id", static_cast<double>(curves[rail_indices[0]]->m_id)},
        {"guide2.id", static_cast<double>(curves[rail_indices[1]]->m_id)}
    });
    if (!surface->ReBuldMesh()) {
        return false;
    }
    AddObject(std::move(surface));
    return true;
}

bool CAlfaDoc::RebuildTwoRailSweepSurface(size_t object_index,
                                          unsigned long profile_id,
                                          unsigned long first_rail_id,
                                          unsigned long second_rail_id) {
    if (object_index >= objects_.size()) {
        return false;
    }
    auto* surface = dynamic_cast<CSurfaceSet*>(objects_[object_index].get());
    const CAlfaObject* profile = FindObjectById(profile_id);
    const CAlfaObject* first_rail = FindObjectById(first_rail_id);
    const CAlfaObject* second_rail = FindObjectById(second_rail_id);
    if (!surface || !profile || !first_rail || !second_rail) {
        return false;
    }

    TopoDS_Shape shape = make_two_rail_sweep_surface(*profile, *first_rail, *second_rail);
    if (shape.IsNull()) {
        return false;
    }
    surface->m_Shape = shape;
    return surface->ReBuldMesh();
}

bool CAlfaDoc::CreateTwoRailSweepSolidFromSelection() {
    if (selected_object_indices_.size() != 3) {
        return false;
    }

    std::array<CAlfaObject*, 3> curves{};
    std::size_t profile_index = 0;
    int closed_count = 0;
    for (std::size_t i = 0; i < selected_object_indices_.size(); ++i) {
        const std::size_t index = selected_object_indices_[i];
        if (index >= objects_.size() || !objects_[index]
            || !IsObjectVisible(*objects_[index])) {
            return false;
        }
        SweepCurveSamples samples;
        if (!sweep_curve_samples(*objects_[index], samples)) {
            return false;
        }
        if (samples.closed) {
            if (!dynamic_cast<CSmartLine*>(objects_[index].get())) {
                return false;
            }
            profile_index = i;
            ++closed_count;
        }
        curves[i] = objects_[index].get();
    }
    if (closed_count != 1) {
        return false;
    }

    std::array<std::size_t, 2> rail_indices{};
    std::size_t rail_count = 0;
    for (std::size_t i = 0; i < curves.size(); ++i) {
        if (i != profile_index) {
            SweepCurveSamples rail_samples;
            if (!sweep_curve_samples(*curves[i], rail_samples)
                || rail_samples.closed) {
                return false;
            }
            rail_indices[rail_count++] = i;
        }
    }

    for (CAlfaObject* curve : curves) {
        EnsureObjectId(*curve);
    }
    TopoDS_Shape shape = make_two_rail_sweep_solid(
        *curves[profile_index],
        *curves[rail_indices[0]],
        *curves[rail_indices[1]]);
    if (shape.IsNull()) {
        return false;
    }

    auto solid = std::make_unique<CSolid>(shape);
    solid->SetName("Two-Rail Sweep Solid");
    solid->SetColor({0.70f, 0.72f, 0.68f});
    solid->SetParametricOperation(0, "SolidSweepTwoRails", "Two-Rail Sweep Solid", {
        {"profile.id", static_cast<double>(curves[profile_index]->m_id)},
        {"guide1.id", static_cast<double>(curves[rail_indices[0]]->m_id)},
        {"guide2.id", static_cast<double>(curves[rail_indices[1]]->m_id)}
    });
    if (!solid->ReBuldMesh()) {
        return false;
    }
    AddObject(std::move(solid));
    return true;
}

bool CAlfaDoc::RebuildTwoRailSweepSolid(size_t object_index,
                                        unsigned long profile_id,
                                        unsigned long first_rail_id,
                                        unsigned long second_rail_id) {
    if (object_index >= objects_.size()) {
        return false;
    }
    auto* solid = dynamic_cast<CSolid*>(objects_[object_index].get());
    const auto* profile = dynamic_cast<const CSmartLine*>(FindObjectById(profile_id));
    const CAlfaObject* first_rail = FindObjectById(first_rail_id);
    const CAlfaObject* second_rail = FindObjectById(second_rail_id);
    if (!solid || !profile || !profile->IsClosed()
        || !first_rail || !second_rail) {
        return false;
    }
    TopoDS_Shape shape = make_two_rail_sweep_solid(
        *profile, *first_rail, *second_rail);
    if (shape.IsNull()) {
        return false;
    }
    solid->m_Shape = shape;
    return solid->ReBuldMesh();
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

bool CAlfaDoc::SewSelectedSurfacesToSolid(double tolerance,
                                          std::string* error_message,
                                          double* used_tolerance) {
    const auto fail = [error_message](const char* message) {
        if (error_message) {
            *error_message = message;
        }
        return false;
    };
    if (error_message) {
        error_message->clear();
    }
    if (used_tolerance) {
        *used_tolerance = 0.0;
    }
    if (!std::isfinite(tolerance) || tolerance <= 0.0) {
        tolerance = 0.02;
    }

    std::vector<size_t> source_indices;
    source_indices.reserve(selected_object_indices_.size());
    for (size_t index : selected_object_indices_) {
        if (index < objects_.size()
            && objects_[index]
            && dynamic_cast<CSurfaceSet*>(objects_[index].get())) {
            source_indices.push_back(index);
        }
    }
    std::sort(source_indices.begin(), source_indices.end());
    source_indices.erase(
        std::unique(source_indices.begin(), source_indices.end()),
        source_indices.end());
    if (source_indices.size() < 2) {
        return fail("Select at least two surface objects");
    }

    Color source_color{};
    Material source_material;
    int source_layer = 0;
    std::string source_group;
    bool have_source_style = false;

    try {
        for (size_t index : source_indices) {
            auto* surface = dynamic_cast<CSurfaceSet*>(objects_[index].get());
            if (surface->m_Shape.IsNull()) {
                return fail("A selected surface object has no geometry");
            }
            TopExp_Explorer faces(surface->m_Shape, TopAbs_FACE);
            if (!faces.More()) {
                return fail("A selected surface object contains no faces");
            }
            if (!have_source_style) {
                source_color = surface->GetColor();
                source_material = surface->GetMaterial();
                source_layer = surface->m_LayerID;
                source_group = surface->GetGroupName();
                have_source_style = true;
            }
        }

        std::unique_ptr<BRepBuilderAPI_Sewing> sewing;
        double actual_tolerance = tolerance;
        for (double candidate : {tolerance, tolerance * 1.25,
                                 tolerance * 1.5, tolerance * 2.0}) {
            auto attempt = std::make_unique<BRepBuilderAPI_Sewing>(
                candidate, Standard_True, Standard_True,
                Standard_True, Standard_False);
            for (size_t index : source_indices) {
                auto* surface = dynamic_cast<CSurfaceSet*>(objects_[index].get());
                attempt->Add(surface->m_Shape);
            }
            attempt->Perform();
            sewing = std::move(attempt);
            actual_tolerance = candidate;
            if (sewing->NbFreeEdges() == 0
                || sewing->NbMultipleEdges() != 0) {
                break;
            }
        }

        const TopoDS_Shape sewed = sewing->SewedShape();
        if (sewed.IsNull()) {
            return fail("The selected surfaces could not be sewn");
        }
        if (sewing->NbFreeEdges() != 0) {
            return fail("The selected surfaces do not form a closed shell");
        }
        if (sewing->NbMultipleEdges() != 0) {
            return fail("The selected surfaces form a non-manifold shell");
        }

        TopoDS_Shell shell;
        int shell_count = 0;
        if (sewed.ShapeType() == TopAbs_SHELL) {
            shell = TopoDS::Shell(sewed);
            shell_count = 1;
        } else {
            for (TopExp_Explorer shells(sewed, TopAbs_SHELL);
                 shells.More(); shells.Next()) {
                shell = TopoDS::Shell(shells.Current());
                ++shell_count;
            }
        }
        if (shell_count != 1 || shell.IsNull()) {
            return fail(shell_count == 0
                ? "Sewing did not produce a closed shell"
                : "The selected surfaces form more than one shell");
        }

        BRepBuilderAPI_MakeSolid solid_builder(shell);
        if (!solid_builder.IsDone() || solid_builder.Solid().IsNull()) {
            return fail("The sewn shell could not be converted to a solid");
        }
        ShapeFix_Solid solid_fixer(solid_builder.Solid());
        solid_fixer.Perform();
        TopoDS_Shape result_shape = solid_fixer.Solid();
        if (result_shape.IsNull()
            || result_shape.ShapeType() != TopAbs_SOLID
            || !BRepCheck_Analyzer(result_shape).IsValid()) {
            return fail("The sewn shell does not define a valid solid");
        }

        GProp_GProps volume_properties;
        BRepGProp::VolumeProperties(result_shape, volume_properties);
        if (std::fabs(volume_properties.Mass()) <= 1.0e-9) {
            return fail("The sewn shell has no enclosed volume");
        }

        auto result = std::make_unique<CSolid>(result_shape);
        result->SetName("Sewn Solid");
        if (have_source_style) {
            result->SetColor(source_color);
            result->SetMaterial(source_material);
            result->m_LayerID = source_layer;
            result->SetGroupName(source_group);
        }
        if (!result->ReBuldMesh()) {
            return fail("The solid was created, but its display mesh could not be built");
        }

        std::sort(source_indices.begin(), source_indices.end(), std::greater<size_t>());
        for (size_t index : source_indices) {
            objects_.erase(objects_.begin()
                + static_cast<ObjectList::difference_type>(index));
        }
        AddObject(std::move(result));
        if (have_source_style && !objects_.empty()) {
            objects_.back()->m_LayerID = source_layer;
        }
        active_object_index_ = selected_object_index_;
        if (used_tolerance) {
            *used_tolerance = actual_tolerance;
        }
        return true;
    } catch (const Standard_Failure& failure) {
        if (error_message) {
            const char* message = failure.GetMessageString();
            *error_message = message && *message
                ? std::string("OpenCascade: ") + message
                : "OpenCascade failed while sewing the surfaces";
        }
        return false;
    }
}

int CAlfaDoc::RebuildVisibleObjectMeshes(float mesh_deflection) {
    if (!std::isfinite(mesh_deflection) || mesh_deflection <= 0.0f) {
        return 0;
    }
    int rebuilt = 0;
    for (const ObjectPtr& object : objects_) {
        auto* solid = dynamic_cast<CSolid*>(object.get());
        if (!solid || solid->m_Shape.IsNull()
            || !IsObjectVisible(*solid)) {
            continue;
        }
        if (solid->ReBuldMesh(mesh_deflection)) {
            ++rebuilt;
        }
    }
    return rebuilt;
}

bool CAlfaDoc::DeleteSelectedObject() {
    if (!HasSelection()) {
        return false;
    }

    std::vector<size_t> indices;
    std::set<unsigned long> visited_ids;
    std::function<void(size_t)> collect_object;
    collect_object = [&](size_t index) {
        if (index >= objects_.size() || !objects_[index]
            || !visited_ids.insert(objects_[index]->m_id).second) {
            return;
        }
        indices.push_back(index);
        if (const auto* group = dynamic_cast<const CGroup*>(objects_[index].get())) {
            for (unsigned long id : group->GetElementIds()) {
                collect_object(FindObjectIndexById(id));
            }
        }
    };
    for (size_t index : selected_object_indices_) {
        collect_object(index);
    }
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
    if (spline) return spline->SetPoint(selected_point_index_, point);
    CSmartLine* sketch = GetSelectedSketch();
    return sketch && sketch->MoveNodeWorld(selected_point_index_, point);
}

bool CAlfaDoc::MoveSelectedCurvePoints(Vec3 delta) {
    if (selected_curve_points_.empty()) {
        return false;
    }

    const auto point_is_selected = [this](size_t object_index, size_t point_index) {
        return std::find(selected_curve_points_.begin(), selected_curve_points_.end(),
                         std::pair<size_t, size_t>{object_index, point_index})
            != selected_curve_points_.end();
    };
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
                const bool attached_control_selected =
                    spline->IsBezierChain()
                    && selected.second % 3 == 0
                    && ((selected.second > 0
                         && point_is_selected(selected.first, selected.second - 1))
                        || (selected.second + 1 < spline->GetPointCount()
                            && point_is_selected(selected.first, selected.second + 1)));
                const bool opposite_control_selected =
                    spline->IsBezierChain() && selected.second % 3 != 0
                    && (selected.second % 3 == 1
                        ? selected.second >= 2
                            && point_is_selected(selected.first, selected.second - 2)
                        : selected.second + 2 < spline->GetPointCount()
                            && point_is_selected(selected.first, selected.second + 2));
                moved = (attached_control_selected
                    || opposite_control_selected
                    ? spline->SetPointDirect(selected.second, point)
                    : spline->SetPoint(selected.second, point)) || moved;
            }
        } else if (auto* sketch = dynamic_cast<CSmartLine*>(objects_[selected.first].get())) {
            if (selected.second < sketch->GetNodeCount()) {
                CPoint3d point = sketch->GetNodeWorld(selected.second);
                point.x += delta.x;
                point.y += delta.y;
                point.z += delta.z;
                moved = sketch->MoveNodeWorld(selected.second, point) || moved;
            }
        }
    }
    return moved;
}

bool CAlfaDoc::RotateSelectedCurvePoints(Vec3 center, Vec3 axis, float angle) {
    if (selected_curve_points_.empty() || dot(axis, axis) <= 0.000001f) return false;
    const auto point_is_selected = [this](size_t object_index, size_t point_index) {
        return std::find(selected_curve_points_.begin(), selected_curve_points_.end(),
                         std::pair<size_t, size_t>{object_index, point_index})
            != selected_curve_points_.end();
    };
    bool changed = false;
    for (const auto& selected : selected_curve_points_) {
        if (selected.first >= objects_.size()) continue;
        CPoint3d point;
        if (auto* polyline = dynamic_cast<CPolyline*>(objects_[selected.first].get())) {
            if (selected.second < polyline->GetPointCount()) {
                point = polyline->GetPoints()[selected.second];
                const Vec3 rotated = center + rotate_around_axis(to_vec3(point) - center, axis, angle);
                changed = polyline->SetPoint(selected.second, CPoint3d(rotated.x, rotated.y, rotated.z)) || changed;
            }
        } else if (auto* spline = dynamic_cast<CBSpline*>(objects_[selected.first].get())) {
            if (selected.second < spline->GetPointCount()) {
                point = spline->GetPoints()[selected.second];
                const Vec3 rotated = center + rotate_around_axis(to_vec3(point) - center, axis, angle);
                const bool attached_control_selected =
                    spline->IsBezierChain()
                    && selected.second % 3 == 0
                    && ((selected.second > 0
                         && point_is_selected(selected.first, selected.second - 1))
                        || (selected.second + 1 < spline->GetPointCount()
                            && point_is_selected(selected.first, selected.second + 1)));
                const bool opposite_control_selected =
                    spline->IsBezierChain() && selected.second % 3 != 0
                    && (selected.second % 3 == 1
                        ? selected.second >= 2
                            && point_is_selected(selected.first, selected.second - 2)
                        : selected.second + 2 < spline->GetPointCount()
                            && point_is_selected(selected.first, selected.second + 2));
                const CPoint3d target(rotated.x, rotated.y, rotated.z);
                changed = (attached_control_selected
                    || opposite_control_selected
                    ? spline->SetPointDirect(selected.second, target)
                    : spline->SetPoint(selected.second, target)) || changed;
            }
        } else if (auto* sketch = dynamic_cast<CSmartLine*>(objects_[selected.first].get())) {
            if (selected.second < sketch->GetNodeCount()) {
                point = sketch->GetNodeWorld(selected.second);
                const Vec3 rotated = center + rotate_around_axis(to_vec3(point) - center, axis, angle);
                changed = sketch->MoveNodeWorld(selected.second, CPoint3d(rotated.x, rotated.y, rotated.z)) || changed;
            }
        }
    }
    return changed;
}

bool CAlfaDoc::ScaleSelectedCurvePoints(Vec3 center, Vec3 axis, float factor) {
    if (selected_curve_points_.empty() || factor <= 0.0001f) return false;
    const bool uniform = dot(axis, axis) <= 0.000001f;
    const auto point_is_selected = [this](size_t object_index, size_t point_index) {
        return std::find(selected_curve_points_.begin(), selected_curve_points_.end(),
                         std::pair<size_t, size_t>{object_index, point_index})
            != selected_curve_points_.end();
    };
    bool changed = false;
    for (const auto& selected : selected_curve_points_) {
        if (selected.first >= objects_.size()) continue;
        auto scaled_point = [&](CPoint3d point) {
            const Vec3 local = to_vec3(point) - center;
            const Vec3 scaled = center + (uniform ? scale_uniform(local, factor)
                                                   : scale_along_axis(local, axis, factor));
            return CPoint3d(scaled.x, scaled.y, scaled.z);
        };
        if (auto* polyline = dynamic_cast<CPolyline*>(objects_[selected.first].get())) {
            if (selected.second < polyline->GetPointCount())
                changed = polyline->SetPoint(selected.second, scaled_point(polyline->GetPoints()[selected.second])) || changed;
        } else if (auto* spline = dynamic_cast<CBSpline*>(objects_[selected.first].get())) {
            if (selected.second < spline->GetPointCount()) {
                const CPoint3d target = scaled_point(spline->GetPoints()[selected.second]);
                const bool attached_control_selected =
                    spline->IsBezierChain()
                    && selected.second % 3 == 0
                    && ((selected.second > 0
                         && point_is_selected(selected.first, selected.second - 1))
                        || (selected.second + 1 < spline->GetPointCount()
                            && point_is_selected(selected.first, selected.second + 1)));
                const bool opposite_control_selected =
                    spline->IsBezierChain() && selected.second % 3 != 0
                    && (selected.second % 3 == 1
                        ? selected.second >= 2
                            && point_is_selected(selected.first, selected.second - 2)
                        : selected.second + 2 < spline->GetPointCount()
                            && point_is_selected(selected.first, selected.second + 2));
                changed = (attached_control_selected
                    || opposite_control_selected
                    ? spline->SetPointDirect(selected.second, target)
                    : spline->SetPoint(selected.second, target)) || changed;
            }
        } else if (auto* sketch = dynamic_cast<CSmartLine*>(objects_[selected.first].get())) {
            if (selected.second < sketch->GetNodeCount())
                changed = sketch->MoveNodeWorld(selected.second, scaled_point(sketch->GetNodeWorld(selected.second))) || changed;
        }
    }
    return changed;
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
    // A selected free CPolyline has priority over nearby Sketch geometry.
    // This is important for the Curves-panel Fillets tool in mixed scenes.
    if (auto* selected_polyline = GetSelectedPolyline()) {
        size_t selected_object = 0;
        size_t selected_point = 0;
        if (FindPolylinePointAtScreen(point, world_to_screen, tolerance,
                                      selected_object, selected_point)
            && selected_object == selected_object_index_) {
            if (!selected_polyline->ApplyFillet(selected_point, radius)) {
                return false;
            }
            active_object_index_ = selected_object_index_;
            selected_object_indices_ = {selected_object_index_};
            has_selected_object_ = true;
            ClearPointSelection();
            return true;
        }
    }

    std::size_t sketch_object_index = objects_.size();
    std::size_t sketch_line_index = 0;
    float best_sketch_distance = tolerance;
    for (std::size_t object_index = 0; object_index < objects_.size(); ++object_index) {
        auto* sketch = dynamic_cast<CSmartLine*>(objects_[object_index].get());
        if (!sketch) {
            continue;
        }
        for (std::size_t line_index = 0; line_index < sketch->GetNumLines(); ++line_index) {
            const CLinkLine* line = sketch->GetLine(line_index);
            if (!line) {
                continue;
            }
            const CPoint3d world_point = sketch->LocalToWorld(line->GetEnd());
            DomPoint screen_point{};
            if (!world_to_screen(
                    Vec3{
                        static_cast<float>(world_point.x),
                        static_cast<float>(world_point.y),
                        static_cast<float>(world_point.z)},
                    screen_point)) {
                continue;
            }
            const float dx = static_cast<float>(point.x - screen_point.x);
            const float dy = static_cast<float>(point.y - screen_point.y);
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance <= best_sketch_distance) {
                best_sketch_distance = distance;
                sketch_object_index = object_index;
                sketch_line_index = line_index;
            }
        }
    }
    if (sketch_object_index < objects_.size()) {
        auto* sketch = static_cast<CSmartLine*>(objects_[sketch_object_index].get());
        if (!sketch->AddFillet(sketch_line_index, radius)) {
            return false;
        }
        selected_object_index_ = sketch_object_index;
        active_object_index_ = sketch_object_index;
        selected_object_indices_ = {sketch_object_index};
        has_selected_object_ = true;
        ClearPointSelection();
        return true;
    }

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
    const CSmartLine* sketch = GetSelectedSketch();
    if (sketch && selected_point_index_ < sketch->GetNodeCount()) {
        point = sketch->GetNodeWorld(selected_point_index_);
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
        } else if (const auto* sketch = dynamic_cast<const CSmartLine*>(objects_[selected.first].get())) {
            if (selected.second < sketch->GetNodeCount()) {
                points.push_back(sketch->GetNodeWorld(selected.second));
            }
        }
    }
    return points;
}

const std::vector<std::pair<size_t, size_t>>&
CAlfaDoc::GetSelectedCurvePoints() const {
    return selected_curve_points_;
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

    if (moved) {
        UpdateAttachedSketches();
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

    if (rotated) {
        UpdateAttachedSketches();
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

    if (scaled) {
        UpdateAttachedSketches();
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

    if (scaled) {
        UpdateAttachedSketches();
    }
    return scaled;
}

std::vector<size_t> CAlfaDoc::GetSelectedTransformRootIndices() const {
    std::vector<size_t> selected_indices;
    selected_indices.reserve(selected_object_indices_.size());
    for (size_t index : selected_object_indices_) {
        if (index < objects_.size() && objects_[index]) {
            selected_indices.push_back(index);
        }
    }
    if (selected_indices.size() < 2) {
        return selected_indices;
    }

    std::function<bool(const CGroup&, unsigned long, std::set<const CGroup*>&)> contains_object;
    contains_object = [&](const CGroup& group,
                          unsigned long object_id,
                          std::set<const CGroup*>& visited_groups) {
        if (!visited_groups.insert(&group).second) {
            return false;
        }
        for (unsigned long child_id : group.GetElementIds()) {
            if (child_id == object_id) {
                return true;
            }
            const auto* child_group = dynamic_cast<const CGroup*>(FindObjectById(child_id));
            if (child_group && contains_object(*child_group, object_id, visited_groups)) {
                return true;
            }
        }
        return false;
    };

    std::vector<size_t> root_indices;
    root_indices.reserve(selected_indices.size());
    for (size_t candidate_index : selected_indices) {
        const CAlfaObject* candidate = objects_[candidate_index].get();
        bool contained_by_selected_group = false;
        for (size_t possible_parent_index : selected_indices) {
            if (possible_parent_index == candidate_index) {
                continue;
            }
            const auto* possible_parent =
                dynamic_cast<const CGroup*>(objects_[possible_parent_index].get());
            if (!possible_parent) {
                continue;
            }
            std::set<const CGroup*> visited_groups;
            if (contains_object(*possible_parent, candidate->m_id, visited_groups)) {
                contained_by_selected_group = true;
                break;
            }
        }
        if (!contained_by_selected_group) {
            root_indices.push_back(candidate_index);
        }
    }
    return root_indices;
}

bool CAlfaDoc::PreviewMoveSelectedObjects(Vec3 delta) {
    if (!HasSelection()) {
        return false;
    }

    bool moved = false;
    for (size_t index : GetSelectedTransformRootIndices()) {
        if (index >= objects_.size()) {
            continue;
        }
        if (auto* group = dynamic_cast<CGroup*>(objects_[index].get())) {
            group->PreviewTranslate(delta);
        } else if (auto* solid = dynamic_cast<CSolid*>(objects_[index].get())) {
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
    for (size_t index : GetSelectedTransformRootIndices()) {
        if (index >= objects_.size()) {
            continue;
        }
        if (auto* group = dynamic_cast<CGroup*>(objects_[index].get())) {
            group->PreviewRotate(center, axis, angle);
        } else if (auto* solid = dynamic_cast<CSolid*>(objects_[index].get())) {
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
    for (size_t index : GetSelectedTransformRootIndices()) {
        if (index >= objects_.size()) {
            continue;
        }
        if (auto* group = dynamic_cast<CGroup*>(objects_[index].get())) {
            group->PreviewScale(center, axis, factor);
        } else if (auto* solid = dynamic_cast<CSolid*>(objects_[index].get())) {
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
    for (size_t index : GetSelectedTransformRootIndices()) {
        if (index < objects_.size()) {
            if (auto* group = dynamic_cast<CGroup*>(objects_[index].get())) {
                moved = group->CommitTranslate(delta) || moved;
            } else if (auto* solid = dynamic_cast<CSolid*>(objects_[index].get())) {
                solid->Translate(delta);
                moved = true;
            }
        }
    }
    if (moved) {
        UpdateAttachedSketches();
    }
    return moved;
}

bool CAlfaDoc::CommitRotateSelectedSolids(Vec3 center, Vec3 axis, float angle) {
    bool rotated = false;
    for (size_t index : GetSelectedTransformRootIndices()) {
        if (index < objects_.size()) {
            if (auto* group = dynamic_cast<CGroup*>(objects_[index].get())) {
                rotated = group->CommitRotate(center, axis, angle) || rotated;
            } else if (auto* solid = dynamic_cast<CSolid*>(objects_[index].get())) {
                solid->Rotate(center, axis, angle);
                rotated = true;
            }
        }
    }
    if (rotated) {
        UpdateAttachedSketches();
    }
    return rotated;
}

bool CAlfaDoc::CommitScaleSelectedSolids(Vec3 center, Vec3 axis, float factor) {
    if (factor <= 0.0001f) {
        return false;
    }

    bool scaled = false;
    for (size_t index : GetSelectedTransformRootIndices()) {
        if (index < objects_.size()) {
            if (auto* group = dynamic_cast<CGroup*>(objects_[index].get())) {
                scaled = group->CommitScale(center, axis, factor) || scaled;
            } else if (auto* solid = dynamic_cast<CSolid*>(objects_[index].get())) {
                solid->Scale(center, axis, factor);
                scaled = true;
            }
        }
    }
    if (scaled) {
        UpdateAttachedSketches();
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
    if (!build_boolean_shape(operation, body->m_Shape, tool->m_Shape, result_shape)) {
        return false;
    }

    auto result = std::make_unique<CSolid>(result_shape);
    result->m_id = body->m_id;
    result->SetName(boolean_operation_name(operation));
    result->SetColor(body->GetColor());
    result->SetMaterial(body->GetMaterial());
    result->SetMaterialId(body->GetMaterialId());
    result->SetGroupName(body->GetGroupName());
    result->SetVisible(body->IsVisible());
    result->m_LayerID = body->m_LayerID;
    result->CopyOperationTreeFrom(*body);
    const size_t boolean_tool_index = result->AddBooleanToolCopy(*tool);
    if (result->GetNumOperations() > 0) {
        result->SetParametricOperation(result->GetOperationTree().size(),
                                      "boolean",
                                      boolean_operation_name(operation),
                                      {
                                          {"operation", static_cast<double>(operation == BooleanOperation::Union ? 0 : operation == BooleanOperation::Cut ? 1 : 2)},
                                          {"tool", static_cast<double>(boolean_tool_index)}
                                      });
    }
    result->ReBuldMesh();

    objects_[body_index] = std::move(result);
    objects_.erase(objects_.begin() + static_cast<ObjectList::difference_type>(tool_index));

    size_t result_index = body_index;
    if (tool_index < body_index) {
        --result_index;
    }

    selected_object_index_ = result_index;
    selected_object_indices_ = {selected_object_index_};
    active_object_index_ = selected_object_index_;
    has_selected_object_ = true;
    ClearPointSelection();
    UpdateAttachedSketches();
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

    std::vector<std::pair<int, int>> edge_refs;
    if (!all_edges) {
        edge_refs = solid->GetSelectedEdgeRefs();
    }
    std::vector<TopoDS_Edge> edges = all_edges ? solid->GetAllTopoEdges() : solid->GetSelectedTopoEdges();
    edges = unique_edges(edges);
    if (edges.empty()) {
        return false;
    }

    live_fillet_ = std::make_unique<LiveFilletData>();
    live_fillet_->base_shape = solid->m_Shape;
    live_fillet_->edges = std::move(edges);
    live_fillet_->edge_refs = std::move(edge_refs);
    live_fillet_->object_index = solid_index;
    return true;
}

bool CAlfaDoc::HasLiveFillet() const {
    return live_fillet_ && live_fillet_->object_index < objects_.size();
}

std::vector<std::pair<int, int>> CAlfaDoc::GetLiveFilletEdgeRefs() const {
    return live_fillet_ ? live_fillet_->edge_refs : std::vector<std::pair<int, int>>{};
}

std::vector<int> CAlfaDoc::GetLiveFilletCreatedSurfaceIndices() const {
    if (!live_fillet_ || live_fillet_->object_index >= objects_.size()) {
        return {};
    }
    return live_fillet_->created_surface_indices;
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
        live_fillet_->created_surface_indices.clear();
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
            live_fillet_->created_surface_indices =
                generated_face_indices(fillet, live_fillet_->edges, result_shape);
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
    UpdateAttachedSketches();
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
    live_chamfer_->edge_refs = solid->GetSelectedEdgeRefs();
    live_chamfer_->object_index = solid_index;
    return true;
}

bool CAlfaDoc::HasLiveChamfer() const {
    return live_chamfer_ && live_chamfer_->object_index < objects_.size();
}

std::vector<std::pair<int, int>> CAlfaDoc::GetLiveChamferEdgeRefs() const {
    return live_chamfer_ ? live_chamfer_->edge_refs : std::vector<std::pair<int, int>>{};
}

std::vector<int> CAlfaDoc::GetLiveChamferCreatedSurfaceIndices() const {
    if (!live_chamfer_ || live_chamfer_->object_index >= objects_.size()) {
        return {};
    }
    return live_chamfer_->created_surface_indices;
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
        live_chamfer_->created_surface_indices.clear();
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
            live_chamfer_->created_surface_indices =
                generated_face_indices(chamfer, live_chamfer_->edges, result_shape);
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
    UpdateAttachedSketches();
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
        if (index >= objects_.size() || !objects_[index] || !IsObjectVisible(*objects_[index])) {
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

bool CAlfaDoc::GetTransformGizmoCenter(Vec3& center) const {
    if (!HasSelection()) {
        return false;
    }
    if (has_transform_gizmo_origin_) {
        center = transform_gizmo_origin_;
        return true;
    }
    const std::vector<CPoint3d> selected_points = GetSelectedCurvePointPositions();
    if (!selected_points.empty()) {
        Vec3 minimum = to_vec3(selected_points.front());
        Vec3 maximum = minimum;
        for (const CPoint3d& point : selected_points) {
            const Vec3 value = to_vec3(point);
            minimum.x = std::min(minimum.x, value.x);
            minimum.y = std::min(minimum.y, value.y);
            minimum.z = std::min(minimum.z, value.z);
            maximum.x = std::max(maximum.x, value.x);
            maximum.y = std::max(maximum.y, value.y);
            maximum.z = std::max(maximum.z, value.z);
        }
        center = (minimum + maximum) * 0.5f;
        return true;
    }
    return GetSelectionCenter(center);
}

void CAlfaDoc::SetTransformGizmoOrigin(Vec3 origin) {
    transform_gizmo_origin_ = origin;
    has_transform_gizmo_origin_ = true;
}

void CAlfaDoc::ClearTransformGizmoOrigin() {
    has_transform_gizmo_origin_ = false;
    transform_gizmo_origin_ = {};
}

bool CAlfaDoc::PickTransformGizmoOriginAtScreen(
    DomPoint point,
    const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
    float tolerance,
    Vec3& origin) const {
    if (!world_to_screen || tolerance <= 0.0f) {
        return false;
    }

    // Vertices have priority over edges, so a click close to an endpoint
    // always produces an exact and predictable snap.
    float best_vertex_distance = tolerance;
    bool found_vertex = false;
    for (const ObjectPtr& object : objects_) {
        const auto* solid = object ? dynamic_cast<const CSolid*>(object.get()) : nullptr;
        if (!solid || solid->m_Shape.IsNull() || !IsObjectVisible(*solid) || !IsObjectSelectable(*solid)) {
            continue;
        }
        for (TopExp_Explorer explorer(solid->m_Shape, TopAbs_VERTEX); explorer.More(); explorer.Next()) {
            const gp_Pnt p = BRep_Tool::Pnt(TopoDS::Vertex(explorer.Current()));
            const Vec3 world{static_cast<float>(p.X()), static_cast<float>(p.Y()), static_cast<float>(p.Z())};
            DomPoint screen{};
            if (!world_to_screen(world, screen)) {
                continue;
            }
            const float dx = static_cast<float>(screen.x - point.x);
            const float dy = static_cast<float>(screen.y - point.y);
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance <= best_vertex_distance) {
                best_vertex_distance = distance;
                origin = world;
                found_vertex = true;
            }
        }
    }
    if (found_vertex) {
        return true;
    }

    float best_edge_distance = tolerance;
    bool found_edge = false;
    constexpr int kCurveSamples = 64;
    for (const ObjectPtr& object : objects_) {
        const auto* solid = object ? dynamic_cast<const CSolid*>(object.get()) : nullptr;
        if (!solid || solid->m_Shape.IsNull() || !IsObjectVisible(*solid) || !IsObjectSelectable(*solid)) {
            continue;
        }
        for (TopExp_Explorer explorer(solid->m_Shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
            const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
            BRepAdaptor_Curve curve(edge);
            const double first = curve.FirstParameter();
            const double last = curve.LastParameter();
            if (!std::isfinite(first) || !std::isfinite(last) || last <= first) {
                continue;
            }

            DomPoint previous_screen{};
            double previous_parameter = first;
            bool has_previous = false;
            for (int sample = 0; sample <= kCurveSamples; ++sample) {
                const double parameter = first + (last - first)
                    * static_cast<double>(sample) / static_cast<double>(kCurveSamples);
                const gp_Pnt current_world = curve.Value(parameter);
                const Vec3 current{
                    static_cast<float>(current_world.X()),
                    static_cast<float>(current_world.Y()),
                    static_cast<float>(current_world.Z())};
                DomPoint current_screen{};
                if (!world_to_screen(current, current_screen)) {
                    has_previous = false;
                    continue;
                }

                if (has_previous) {
                    const float vx = static_cast<float>(current_screen.x - previous_screen.x);
                    const float vy = static_cast<float>(current_screen.y - previous_screen.y);
                    const float wx = static_cast<float>(point.x - previous_screen.x);
                    const float wy = static_cast<float>(point.y - previous_screen.y);
                    const float length_squared = vx * vx + vy * vy;
                    const float segment_t = length_squared > 0.0001f
                        ? std::clamp((wx * vx + wy * vy) / length_squared, 0.0f, 1.0f)
                        : 0.0f;
                    const float nearest_x = static_cast<float>(previous_screen.x) + vx * segment_t;
                    const float nearest_y = static_cast<float>(previous_screen.y) + vy * segment_t;
                    const float dx = static_cast<float>(point.x) - nearest_x;
                    const float dy = static_cast<float>(point.y) - nearest_y;
                    const float distance = std::sqrt(dx * dx + dy * dy);
                    if (distance <= best_edge_distance) {
                        best_edge_distance = distance;
                        if (curve.GetType() == GeomAbs_Circle) {
                            const gp_Pnt center = curve.Circle().Location();
                            origin = {
                                static_cast<float>(center.X()),
                                static_cast<float>(center.Y()),
                                static_cast<float>(center.Z())};
                        } else {
                            const double nearest_parameter = previous_parameter
                                + (parameter - previous_parameter) * static_cast<double>(segment_t);
                            const gp_Pnt nearest = curve.Value(nearest_parameter);
                            origin = {
                                static_cast<float>(nearest.X()),
                                static_cast<float>(nearest.Y()),
                                static_cast<float>(nearest.Z())};
                        }
                        found_edge = true;
                    }
                }

                previous_screen = current_screen;
                previous_parameter = parameter;
                has_previous = true;
            }
        }
    }
    return found_edge;
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

void CAlfaDoc::EnsureDefaultLayer() {
    if (!m_Layers.empty()) {
        if (!GetLayerByID(Work_layer)) {
            Work_layer = m_Layers.front()->ID();
        }
        return;
    }
    m_Layers.push_back(new CLayer(1, "Default"));
    Work_layer = 1;
}

CLayer* CAlfaDoc::AddLayer(const std::string& name) {
    int next_id = 1;
    for (const CLayer* layer : m_Layers) {
        if (layer) {
            next_id = std::max(next_id, layer->ID() + 1);
        }
    }

    std::string layer_name = name.empty() ? "Layer " + std::to_string(next_id) : name;
    auto* layer = new CLayer(next_id, layer_name);
    m_Layers.push_back(layer);
    Work_layer = next_id;
    return layer;
}

CLayer* CAlfaDoc::GetLayerByID(int layer_id) {
    for (CLayer* layer : m_Layers) {
        if (layer && layer->ID() == layer_id) {
            return layer;
        }
    }
    return nullptr;
}

const CLayer* CAlfaDoc::GetLayerByID(int layer_id) const {
    for (const CLayer* layer : m_Layers) {
        if (layer && layer->ID() == layer_id) {
            return layer;
        }
    }
    return nullptr;
}

int CAlfaDoc::GetWorkLayerID() const {
    if (GetLayerByID(Work_layer)) {
        return Work_layer;
    }
    return m_Layers.empty() || !m_Layers.front() ? 0 : m_Layers.front()->ID();
}

bool CAlfaDoc::SetWorkLayer(int layer_id) {
    if (!GetLayerByID(layer_id)) {
        return false;
    }
    Work_layer = layer_id;
    return true;
}

bool CAlfaDoc::IsLayerVisible(int layer_id) const {
    const CLayer* layer = GetLayerByID(layer_id);
    return !layer || layer->Visible;
}

bool CAlfaDoc::IsLayerSelectable(int layer_id) const {
    const CLayer* layer = GetLayerByID(layer_id);
    return !layer || layer->Selectable;
}

bool CAlfaDoc::IsObjectVisible(const CAlfaObject& object) const {
    return object.IsVisible() && IsLayerVisible(object.m_LayerID);
}

bool CAlfaDoc::IsObjectSelectable(const CAlfaObject& object) const {
    return IsObjectVisible(object) && IsLayerSelectable(object.m_LayerID);
}

bool CAlfaDoc::CreateAssociativeCloneFromSelection() {
    if (selected_object_indices_.size() != 1
        || selected_object_indices_.front() >= objects_.size()) {
        return false;
    }
    CSolid* source = dynamic_cast<CSolid*>(objects_[selected_object_indices_.front()].get());
    if (!source || source->m_Shape.IsNull()) {
        return false;
    }
    EnsureObjectId(*source);
    auto clone = std::make_unique<CAssociativeClone>(source->m_Shape, source->m_id);
    clone->SetName(source->GetName() + " Linked Copy");
    clone->SetGroupName(source->GetGroupName());
    clone->SetVisible(source->IsVisible());
    clone->SetColor(source->GetColor());
    clone->SetMaterial(source->GetMaterial());
    clone->SetMaterialId(source->GetMaterialId());
    clone->m_LayerID = source->m_LayerID;
    clone->InitSurfaces();
    clone->ReBuldMesh();
    AddObject(std::move(clone));
    return true;
}

bool CAlfaDoc::RebuildAssociativeClones(unsigned long source_id) {
    bool rebuilt = false;
    std::set<unsigned long> changed_ids;
    if (source_id != 0) changed_ids.insert(source_id);
    for (size_t pass = 0; pass < objects_.size(); ++pass) {
        bool pass_changed = false;
        for (const ObjectPtr& object : objects_) {
            auto* clone = dynamic_cast<CAssociativeClone*>(object.get());
            if (!clone || (source_id != 0 && changed_ids.count(clone->GetSourceId()) == 0)) {
                continue;
            }
            const CSolid* source = dynamic_cast<const CSolid*>(FindObjectById(clone->GetSourceId()));
            if (!source || source == clone || !clone->RebuildFromSource(*source)) {
                continue;
            }
            changed_ids.insert(clone->m_id);
            pass_changed = true;
            rebuilt = true;
        }
        if (!pass_changed) break;
        if (source_id == 0) break;
    }
    return rebuilt;
}

bool CAlfaDoc::CreateSolidFromTwoSelectedSketches() {
    if (selected_object_indices_.size() != 2) return false;
    CSmartLine* first = nullptr;
    CSmartLine* second = nullptr;
    for (size_t index : selected_object_indices_) {
        if (index >= objects_.size()) return false;
        CSmartLine* sketch = dynamic_cast<CSmartLine*>(objects_[index].get());
        if (!sketch) return false;
        if (!first) first = sketch; else second = sketch;
    }
    if (!first || !second) return false;
    EnsureObjectId(*first);
    EnsureObjectId(*second);
    TopoDS_Shape shape;
    if (!BuildSolidBetweenSketches(*first, *second, shape)) return false;

    // Early versions of the Solid-panel button created the correct B-Rep but
    // then overwrote both source references with the registry defaults (0).
    // Reuse and repair such a body instead of leaving an unfixable duplicate
    // in existing projects.
    for (size_t index = 0; index < objects_.size(); ++index) {
        auto* existing = dynamic_cast<CSolid*>(objects_[index].get());
        const ParametricFunction* operation = existing
            ? existing->GetOperation(0)
            : nullptr;
        if (!operation || operation->ToolId != "SolidTwoSketches") {
            continue;
        }
        unsigned long stored_first_id = 0;
        unsigned long stored_second_id = 0;
        for (const ParametricParameterValue& parameter : operation->Parameters) {
            if (parameter.id == "profile.id") {
                stored_first_id = static_cast<unsigned long>(parameter.value);
            } else if (parameter.id == "section.id") {
                stored_second_id = static_cast<unsigned long>(parameter.value);
            }
        }
        if (stored_first_id != 0 || stored_second_id != 0) {
            continue;
        }
        existing->SetParametricOperation(0, "SolidTwoSketches", "Body by Two Sketches", {
            {"profile.id", static_cast<double>(first->m_id)},
            {"section.id", static_cast<double>(second->m_id)}});
        return RebuildTwoSketchSolid(index);
    }

    auto solid = std::make_unique<CSolid>(shape);
    solid->SetName("Body by Two Sketches");
    solid->SetColor({0.72f, 0.72f, 0.69f});
    solid->SetParametricOperation(0, "SolidTwoSketches", "Body by Two Sketches", {
        {"profile.id", static_cast<double>(first->m_id)},
        {"section.id", static_cast<double>(second->m_id)}});
    solid->InitSurfaces();
    solid->ReBuldMesh();
    AddObject(std::move(solid));
    return true;
}

bool CAlfaDoc::RebuildTwoSketchSolid(size_t object_index) {
    if (object_index >= objects_.size()) return false;
    CSolid* solid = dynamic_cast<CSolid*>(objects_[object_index].get());
    if (!solid || solid->GetNumOperations() < 1) return false;
    const ParametricFunction* operation = solid->GetOperation(0);
    if (!operation || operation->ToolId != "SolidTwoSketches") return false;
    unsigned long first_id = 0;
    unsigned long second_id = 0;
    for (const ParametricParameterValue& parameter : operation->Parameters) {
        if (parameter.id == "profile.id") first_id = static_cast<unsigned long>(parameter.value);
        if (parameter.id == "section.id") second_id = static_cast<unsigned long>(parameter.value);
    }
    const CSmartLine* first = dynamic_cast<const CSmartLine*>(FindObjectById(first_id));
    const CSmartLine* second = dynamic_cast<const CSmartLine*>(FindObjectById(second_id));
    TopoDS_Shape shape;
    if (!first || !second || !BuildSolidBetweenSketches(*first, *second, shape)) return false;
    solid->Clear();
    solid->m_Shape = shape;
    solid->SetParametricOperation(0, "SolidTwoSketches", "Body by Two Sketches", {
        {"profile.id", static_cast<double>(first_id)},
        {"section.id", static_cast<double>(second_id)}});
    solid->InitSurfaces();
    solid->ReBuldMesh();
    return true;
}

size_t CAlfaDoc::ResolveGroupSelectionIndex(size_t object_index) const {
    if (object_index >= objects_.size() || !objects_[object_index]) {
        return object_index;
    }

    size_t resolved_index = object_index;
    std::set<unsigned long> visited_ids;
    while (resolved_index < objects_.size() && objects_[resolved_index]
           && visited_ids.insert(objects_[resolved_index]->m_id).second) {
        const unsigned long object_id = objects_[resolved_index]->m_id;
        size_t parent_index = objects_.size();
        for (size_t i = objects_.size(); i > 0; --i) {
            const size_t group_index = i - 1;
            const auto* group = dynamic_cast<const CGroup*>(objects_[group_index].get());
            if (group_index != resolved_index && group && IsObjectSelectable(*group)
                && group->Contains(object_id)) {
                parent_index = group_index;
                break;
            }
        }
        if (parent_index >= objects_.size()) {
            break;
        }
        resolved_index = parent_index;
    }
    return resolved_index;
}

void CAlfaDoc::AssignObjectToWorkLayer(CAlfaObject& object) const {
    object.m_LayerID = GetWorkLayerID();
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

Material* CAlfaDoc::FindMaterial(const std::string& name, bool case_insensitive) {
    const auto same_name = [&name, case_insensitive](const Material& material) {
        if (!case_insensitive) {
            return material.name == name;
        }
        return std::equal(
            material.name.begin(), material.name.end(),
            name.begin(), name.end(),
            [](unsigned char left, unsigned char right) {
                return std::tolower(left) == std::tolower(right);
            });
    };
    const auto found = std::find_if(materials_.begin(), materials_.end(), same_name);
    return found == materials_.end() ? nullptr : &*found;
}

const Material* CAlfaDoc::FindMaterial(const std::string& name, bool case_insensitive) const {
    return const_cast<CAlfaDoc*>(this)->FindMaterial(name, case_insensitive);
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
        if (auto* solid = dynamic_cast<CSolid*>(object.get())) {
            for (int surface_index = 0; surface_index < solid->GetNumSurfaces(); ++surface_index) {
                const CSurfaceFace* surface = solid->GetSurfaceFace(surface_index);
                if (surface && surface->MaterialOverride.enabled
                    && surface->MaterialOverride.material_id == id) {
                    solid->ClearSurfaceMaterial(surface_index);
                }
            }
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

std::shared_ptr<const CAlfaDoc::Snapshot> CAlfaDoc::CreateSnapshot() const {
    auto snapshot = std::make_shared<Snapshot>();
    snapshot->objects.reserve(objects_.size());
    for (const ObjectPtr& object : objects_) {
        if (!object) {
            snapshot->objects.push_back(nullptr);
            continue;
        }
        ObjectPtr clone = object->Clone();
        if (!clone) {
            return {};
        }

        // Clone() is also used by Copy and may change identity or name.
        // History snapshots must retain the exact document identity.
        clone->m_col = object->m_col;
        clone->m_selected = object->m_selected;
        clone->m_id = object->m_id;
        clone->m_LayerID = object->m_LayerID;
        clone->SetName(object->GetName());
        clone->SetGroupName(object->GetGroupName());
        clone->CAlfaObject::SetVisible(object->IsVisible());
        clone->CAlfaObject::SetColor(object->GetColor());
        clone->SetMaterial(object->GetMaterial());
        clone->SetMaterialId(object->GetMaterialId());
        clone->SetParametricDefinition(
            object->GetParametricToolId(), object->GetParametricParameters());
        snapshot->objects.push_back(std::move(clone));
    }

    snapshot->materials = materials_;
    snapshot->layers.reserve(m_Layers.size());
    for (const CLayer* layer : m_Layers) {
        if (layer) {
            snapshot->layers.push_back(
                {layer->ID(), layer->Name, layer->Visible, layer->Selectable});
        }
    }
    snapshot->next_object_id = next_object_id_;
    snapshot->active_object_index = active_object_index_;
    snapshot->selected_object_index = selected_object_index_;
    snapshot->selected_face_object_index = selected_face_object_index_;
    snapshot->selected_point_index = selected_point_index_;
    snapshot->selected_object_indices = selected_object_indices_;
    snapshot->selected_curve_points = selected_curve_points_;
    snapshot->selected_solid_face_indices = selected_solid_face_indices_;
    snapshot->work_layer = Work_layer;
    snapshot->has_selected_solid_face = has_selected_solid_face_;
    snapshot->has_selected_object = has_selected_object_;
    snapshot->has_selected_point = has_selected_point_;
    snapshot->has_transform_gizmo_origin = has_transform_gizmo_origin_;
    snapshot->transform_gizmo_origin = transform_gizmo_origin_;
    return snapshot;
}

bool CAlfaDoc::RestoreSnapshot(const Snapshot& snapshot) {
    ObjectList restored;
    restored.reserve(snapshot.objects.size());
    for (const ObjectPtr& object : snapshot.objects) {
        if (!object) {
            restored.push_back(nullptr);
            continue;
        }
        ObjectPtr clone = object->Clone();
        if (!clone) {
            return false;
        }
        clone->m_col = object->m_col;
        clone->m_selected = object->m_selected;
        clone->m_id = object->m_id;
        clone->m_LayerID = object->m_LayerID;
        clone->SetName(object->GetName());
        clone->SetGroupName(object->GetGroupName());
        clone->CAlfaObject::SetVisible(object->IsVisible());
        clone->CAlfaObject::SetColor(object->GetColor());
        clone->SetMaterial(object->GetMaterial());
        clone->SetMaterialId(object->GetMaterialId());
        clone->SetParametricDefinition(
            object->GetParametricToolId(), object->GetParametricParameters());
        restored.push_back(std::move(clone));
    }

    objects_ = std::move(restored);
    materials_ = snapshot.materials;
    for (CLayer* layer : m_Layers) {
        delete layer;
    }
    m_Layers.clear();
    for (const Snapshot::LayerState& state : snapshot.layers) {
        auto* layer = new CLayer(state.id, state.name);
        layer->Visible = state.visible;
        layer->Selectable = state.selectable;
        m_Layers.push_back(layer);
    }
    if (m_Layers.empty()) {
        EnsureDefaultLayer();
    }

    next_object_id_ = snapshot.next_object_id;
    active_object_index_ = snapshot.active_object_index;
    selected_object_index_ = snapshot.selected_object_index;
    selected_face_object_index_ = snapshot.selected_face_object_index;
    selected_point_index_ = snapshot.selected_point_index;
    selected_object_indices_ = snapshot.selected_object_indices;
    selected_curve_points_ = snapshot.selected_curve_points;
    selected_solid_face_indices_ = snapshot.selected_solid_face_indices;
    Work_layer = snapshot.work_layer;
    has_selected_solid_face_ = snapshot.has_selected_solid_face;
    has_selected_object_ = snapshot.has_selected_object;
    has_selected_point_ = snapshot.has_selected_point;
    has_transform_gizmo_origin_ = snapshot.has_transform_gizmo_origin;
    transform_gizmo_origin_ = snapshot.transform_gizmo_origin;
    hidden_selection_highlight_index_ = static_cast<size_t>(-1);

    live_extrude_.reset();
    live_polyline_extrude_.reset();
    live_polyline_revolve_.reset();
    live_fillet_.reset();
    live_chamfer_.reset();
    draft_face_.reset();
    live_thick_solid_.reset();
    return true;
}

void CAlfaDoc::EnsureActivePolyline() {
    if (objects_.empty()) {
        auto polyline = std::make_unique<CPolyline>("Curve 1");
        EnsureObjectId(*polyline);
        AssignDefaultMaterial(*polyline);
        AssignObjectToWorkLayer(*polyline);
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
        EnsureObjectId(*polyline);
        AssignDefaultMaterial(*polyline);
        AssignObjectToWorkLayer(*polyline);
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
        EnsureObjectId(*spline);
        AssignDefaultMaterial(*spline);
        AssignObjectToWorkLayer(*spline);
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
        EnsureObjectId(*spline);
        AssignDefaultMaterial(*spline);
        AssignObjectToWorkLayer(*spline);
        objects_.push_back(std::move(spline));
        active_object_index_ = objects_.size() - 1;
    }
}

void CAlfaDoc::AssignDefaultMaterial(CAlfaObject& object) {
    // A reference image owns its texture and opacity.  Replacing it with the
    // work material would lose both when it is added to the document.
    if (dynamic_cast<CReferenceImage*>(&object)) {
        object.SetMaterialId(0);
        return;
    }
    if (object.GetMaterialId() != 0) {
        if (const Material* material = FindMaterial(object.GetMaterialId())) {
            object.SetMaterial(*material);
        }
        return;
    }

    if (!materials_.empty()) {
        const Color object_color = object.GetColor();
        object.SetMaterial(materials_.front());
        object.SetColor(object_color);
    }
}
