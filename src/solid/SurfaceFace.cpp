#include "SurfaceFace.h"
#include "Solid.h"
#include "SolidTool.h"
#include "../iges/SplineCurve.h"
#include "../Net.h"
#include "../CAlfaDoc.h"
#include "../SurfaceUVMapping.h"
#include "../Line2D.h"

#include "Poly_Triangulation.hxx"
#include <Standard_OutOfMemory.hxx>
#include <BRepTools.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <AIS_ListOfInteractive.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Standard_Handle.hxx>
#include <AIS_InteractiveContext.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <Poly.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BOPAlgo_RemoveFeatures.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <ShapeFix_Shape.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <Geom_BezierCurve.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Poly_Triangle.hxx>
#include <gp_Vec.hxx>
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <utility>

void Step(char* text);

namespace {
bool mesh_plane_normal(const CMesh3D* mesh, Vec3& normal)
{
	if (!mesh)
		return false;

	const std::vector<Vec3>& vertices = mesh->GetVertices();
	const std::vector<CMesh3D::Face>& faces = mesh->GetFaces();
	if (vertices.size() < 3 || faces.empty())
		return false;

	Vec3 origin{};
	bool has_origin = false;
	for (const CMesh3D::Face& face : faces) {
		if (face.deleted || face.corners.size() < 3)
			continue;
		for (size_t i = 1; i + 1 < face.corners.size(); ++i) {
			const size_t i0 = face.corners[0].v;
			const size_t i1 = face.corners[i].v;
			const size_t i2 = face.corners[i + 1].v;
			if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
				continue;
			const Vec3 edge_a = vertices[i1] - vertices[i0];
			const Vec3 edge_b = vertices[i2] - vertices[i0];
			normal = normalize(cross(edge_a, edge_b));
			if (dot(normal, normal) > 0.000001f) {
				origin = vertices[i0];
				has_origin = true;
				break;
			}
		}
		if (has_origin)
			break;
	}

	if (!has_origin)
		return false;

	float max_span = 1.0f;
	Vec3 min_point = vertices.front();
	Vec3 max_point = vertices.front();
	for (const Vec3& vertex : vertices) {
		min_point.x = std::min(min_point.x, vertex.x);
		min_point.y = std::min(min_point.y, vertex.y);
		min_point.z = std::min(min_point.z, vertex.z);
		max_point.x = std::max(max_point.x, vertex.x);
		max_point.y = std::max(max_point.y, vertex.y);
		max_point.z = std::max(max_point.z, vertex.z);
	}
	max_span = std::max({max_span, max_point.x - min_point.x, max_point.y - min_point.y, max_point.z - min_point.z});
	const float tolerance = std::max(0.001f, max_span * 0.001f);

	for (const Vec3& vertex : vertices) {
		if (std::fabs(dot(vertex - origin, normal)) > tolerance)
			return false;
	}
	return true;
}

bool add_mesh_boundary_edges(const CMesh3D* mesh, std::vector<CSplineCurve*>& edges)
{
	if (!mesh)
		return false;

	const std::vector<Vec3>& vertices = mesh->GetVertices();
	const std::vector<CMesh3D::Face>& faces = mesh->GetFaces();
	std::map<std::pair<size_t, size_t>, int> edge_counts;
	for (const CMesh3D::Face& face : faces) {
		if (face.deleted || face.corners.size() < 3)
			continue;
		for (size_t i = 0; i < face.corners.size(); ++i) {
			const size_t a = face.corners[i].v;
			const size_t b = face.corners[(i + 1) % face.corners.size()].v;
			if (a >= vertices.size() || b >= vertices.size() || a == b)
				continue;
			++edge_counts[{std::min(a, b), std::max(a, b)}];
		}
	}

	bool added = false;
	for (const auto& entry : edge_counts) {
		if (entry.second != 1)
			continue;

		const Vec3& a = vertices[entry.first.first];
		const Vec3& b = vertices[entry.first.second];
		if (dot(b - a, b - a) <= 0.000001f)
			continue;

		CSplineCurve* spline = new CSplineCurve;
		CPoint3d p0(a.x, a.y, a.z);
		CPoint3d p1(b.x, b.y, b.z);
		spline->AddPoint(&p0, false);
		spline->AddPoint(&p1, false);
		if (spline->Build()) {
			edges.push_back(spline);
			added = true;
		} else {
			delete spline;
		}
	}
	return added;
}

double sampled_iso_length(CSurfaceFace* surface,
                          double fixed_parameter,
                          double first,
                          double last,
                          bool vary_u)
{
	if (!surface || last <= first)
		return 0.0;

	constexpr int sample_count = 24;
	double length = 0.0;
	CPoint8d previous;
	bool has_previous = false;
	for (int i = 0; i < sample_count; ++i) {
		const double alpha = static_cast<double>(i) / static_cast<double>(sample_count - 1);
		const double current = first + (last - first) * alpha;
		CPoint8d point;
		const bool ok = vary_u
			? surface->GetPoint(current, fixed_parameter, &point)
			: surface->GetPoint(fixed_parameter, current, &point);
		if (!ok) {
			has_previous = false;
			continue;
		}
		if (has_previous) {
			const double dx = point.x - previous.x;
			const double dy = point.y - previous.y;
			const double dz = point.z - previous.z;
			length += std::sqrt(dx * dx + dy * dy + dz * dz);
		}
		previous = point;
		has_previous = true;
	}
	return length;
}

bool is_regular_uv_mesh_surface(const TopoDS_Face& face)
{
	try {
		BRepAdaptor_Surface surface(face);
		const GeomAbs_SurfaceType type = surface.GetType();
		return type == GeomAbs_Cylinder
			|| type == GeomAbs_Cone
			|| type == GeomAbs_Sphere
			|| type == GeomAbs_Torus
			|| type == GeomAbs_SurfaceOfRevolution;
	} catch (const Standard_Failure&) {
		return false;
	}
}

bool build_regular_uv_mesh(CSurfaceFace* surface, float deflection)
{
	if (!surface || surface->m_Face.IsNull())
		return false;

	Standard_Real u_min = 0.0;
	Standard_Real u_max = 0.0;
	Standard_Real v_min = 0.0;
	Standard_Real v_max = 0.0;
	try {
		BRepTools::UVBounds(TopoDS::Face(surface->m_Face), u_min, u_max, v_min, v_max);
	} catch (const Standard_Failure&) {
		return false;
	}

	if (!std::isfinite(u_min) || !std::isfinite(u_max)
		|| !std::isfinite(v_min) || !std::isfinite(v_max)
		|| u_max <= u_min || v_max <= v_min) {
		return false;
	}

	const double mid_u = (u_min + u_max) * 0.5;
	const double mid_v = (v_min + v_max) * 0.5;
	const double u_length = sampled_iso_length(surface, mid_v, u_min, u_max, true);
	const double v_length = sampled_iso_length(surface, mid_u, v_min, v_max, false);
	const double longest = std::max({u_length, v_length, 1.0});
	const int qty_max = std::max(4, static_cast<int>(20.0 / std::max(0.001f, deflection)));
	const auto quantity_for = [qty_max, longest](double length) {
		int quantity = static_cast<int>(std::round(static_cast<double>(qty_max) * length / longest)) + 1;
		quantity = std::clamp(quantity, CSurfaceFace::m_QtyMin, qty_max + 1);
		return quantity;
	};
	const int qty_u = quantity_for(u_length);
	const int qty_v = quantity_for(v_length);

	std::vector<Vec3> vertices;
	std::vector<UV> uvs;
	std::vector<Vec3> normals;
	vertices.reserve(static_cast<size_t>(qty_u * qty_v));
	uvs.reserve(static_cast<size_t>(qty_u * qty_v));
	normals.reserve(static_cast<size_t>(qty_u * qty_v));

	for (int v = 0; v < qty_v; ++v) {
		const double v_alpha = static_cast<double>(v) / static_cast<double>(qty_v - 1);
		const double vv = v_min + (v_max - v_min) * v_alpha;
		for (int u = 0; u < qty_u; ++u) {
			const double u_alpha = static_cast<double>(u) / static_cast<double>(qty_u - 1);
			const double uu = u_min + (u_max - u_min) * u_alpha;
			CPoint8d point;
			if (!surface->GetPoint(uu, vv, &point))
				return false;
			vertices.push_back({static_cast<float>(point.x),
			                    static_cast<float>(point.y),
			                    static_cast<float>(point.z)});
			uvs.push_back({static_cast<float>(uu), static_cast<float>(vv)});
			normals.push_back(normalize({static_cast<float>(point.l),
			                             static_cast<float>(point.m),
			                             static_cast<float>(point.n)}));
		}
	}

	const auto index = [qty_u](int u, int v) {
		return static_cast<size_t>(v * qty_u + u);
	};
	std::vector<CMesh3D::Face> faces;
	faces.reserve(static_cast<size_t>((qty_u - 1) * (qty_v - 1)));
	const bool reverse = TopoDS::Face(surface->m_Face).Orientation() == TopAbs_REVERSED;
	for (int v = 0; v + 1 < qty_v; ++v) {
		for (int u = 0; u + 1 < qty_u; ++u) {
			if (reverse) {
				faces.push_back({index(u, v),
				                 index(u, v + 1),
				                 index(u + 1, v + 1),
				                 index(u + 1, v)});
			} else {
				faces.push_back({index(u, v),
				                 index(u + 1, v),
				                 index(u + 1, v + 1),
				                 index(u, v + 1)});
			}
		}
	}

	if (!surface->pMesh3D)
		surface->pMesh3D = new CMesh3D;
	surface->pMesh3D->SetName("Solid Face");
	surface->pMesh3D->SetColor({0.64f, 0.70f, 0.58f});
	if (!surface->pMesh3D->SetGeometry(std::move(vertices), std::move(faces), std::move(uvs), std::move(normals)))
		return false;

	surface->Umin = u_min;
	surface->Umax = u_max;
	surface->Vmin = v_min;
	surface->Vmax = v_max;
	surface->m_QtyU = qty_u;
	surface->m_QtyV = qty_v;
	surface->IsTrimmed = true;
	surface->IsInitMesh = true;
	return true;
}

std::unique_ptr<CPolyline> copy_polyline_points(const CPolyline* source)
{
	if (!source)
		return {};

	auto copy = std::make_unique<CPolyline>();
	for (const CPoint3d& point : source->GetPoints())
		copy->AddPoint(point);
	return copy;
}

Face2D polyline_to_face2d(const CPolyline* line)
{
	Face2D face;
	if (!line)
		return face;

	for (const CPoint3d& point : line->GetPoints())
		face.verts.push_back({ point.x, point.y });
	if (face.verts.size() > 1 && EqualPoint2(face.verts.front(), face.verts.back(), EPS2D))
		face.verts.pop_back();
	return face;
}

double polygon_area_2d(const Face2D& face)
{
	double area = 0.0;
	if (face.verts.size() < 3)
		return 0.0;
	for (size_t i = 0; i < face.verts.size(); ++i) {
		const cVec2& a = face.verts[i];
		const cVec2& b = face.verts[(i + 1) % face.verts.size()];
		area += a.x * b.y - b.x * a.y;
	}
	return area * 0.5;
}

cVec2 polygon_centroid_2d(const Face2D& face)
{
	cVec2 center{};
	if (face.verts.empty())
		return center;

	double signed_area2 = 0.0;
	for (size_t i = 0; i < face.verts.size(); ++i) {
		const cVec2& a = face.verts[i];
		const cVec2& b = face.verts[(i + 1) % face.verts.size()];
		const double cross_value = a.x * b.y - b.x * a.y;
		signed_area2 += cross_value;
		center.x += (a.x + b.x) * cross_value;
		center.y += (a.y + b.y) * cross_value;
	}

	if (std::fabs(signed_area2) > 1.0e-12) {
		center.x /= 3.0 * signed_area2;
		center.y /= 3.0 * signed_area2;
		return center;
	}

	for (const cVec2& point : face.verts) {
		center.x += point.x;
		center.y += point.y;
	}
	center.x /= static_cast<double>(face.verts.size());
	center.y /= static_cast<double>(face.verts.size());
	return center;
}

cVec2 mesh_face_center_uv(const CMesh3D::Face& face, const std::vector<Vec3>& vertices)
{
	cVec2 center{};
	int count = 0;
	for (const MeshCorner& corner : face.corners) {
		if (corner.v >= vertices.size())
			continue;
		center.x += vertices[corner.v].x;
		center.y += vertices[corner.v].y;
		++count;
	}
	if (count > 0) {
		center.x /= static_cast<double>(count);
		center.y /= static_cast<double>(count);
	}
	return center;
}

cVec2 choose_boundary_keep_point(const CMesh3D* mesh, const std::vector<Face2D>& loops, size_t outer_index)
{
	if (mesh && outer_index < loops.size()) {
		const std::vector<Vec3>& vertices = mesh->GetVertices();
		const std::vector<CMesh3D::Face>& faces = mesh->GetFaces();
		for (const CMesh3D::Face& face : faces) {
			if (face.deleted || face.corners.size() < 3)
				continue;
			const cVec2 center = mesh_face_center_uv(face, vertices);
			if (ClassifyPointInFace2(loops[outer_index], center, EPS2D) == PFP_OUTSIDE)
				continue;

			bool inside_hole = false;
			for (size_t i = 0; i < loops.size(); ++i) {
				if (i == outer_index)
					continue;
				if (ClassifyPointInFace2(loops[i], center, EPS2D) != PFP_OUTSIDE) {
					inside_hole = true;
					break;
				}
			}
			if (!inside_hole)
				return center;
		}
	}

	return outer_index < loops.size() ? polygon_centroid_2d(loops[outer_index]) : cVec2{};
}

bool loop_matches_uv_bounds(CSurfaceFace* surface, const Face2D& loop)
{
	if (!surface || surface->m_Face.IsNull() || loop.verts.size() < 3)
		return false;

	Standard_Real u_min = 0.0;
	Standard_Real u_max = 0.0;
	Standard_Real v_min = 0.0;
	Standard_Real v_max = 0.0;
	BRepTools::UVBounds(TopoDS::Face(surface->m_Face), u_min, u_max, v_min, v_max);
	if (!std::isfinite(u_min) || !std::isfinite(u_max)
		|| !std::isfinite(v_min) || !std::isfinite(v_max)
		|| u_max <= u_min || v_max <= v_min)
		return false;

	const double eps = EPS2D * 10.0;
	const double bounds_area = (u_max - u_min) * (v_max - v_min);
	if (bounds_area <= eps * eps)
		return false;

	if (std::fabs(std::fabs(polygon_area_2d(loop)) - bounds_area) > bounds_area * 0.01)
		return false;

	bool touches_u_min = false;
	bool touches_u_max = false;
	bool touches_v_min = false;
	bool touches_v_max = false;
	for (const cVec2& point : loop.verts) {
		const bool on_u_min = std::fabs(point.x - u_min) <= eps;
		const bool on_u_max = std::fabs(point.x - u_max) <= eps;
		const bool on_v_min = std::fabs(point.y - v_min) <= eps;
		const bool on_v_max = std::fabs(point.y - v_max) <= eps;
		if (!on_u_min && !on_u_max && !on_v_min && !on_v_max)
			return false;
		touches_u_min = touches_u_min || on_u_min;
		touches_u_max = touches_u_max || on_u_max;
		touches_v_min = touches_v_min || on_v_min;
		touches_v_max = touches_v_max || on_v_max;
	}

	return touches_u_min && touches_u_max && touches_v_min && touches_v_max;
}

bool trim_mesh_by_surface_boundary(CMesh3D* mesh,
                                   CSurfaceFace* surface,
                                   const std::vector<CPolyline*>& source_lines,
                                   double delta)
{
	if (!mesh || !surface || source_lines.empty())
		return false;

	std::vector<std::unique_ptr<CPolyline>> storage;
	std::vector<CPolyline*> lines;
	storage.reserve(source_lines.size());
	lines.reserve(source_lines.size());
	for (const CPolyline* source : source_lines) {
		std::unique_ptr<CPolyline> copy = copy_polyline_points(source);
		if (!copy || copy->GetPointCount() < 2)
			continue;
		lines.push_back(copy.get());
		storage.push_back(std::move(copy));
	}
	if (lines.empty())
		return false;

	std::vector<CPolyline*> loops;
	CPolyline::JoinMultuLines(&lines, &loops, delta);
	if (loops.empty())
		return false;

	std::vector<CPolyline*> closed_loops;
	std::vector<Face2D> loop_faces;
	closed_loops.reserve(loops.size());
	loop_faces.reserve(loops.size());
	for (CPolyline* loop : loops) {
		if (!loop || loop->GetPointCount() < 3)
			continue;
		if (loop->P(0)->DistTo(loop->PLast()) <= delta)
			loop->SetClosed(true);
		if (!loop->PutOnSurface(surface))
			continue;

		Face2D loop_face = polyline_to_face2d(loop);
		if (loop_face.verts.size() < 3 || std::fabs(polygon_area_2d(loop_face)) <= 1.0e-9)
			continue;
		closed_loops.push_back(loop);
		loop_faces.push_back(std::move(loop_face));
	}
	if (closed_loops.empty())
		return false;

	size_t outer_index = 0;
	double outer_area = 0.0;
	for (size_t i = 0; i < loop_faces.size(); ++i) {
		const double area = std::fabs(polygon_area_2d(loop_faces[i]));
		if (area > outer_area) {
			outer_area = area;
			outer_index = i;
		}
	}

	const bool outer_matches_uv_bounds = loop_matches_uv_bounds(surface, loop_faces[outer_index]);
	if (closed_loops.size() == 1
		&& (loop_faces[outer_index].verts.size() <= 4 || outer_matches_uv_bounds))
		return false;

	const cVec2 keep = choose_boundary_keep_point(mesh, loop_faces, outer_index);
	CPoint3d pc(keep.x, keep.y, 0.0);
	bool changed = false;
	if (!outer_matches_uv_bounds)
		changed = mesh->TrimByPline(closed_loops[outer_index], pc);
	for (size_t i = 0; i < closed_loops.size(); ++i) {
		if (i == outer_index)
			continue;
		changed = mesh->TrimByPline(closed_loops[i], pc) || changed;
	}
	return changed;
}
}
#include <TopAbs_Orientation.hxx>
#include <TopLoc_Location.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <GCPnts_UniformAbscissa.hxx>
#include <Standard_Failure.hxx>
#include <gp_Pnt2d.hxx>

