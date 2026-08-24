#include "SurfaceFace.h"
#include "../FillContour.h"
#include "Solid.h"
#include "SolidTool.h"
#include "../iges/SplineCurve.h"
#include "../Net.h"
#include "../CAlfaDoc.h"
#include "../SurfaceUVMapping.h"
#include "../Line2D.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <GL/gl.h>

#include "Poly_Triangulation.hxx"
#include <Standard_OutOfMemory.hxx>
#include <BRepTools.hxx>
#include <BRepTools_WireExplorer.hxx>
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
#include <BRepCheck_Analyzer.hxx>
#include <Geom_BezierCurve.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <TColgp_HArray1OfPnt.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRep_Tool.hxx>
#include <ElSLib.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Poly_Triangle.hxx>
#include <TopAbs_State.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Vec.hxx>
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

Material ComposeSurfaceMaterial(
	const Material& substrate,
	const SurfaceMaterialOverride& override_data)
{
	Material base = override_data.enabled
		? override_data.material : substrate;
	if (!override_data.coating_enabled)
		return base;

	const Material& film = override_data.coating_material;
	const float coverage = std::clamp(film.alpha, 0.0f, 1.0f);
	const auto blend_color = [coverage](Color under, Color over) {
		return Color{
			under.r * (1.0f - coverage) + over.r * coverage,
			under.g * (1.0f - coverage) + over.g * coverage,
			under.b * (1.0f - coverage) + over.b * coverage};
	};

	Material result = film;
	result.diffuse = blend_color(base.diffuse, film.diffuse);
	result.ambient = blend_color(base.ambient, film.ambient);
	result.emission = blend_color(base.emission, film.emission);
	// Film attached to an opaque board remains an opaque visible surface.
	// Its source alpha is coverage, not volume transparency.
	result.alpha = base.alpha;
	result.id = override_data.coating_material_id;
	result.name = film.name + " over " + base.name;
	return result;
}

void Step(const char* text);