#include <algorithm>
#include <cmath>

#include "../iges/SplineCurve.h"

namespace {
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

	const float t = std::clamp((static_cast<float>(point.x - start.x) * dx + static_cast<float>(point.y - start.y) * dy) / length_sq, 0.0f, 1.0f);
	const float closest_x = static_cast<float>(start.x) + t * dx;
	const float closest_y = static_cast<float>(start.y) + t * dy;
	const float px = static_cast<float>(point.x) - closest_x;
	const float py = static_cast<float>(point.y) - closest_y;
	return std::sqrt(px * px + py * py);
}
}

//using namespace std;

#if 0
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKVCAF.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKVrml.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKStl.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKBRep.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKIGES.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKShHealing.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKSTEP.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKSTEP209.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKSTEPAttr.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKSTEPBase.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKBool.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKCAF.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKCDF.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKernel.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKV3d.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKG3d.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKOpenGl.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKPrim.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKLCAF.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKGeomAlgo.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKGeomBase.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKService.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKG2d.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKXSBase.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKMath.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKFeat.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKFillet.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKOffset.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKTopAlgo.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKBO.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKHLR.lib")
#pragma comment(lib, "Libs/OpenCascade/Libs/win/lib/TKMesh.lib")
#endif


bool IsEqual(double val1, double val2, float delta);

int CSurfaceFace::m_QtyMin = 3;

CSurfaceFace::CSurfaceFace()
{
	Alloc();
}

void CSurfaceFace::Alloc()
{
	m_ID = 0;
	m_Mesh = NULL;
	pMesh3D = new CMesh3D;
	IsSelected = false;

	IsInitEdges = false;
	IsInitMesh = false;
	IsTrimmed = false;
	TypeGeom = 0;
	m_TypeMesh = REGULAR_MESH;
	m_BoundLine = NULL;
	lenEdgeMax = -1;
	m_Net = NULL;
}


CSurfaceFace::~CSurfaceFace()
{
	for (CSplineCurve* edge : m_Edges)
		delete edge;
	m_Edges.clear();
	m_TopoEdges.clear();
	for (CPolyline* polyline : Polylines)
		delete polyline;
	Polylines.clear();
	for (CSplineCurve* bound : BoundSpl)
		delete bound;
	BoundSpl.clear();

	if (m_BoundLine)
		delete m_BoundLine;
	m_BoundLine = NULL;
	if (pMesh3D)
		delete pMesh3D;
	pMesh3D = NULL;
	if (m_Mesh)
		delete m_Mesh;
	m_Mesh = NULL;
	if (m_Net)
		delete m_Net;
}

CSurfaceFace::CSurfaceFace(TopoDS_Shape shape)
{
	Alloc();
	m_Face = shape;
}