namespace {
bool is_untrimmed_planar_quad(const TopoDS_Face& face)
{
	if (face.IsNull())
		return false;

	try {
		BRepAdaptor_Surface surface(face);
		if (surface.GetType() != GeomAbs_Plane)
			return false;

		Standard_Real u_min = 0.0;
		Standard_Real u_max = 0.0;
		Standard_Real v_min = 0.0;
		Standard_Real v_max = 0.0;
		BRepTools::UVBounds(face, u_min, u_max, v_min, v_max);
		if (!std::isfinite(u_min) || !std::isfinite(u_max)
			|| !std::isfinite(v_min) || !std::isfinite(v_max)
			|| u_max <= u_min || v_max <= v_min) {
			return false;
		}

		int edge_count = 0;
		for (TopExp_Explorer edge_explorer(face, TopAbs_EDGE);
			edge_explorer.More(); edge_explorer.Next()) {
			const TopoDS_Edge edge = TopoDS::Edge(edge_explorer.Current());
			if (!edge.IsNull() && !BRep_Tool::Degenerated(edge))
				++edge_count;
		}
		if (edge_count != 4)
			return false;

		const double span = std::max(u_max - u_min, v_max - v_min);
		const double tolerance = std::max(1.0e-7, span * 1.0e-7);
		unsigned int corner_mask = 0;
		std::vector<std::pair<double, double>> unique_vertices;
		for (TopExp_Explorer vertex_explorer(face, TopAbs_VERTEX);
			vertex_explorer.More(); vertex_explorer.Next()) {
			const TopoDS_Vertex vertex = TopoDS::Vertex(vertex_explorer.Current());
			double u = 0.0;
			double v = 0.0;
			ElSLib::Parameters(surface.Plane(), BRep_Tool::Pnt(vertex), u, v);

			const bool at_u_min = std::fabs(u - u_min) <= tolerance;
			const bool at_u_max = std::fabs(u - u_max) <= tolerance;
			const bool at_v_min = std::fabs(v - v_min) <= tolerance;
			const bool at_v_max = std::fabs(v - v_max) <= tolerance;
			if (!(at_u_min || at_u_max) || !(at_v_min || at_v_max))
				return false;

			bool duplicate = false;
			for (const auto& existing : unique_vertices) {
				if (std::fabs(existing.first - u) <= tolerance
					&& std::fabs(existing.second - v) <= tolerance) {
					duplicate = true;
					break;
				}
			}
			if (!duplicate)
				unique_vertices.emplace_back(u, v);

			if (at_u_min && at_v_min) corner_mask |= 1u;
			if (at_u_max && at_v_min) corner_mask |= 2u;
			if (at_u_max && at_v_max) corner_mask |= 4u;
			if (at_u_min && at_v_max) corner_mask |= 8u;
		}

		return unique_vertices.size() == 4 && corner_mask == 15u;
	} catch (const Standard_Failure&) {
		return false;
	}
}

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

bool is_complete_spherical_face(const TopoDS_Face& face)
{
	if (face.IsNull())
		return false;
	try {
		BRepAdaptor_Surface surface(face);
		if (surface.GetType() != GeomAbs_Sphere)
			return false;

		Standard_Real u_min = 0.0;
		Standard_Real u_max = 0.0;
		Standard_Real v_min = 0.0;
		Standard_Real v_max = 0.0;
		BRepTools::UVBounds(face, u_min, u_max, v_min, v_max);
		constexpr double pi = 3.14159265358979323846;
		const double tolerance = 1.0e-6;
		if (std::fabs((u_max - u_min) - 2.0 * pi) > tolerance
			|| std::fabs((v_max - v_min) - pi) > tolerance) {
			return false;
		}

		// A complete sphere has only its duplicated parameterisation seam and
		// two degenerated pole edges. Extra real edges mean a spherical patch or
		// a cutout, which still requires contour trimming.
		int non_degenerated_edges = 0;
		for (TopExp_Explorer edges(face, TopAbs_EDGE);
			 edges.More(); edges.Next()) {
			const TopoDS_Edge edge = TopoDS::Edge(edges.Current());
			if (!edge.IsNull() && !BRep_Tool::Degenerated(edge))
				++non_degenerated_edges;
		}
		return non_degenerated_edges <= 2;
	} catch (const Standard_Failure&) {
		return false;
	}
}

GeomAbs_SurfaceType surface_type_of(const TopoDS_Face& face)
{
	try {
		BRepAdaptor_Surface surface(face);
		return surface.GetType();
	} catch (const Standard_Failure&) {
		return GeomAbs_OtherSurface;
	}
}

double target_quad_step(float deflection, GeomAbs_SurfaceType surface_type)
{
	const double base_step = std::clamp(static_cast<double>(deflection), 0.25, 20.0);
//	return base_step * 0.72;
	switch (surface_type) {
	case GeomAbs_Plane:
		return base_step * 0.72;
	case GeomAbs_Cylinder:
		return base_step/ 1.7;
	case GeomAbs_Cone:
	case GeomAbs_Sphere:
	case GeomAbs_Torus:
	case GeomAbs_SurfaceOfRevolution:
//		return base_step * 1.4;
		return base_step * 0.8;
	default:
		return base_step;
	}
}

int mesh_point_quantity_for_length(double length, float deflection, GeomAbs_SurfaceType surface_type)
{
	if (!std::isfinite(length) || length <= 0.0)
		return CSurfaceFace::m_QtyMin;

	const double step = target_quad_step(deflection, surface_type);
	const int max_quantity = std::max(7, static_cast<int>(std::ceil(160.0 / std::max(0.25, step))) + 1);
	int quantity = static_cast<int>(std::ceil(length / step)) + 1;
	quantity = std::clamp(quantity, CSurfaceFace::m_QtyMin, max_quantity);
	if (IsEven(quantity))
		++quantity;
	return quantity;
}

int normalized_quadro_point_quantity(double length,
	                                  double maximum_edge_length,
	                                  float deflection)
{
	if (!std::isfinite(length) || length <= 0.0
		|| !std::isfinite(maximum_edge_length) || maximum_edge_length <= 0.0
		|| !std::isfinite(deflection) || deflection <= 0.0f) {
		return CSurfaceFace::m_QtyMin;
	}

	// Original 3DCoat-compatible Low Poly rule. Deflection is 1 / Density,
	// so the longest edge receives approximately 20 * Density intervals.
	int maximum_quantity = static_cast<int>(20.0 / deflection);
	maximum_quantity = std::max(maximum_quantity, 4);
	int quantity = static_cast<int>(
		static_cast<double>(maximum_quantity) * length / maximum_edge_length);
	if (IsEven(quantity))
		++quantity;
	return std::max(quantity, CSurfaceFace::m_QtyMin);
}

void adjust_mesh_quantities_from_prepared_edges(CSurfaceFace* surface,
                                                double u_min,
                                                double u_max,
                                                double v_min,
                                                double v_max,
                                                int& qty_u,
                                                int& qty_v)
{
	if (!surface || u_max <= u_min || v_max <= v_min)
		return;

	const double uv_span = std::max(u_max - u_min, v_max - v_min);
	const double edge_eps = std::max(uv_span * 1.0e-4, 1.0e-7);

	for (int edge_index = 0; edge_index < surface->GetPreparedPolylineCount(); ++edge_index) {
		std::vector<CPoint3d> source_points;
		if (!surface->GetPreparedPolylinePoints(edge_index, source_points) || source_points.size() < 2)
			continue;
		CPolyline uv_edge;
		for (const CPoint3d& point : source_points)
			uv_edge.AddPoint(point);
		if (!uv_edge.PutOnSurface(surface) || uv_edge.GetPointCount() < 2)
			continue;
		double edge_u_min = uv_edge.GetPoints().front().x;
		double edge_u_max = edge_u_min;
		double edge_v_min = uv_edge.GetPoints().front().y;
		double edge_v_max = edge_v_min;
		for (const CPoint3d& point : uv_edge.GetPoints()) {
			edge_u_min = std::min(edge_u_min, point.x);
			edge_u_max = std::max(edge_u_max, point.x);
			edge_v_min = std::min(edge_v_min, point.y);
			edge_v_max = std::max(edge_v_max, point.y);
		}

		const int point_count = static_cast<int>(uv_edge.GetPointCount());
		const double edge_u_span = edge_u_max - edge_u_min;
		const double edge_v_span = edge_v_max - edge_v_min;
		const double edge_u_mid = (edge_u_min + edge_u_max) * 0.5;
		const double edge_v_mid = (edge_v_min + edge_v_max) * 0.5;
		const bool on_v_bound = std::fabs(edge_v_mid - v_min) <= edge_eps
			|| std::fabs(edge_v_mid - v_max) <= edge_eps;
		const bool on_u_bound = std::fabs(edge_u_mid - u_min) <= edge_eps
			|| std::fabs(edge_u_mid - u_max) <= edge_eps;

		const bool u_direction_edge = edge_v_span <= edge_eps && on_v_bound;
		const bool v_direction_edge = edge_u_span <= edge_eps && on_u_bound;
		if (u_direction_edge) {
			qty_u = std::max(qty_u, point_count);
			continue;
		}
		if (v_direction_edge) {
			qty_v = std::max(qty_v, point_count);
			continue;
		}

		const double dominant_ratio = 2.5;
		if (edge_u_span > edge_v_span * dominant_ratio)
			qty_u = std::max(qty_u, point_count);
		else if (edge_v_span > edge_u_span * dominant_ratio)
			qty_v = std::max(qty_v, point_count);
	}
}

bool build_regular_uv_mesh(CSurfaceFace* surface, float deflection)
{
	if (!surface || surface->m_Face.IsNull())
		return false;
	CNet net;
	// The adaptive net controls both the visible silhouette and interpolation
	// of highlights.  Use half of the general mesh deflection here so smooth
	// standalone surfaces do not look faceted at the scene tessellation value.
	if (net.Build(surface, static_cast<double>(deflection) * 0.5) != 0)
		return false;
	if (!surface->pMesh3D)
		surface->pMesh3D = new CMesh3D;
	surface->pMesh3D->SetName("Solid Face");
	surface->pMesh3D->SetColor({0.64f, 0.70f, 0.58f});
	if (TopoDS::Face(surface->m_Face).Orientation() == TopAbs_REVERSED)
		net.ReversPoints();
	if (!net.BuildMesh3D(surface->pMesh3D))
		return false;

	surface->IsTrimmed = false;
	surface->IsInitMesh = true;
	return true;
}

size_t active_face_count(const CMesh3D* mesh)
{
	if (!mesh)
		return 0;

	size_t count = 0;
	for (const CMesh3D::Face& face : mesh->GetFaces()) {
		if (!face.deleted && face.corners.size() >= 3)
			++count;
	}
	return count;
}

bool trim_removed_too_much(size_t source_count, size_t result_count, GeomAbs_SurfaceType surface_type)
{
	if (source_count == 0)
		return false;
	if (result_count == 0)
		return true;

	const bool fragile_periodic_surface = surface_type == GeomAbs_Cylinder
		|| surface_type == GeomAbs_Cone
		|| surface_type == GeomAbs_Sphere
		|| surface_type == GeomAbs_Torus;
	if (fragile_periodic_surface && result_count * 20 < source_count)
		return true;

	if (!fragile_periodic_surface && result_count * 4 < source_count)
		return true;

	return false;
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
	try {
		BRepTools::UVBounds(TopoDS::Face(surface->m_Face), u_min, u_max, v_min, v_max);
	} catch (const Standard_Failure&) {
		return false;
	}
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

bool uv_bounds_for_surface(CSurfaceFace* surface,
                           double& u_min,
                           double& u_max,
                           double& v_min,
                           double& v_max);

bool loop_touches_uv_bounds(CSurfaceFace* surface, const Face2D& loop)
{
	if (!surface || loop.verts.empty())
		return false;

	double u_min = 0.0;
	double u_max = 0.0;
	double v_min = 0.0;
	double v_max = 0.0;
	if (!uv_bounds_for_surface(surface, u_min, u_max, v_min, v_max))
		return false;

	const double eps = std::max(std::max(u_max - u_min, v_max - v_min) * 1.0e-5, EPS2D * 10.0);
	for (const cVec2& point : loop.verts) {
		if (std::fabs(point.x - u_min) <= eps
			|| std::fabs(point.x - u_max) <= eps
			|| std::fabs(point.y - v_min) <= eps
			|| std::fabs(point.y - v_max) <= eps) {
			return true;
		}
	}
	return false;
}

bool loop_is_single_inner_cut(CSurfaceFace* surface, const Face2D& loop)
{
	if (!surface || surface->m_Face.IsNull() || loop.verts.size() < 3)
		return false;
	if (!is_regular_uv_mesh_surface(TopoDS::Face(surface->m_Face)))
		return false;
	if (loop_touches_uv_bounds(surface, loop))
		return false;

	double u_min = 0.0;
	double u_max = 0.0;
	double v_min = 0.0;
	double v_max = 0.0;
	if (!uv_bounds_for_surface(surface, u_min, u_max, v_min, v_max))
		return false;

	const double bounds_area = (u_max - u_min) * (v_max - v_min);
	const double loop_area = std::fabs(polygon_area_2d(loop));
	return bounds_area > 1.0e-12 && loop_area > 1.0e-12 && loop_area < bounds_area * 0.95;
}

bool loop_contains_occt_out_faces(CSurfaceFace* surface, const CMesh3D* mesh, const Face2D& loop)
{
	if (!surface || !mesh || surface->m_Face.IsNull() || loop.verts.size() < 3)
		return false;

	const TopoDS_Face topo_face = TopoDS::Face(surface->m_Face);
	const std::vector<Vec3>& vertices = mesh->GetVertices();
	const std::vector<CMesh3D::Face>& faces = mesh->GetFaces();
	size_t outside_count = 0;
	size_t inside_count = 0;
	for (const CMesh3D::Face& mesh_face : faces) {
		if (mesh_face.deleted || mesh_face.corners.size() < 3)
			continue;
		const cVec2 center = mesh_face_center_uv(mesh_face, vertices);
		if (ClassifyPointInFace2(loop, center, EPS2D) == PFP_OUTSIDE)
			continue;
		try {
			BRepClass_FaceClassifier classifier(topo_face, gp_Pnt2d(center.x, center.y), EPS2D, Standard_False);
			if (classifier.State() == TopAbs_OUT)
				++outside_count;
			else
				++inside_count;
		} catch (const Standard_Failure&) {
		}
	}
	return outside_count > 0 && outside_count > inside_count;
}

bool loop_contains_loop(const Face2D& container, const Face2D& candidate)
{
	if (container.verts.size() < 3 || candidate.verts.size() < 3)
		return false;

	const cVec2 center = polygon_centroid_2d(candidate);
	return ClassifyPointInFace2(container, center, EPS2D) != PFP_OUTSIDE;
}

bool regular_surface_needs_boundary_trim(CSurfaceFace* surface,
                                         const std::vector<CPolyline*>& source_lines,
                                         double delta)
{
	if (!surface || source_lines.empty())
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

	std::vector<Face2D> loop_faces;
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
		loop_faces.push_back(std::move(loop_face));
	}
	if (loop_faces.empty()) {
		for (CPolyline* loop : loops) {
			if (!loop || loop->GetPointCount() < 2)
				continue;
			if (loop->P(0)->DistTo(loop->PLast()) > delta)
				return true;
		}
		return false;
	}

	if (loop_faces.size() == 1)
		return !loop_matches_uv_bounds(surface, loop_faces.front());

	for (size_t i = 0; i < loop_faces.size(); ++i) {
		for (size_t j = 0; j < loop_faces.size(); ++j) {
			if (i == j)
				continue;
			if (loop_contains_loop(loop_faces[i], loop_faces[j]))
				return true;
		}
	}
	return false;
}

cVec2 choose_point_outside_loop(const CMesh3D* mesh, const Face2D& loop)
{
	if (mesh) {
		const std::vector<Vec3>& vertices = mesh->GetVertices();
		const std::vector<CMesh3D::Face>& faces = mesh->GetFaces();
		for (const CMesh3D::Face& face : faces) {
			if (face.deleted || face.corners.size() < 3)
				continue;
			const cVec2 center = mesh_face_center_uv(face, vertices);
			if (ClassifyPointInFace2(loop, center, EPS2D) == PFP_OUTSIDE)
				return center;
		}
	}

	cVec2 point = polygon_centroid_2d(loop);
	if (loop.verts.empty())
		return point;
	point.x = loop.verts.front().x + 1.0;
	point.y = loop.verts.front().y + 1.0;
	return point;
}

bool uv_bounds_for_surface(CSurfaceFace* surface,
                           double& u_min,
                           double& u_max,
                           double& v_min,
                           double& v_max)
{
	if (!surface || surface->m_Face.IsNull())
		return false;
	try {
		Standard_Real su_min = 0.0;
		Standard_Real su_max = 0.0;
		Standard_Real sv_min = 0.0;
		Standard_Real sv_max = 0.0;
		BRepTools::UVBounds(TopoDS::Face(surface->m_Face), su_min, su_max, sv_min, sv_max);
		if (!std::isfinite(su_min) || !std::isfinite(su_max)
			|| !std::isfinite(sv_min) || !std::isfinite(sv_max)
			|| su_max <= su_min || sv_max <= sv_min)
			return false;
		u_min = su_min;
		u_max = su_max;
		v_min = sv_min;
		v_max = sv_max;
		return true;
	} catch (const Standard_Failure&) {
		return false;
	}
}

bool snap_to_uv_bounds(cVec2& point,
                       double u_min,
                       double u_max,
                       double v_min,
                       double v_max,
                       double eps)
{
	const bool on_u_min = std::fabs(point.x - u_min) <= eps;
	const bool on_u_max = std::fabs(point.x - u_max) <= eps;
	const bool on_v_min = std::fabs(point.y - v_min) <= eps;
	const bool on_v_max = std::fabs(point.y - v_max) <= eps;
	if (!on_u_min && !on_u_max && !on_v_min && !on_v_max)
		return false;

	if (on_u_min)
		point.x = u_min;
	else if (on_u_max)
		point.x = u_max;
	if (on_v_min)
		point.y = v_min;
	else if (on_v_max)
		point.y = v_max;
	return true;
}

double uv_boundary_parameter(const cVec2& point,
                             double u_min,
                             double u_max,
                             double v_min,
                             double v_max,
                             double eps)
{
	const double width = u_max - u_min;
	const double height = v_max - v_min;
	if (std::fabs(point.y - v_min) <= eps)
		return std::clamp(point.x - u_min, 0.0, width);
	if (std::fabs(point.x - u_max) <= eps)
		return width + std::clamp(point.y - v_min, 0.0, height);
	if (std::fabs(point.y - v_max) <= eps)
		return width + height + std::clamp(u_max - point.x, 0.0, width);
	if (std::fabs(point.x - u_min) <= eps)
		return width + height + width + std::clamp(v_max - point.y, 0.0, height);
	return 0.0;
}

cVec2 uv_boundary_point(double parameter,
                        double u_min,
                        double u_max,
                        double v_min,
                        double v_max)
{
	const double width = u_max - u_min;
	const double height = v_max - v_min;
	const double perimeter = 2.0 * (width + height);
	while (parameter < 0.0)
		parameter += perimeter;
	while (parameter >= perimeter)
		parameter -= perimeter;

	if (parameter <= width)
		return { u_min + parameter, v_min };
	parameter -= width;
	if (parameter <= height)
		return { u_max, v_min + parameter };
	parameter -= height;
	if (parameter <= width)
		return { u_max - parameter, v_max };
	parameter -= width;
	return { u_min, v_max - parameter };
}

std::vector<cVec2> uv_boundary_path(cVec2 from,
                                    cVec2 to,
                                    double u_min,
                                    double u_max,
                                    double v_min,
                                    double v_max,
                                    double eps,
                                    bool forward)
{
	const double width = u_max - u_min;
	const double height = v_max - v_min;
	const double perimeter = 2.0 * (width + height);
	double start = uv_boundary_parameter(from, u_min, u_max, v_min, v_max, eps);
	double end = uv_boundary_parameter(to, u_min, u_max, v_min, v_max, eps);
	if (!forward) {
		start = perimeter - start;
		end = perimeter - end;
	}
	if (end <= start + eps)
		end += perimeter;

	std::vector<cVec2> path;
	path.push_back(from);
	const std::vector<double> corners = {
		0.0,
		width,
		width + height,
		width + height + width,
		perimeter
	};
	for (double corner : corners) {
		double candidate = corner;
		while (candidate <= start + eps)
			candidate += perimeter;
		if (candidate < end - eps) {
			cVec2 point = uv_boundary_point(forward ? candidate : perimeter - candidate,
			                                u_min, u_max, v_min, v_max);
			if (path.empty() || !EqualPoint2(path.back(), point, eps))
				path.push_back(point);
		}
	}
	if (path.empty() || !EqualPoint2(path.back(), to, eps))
		path.push_back(to);
	return path;
}

std::unique_ptr<CPolyline> make_closed_uv_boundary_trim_loop(CSurfaceFace* surface,
                                                             const CMesh3D* mesh,
                                                             const CPolyline* open_loop,
                                                             double delta)
{
	if (!surface || !open_loop || open_loop->GetPointCount() < 2)
		return {};

	double u_min = 0.0;
	double u_max = 0.0;
	double v_min = 0.0;
	double v_max = 0.0;
	if (!uv_bounds_for_surface(surface, u_min, u_max, v_min, v_max))
		return {};

	const double eps = std::max(delta, std::max(u_max - u_min, v_max - v_min) * 1.0e-5);
	const std::vector<CPoint3d>& source = open_loop->GetPoints();
	cVec2 first(source.front().x, source.front().y);
	cVec2 last(source.back().x, source.back().y);
	if (!snap_to_uv_bounds(first, u_min, u_max, v_min, v_max, eps)
		|| !snap_to_uv_bounds(last, u_min, u_max, v_min, v_max, eps)
		|| EqualPoint2(first, last, eps)) {
		return {};
	}

	struct BoundaryLoopCandidate {
		std::vector<cVec2> points;
		double area = 0.0;
		size_t outside_count = 0;
		size_t inside_count = 0;
	};

	auto build_candidate = [&](bool forward) {
		std::vector<cVec2> points;
		points.reserve(source.size() + 6);
		for (const CPoint3d& point : source)
			points.push_back({ point.x, point.y });
		points.front() = first;
		points.back() = last;

		const std::vector<cVec2> boundary = uv_boundary_path(last, first, u_min, u_max, v_min, v_max, eps, forward);
		for (size_t i = 1; i < boundary.size(); ++i) {
			if (!EqualPoint2(points.back(), boundary[i], eps))
				points.push_back(boundary[i]);
		}
		Face2D face;
		face.verts = points;
		if (face.verts.size() > 1 && EqualPoint2(face.verts.front(), face.verts.back(), eps))
			face.verts.pop_back();

		BoundaryLoopCandidate candidate;
		candidate.points = std::move(points);
		candidate.area = std::fabs(polygon_area_2d(face));
		if (mesh && candidate.area > 1.0e-12) {
			const TopoDS_Face topo_face = TopoDS::Face(surface->m_Face);
			const std::vector<Vec3>& vertices = mesh->GetVertices();
			const std::vector<CMesh3D::Face>& faces = mesh->GetFaces();
			for (const CMesh3D::Face& mesh_face : faces) {
				if (mesh_face.deleted || mesh_face.corners.size() < 3)
					continue;
				const cVec2 center = mesh_face_center_uv(mesh_face, vertices);
				if (ClassifyPointInFace2(face, center, EPS2D) == PFP_OUTSIDE)
					continue;
				try {
					BRepClass_FaceClassifier classifier(topo_face, gp_Pnt2d(center.x, center.y), EPS2D, Standard_False);
					if (classifier.State() == TopAbs_OUT)
						++candidate.outside_count;
					else
						++candidate.inside_count;
				} catch (const Standard_Failure&) {
				}
			}
		}
		return candidate;
	};

	auto candidate_a = build_candidate(true);
	auto candidate_b = build_candidate(false);
	const BoundaryLoopCandidate* best_candidate = &candidate_a;
	const long long score_a = static_cast<long long>(candidate_a.outside_count) - static_cast<long long>(candidate_a.inside_count);
	const long long score_b = static_cast<long long>(candidate_b.outside_count) - static_cast<long long>(candidate_b.inside_count);
	const bool a_deletes_outside = candidate_a.outside_count > 0 && score_a > 0;
	const bool b_deletes_outside = candidate_b.outside_count > 0 && score_b > 0;
	if (b_deletes_outside && (!a_deletes_outside || score_b > score_a)) {
		best_candidate = &candidate_b;
	} else if (!a_deletes_outside && !b_deletes_outside) {
		return {};
	}
	if (best_candidate->area <= 1.0e-12 || best_candidate->points.size() < 4)
		return {};

	auto closed = std::make_unique<CPolyline>();
	for (const cVec2& point : best_candidate->points)
		closed->AddPoint(CPoint3d(point.x, point.y, 0.0));
	if (!EqualPoint2(best_candidate->points.front(), best_candidate->points.back(), eps))
		closed->AddPoint(CPoint3d(best_candidate->points.front().x, best_candidate->points.front().y, 0.0));
	closed->SetClosed(true);
	return closed;
}

std::vector<cVec2> clip_polygon_to_half_plane(const std::vector<cVec2>& input,
                                              const std::function<bool(const cVec2&)>& inside,
                                              const std::function<cVec2(const cVec2&, const cVec2&)>& intersection,
                                              double eps)
{
	std::vector<cVec2> output;
	if (input.empty())
		return output;

	cVec2 previous = input.back();
	bool previous_inside = inside(previous);
	for (const cVec2& current : input) {
		const bool current_inside = inside(current);
		if (current_inside) {
			if (!previous_inside)
				output.push_back(intersection(previous, current));
			output.push_back(current);
		} else if (previous_inside) {
			output.push_back(intersection(previous, current));
		}
		previous = current;
		previous_inside = current_inside;
	}

	std::vector<cVec2> cleaned;
	cleaned.reserve(output.size());
	for (const cVec2& point : output) {
		if (cleaned.empty() || !EqualPoint2(cleaned.back(), point, eps))
			cleaned.push_back(point);
	}
	if (cleaned.size() > 1 && EqualPoint2(cleaned.front(), cleaned.back(), eps))
		cleaned.pop_back();
	return cleaned;
}

std::unique_ptr<CPolyline> make_uv_bounds_clipped_trim_loop(CSurfaceFace* surface,
                                                            const CPolyline* closed_loop,
                                                            double delta)
{
	if (!surface || !closed_loop || !closed_loop->IsClosed() || closed_loop->GetPointCount() < 3)
		return {};

	double u_min = 0.0;
	double u_max = 0.0;
	double v_min = 0.0;
	double v_max = 0.0;
	if (!uv_bounds_for_surface(surface, u_min, u_max, v_min, v_max))
		return {};

	const double eps = std::max(delta, std::max(u_max - u_min, v_max - v_min) * 1.0e-5);
	std::vector<cVec2> polygon;
	polygon.reserve(closed_loop->GetPointCount());
	bool needs_clip = false;
	for (const CPoint3d& point : closed_loop->GetPoints()) {
		cVec2 uv(point.x, point.y);
		if (uv.x < u_min - eps || uv.x > u_max + eps || uv.y < v_min - eps || uv.y > v_max + eps)
			needs_clip = true;
		if (polygon.empty() || !EqualPoint2(polygon.back(), uv, eps))
			polygon.push_back(uv);
	}
	if (polygon.size() > 1 && EqualPoint2(polygon.front(), polygon.back(), eps))
		polygon.pop_back();
	if (!needs_clip || polygon.size() < 3)
		return {};

	const auto intersect_x = [](double x_value, const cVec2& a, const cVec2& b) {
		const double dx = b.x - a.x;
		const double t = std::fabs(dx) > 1.0e-20 ? (x_value - a.x) / dx : 0.0;
		return cVec2(x_value, a.y + (b.y - a.y) * std::clamp(t, 0.0, 1.0));
	};
	const auto intersect_y = [](double y_value, const cVec2& a, const cVec2& b) {
		const double dy = b.y - a.y;
		const double t = std::fabs(dy) > 1.0e-20 ? (y_value - a.y) / dy : 0.0;
		return cVec2(a.x + (b.x - a.x) * std::clamp(t, 0.0, 1.0), y_value);
	};

	polygon = clip_polygon_to_half_plane(polygon,
		[u_min, eps](const cVec2& point) { return point.x >= u_min - eps; },
		[u_min, &intersect_x](const cVec2& a, const cVec2& b) { return intersect_x(u_min, a, b); },
		eps);
	polygon = clip_polygon_to_half_plane(polygon,
		[u_max, eps](const cVec2& point) { return point.x <= u_max + eps; },
		[u_max, &intersect_x](const cVec2& a, const cVec2& b) { return intersect_x(u_max, a, b); },
		eps);
	polygon = clip_polygon_to_half_plane(polygon,
		[v_min, eps](const cVec2& point) { return point.y >= v_min - eps; },
		[v_min, &intersect_y](const cVec2& a, const cVec2& b) { return intersect_y(v_min, a, b); },
		eps);
	polygon = clip_polygon_to_half_plane(polygon,
		[v_max, eps](const cVec2& point) { return point.y <= v_max + eps; },
		[v_max, &intersect_y](const cVec2& a, const cVec2& b) { return intersect_y(v_max, a, b); },
		eps);

	Face2D clipped_face;
	clipped_face.verts = polygon;
	if (polygon.size() < 3 || std::fabs(polygon_area_2d(clipped_face)) <= 1.0e-9)
		return {};

	auto clipped = std::make_unique<CPolyline>();
	for (const cVec2& point : polygon)
		clipped->AddPoint(CPoint3d(std::clamp(point.x, u_min, u_max), std::clamp(point.y, v_min, v_max), 0.0));
	clipped->AddPoint(CPoint3d(clipped->GetPoints().front().x, clipped->GetPoints().front().y, 0.0));
	clipped->SetClosed(true);
	return clipped;
}

bool trim_mesh_by_independent_boundary_loops(CMesh3D* mesh,
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

	bool changed = false;
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

		const cVec2 keep = choose_point_outside_loop(mesh, loop_face);
		CPoint3d pc(keep.x, keep.y, 0.0);
		const bool trimmed = mesh->TrimByPline(loop, pc);
		changed = trimmed || changed;
	}
	return changed;
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
	std::vector<CPolyline*> boundary_cut_loops;
	std::vector<Face2D> boundary_cut_faces;
	std::vector<std::unique_ptr<CPolyline>> boundary_closed_storage;
	closed_loops.reserve(loops.size());
	loop_faces.reserve(loops.size());
	boundary_cut_loops.reserve(loops.size());
	boundary_cut_faces.reserve(loops.size());
	for (CPolyline* loop : loops) {
		if (!loop || loop->GetPointCount() < 2)
			continue;
		if (loop->P(0)->DistTo(loop->PLast()) <= delta)
			loop->SetClosed(true);
		if (!loop->PutOnSurface(surface))
			continue;

		CPolyline* trim_loop = loop;
		bool boundary_cut_loop = false;
		if (!trim_loop->IsClosed()) {
			std::unique_ptr<CPolyline> closed_by_boundary =
				make_closed_uv_boundary_trim_loop(surface, mesh, trim_loop, delta);
			if (!closed_by_boundary)
				continue;
			trim_loop = closed_by_boundary.get();
			boundary_cut_loop = true;
			boundary_closed_storage.push_back(std::move(closed_by_boundary));
		}
		else {
			std::unique_ptr<CPolyline> clipped_by_boundary =
				make_uv_bounds_clipped_trim_loop(surface, trim_loop, delta);
			if (clipped_by_boundary) {
				trim_loop = clipped_by_boundary.get();
				boundary_cut_loop = true;
				boundary_closed_storage.push_back(std::move(clipped_by_boundary));
			}
		}

		Face2D loop_face = polyline_to_face2d(trim_loop);
		if (loop_face.verts.size() < 3 || std::fabs(polygon_area_2d(loop_face)) <= 1.0e-9)
			continue;
		if (boundary_cut_loop) {
			boundary_cut_loops.push_back(trim_loop);
			boundary_cut_faces.push_back(std::move(loop_face));
			continue;
		}
		closed_loops.push_back(trim_loop);
		loop_faces.push_back(std::move(loop_face));
	}
	bool NeedTest = false;
	if (NeedTest && surface->m_ID == 2) {
		CAlfaDoc* pDoc = GetAlfaDoc();
		if (pDoc) {
			pDoc->AddLayer("Mesh3D ID =0");
			auto mesh_copy = mesh->Clone();
			pDoc->AddObject(std::move(mesh_copy));

			auto dump_loop = [pDoc](CPolyline* loop) {
				if (!loop)
					return;

				auto loop_copy_object = loop->Clone();
				CPolyline* loop_copy = dynamic_cast<CPolyline*>(loop_copy_object.get());
				if (!loop_copy)
					return;
				loop_copy->printToFile("loop.txt");
				loop_copy->SetColor({ 1.0f, 0.12f, 0.05f });
				pDoc->AddObject(std::move(loop_copy_object));
			};

			for (CPolyline* loop : closed_loops)
				dump_loop(loop);
			for (CPolyline* loop : boundary_cut_loops)
				dump_loop(loop);
		}
	}

	bool changed = false;
	// Boundary-cut loops are artificial UV closures around a face boundary. Trimming by
	// those loops can pick the wrong side on rounded corner faces; OCCT classification
	// below decides which mesh cells are actually outside the TopoDS_Face.
	(void)boundary_cut_loops;
	(void)boundary_cut_faces;
	if (closed_loops.empty())
		return changed;

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
		&& !outer_matches_uv_bounds
		&& loop_is_single_inner_cut(surface, loop_faces[outer_index])
		&& loop_contains_occt_out_faces(surface, mesh, loop_faces[outer_index])) {
		const cVec2 keep = choose_point_outside_loop(mesh, loop_faces[outer_index]);
		CPoint3d pc(keep.x, keep.y, 0.0);
		return mesh->TrimByPline(closed_loops[outer_index], pc) || changed;
	}

	if (closed_loops.size() == 1
		&& (loop_faces[outer_index].verts.size() <= 4 || outer_matches_uv_bounds))
		return changed;

	const cVec2 keep = choose_boundary_keep_point(mesh, loop_faces, outer_index);
	CPoint3d pc(keep.x, keep.y, 0.0);
	if (!outer_matches_uv_bounds)
		changed = mesh->TrimByPline(closed_loops[outer_index], pc);
	for (size_t i = 0; i < closed_loops.size(); ++i) {
		if (i == outer_index)
			continue;
		changed = mesh->TrimByPline(closed_loops[i], pc) || changed;
	}
	if (changed) {
		mesh->KeepConnectedComponentAt(pc);
	}
	return changed;
}