bool CSurfaceFace::BuldMeshTriangle(float Deflection, float AngDeflection)
{
	TopLoc_Location aLoc;
	const TopoDS_Face theFace = TopoDS::Face(m_Face);
	Handle(Poly_Triangulation) aTriangulation =
		BRep_Tool::Triangulation(theFace, aLoc, Poly_MeshPurpose_NONE);
	if (aTriangulation.IsNull()) {
		const Standard_Real linear_deflection = std::max<Standard_Real>(Deflection, 0.0001);
		const Standard_Real angular_deflection =
			std::clamp<Standard_Real>(AngDeflection, 0.01, 1.0);
		BRepMesh_IncrementalMesh mesher(
			theFace,
			linear_deflection,
			false,
			angular_deflection,
			true);
		mesher.Perform();
		aTriangulation = BRep_Tool::Triangulation(theFace, aLoc, Poly_MeshPurpose_NONE);
	}
	if (aTriangulation.IsNull())
		return false;

	std::vector<Vec3> vertices;
	vertices.reserve(static_cast<size_t>(aTriangulation->NbNodes()));
	std::vector<UV> uvs;
	if (aTriangulation->HasUVNodes()) {
		uvs.reserve(static_cast<size_t>(aTriangulation->NbNodes()));
	}
	const gp_Trsf& transform = aLoc.Transformation();
	for (Standard_Integer nodeIndex = 1; nodeIndex <= aTriangulation->NbNodes(); ++nodeIndex) {
		gp_Pnt point = aTriangulation->Node(nodeIndex);
		point.Transform(transform);
		vertices.push_back({
			static_cast<float>(point.X()),
			static_cast<float>(point.Y()),
			static_cast<float>(point.Z())
		});
		if (aTriangulation->HasUVNodes()) {
			const gp_Pnt2d uv = aTriangulation->UVNode(nodeIndex);
			uvs.push_back({static_cast<float>(uv.X()), static_cast<float>(uv.Y())});
		}
	}

	std::vector<CMesh3D::Face> faces;
	faces.reserve(static_cast<size_t>(aTriangulation->NbTriangles()));
	const bool reverseWinding = theFace.Orientation() == TopAbs_REVERSED;
	for (Standard_Integer triangleIndex = 1; triangleIndex <= aTriangulation->NbTriangles(); ++triangleIndex) {
		Standard_Integer n1 = 0;
		Standard_Integer n2 = 0;
		Standard_Integer n3 = 0;
		aTriangulation->Triangle(triangleIndex).Get(n1, n2, n3);
		if (reverseWinding) {
			std::swap(n2, n3);
		}
		faces.push_back({
			static_cast<size_t>(n1 - 1),
			static_cast<size_t>(n2 - 1),
			static_cast<size_t>(n3 - 1)
		});
	}

	if (!pMesh3D) {
		pMesh3D = new CMesh3D;
	}
	pMesh3D->SetName("Solid Face");
	pMesh3D->SetColor({0.64f, 0.70f, 0.58f});
	if (!pMesh3D->SetGeometry(std::move(vertices), std::move(faces), std::move(uvs))) {
		return false;
	}

	IsInitMesh = true;

/*	auto meshCopy = pMesh3D->Clone();
	auto* M_cpy = dynamic_cast<CMesh3D*>(meshCopy.get());

	char buffer[256];
	sprintf(buffer, "C:\\Temp\\mesh_uv_%d.obj", m_ID);
	if (M_cpy && M_cpy->PutOnSurface(this)) {
		M_cpy->ExportToObj(buffer);
	}*/

	return true;
}


bool CSurfaceFace::BuldMesh(float Deflection, bool MeshQuadro)
{
//	PrepareEdges(Deflection);
	// Prepare edges of Surfaces
	//Analiz of Surfaces
//	DefineTypeMesh();

	return BuldMeshTriangle(Deflection, 0.3);
}

bool CSurfaceFace::IsPlanar() const
{
	if (m_Face.IsNull())
		return false;
	try {
		const TopoDS_Face face = TopoDS::Face(m_Face);
		BRepAdaptor_Surface surface(face);
		if (surface.GetType() == GeomAbs_Plane)
			return true;
	} catch (const Standard_Failure&) {
	}

	Vec3 normal{};
	return mesh_plane_normal(pMesh3D, normal);
}