bool delete_mesh_faces_outside_occt_face(CMesh3D* mesh, CSurfaceFace* surface)
{
	if (!mesh || !surface || surface->m_Face.IsNull())
		return false;

	const TopoDS_Face face = TopoDS::Face(surface->m_Face);
	std::vector<Vec3>& vertices = mesh->GetVertices();
	std::vector<CMesh3D::Face>& faces = mesh->GetFaces();
	std::vector<size_t> outside_faces;
	size_t active_count = 0;
	for (size_t face_index = 0; face_index < faces.size(); ++face_index) {
		CMesh3D::Face& mesh_face = faces[face_index];
		if (mesh_face.deleted || mesh_face.corners.size() < 3)
			continue;
		++active_count;

		cVec2 center{};
		int count = 0;
		for (const MeshCorner& corner : mesh_face.corners) {
			if (corner.v >= vertices.size())
				continue;
			center.x += vertices[corner.v].x;
			center.y += vertices[corner.v].y;
			++count;
		}
		if (count == 0)
			continue;

		center.x /= static_cast<double>(count);
		center.y /= static_cast<double>(count);
		try {
			BRepClass_FaceClassifier classifier(face, gp_Pnt2d(center.x, center.y), EPS2D, Standard_False);
			if (classifier.State() == TopAbs_OUT) {
				outside_faces.push_back(face_index);
			}
		} catch (const Standard_Failure&) {
			return false;
		}
	}

	if (outside_faces.empty())
		return false;

	const size_t remaining_count = active_count > outside_faces.size()
		? active_count - outside_faces.size()
		: 0;
	const GeomAbs_SurfaceType surface_type = surface_type_of(face);
	// On periodic fillet/cylinder UV domains the classifier can occasionally report
	// nearly every sample as OUT. Keep the mesh in that case and let boundary trim decide.
	if (trim_removed_too_much(active_count, remaining_count, surface_type))
		return false;

	for (size_t face_index : outside_faces)
		faces[face_index].deleted = true;
	return true;
}
}
#include <TopAbs_Orientation.hxx>
#include <TopAbs_State.hxx>
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
	Closed = false;
	Umin = 0.0;
	Umax = 0.0;
	Vmin = 0.0;
	Vmax = 0.0;
	m_QtyU = 0;
	m_QtyV = 0;
	Norm0 = {};
	m_p0 = {};
	TextureTransform = {};
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