bool CSurfaceFace::GetCenterAndNormal(Vec3& center, Vec3& normal) const
{
	if (!pMesh3D)
		return false;

	Vec3 min_point{};
	Vec3 max_point{};
	if (!pMesh3D->GetBounds(min_point, max_point))
		return false;
	center = (min_point + max_point) * 0.5f;

	if (m_Face.IsNull())
		return false;
	try {
		const TopoDS_Face face = TopoDS::Face(m_Face);
		BRepAdaptor_Surface surface(face);
		if (surface.GetType() == GeomAbs_Plane) {
			const gp_Dir direction = surface.Plane().Axis().Direction();
			const float sign = face.Orientation() == TopAbs_REVERSED ? -1.0f : 1.0f;
			normal = normalize({static_cast<float>(direction.X()) * sign,
			                    static_cast<float>(direction.Y()) * sign,
			                    static_cast<float>(direction.Z()) * sign});
			return dot(normal, normal) > 0.000001f;
		}
	} catch (const Standard_Failure&) {
	}

	return mesh_plane_normal(pMesh3D, normal);
}

void CSurfaceFace::UpdateRGB()
{
	// The old Dom-3D code rebuilt a custom mesh container here.
	// Dom3D Pro will fill CMesh3D directly from OpenCascade triangulation.
}


bool CSurfaceFace::InitEdges()
{
	for (CSplineCurve* edge : m_Edges)
		delete edge;
	m_Edges.clear();
	m_TopoEdges.clear();
	for (CSplineCurve* bound : BoundSpl)
		delete bound;
	BoundSpl.clear();
	lenEdgeMax = 0.0f;

	if (m_Face.IsNull())
		return false;

	const TopoDS_Face face = TopoDS::Face(m_Face);
	for (TopExp_Explorer edge_explorer(face, TopAbs_EDGE); edge_explorer.More(); edge_explorer.Next()) {
		const TopoDS_Edge edge = TopoDS::Edge(edge_explorer.Current());
		if (edge.IsNull() || BRep_Tool::Degenerated(edge))
			continue;

		try {
			BRepAdaptor_Curve curve(edge);
			const Standard_Real first = curve.FirstParameter();
			const Standard_Real last = curve.LastParameter();
			if (!std::isfinite(first) || !std::isfinite(last) || last <= first)
				continue;

			const int sample_count = 18;
			CSplineCurve* spline = new CSplineCurve;
			for (int i = 0; i < sample_count; ++i) {
				const double t = first + (last - first) * static_cast<double>(i) / static_cast<double>(sample_count - 1);
				const gp_Pnt p = curve.Value(t);
				CPoint3d point(p.X(), p.Y(), p.Z());
				spline->AddPoint(&point, false);
			}

			if (edge.Orientation() == TopAbs_REVERSED)
				spline->Revers();

			if (spline->np() >= 2 && spline->Build()) {
				lenEdgeMax = std::max(lenEdgeMax, static_cast<float>(spline->GetLength()));
				m_Edges.push_back(spline);
				m_TopoEdges.push_back(edge);
			} else {
				delete spline;
			}
		} catch (const Standard_Failure&) {
			// A face may contain a singular/degenerated edge (common for
			// revolved profiles touching the axis). Keep the other valid
			// edges selectable instead of discarding the whole face.
		}
	}

	if (m_Edges.empty()) {
		add_mesh_boundary_edges(pMesh3D, m_Edges);
	}

/*

	int n = 0;
	for (CPolyline* edge : Polylines) {
		if (edge) {
			auto curveCopy = edge->Clone();
			auto* curveUV = dynamic_cast<CPolyline*>(curveCopy.get());
			curveUV->PutOnSurface(this);
			sprintf(buffer, "edge%d%d", m_ID, n++);
			curveUV->printToFile(buffer);
		}
	}
*/

	const bool has_render_edges = !m_Edges.empty();
	const bool has_net_edges = InitEdges3DCoat();
	IsInitEdges = has_render_edges || has_net_edges;
	return IsInitEdges;
}
bool CSurfaceFace::InitEdges3DCoat()
{
	for (int i = 0; i < Polylines.size(); i++)
		delete Polylines[i];
	Polylines.clear();
	for (int i = 0; i < BoundSpl.size(); i++)
		delete BoundSpl[i];
	BoundSpl.clear();
	TopoDS_Face F1 = TopoDS::Face(m_Face);
	Handle(Geom_Surface) surf = BRep_Tool::Surface(F1);
	char buf[1120];
	if (F1.IsNull())
		return false;
	Standard_Real U1;
	Standard_Real U2;
	Standard_Real V1;
	Standard_Real V2;
	BRepTools::UVBounds(F1, U1, U2, V1, V2);
	Umin = U1;
	Umax = U2;
	Vmin = V1;
	Vmax = V2;
	gp_Pnt P;
	const Standard_Real U = (U1 + U2) / 2.0;
	const Standard_Real V = (V1 + V2) / 2.0;
	gp_Vec D1U;
	gp_Vec D1V;
	surf->D1(U, V, P, D1U, D1V);
	CVector vx(D1U.X(), D1U.Y(), D1U.Z());
	CVector vy(D1V.X(), D1V.Y(), D1V.Z());
	CVector vz(&vx, &vy);
	if (m_Face.Orientation() == TopAbs_REVERSED)
		vz.Revers();


//	m_p0.Set(P.X(), P.Y(), P.Z());
//	Norm0.Set(vz.l, vz.m, vz.n);

	int np = 7;
	float dU = U2 - U1;
	float stepU = dU / (float)(np - 1);
	float dV = V2 - V1;
	float stepV = dV / (float)(np - 1);

	CSplineCurve* spl = new CSplineCurve(np);
	for (int i = 0; i < np; i++) {
		Standard_Real Ui = U1 + stepU * i;
		Standard_Real Vi = V1;
		gp_Pnt Pi;
		surf->D0(Ui, Vi, Pi);
		spl->Pnt(i)->x = Pi.X();
		spl->Pnt(i)->y = Pi.Y();
		spl->Pnt(i)->z = Pi.Z();
	}
	spl->Build();// Spl0
	BoundSpl.push_back(spl);
	//=======================
	spl = new CSplineCurve(np);
	for (int i = 0; i < np; i++) {
		Standard_Real Ui = U1 + stepU * i;
		Standard_Real Vi = V2;
		gp_Pnt Pi;
		surf->D0(Ui, Vi, Pi);
		spl->Pnt(i)->x = Pi.X();
		spl->Pnt(i)->y = Pi.Y();
		spl->Pnt(i)->z = Pi.Z();
	}
	spl->Build();// Spl1
	BoundSpl.push_back(spl);

	//===========
	spl = new CSplineCurve(np);
	for (int i = 0; i < np; i++) {
		Standard_Real Ui = U1;
		Standard_Real Vi = V1 + stepV * i;
		gp_Pnt Pi;
		surf->D0(Ui, Vi, Pi);
		spl->Pnt(i)->x = Pi.X();
		spl->Pnt(i)->y = Pi.Y();
		spl->Pnt(i)->z = Pi.Z();
	}
	spl->Build();
	BoundSpl.push_back(spl);
	//===========
	spl = new CSplineCurve(np);
	for (int i = 0; i < np; i++) {
		Standard_Real Ui = U2;
		Standard_Real Vi = V1 + stepV * i;
		gp_Pnt Pi;
		surf->D0(Ui, Vi, Pi);
		spl->Pnt(i)->x = Pi.X();
		spl->Pnt(i)->y = Pi.Y();
		spl->Pnt(i)->z = Pi.Z();
	}
	spl->Build();
	BoundSpl.push_back(spl);
	//	Step(" ------ InitEdges  ------- ");
	//	for (int j = 0; j < BoundSpl.Count(); j++)
	//		BoundSpl[j]->print();

	std::vector<CPolyline*> plines;
	GetEdges(plines);
	float lenEdgeMax2 = 0;
	for (int j = 0; j < plines.size(); j++) {
//		plines[j]->m_Thickness = 1.0;
		plines[j]->m_col = 0xDD0FDD00;
		Polylines.push_back(plines[j]);
		double len = plines[j]->GetLength();
		if (len > lenEdgeMax2)
			lenEdgeMax2 = len;
		//	plines[j]->print();
	}

//	if (SingleFace)
		lenEdgeMax = lenEdgeMax2;

	IsInitEdges = true;
	return true;
}