bool CSurfaceFace::CreateRuled(CSplineCurve* gener, CVector& dir, double dist)
{
	if (!gener || gener->np() < 2 || !std::isfinite(dist)
		|| std::fabs(dist) <= 1.0e-9)
		return false;

	const double direction_length = std::sqrt(
		dir.l * dir.l + dir.m * dir.m + dir.n * dir.n);
	if (!std::isfinite(direction_length) || direction_length <= 1.0e-12)
		return false;

	try {
		// CSplineCurve is the legacy piecewise spline used by the facade
		// builders. Evaluate its already-trimmed parameter range and interpolate
		// one smooth OCCT curve before sweeping it along the requested vector.
		const bool periodic = gener->IsClosed();
		const int sample_count = std::max(64, (gener->np() - 1) * 24 + 1);
		Handle(TColgp_HArray1OfPnt) samples =
			new TColgp_HArray1OfPnt(1, sample_count);
		const double last_parameter = static_cast<double>(gener->np() - 1);
		for (int index = 0; index < sample_count; ++index) {
			const double denominator = periodic
				? static_cast<double>(sample_count)
				: static_cast<double>(sample_count - 1);
			CPoint3d point;
			if (!gener->GetPoint(
					last_parameter * static_cast<double>(index) / denominator,
					&point)
				|| !std::isfinite(point.x)
				|| !std::isfinite(point.y)
				|| !std::isfinite(point.z)) {
				return false;
			}
			samples->SetValue(index + 1, gp_Pnt(point.x, point.y, point.z));
		}

		GeomAPI_Interpolate interpolation(samples, periodic, 1.0e-7);
		interpolation.Perform();
		if (!interpolation.IsDone() || interpolation.Curve().IsNull())
			return false;

		BRepBuilderAPI_MakeEdge edge_builder(interpolation.Curve());
		if (!edge_builder.IsDone())
			return false;

		const double scale = dist / direction_length;
		BRepPrimAPI_MakePrism prism(
			edge_builder.Edge(),
			gp_Vec(dir.l * scale, dir.m * scale, dir.n * scale),
			Standard_False, Standard_True);
		prism.Build();
		if (!prism.IsDone() || prism.Shape().IsNull())
			return false;

		TopoDS_Face ruled_face;
		if (prism.Shape().ShapeType() == TopAbs_FACE) {
			ruled_face = TopoDS::Face(prism.Shape());
		} else {
			TopExp_Explorer faces(prism.Shape(), TopAbs_FACE);
			if (!faces.More())
				return false;
			ruled_face = TopoDS::Face(faces.Current());
			faces.Next();
			if (faces.More())
				return false;
		}
		if (ruled_face.IsNull() || !BRepCheck_Analyzer(ruled_face).IsValid())
			return false;

		m_Face = ruled_face;
		IsInitMesh = false;
		IsTrimmed = false;
		m_TypeMesh = REGULAR_MESH;
		return InitEdges();
	} catch (const Standard_Failure&) {
		return false;
	}
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
			false);
		// The shape-taking constructor calls Perform() itself.
		if (!mesher.IsDone())
			return false;
		aTriangulation = BRep_Tool::Triangulation(theFace, aLoc, Poly_MeshPurpose_NONE);
	}
	if (aTriangulation.IsNull())
		return false;

	std::vector<Vec3> vertices;
	vertices.reserve(static_cast<size_t>(aTriangulation->NbNodes()));
	std::vector<UV> uvs;
	std::vector<Vec3> normals;
	if (aTriangulation->HasUVNodes()) {
		uvs.reserve(static_cast<size_t>(aTriangulation->NbNodes()));
		normals.reserve(static_cast<size_t>(aTriangulation->NbNodes()));
	}
	const gp_Trsf& transform = aLoc.Transformation();
	const bool reverseWinding = theFace.Orientation() == TopAbs_REVERSED;
	BRepAdaptor_Surface analytic_surface(theFace);
	const bool spherical_surface =
		analytic_surface.GetType() == GeomAbs_Sphere;
	gp_Pnt spherical_center;
	if (spherical_surface) {
		spherical_center = analytic_surface.Sphere().Location();
	}
	bool analytic_normals_valid = aTriangulation->HasUVNodes();
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
			try {
				gp_Vec normal;
				if (spherical_surface) {
					// A parametric sphere has a singular derivative at each pole.
					// Even a tiny numerical residue can normalize into an arbitrary
					// sideways vector and appear as a black dot in Cycles.  The exact
					// analytic normal is radial everywhere, including poles and seam.
					normal = gp_Vec(spherical_center, point);
				} else {
					gp_Pnt surface_point;
					gp_Vec derivative_u;
					gp_Vec derivative_v;
					analytic_surface.D1(
						uv.X(), uv.Y(), surface_point,
						derivative_u, derivative_v);
					normal = derivative_u.Crossed(derivative_v);
				}
				if (normal.SquareMagnitude() <= 1.0e-24) {
					analytic_normals_valid = false;
					normals.push_back({});
				} else {
					normal.Normalize();
					if (reverseWinding) {
						normal.Reverse();
					}
					normals.push_back({
						static_cast<float>(normal.X()),
						static_cast<float>(normal.Y()),
						static_cast<float>(normal.Z())});
				}
			} catch (const Standard_Failure&) {
				analytic_normals_valid = false;
				normals.push_back({});
			}
		}
	}
	if (!analytic_normals_valid) {
		normals.clear();
	}

	std::vector<CMesh3D::Face> faces;
	faces.reserve(static_cast<size_t>(aTriangulation->NbTriangles()));
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
	if (!pMesh3D->SetGeometry(
			std::move(vertices), std::move(faces),
			std::move(uvs), std::move(normals))) {
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
	if (MeshQuadro) {
		if (!IsInitEdges)
			InitEdges();
		if (!Polylines.empty() || InitEdges()) {
			PrepareEdges(Deflection, true);
			if (BuildTrimmingMesh(nullptr, Deflection))
				return true;
		}
	}
	return BuldMeshTriangle(Deflection, 0.3f);
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

bool CSurfaceFace::IsSpherical() const
{
	if (m_Face.IsNull())
		return false;
	try {
		const TopoDS_Face face = TopoDS::Face(m_Face);
		return BRepAdaptor_Surface(face).GetType() == GeomAbs_Sphere;
	} catch (const Standard_Failure&) {
		return false;
	}
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

		// Curved faces (especially fillet cylinders and tori) need a real
		// surface normal. The old planar-mesh fallback deliberately rejects
		// them, so evaluate the OpenCascade surface at the middle of its UV
		// bounds instead.
		Standard_Real u_min = 0.0;
		Standard_Real u_max = 0.0;
		Standard_Real v_min = 0.0;
		Standard_Real v_max = 0.0;
		BRepTools::UVBounds(face, u_min, u_max, v_min, v_max);
		if (std::isfinite(u_min) && std::isfinite(u_max)
			&& std::isfinite(v_min) && std::isfinite(v_max)
			&& u_max > u_min && v_max > v_min) {
			gp_Pnt point;
			gp_Vec du;
			gp_Vec dv;
			surface.D1(
				(u_min + u_max) * 0.5,
				(v_min + v_max) * 0.5,
				point,
				du,
				dv);
			gp_Vec direction = du.Crossed(dv);
			if (direction.SquareMagnitude() > 1.0e-18) {
				direction.Normalize();
				if (face.Orientation() == TopAbs_REVERSED)
					direction.Reverse();
				center = {
					static_cast<float>(point.X()),
					static_cast<float>(point.Y()),
					static_cast<float>(point.Z())};
				normal = {
					static_cast<float>(direction.X()),
					static_cast<float>(direction.Y()),
					static_cast<float>(direction.Z())};
				return true;
			}
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

	// m_Edges is the edge representation used by the regular OCCT renderer and
	// by picking.  InitEdges3DCoat() builds a second, legacy set of boundary
	// splines for the optional quad-mesh path.  Running that old numerical
	// spline builder here for every ordinary triangulated face is unnecessary
	// and, for complex loft faces, can corrupt the heap before GetEdges() makes
	// its next small allocation.  The quad-mesh path invokes it explicitly.
	const bool has_render_edges = !m_Edges.empty();
	IsInitEdges = has_render_edges;
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
	char buf[1120];
	if (F1.IsNull())
		return false;
	// BRep_Tool::Surface(face) returns the underlying geometry without the
	// face's TopLoc_Location.  After a history Move that left the boundary
	// splines at the original position while GetPoint() evaluated the regular
	// grid in world space, so trimming removed a diagonal part of the sphere.
	// BRepAdaptor_Surface evaluates both points and derivatives with the face
	// location applied.
	BRepAdaptor_Surface surf(F1);
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
	surf.D1(U, V, P, D1U, D1V);
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
		surf.D0(Ui, Vi, Pi);
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
		surf.D0(Ui, Vi, Pi);
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
		surf.D0(Ui, Vi, Pi);
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
		surf.D0(Ui, Vi, Pi);
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



void CSurfaceFace::RenderEdges(const Color& color,
                              const std::vector<int>& selected_edge_indices,
                              bool draw_regular_edges,
                              bool surface_selected) const
{
	// Project files can restore a ready triangulation without going through
	// ReBuldMesh().  Make the topological representation available for both
	// drawing and edge picking on that path as well.
	if (!IsInitEdges)
		const_cast<CSurfaceFace*>(this)->InitEdges();

	const float width = 0.9f;
	const Color edge_color = surface_selected
		? CAlfaObject::SelectedColor : color;
	const float r = std::clamp(edge_color.r, 0.0f, 1.0f);
	const float g = std::clamp(edge_color.g, 0.0f, 1.0f);
	const float b = std::clamp(edge_color.b, 0.0f, 1.0f);

	// A closed analytic sphere has only a parameterisation seam and degenerate
	// pole edges. They are not physical body edges and must not be displayed.
	if (draw_regular_edges && !IsSpherical()) {
		for (int i = 0; i < static_cast<int>(m_Edges.size()); ++i) {
			CSplineCurve* edge = m_Edges[static_cast<size_t>(i)];
			if (!edge)
				continue;
			if (std::find(selected_edge_indices.begin(), selected_edge_indices.end(), i) != selected_edge_indices.end())
				continue;
			else
				edge->Draw(r, g, b, width, 16, false, false);
		}
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

void CSurfaceFace::RenderContour(const Color& color, bool surface_selected) const
{
	if (!pMesh3D)
		return;
	const std::vector<Vec3>& vertices = pMesh3D->GetVertices();
	const std::vector<CMesh3D::Face>& faces = pMesh3D->GetFaces();
	if (vertices.empty() || faces.empty())
		return;

	struct ContourEdge {
		size_t first = 0;
		size_t second = 0;
		unsigned char facing_mask = 0;
		int adjacent_faces = 0;
		bool edge_on = false;
	};
	std::unordered_map<unsigned long long, ContourEdge> contour_edges;
	contour_edges.reserve(faces.size() * 2);

	GLdouble model_view[16]{};
	glGetDoublev(GL_MODELVIEW_MATRIX, model_view);
	const auto eye_point = [&model_view](const Vec3& point) {
		return Vec3{
			static_cast<float>(model_view[0] * point.x + model_view[4] * point.y
				+ model_view[8] * point.z + model_view[12]),
			static_cast<float>(model_view[1] * point.x + model_view[5] * point.y
				+ model_view[9] * point.z + model_view[13]),
			static_cast<float>(model_view[2] * point.x + model_view[6] * point.y
				+ model_view[10] * point.z + model_view[14])};
	};
	const auto eye_vector = [&model_view](const Vec3& vector) {
		return Vec3{
			static_cast<float>(model_view[0] * vector.x + model_view[4] * vector.y
				+ model_view[8] * vector.z),
			static_cast<float>(model_view[1] * vector.x + model_view[5] * vector.y
				+ model_view[9] * vector.z),
			static_cast<float>(model_view[2] * vector.x + model_view[6] * vector.y
				+ model_view[10] * vector.z)};
	};
	const bool spherical = IsSpherical();
	Vec3 sphere_center{};
	if (spherical) {
		try {
			const gp_Pnt center = BRepAdaptor_Surface(
				TopoDS::Face(m_Face)).Sphere().Location();
			sphere_center = {
				static_cast<float>(center.X()),
				static_cast<float>(center.Y()),
				static_cast<float>(center.Z())};
		} catch (const Standard_Failure&) {
		}
	}

	for (const CMesh3D::Face& face : faces) {
		if (face.deleted || face.corners.size() < 3)
			continue;
		const size_t a_index = face.corners[0].v;
		const size_t b_index = face.corners[1].v;
		const size_t c_index = face.corners[2].v;
		if (a_index >= vertices.size() || b_index >= vertices.size()
			|| c_index >= vertices.size()) {
			continue;
		}
		const Vec3 a = eye_point(vertices[a_index]);
		const Vec3 b = eye_point(vertices[b_index]);
		const Vec3 c = eye_point(vertices[c_index]);
		unsigned char facing = 0u;
		if (spherical) {
			const Vec3 world_center =
				(vertices[a_index] + vertices[b_index] + vertices[c_index])
				/ 3.0f;
			const Vec3 eye_center = eye_point(world_center);
			const Vec3 eye_normal = normalize(
				eye_vector(normalize(world_center - sphere_center)));
			const float direction = dot(
				eye_normal, normalize(eye_center * -1.0f));
			facing = direction > 0.00001f ? 1u
				: direction < -0.00001f ? 2u : 0u;
		} else {
			const float projected_winding = cross(b - a, c - a).z;
			facing = projected_winding > 0.0000001f ? 1u
				: projected_winding < -0.0000001f ? 2u : 0u;
		}

		for (size_t corner = 0; corner < face.corners.size(); ++corner) {
			size_t first = face.corners[corner].v;
			size_t second = face.corners[(corner + 1) % face.corners.size()].v;
			if (first >= vertices.size() || second >= vertices.size()
				|| first == second) {
				continue;
			}
			if (first > second)
				std::swap(first, second);
			const unsigned long long key =
				(static_cast<unsigned long long>(first) << 32)
				| static_cast<unsigned long long>(second);
			ContourEdge& edge = contour_edges[key];
			edge.first = first;
			edge.second = second;
			edge.facing_mask |= facing;
			edge.edge_on = edge.edge_on || facing == 0u;
			++edge.adjacent_faces;
		}
	}

	const Color contour_color = surface_selected
		? CAlfaObject::SelectedColor : color;
	glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_LINE_BIT
		| GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glLineWidth(surface_selected ? 1.7f : 1.15f);
	glColor4f(
		std::clamp(contour_color.r, 0.0f, 1.0f),
		std::clamp(contour_color.g, 0.0f, 1.0f),
		std::clamp(contour_color.b, 0.0f, 1.0f), 1.0f);
	glBegin(GL_LINES);
	for (const auto& item : contour_edges) {
		const ContourEdge& edge = item.second;
		// Topological boundary curves are rendered by RenderEdges().  Contours
		// are the view-dependent internal silhouette between front- and
		// back-facing mesh regions.
		if (edge.adjacent_faces < 2
			|| (edge.facing_mask != 3u && !edge.edge_on)) {
			continue;
		}
		const Vec3& first = vertices[edge.first];
		const Vec3& second = vertices[edge.second];
		glVertex3f(first.x, first.y, first.z);
		glVertex3f(second.x, second.y, second.z);
	}
	glEnd();
	glPopAttrib();
}

void CSurfaceFace::RenderOutline(
	const Color& color,
	const std::vector<int>& selected_edge_indices,
	bool draw_regular_edges,
	bool surface_selected) const
{
	RenderEdges(color, selected_edge_indices, draw_regular_edges, surface_selected);
	if (draw_regular_edges)
		RenderContour(color, surface_selected);
}

void CSurfaceFace::PreviewTranslate(Vec3 delta)
{
	if (pMesh3D)
		pMesh3D->Translate(delta);

	CPoint3d from(0.0, 0.0, 0.0);
	CPoint3d to(delta.x, delta.y, delta.z);
	for (CSplineCurve* edge : m_Edges) {
		if (edge)
			edge->Move(&from, &to);
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

bool CSurfaceFace::CommitPreviewTranslate(Vec3 delta)
{
	gp_Trsf transform;
	transform.SetTranslation(gp_Vec(delta.x, delta.y, delta.z));
	try {
		if (!m_Face.IsNull()) {
			BRepBuilderAPI_Transform face_builder(m_Face, transform, false);
			if (!face_builder.IsDone() || face_builder.Shape().IsNull())
				return false;
			m_Face = face_builder.Shape();
		}
		for (TopoDS_Edge& edge : m_TopoEdges) {
			if (edge.IsNull()) continue;
			BRepBuilderAPI_Transform edge_builder(edge, transform, false);
			if (!edge_builder.IsDone() || edge_builder.Shape().IsNull())
				return false;
			edge = TopoDS::Edge(edge_builder.Shape());
		}
		return true;
	} catch (const Standard_Failure&) {
		return false;
	}
}

bool CSurfaceFace::CommitPreviewRotate(Vec3 center, Vec3 axis, float angle)
{
	const Vec3 unit_axis = normalize(axis);
	if (std::fabs(angle) <= 0.000001f)
		return true;
	if (dot(unit_axis, unit_axis) <= 0.000001f)
		return false;
	gp_Trsf transform;
	transform.SetRotation(
		gp_Ax1(gp_Pnt(center.x, center.y, center.z),
		       gp_Dir(unit_axis.x, unit_axis.y, unit_axis.z)),
		angle);
	try {
		if (!m_Face.IsNull()) {
			BRepBuilderAPI_Transform face_builder(m_Face, transform, false);
			if (!face_builder.IsDone() || face_builder.Shape().IsNull())
				return false;
			m_Face = face_builder.Shape();
		}
		for (TopoDS_Edge& edge : m_TopoEdges) {
			if (edge.IsNull()) continue;
			BRepBuilderAPI_Transform edge_builder(edge, transform, false);
			if (!edge_builder.IsDone() || edge_builder.Shape().IsNull())
				return false;
			edge = TopoDS::Edge(edge_builder.Shape());
		}
		return true;
	} catch (const Standard_Failure&) {
		return false;
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
                                     int& edge_index,
                                     float* screen_distance) const
{
	bool found = false;
	float best_distance = tolerance;
	edge_index = -1;

	for (int edge_i = 0; edge_i < static_cast<int>(m_Edges.size()); ++edge_i) {
		CSplineCurve* edge = m_Edges[static_cast<size_t>(edge_i)];
		if (!edge || edge->np() < 2)
			continue;

		DomPoint previous_screen{};
		bool has_previous = false;

		// InitEdges() already samples every OCCT topological curve into 18
		// screen-picking points.  Calling the legacy INKM spline evaluator 16
		// additional times between every pair made a kitchen-sized document do
		// millions of expensive evaluations on a single mouse move.  The stored
		// points are the original curve samples and are sufficiently dense for
		// pixel-tolerance edge picking.
		for (int point_index = 0; point_index < edge->np(); ++point_index) {
			const CPoint3d* p = edge->Pnt(point_index);
			if (!p)
				continue;

			DomPoint current_screen{};
			if (!world_to_screen({static_cast<float>(p->x), static_cast<float>(p->y), static_cast<float>(p->z)}, current_screen)) {
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

	if (found && screen_distance)
		*screen_distance = best_distance;
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

bool CSurfaceFace::GetEdgePolylinePoints(int edge_index, std::vector<Vec3>& points) const
{
	points.clear();
	if (edge_index < 0 || edge_index >= static_cast<int>(m_Edges.size()))
		return false;

	CSplineCurve* edge = m_Edges[static_cast<size_t>(edge_index)];
	if (!edge || edge->np() < 2)
		return false;

	const int steps = std::max(16, edge->np() * 3);
	points.reserve(static_cast<size_t>(steps + 1));
	for (int step = 0; step <= steps; ++step) {
		const double parameter = static_cast<double>(edge->np() - 1)
			* static_cast<double>(step) / static_cast<double>(steps);
		CPoint3d point;
		if (edge->GetPoint(parameter, &point)) {
			points.push_back({
				static_cast<float>(point.x),
				static_cast<float>(point.y),
				static_cast<float>(point.z)
			});
		}
	}
	return points.size() >= 2;
}

void CSurfaceFace::PrepareEdges(float Deflection, bool normalized_quadro_density)
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

	GeomAbs_SurfaceType surface_type = GeomAbs_OtherSurface;
	if (!m_Face.IsNull())
		surface_type = surface_type_of(TopoDS::Face(m_Face));

	for (CPolyline* pLine : Edges2) {
		CSplineCurve spl;
		spl.Create(pLine);
		CPolyline* pline = new CPolyline;
		float len = spl.GetLength();
		int Qty = normalized_quadro_density
			? normalized_quadro_point_quantity(len, lenEdgeMax, Deflection)
			: mesh_point_quantity_for_length(len, Deflection, surface_type);
		spl.MakePolylineByQtyKnots(pline, Qty);
		pline->TmpLen = spl.GetLength();
		pline->Dir1.l = spl.P(0)->l;
		pline->Dir1.m = spl.P(0)->m;
		pline->Dir1.n = spl.P(0)->n;
		Polylines.push_back(pline);
	}

/*	char buf[120];
	for (CPolyline* pLine : Polylines) {
		if (m_ID == 3 || m_ID == 6) {
			sprintf(buf, "Polylines%d", m_ID);
			pLine->printToFile(buf);
		}

	}
*/

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

bool CSurfaceFace::GetPreparedTopoEdge(int edge_index, TopoDS_Edge& edge) const
{
	if (edge_index < 0 || edge_index >= static_cast<int>(Polylines.size()))
		return false;
	const CPolyline* polyline = Polylines[static_cast<size_t>(edge_index)];
	if (!polyline || polyline->GetPointCount() < 2 || m_TopoEdges.empty())
		return false;

	const std::vector<CPoint3d>& points = polyline->GetPoints();
	const auto point_segment_distance_sq = [](const gp_Pnt& point,
	                                          const CPoint3d& first,
	                                          const CPoint3d& second) {
		const double edge_x = second.x - first.x;
		const double edge_y = second.y - first.y;
		const double edge_z = second.z - first.z;
		const double length_sq = edge_x * edge_x + edge_y * edge_y + edge_z * edge_z;
		double alpha = 0.0;
		if (length_sq > 1.0e-20) {
			alpha = ((point.X() - first.x) * edge_x
			       + (point.Y() - first.y) * edge_y
			       + (point.Z() - first.z) * edge_z) / length_sq;
			alpha = std::clamp(alpha, 0.0, 1.0);
		}
		const double dx = point.X() - (first.x + edge_x * alpha);
		const double dy = point.Y() - (first.y + edge_y * alpha);
		const double dz = point.Z() - (first.z + edge_z * alpha);
		return dx * dx + dy * dy + dz * dz;
	};

	double best_score = std::numeric_limits<double>::max();
	const TopoDS_Edge* best_edge = nullptr;
	for (const TopoDS_Edge& candidate : m_TopoEdges) {
		if (candidate.IsNull() || BRep_Tool::Degenerated(candidate))
			continue;
		try {
			BRepAdaptor_Curve curve(candidate);
			const double first_parameter = curve.FirstParameter();
			const double last_parameter = curve.LastParameter();
			if (!std::isfinite(first_parameter) || !std::isfinite(last_parameter)
				|| last_parameter <= first_parameter) {
				continue;
			}

			double score = 0.0;
			constexpr int sample_count = 7;
			for (int sample = 0; sample < sample_count; ++sample) {
				const double alpha = static_cast<double>(sample)
					/ static_cast<double>(sample_count - 1);
				const gp_Pnt point = curve.Value(
					first_parameter + (last_parameter - first_parameter) * alpha);
				double nearest_sq = std::numeric_limits<double>::max();
				for (size_t i = 1; i < points.size(); ++i) {
					nearest_sq = std::min(
						nearest_sq,
						point_segment_distance_sq(point, points[i - 1], points[i]));
				}
				score += nearest_sq;
			}
			score /= static_cast<double>(sample_count);
			if (score < best_score) {
				best_score = score;
				best_edge = &candidate;
			}
		} catch (const Standard_Failure&) {
			continue;
		}
	}

	if (!best_edge)
		return false;
	edge = *best_edge;
	return true;
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

void CSurfaceFace::UpdateMeshTypeFromBoundary()
{
	m_TypeMesh = TRIMMED_MESH;
	if (Polylines.empty())
		return;
	if (is_complete_spherical_face(TopoDS::Face(m_Face))) {
		m_TypeMesh = REGULAR_MESH;
		return;
	}
	if (is_untrimmed_planar_quad(TopoDS::Face(m_Face))) {
		m_TypeMesh = REGULAR_MESH;
		return;
	}

	m_TypeMesh = REGULAR_MESH;
	for (CPolyline* line : Polylines) {
		if (!IsBoundLine(line)) {
			m_TypeMesh = TRIMMED_MESH;
			break;
		}
	}
}

bool CSurfaceFace::GetRegularMeshBoundaryPoints(int edge_index, std::vector<CPoint3d>& points) const
{
	points.clear();
	if (m_TypeMesh != REGULAR_MESH || !pMesh3D
		|| edge_index < 0 || edge_index >= static_cast<int>(Polylines.size())
		|| !Polylines[static_cast<size_t>(edge_index)]
		|| m_QtyU < 2 || m_QtyV < 2) {
		return false;
	}

	const std::vector<Vec3>& vertices = pMesh3D->GetVertices();
	const size_t expected_vertex_count = static_cast<size_t>(m_QtyU) * static_cast<size_t>(m_QtyV);
	if (vertices.size() < expected_vertex_count)
		return false;

	CPolyline* target = Polylines[static_cast<size_t>(edge_index)];
	const std::vector<CPoint3d>& target_points = target->GetPoints();
	if (target_points.size() < 2)
		return false;

	std::vector<std::vector<size_t>> candidates(4);
	candidates[0].reserve(static_cast<size_t>(m_QtyU));
	candidates[1].reserve(static_cast<size_t>(m_QtyU));
	candidates[2].reserve(static_cast<size_t>(m_QtyV));
	candidates[3].reserve(static_cast<size_t>(m_QtyV));
	for (int u = 0; u < m_QtyU; ++u) {
		candidates[0].push_back(static_cast<size_t>(u));
		candidates[1].push_back(static_cast<size_t>((m_QtyV - 1) * m_QtyU + u));
	}
	for (int v = 0; v < m_QtyV; ++v) {
		candidates[2].push_back(static_cast<size_t>(v * m_QtyU));
		candidates[3].push_back(static_cast<size_t>(v * m_QtyU + m_QtyU - 1));
	}

	const auto point_segment_distance_sq = [](const Vec3& point,
	                                          const CPoint3d& first,
	                                          const CPoint3d& second) {
		const Vec3 a{static_cast<float>(first.x), static_cast<float>(first.y), static_cast<float>(first.z)};
		const Vec3 b{static_cast<float>(second.x), static_cast<float>(second.y), static_cast<float>(second.z)};
		const Vec3 ab = b - a;
		const float length_sq = dot(ab, ab);
		const float alpha = length_sq > 1.0e-20f
			? std::clamp(dot(point - a, ab) / length_sq, 0.0f, 1.0f)
			: 0.0f;
		const Vec3 delta = point - (a + ab * alpha);
		return dot(delta, delta);
	};

	double best_score = std::numeric_limits<double>::max();
	size_t best_candidate = candidates.size();
	for (size_t candidate_index = 0; candidate_index < candidates.size(); ++candidate_index) {
		double score = 0.0;
		for (size_t vertex_index : candidates[candidate_index]) {
			double distance_sq = std::numeric_limits<double>::max();
			for (size_t i = 1; i < target_points.size(); ++i) {
				distance_sq = std::min(
					distance_sq,
					static_cast<double>(point_segment_distance_sq(
						vertices[vertex_index],
						target_points[i - 1],
						target_points[i])));
			}
			score += distance_sq;
		}
		score /= static_cast<double>(candidates[candidate_index].size());
		if (score < best_score) {
			best_score = score;
			best_candidate = candidate_index;
		}
	}
	if (best_candidate >= candidates.size())
		return false;

	points.reserve(candidates[best_candidate].size());
	for (size_t vertex_index : candidates[best_candidate]) {
		const Vec3& vertex = vertices[vertex_index];
		points.emplace_back(vertex.x, vertex.y, vertex.z);
	}

	const auto point_distance = [](const CPoint3d& first, const CPoint3d& second) {
		const double dx = first.x - second.x;
		const double dy = first.y - second.y;
		const double dz = first.z - second.z;
		return std::sqrt(dx * dx + dy * dy + dz * dz);
	};
	const bool target_closed = target->IsClosed()
		|| point_distance(target_points.front(), target_points.back()) <= target->GetLength() * 1.0e-5;
	if (!target_closed && points.size() >= 2) {
		const double same_direction =
			point_distance(points.front(), target_points.front())
			+ point_distance(points.back(), target_points.back());
		const double reverse_direction =
			point_distance(points.front(), target_points.back())
			+ point_distance(points.back(), target_points.front());
		if (reverse_direction < same_direction)
			std::reverse(points.begin(), points.end());
	}
	return points.size() >= 2;
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

	// A face is not trimmed when every topological edge lies on one of the
	// four natural UV boundaries built in InitEdges3DCoat().  In that case the
	// complete rectangular UV net already represents the OCCT face and must
	// not be passed through the contour trimming code.
	UpdateMeshTypeFromBoundary();

	// The adaptive CNet::Build() is used only by the fast display renderer.
	// It refines by geometric deviation, so a planar face legitimately remains
	// a single quad. Low Poly has different semantics: Density determines a
	// regular QtyS x QtyT grid, including on planar and trimmed faces, and must
	// therefore continue to CNet::BuildNetByTwoQty() below.
	const bool low_poly_quadro = psol && psol->MeshQuadro;
	const bool use_adaptive_net = !low_poly_quadro
		&& (m_TypeMesh == REGULAR_MESH
			|| is_regular_uv_mesh_surface(F1));
	if (use_adaptive_net && build_regular_uv_mesh(this, Deflection)) {
		if (m_TypeMesh == REGULAR_MESH)
			return true;

		const double delta = 0.01;
		if (!pMesh3D || Polylines.empty()) {
			return true;
		}
		const GeomAbs_SurfaceType regular_type = surface_type_of(F1);
		const std::vector<Vec3> regular_vertices = pMesh3D->GetVertices();
		const std::vector<CMesh3D::Face> regular_faces = pMesh3D->GetFaces();
		const std::vector<UV> regular_uvs = pMesh3D->GetUVs();
		const std::vector<Vec3> regular_normals = pMesh3D->GetNormals();
		const size_t regular_face_count = active_face_count(pMesh3D);
		try {
			if (pMesh3D->PutOnSurface(this)) {
				const bool trimmed = trim_mesh_by_surface_boundary(pMesh3D, this, Polylines, delta);
				const bool classified = delete_mesh_faces_outside_occt_face(pMesh3D, this);
				const size_t trimmed_face_count = active_face_count(pMesh3D);
				if (trim_removed_too_much(regular_face_count, trimmed_face_count, regular_type)) {
					pMesh3D->SetGeometry(regular_vertices, regular_faces, regular_uvs, regular_normals);
					return true;
				}
				if (pMesh3D->RestoreTo3DFromUVSurface(this))
					IsTrimmed = trimmed || classified;
			}
		} catch (const Standard_Failure&) {
			if (pMesh3D)
				pMesh3D->SetGeometry(regular_vertices, regular_faces, regular_uvs, regular_normals);
			return true;
		}
		return true;
	}
	Handle(Geom_Surface) surf = BRep_Tool::Surface(F1);
	if (surf.IsNull())
		return false;
	if (BoundSpl.size() < 4 || Polylines.empty())
		return build_regular_uv_mesh(this, Deflection);
	int QtyS = 2;
	int QtyT = 2;
	const GeomAbs_SurfaceType surface_type = surface_type_of(F1);
	float len = BoundSpl[0]->GetLength();
	int Qty1 = low_poly_quadro
		? normalized_quadro_point_quantity(len, lenEdgeMax, Deflection)
		: mesh_point_quantity_for_length(len, Deflection, surface_type);
	QtyS = Qty1;
	len = BoundSpl[2]->GetLength();
	int Qty2 = low_poly_quadro
		? normalized_quadro_point_quantity(len, lenEdgeMax, Deflection)
		: mesh_point_quantity_for_length(len, Deflection, surface_type);
	QtyT = Qty2;
	double trim_u_min = Umin;
	double trim_u_max = Umax;
	double trim_v_min = Vmin;
	double trim_v_max = Vmax;
	if (uv_bounds_for_surface(this, trim_u_min, trim_u_max, trim_v_min, trim_v_max)) {
		adjust_mesh_quantities_from_prepared_edges(this, trim_u_min, trim_u_max, trim_v_min, trim_v_max, QtyS, QtyT);
	}
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
	if (m_TypeMesh == REGULAR_MESH) {
		IsInitMesh = true;
		return true;
	}
//  ====== Trimming =====
	const std::vector<Vec3> source_vertices = pMesh3D ? pMesh3D->GetVertices() : std::vector<Vec3>{};
	const std::vector<CMesh3D::Face> source_faces = pMesh3D ? pMesh3D->GetFaces() : std::vector<CMesh3D::Face>{};
	const std::vector<UV> source_uvs = pMesh3D ? pMesh3D->GetUVs() : std::vector<UV>{};
	const std::vector<Vec3> source_normals = pMesh3D ? pMesh3D->GetNormals() : std::vector<Vec3>{};
	const size_t source_face_count = active_face_count(pMesh3D);
	bool trimmed = false;
	bool classified = false;
	try {
		if (pMesh3D && pMesh3D->PutOnSurface(this)) {
			double delta = 0.01;
			trimmed = trim_mesh_by_surface_boundary(pMesh3D, this, Polylines, delta);
			classified = delete_mesh_faces_outside_occt_face(pMesh3D, this);
			const size_t trimmed_face_count = active_face_count(pMesh3D);
			if (trim_removed_too_much(source_face_count, trimmed_face_count, surface_type)
				&& !source_vertices.empty() && !source_faces.empty()) {
				pMesh3D->SetGeometry(source_vertices, source_faces, source_uvs, source_normals);
				trimmed = false;
				classified = false;
			}
		}
	} catch (const Standard_Failure&) {
		if (pMesh3D && !source_vertices.empty() && !source_faces.empty()) {
			pMesh3D->SetGeometry(source_vertices, source_faces, source_uvs, source_normals);
		}
		trimmed = false;
		classified = false;
	}


//	if (m_ID == 0) 
//		pMesh3D->ExportToObj("c:\\temp\\Mesh3D_0Trimmed.obj");

	try {
		if (pMesh3D)
			pMesh3D->RestoreTo3DFromUVSurface(this);
	} catch (const Standard_Failure&) {
		if (pMesh3D && !source_vertices.empty() && !source_faces.empty()) {
			pMesh3D->SetGeometry(source_vertices, source_faces, source_uvs, source_normals);
		}
		trimmed = false;
		classified = false;
	}


	IsTrimmed = trimmed || classified;
	IsInitMesh = true;
	return true;
}

void CSurfaceFace::MakeFilledContour(const std::vector<Vec3>& contour, Vec3 normal, CMesh3D* quad_mesh)
{
	if (!quad_mesh)
		return;
	if (contour.size() < 3) {
		quad_mesh->Clear();
		return;
	}

	CMesh3D triangle_mesh;
	FillContorByTriangles(&triangle_mesh, contour, normal);

	ContourQuadrangulator quadrangulator;
	quadrangulator.CreateFromMesh(&triangle_mesh);
	quadrangulator.Quadrangulate(quad_mesh);
}

bool CSurfaceFace::MakeQuadMeshFromBoundary(
	float density,
	CMesh3D* quad_mesh,
	std::vector<Vec3>* triangulation_boundary)
{
	if (!quad_mesh || m_Face.IsNull())
		return false;

	quad_mesh->Clear();
	if (triangulation_boundary)
		triangulation_boundary->clear();
	density = std::max(density, 0.0001f);

	try {
		const TopoDS_Face face = TopoDS::Face(m_Face);
		const TopoDS_Wire outer_wire = BRepTools::OuterWire(face);
		if (outer_wire.IsNull())
			return false;

		// Same normalized convention as Low Poly: the longest edge receives
		// approximately 20 * Density intervals. Density is not a distance in
		// millimetres; treating it that way can create millions of cells.
		double maximum_edge_length = 0.0;
		for (BRepTools_WireExplorer explorer(outer_wire, face); explorer.More(); explorer.Next()) {
			const TopoDS_Edge edge = explorer.Current();
			if (edge.IsNull() || BRep_Tool::Degenerated(edge))
				continue;
			try {
				BRepAdaptor_Curve curve(edge);
				maximum_edge_length = std::max(maximum_edge_length,
					GCPnts_AbscissaPoint::Length(
						curve, curve.FirstParameter(), curve.LastParameter()));
			} catch (const Standard_Failure&) {
			}
		}
		if (!std::isfinite(maximum_edge_length) || maximum_edge_length <= 1.0e-12)
			return false;
		const int maximum_intervals = std::clamp(
			static_cast<int>(std::ceil(20.0 * density)), 4, 256);
		const double target_step = maximum_edge_length / maximum_intervals;

		std::vector<Vec3> boundary;
		const auto append_unique = [&boundary](const gp_Pnt& point) {
			const Vec3 value{
				static_cast<float>(point.X()),
				static_cast<float>(point.Y()),
				static_cast<float>(point.Z())};
			if (boundary.empty() || dot(value - boundary.back(), value - boundary.back()) > 1.0e-12f)
				boundary.push_back(value);
		};

		for (BRepTools_WireExplorer explorer(outer_wire, face); explorer.More(); explorer.Next()) {
			const TopoDS_Edge edge = explorer.Current();
			if (edge.IsNull() || BRep_Tool::Degenerated(edge))
				continue;

			BRepAdaptor_Curve curve(edge);
			const double first = curve.FirstParameter();
			const double last = curve.LastParameter();
			double length = 0.0;
			try {
				length = GCPnts_AbscissaPoint::Length(curve, first, last);
			} catch (const Standard_Failure&) {
				length = 0.0;
			}
			const int segments = std::clamp(
				static_cast<int>(std::round(length / target_step)),
				1, maximum_intervals);
			const bool reversed = edge.Orientation() == TopAbs_REVERSED;
			GCPnts_UniformAbscissa uniform(curve, segments + 1, first, last);
			for (int sample = 0; sample <= segments; ++sample) {
				double parameter = 0.0;
				if (uniform.IsDone() && uniform.NbPoints() == segments + 1) {
					const int point_index = reversed
						? segments + 1 - sample : sample + 1;
					parameter = uniform.Parameter(point_index);
				} else {
					const double alpha = static_cast<double>(sample) / segments;
					parameter = reversed
						? last + (first - last) * alpha
						: first + (last - first) * alpha;
				}
				append_unique(curve.Value(parameter));
			}
		}

		if (boundary.size() > 2
			&& dot(boundary.front() - boundary.back(), boundary.front() - boundary.back()) <= 1.0e-10f) {
			boundary.pop_back();
		}
		if (boundary.size() < 3)
			return false;
		if (triangulation_boundary)
			*triangulation_boundary = boundary;

		// Keep the exact sampled 3D contour used as the source for UV mapping.
		// printToFile() writes files without an explicit directory to C:\temp.
		CPolyline world_contour("Fill Contour boundary 3D");
		for (const Vec3& point : boundary)
			world_contour.AddPoint(CPoint3d(point.x, point.y, point.z));
		world_contour.SetClosed(true);
		world_contour.printToFile("FillContourBoundary3D.txt");

		SurfaceUVMapping mapping(this);
		if (!mapping.IsValid())
			return false;

		std::vector<SurfaceUVPoint> uv_boundary;
		uv_boundary.reserve(boundary.size());
		for (const Vec3& point : boundary) {
			SurfaceUVPoint uv;
			if (!mapping.Project(point, uv))
				return false;
			if (!uv_boundary.empty())
				uv = mapping.UnwrapNear(uv, uv_boundary.back());
			uv_boundary.push_back(uv);
		}

		double world_perimeter = 0.0;
		double uv_perimeter = 0.0;
		for (size_t i = 0; i < boundary.size(); ++i) {
			const size_t next = (i + 1) % boundary.size();
			world_perimeter += std::sqrt(static_cast<double>(
				dot(boundary[next] - boundary[i], boundary[next] - boundary[i])));
			const double du = uv_boundary[next].u - uv_boundary[i].u;
			const double dv = uv_boundary[next].v - uv_boundary[i].v;
			uv_perimeter += std::sqrt(du * du + dv * dv);
		}
		if (world_perimeter <= 1.0e-12 || uv_perimeter <= 1.0e-12)
			return false;

		const double world_units_per_uv = world_perimeter / uv_perimeter;
		const float uv_density = static_cast<float>(std::max(
			target_step / world_units_per_uv, 1.0e-6));

		CPolyline uv_contour("Surface boundary UV");
		for (const SurfaceUVPoint& uv : uv_boundary)
			uv_contour.AddPoint(CPoint3d(uv.u, uv.v, 0.0));
		uv_contour.SetClosed(true);
		// This is the contour passed verbatim to CreateFromBoundary().  Dump it
		// separately because a bad seam unwrap on a fillet is visible here even
		// when the sampled 3D boundary itself looks correct.
		uv_contour.printToFile("FillContourBoundaryUV.txt");

		if (!quad_mesh->CreateFromBoundary(&uv_contour, uv_density))
			return false;
		if (!quad_mesh->RestoreTo3DFromUVSurface(this)) {
			quad_mesh->Clear();
			return false;
		}
		return !quad_mesh->GetFaces().empty();
	} catch (const Standard_Failure&) {
		quad_mesh->Clear();
		return false;
	}
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
		// The curve returned by BRep_Tool is expressed in its local
		// coordinates.  A replayed Move is stored in the edge location; without
		// applying it, the trim boundary remains around the original sphere and
		// incorrectly cuts the translated regular mesh.
		pnt.Transform(Loc.Transformation());
		CPoint3d p3d(pnt.X(), pnt.Y(), pnt.Z());
		pl->AddPoint(&p3d);
	}
}

void CSurfaceFace::GetEdges(std::vector<CPolyline*>& plines)
{
	if (m_Face.IsNull())
		return;

	for (TopExp_Explorer edge_explorer(m_Face, TopAbs_EDGE);
		edge_explorer.More(); edge_explorer.Next()) {
		try {
			TopoDS_Edge edge = TopoDS::Edge(edge_explorer.Current());
			if (edge.IsNull() || BRep_Tool::Degenerated(edge))
				continue;

			std::unique_ptr<CPolyline> polyline = std::make_unique<CPolyline>();
			GetPointFromCurve(edge, 5, polyline.get());
			if (polyline->np() >= 2)
				plines.push_back(polyline.release());
		} catch (const Standard_Failure&) {
			// Singular loft seams and degenerated poles are not display edges.
		}
	}
}

bool CSurfaceFace::IsBoundLine(CPolyline* line)
{
	if (!line || line->np() < 2 || BoundSpl.empty())
		return false;

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