void CSurfaceFace::RenderEdges(bool selected, const std::vector<int>& selected_edge_indices) const
{
	const float width = selected ? 1.15f : 0.9f;
	const float r = 0.035f;
	const float g = 0.038f;
	const float b = 0.040f;

	for (int i = 0; i < static_cast<int>(m_Edges.size()); ++i) {
		CSplineCurve* edge = m_Edges[static_cast<size_t>(i)];
		if (!edge)
			continue;
		if (std::find(selected_edge_indices.begin(), selected_edge_indices.end(), i) != selected_edge_indices.end())
			continue;
		else
			edge->Draw(r, g, b, width, 16, false, false);
	}

	for (int edge_index : selected_edge_indices) {
		if (edge_index < 0 || edge_index >= static_cast<int>(m_Edges.size()))
			continue;
		CSplineCurve* edge = m_Edges[static_cast<size_t>(edge_index)];
		if (!edge)
			continue;
		edge->Draw(1.0f, 0.22f, 0.12f, 5.5f, 24, false, true);
	}
}

void CSurfaceFace::PreviewTranslate(Vec3 delta)
{
	if (pMesh3D)
		pMesh3D->Translate(delta);

	CPoint3d from(0.0, 0.0, 0.0);
	CPoint3d to(delta.x, delta.y, delta.z);
	for (CSplineCurve* edge : m_Edges) {
		if (edge) {
			edge->Move(&from, &to);
			edge->Update();
		}
	}
}

void CSurfaceFace::PreviewRotate(Vec3 center, Vec3 axis, float angle)
{
	const Vec3 unit_axis = normalize(axis);
	if (std::fabs(angle) <= 0.000001f || dot(unit_axis, unit_axis) <= 0.000001f)
		return;

	if (pMesh3D)
		pMesh3D->Rotate(center, unit_axis, angle);

	CPoint3d p0(center.x, center.y, center.z);
	CPoint3d p1(center.x + unit_axis.x, center.y + unit_axis.y, center.z + unit_axis.z);
	for (CSplineCurve* edge : m_Edges) {
		if (edge)
			edge->Rotate(&p0, &p1, angle);
	}
}

void CSurfaceFace::PreviewScale(Vec3 center, Vec3 axis, float factor)
{
	if (factor <= 0.000001f || std::fabs(factor - 1.0f) <= 0.000001f)
		return;

	if (pMesh3D)
		pMesh3D->Scale(center, axis, factor);

	const Vec3 unit_axis = normalize(axis);
	const bool uniform = dot(unit_axis, unit_axis) <= 0.000001f;
	const double sx = uniform || std::fabs(unit_axis.x) > 0.5f ? factor : 1.0;
	const double sy = uniform || std::fabs(unit_axis.y) > 0.5f ? factor : 1.0;
	const double sz = uniform || std::fabs(unit_axis.z) > 0.5f ? factor : 1.0;
	CPoint3d p0(center.x, center.y, center.z);
	for (CSplineCurve* edge : m_Edges) {
		if (edge)
			edge->Zoom(&p0, sx, sy, sz);
	}
}

bool CSurfaceFace::HitTestEdgeScreen(DomPoint point,
                                     const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                     float tolerance,
                                     int& edge_index) const
{
	bool found = false;
	float best_distance = tolerance;
	edge_index = -1;

	for (int edge_i = 0; edge_i < static_cast<int>(m_Edges.size()); ++edge_i) {
		CSplineCurve* edge = m_Edges[static_cast<size_t>(edge_i)];
		if (!edge || edge->np() < 1)
			continue;

		const int steps = std::max(1, (edge->np() - 1) * 16);
		DomPoint previous_screen{};
		bool has_previous = false;

		for (int step = 0; step <= steps; ++step) {
			const double s = (static_cast<double>(edge->np() - 1) * static_cast<double>(step)) / static_cast<double>(steps);
			CPoint3d p;
			if (!edge->GetPoint(s, &p))
				continue;

			DomPoint current_screen{};
			if (!world_to_screen({static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z)}, current_screen)) {
				has_previous = false;
				continue;
			}

			if (has_previous) {
				const float distance = distance_to_screen_segment(point, previous_screen, current_screen);
				if (distance < best_distance) {
					best_distance = distance;
					edge_index = edge_i;
					found = true;
				}
			}

			previous_screen = current_screen;
			has_previous = true;
		}
	}

	return found;
}

const TopoDS_Edge* CSurfaceFace::GetTopoEdge(int edge_index) const
{
	if (edge_index < 0 || edge_index >= static_cast<int>(m_TopoEdges.size()))
		return nullptr;
	return &m_TopoEdges[static_cast<size_t>(edge_index)];
}

bool CSurfaceFace::GetEdgeEndpoints(int edge_index, Vec3& start, Vec3& end) const
{
	if (edge_index < 0 || edge_index >= static_cast<int>(m_Edges.size()))
		return false;

	CSplineCurve* edge = m_Edges[static_cast<size_t>(edge_index)];
	if (!edge || edge->np() < 2)
		return false;

	CPoint3d p0;
	CPoint3d p1;
	if (!edge->GetPoint(0.0, &p0) || !edge->GetPoint(static_cast<double>(edge->np() - 1), &p1))
		return false;

	start = {static_cast<float>(p0.x), static_cast<float>(p0.y), static_cast<float>(p0.z)};
	end = {static_cast<float>(p1.x), static_cast<float>(p1.y), static_cast<float>(p1.z)};
	return dot(end - start, end - start) > 0.000001f;
}

void CSurfaceFace::PrepareEdges(float Deflection)
{
	if (!IsInitEdges)
		InitEdges();
	if (Polylines.empty())
		return;
	std::vector<CPolyline*> Edges2;
	Edges2.push_back(Polylines[0]);
	bool NeedRevers = false;
	Polylines.erase(Polylines.begin());
	while (Polylines.size()) {
		float dist_min = 1e15;
		int j_min = 0;
		CPolyline* LastEdge = *Edges2.rbegin();
		bool NeedRevers = false;
		for (int j = 0; j < Polylines.size(); j++) {
			float dist = LastEdge->PLast()->DistTo(Polylines[j]->P(0));
			if (dist < dist_min) {
				dist_min = dist;
				NeedRevers = false;
				j_min = j;
			}
			float dist2 = LastEdge->PLast()->DistTo(Polylines[j]->PLast());
			if (dist2 < dist_min) {
				dist_min = dist2;
				j_min = j;
				NeedRevers = true;
			}
		}
		//=============
		Edges2.push_back(Polylines[j_min]);
		if (NeedRevers)
			Polylines[j_min]->Revers();
		Polylines.erase(Polylines.begin() + j_min);
	}

	int QtyMax = (int)20.0 / Deflection;
	if (QtyMax < 4)
		QtyMax = 4;

	for (CPolyline* pLine : Edges2) {
		CSplineCurve spl;
		spl.Create(pLine);
		CPolyline* pline = new CPolyline;
		float len = spl.GetLength();
		int Qty = QtyMax / lenEdgeMax * len;
		if (IsEven(Qty))
			Qty++;
		if (Qty < m_QtyMin)
			Qty = m_QtyMin;
		spl.MakePolylineByQtyKnots(pline, Qty);
		pline->TmpLen = spl.GetLength();
		pline->Dir1.l = spl.P(0)->l;
		pline->Dir1.m = spl.P(0)->m;
		pline->Dir1.n = spl.P(0)->n;
		Polylines.push_back(pline);
	}

	char buf[120];
	for (CPolyline* pLine : Polylines) {
		if (m_ID == 3 || m_ID == 6) {
			sprintf(buf, "Polylines%d", m_ID);
			pLine->printToFile(buf);
		}

	}
}

int CSurfaceFace::GetPreparedPolylineCount() const
{
	return static_cast<int>(Polylines.size());
}

int CSurfaceFace::GetPreparedPolylinePointCount(int edge_index) const
{
	if (edge_index < 0 || edge_index >= static_cast<int>(Polylines.size()) || !Polylines[static_cast<size_t>(edge_index)])
		return 0;
	return static_cast<int>(Polylines[static_cast<size_t>(edge_index)]->np());
}

bool CSurfaceFace::GetPreparedPolylineEndpoints(int edge_index, Vec3& start, Vec3& end) const
{
	if (edge_index < 0 || edge_index >= static_cast<int>(Polylines.size()))
		return false;
	const CPolyline* polyline = Polylines[static_cast<size_t>(edge_index)];
	if (!polyline || polyline->GetPointCount() < 2)
		return false;

	const std::vector<CPoint3d>& points = polyline->GetPoints();
	const CPoint3d& p0 = points.front();
	const CPoint3d& p1 = points.back();
	start = { static_cast<float>(p0.x), static_cast<float>(p0.y), static_cast<float>(p0.z) };
	end = { static_cast<float>(p1.x), static_cast<float>(p1.y), static_cast<float>(p1.z) };
	return dot(end - start, end - start) > 0.000001f;
}

bool CSurfaceFace::GetPreparedPolylinePoints(int edge_index, std::vector<CPoint3d>& points) const
{
	if (edge_index < 0 || edge_index >= static_cast<int>(Polylines.size()))
		return false;
	const CPolyline* polyline = Polylines[static_cast<size_t>(edge_index)];
	if (!polyline || polyline->GetPointCount() < 2)
		return false;

	points = polyline->GetPoints();
	return points.size() >= 2;
}

bool CSurfaceFace::SetPreparedPolylinePointCount(int edge_index, int point_count)
{
	if (point_count < 2 || edge_index < 0 || edge_index >= static_cast<int>(Polylines.size()))
		return false;
	CPolyline* polyline = Polylines[static_cast<size_t>(edge_index)];
	if (!polyline || polyline->GetPointCount() < 2)
		return false;
	if (static_cast<int>(polyline->GetPointCount()) == point_count)
		return true;

	const std::vector<CPoint3d> source = polyline->GetPoints();
	std::vector<double> length_at(source.size(), 0.0);
	for (size_t i = 1; i < source.size(); ++i) {
		const double dx = source[i].x - source[i - 1].x;
		const double dy = source[i].y - source[i - 1].y;
		const double dz = source[i].z - source[i - 1].z;
		length_at[i] = length_at[i - 1] + std::sqrt(dx * dx + dy * dy + dz * dz);
	}

	const double total_length = length_at.back();
	if (total_length <= 1.0e-9)
		return false;

	std::vector<CPoint3d> resampled;
	resampled.reserve(static_cast<size_t>(point_count));
	for (int i = 0; i < point_count; ++i) {
		const double target = total_length * static_cast<double>(i) / static_cast<double>(point_count - 1);
		size_t segment = 1;
		while (segment + 1 < length_at.size() && length_at[segment] < target)
			++segment;

		const double segment_start = length_at[segment - 1];
		const double segment_end = length_at[segment];
		const double segment_length = segment_end - segment_start;
		const double alpha = segment_length > 1.0e-12 ? (target - segment_start) / segment_length : 0.0;
		const CPoint3d& a = source[segment - 1];
		const CPoint3d& b = source[segment];
		resampled.emplace_back(a.x + (b.x - a.x) * alpha,
		                       a.y + (b.y - a.y) * alpha,
		                       a.z + (b.z - a.z) * alpha);
	}

	polyline->Clear();
	for (const CPoint3d& point : resampled)
		polyline->AddPoint(point);
	polyline->TmpLen = polyline->GetLength();
	return true;
}

bool CSurfaceFace::SetPreparedPolylinePoints(int edge_index, const std::vector<CPoint3d>& points)
{
	if (points.size() < 2 || edge_index < 0 || edge_index >= static_cast<int>(Polylines.size()))
		return false;
	CPolyline* polyline = Polylines[static_cast<size_t>(edge_index)];
	if (!polyline)
		return false;

	polyline->Clear();
	for (const CPoint3d& point : points)
		polyline->AddPoint(point);
	polyline->TmpLen = polyline->GetLength();
	return true;
}

void CSurfaceFace::DumpPreparedPolylinesToScene() const
{
	CAlfaDoc* pDoc = GetAlfaDoc();
	if (!pDoc)
		return;

	int debug_index = 0;
	for (CPolyline* pLine : Polylines) {
		if (!pLine)
			continue;

		auto line_copy = pLine->Clone();
		if (!line_copy)
			continue;

		line_copy->SetName("Surface edge debug " + std::to_string(m_ID) + "." + std::to_string(debug_index++));
		line_copy->SetColor({ 1.0f, 0.12f, 0.05f });
		pDoc->AddObject(std::move(line_copy));
	}
}

bool CSurfaceFace::BuildTrimmingMesh(CSolid* psol, float Deflection){
	TopoDS_Face F1 = TopoDS::Face(m_Face);
	if (F1.IsNull())
		return false;
	if (is_regular_uv_mesh_surface(F1) && build_regular_uv_mesh(this, Deflection))
		return true;
	Handle(Geom_Surface) surf = BRep_Tool::Surface(F1);
	if (surf.IsNull())
		return false;
	if (BoundSpl.size() < 4 || Polylines.empty())
		return build_regular_uv_mesh(this, Deflection);
	int QtyS = 2;
	int QtyT = 2;
	int QtyMax = (int)20.0 / Deflection;
	if (QtyMax < 4)
		QtyMax = 4;
	float len = BoundSpl[0]->GetLength();
	int Qty1 = QtyMax / lenEdgeMax * len;
	if (IsEven(Qty1))
		Qty1++;
	if (Qty1 < m_QtyMin)
		Qty1 = m_QtyMin;
	QtyS = Qty1;
	len = BoundSpl[2]->GetLength();
	int Qty2 = QtyMax / lenEdgeMax * len;
	if (IsEven(Qty2))
		Qty2++;
	if (Qty2 < m_QtyMin)
		Qty2 = m_QtyMin;
	QtyT = Qty2;

	if (TypeGeom == SPHERES_SURF && Polylines.size() < 3) {
		QtyS = QtyT = (MAX(QtyS, QtyT)) * 2.0;
	}

	if (!m_Net)
		m_Net = new CNet;
	int rez = m_Net->BuildNetByTwoQty(this, QtyS, QtyT);
	if (rez)
		return false;

	if (m_Face.Orientation() == TopAbs_REVERSED)
		m_Net->ReversPoints();
//	pMesh3D->Clear();
	m_Net->BuildMesh3D(pMesh3D);
	IsTrimmed = false;
//  ====== Trimming =====
	pMesh3D->PutOnSurface(this);
	if (m_ID == 3) {
		pMesh3D->ExportToObj("c:\\temp\\Mesh3D_3.obj");
		for (int i = 0; i < Polylines.size(); i++) {
			Polylines[i]->printToFile("Polylines3D_ID_3");
			Polylines[i]->PutOnSurface(this);
			Polylines[i]->printToFile("Trim_Polylines_ID_3");
			Polylines[i]->RestoreTo3DFromUVSurface(this);
		}
	}


	double delta = 0.01;
	trim_mesh_by_surface_boundary(pMesh3D, this, Polylines, delta);


	if (m_ID == 3) 
		pMesh3D->ExportToObj("c:\\temp\\Mesh3D_3Trimmed.obj");

	pMesh3D->RestoreTo3DFromUVSurface(this);


	IsTrimmed = true;
	IsInitMesh = true;
	return true;
}
void GetPointFromCurve(TopoDS_Edge& ed, int gtystep, CPolyline* pl)
{
	Standard_Real f, l, prm;
	TopLoc_Location Loc;
	Handle(Geom_Curve) C = BRep_Tool::Curve(ed, Loc, f, l);
	if (C.IsNull())
		return;

	const int NECHANT = gtystep + 1;
	Standard_Real delta = (l - f) / NECHANT * 0.123456;
	float step = (l - f) / float(NECHANT);

	for (int i = 0; i <= NECHANT; i++) {
		prm = f + step * i;
		gp_Pnt pnt = C->Value(prm);
		CPoint3d p3d(pnt.X(), pnt.Y(), pnt.Z());
		pl->AddPoint(&p3d);
	}
}

void CSurfaceFace::GetEdges(std::vector<CPolyline*>& plines)
{
	for (TopExp_Explorer aExpFace(m_Face, TopAbs_FACE); aExpFace.More(); aExpFace.Next())
	{
		TopoDS_Face curFace = TopoDS::Face(aExpFace.Current());
		for (TopExp_Explorer wExpl(curFace, TopAbs_EDGE); wExpl.More(); wExpl.Next())
		{
			TopoDS_Shape aSelShape = wExpl.Current();
			int gtystep = 5;
			TopoDS_Edge ed = TopoDS::Edge(aSelShape);
			CPolyline* pl = new CPolyline;
			GetPointFromCurve(ed, gtystep, pl);
			if (pl->np() < 2)
				continue;
			plines.push_back(pl);
		}
	}
}

bool CSurfaceFace::IsBoundLine(CPolyline* line)
{
	float deltaMax = line->GetLength() * 0.01;
	int IndMidle = (int)line->np() / 2;
	for (int i = 0; i < BoundSpl.size(); i++) {
		double dist1 = BoundSpl[i]->GetDistMin(line->P(0));
		double dist2 = BoundSpl[i]->GetDistMin(line->P(IndMidle));
		double dist3 = BoundSpl[i]->GetDistMin(line->PLast());
		if (dist1 < deltaMax && dist2 < deltaMax && dist3 < deltaMax)
			return true;
	}

	return false;
}
