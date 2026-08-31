#include "SurfaceFace.h"
#include "../FillContour.h"
#include "Solid.h"
#include "SolidTool.h"
#include "../iges/SplineCurve.h"
#include "../Net.h"
#include "../CAlfaDoc.h"
#include "../SurfaceUVMapping.h"
#include "../SurfacePatchBuilder.h"
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
#include <array>
#include <cmath>
#include <functional>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
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

	// 3DCoat calibration: at Density 0.20 an edge about one quarter of the
	// body's longest edge still uses Qty Min, while at 0.296 it already gains
	// the next pair of intervals. A reference count of 20 left that edge pinned
	// to Qty Min all the way through Density 0.693. Sixty reproduces the four
	// measured 3DCoat levels while retaining length-proportional sizing.
	// Odd point counts are required by the quad front.
	constexpr double density_reference_quantity = 60.0;
	int maximum_quantity = static_cast<int>(
		density_reference_quantity / deflection);
	if (maximum_quantity < 4)
		maximum_quantity = 4;
	int quantity = static_cast<int>(
		static_cast<double>(maximum_quantity) / maximum_edge_length * length);
	if (IsEven(quantity))
		++quantity;
	if (quantity < CSurfaceFace::m_QtyMin)
		quantity = CSurfaceFace::m_QtyMin;

	// Flooring point counts creates a severe threshold at low density.  On the
	// Box_And_Boss regression at Density 0.25 the 218.10 reference edge has a
	// 15.58 step, while the 55.14 boss edge remained at Qty Min and jumped to a
	// 27.57 step. Keep the measured 3DCoat levels, including Qty Min at 0.20,
	// but advance by the next even pair of intervals once a local edge becomes
	// more than 1.6 times coarser than the body's reference step.
	int maximum_point_count = maximum_quantity;
	if (IsEven(maximum_point_count))
		++maximum_point_count;
	maximum_point_count = std::max(
		maximum_point_count, CSurfaceFace::m_QtyMin);
	const int maximum_segments = std::max(2, maximum_point_count - 1);
	const double reference_step = maximum_edge_length / maximum_segments;
	constexpr double maximum_step_ratio = 1.6;
	while (quantity < maximum_point_count
		&& length / std::max(2, quantity - 1)
			> reference_step * maximum_step_ratio) {
		quantity += 2;
	}
	return quantity;
}

bool has_concave_planar_outer_wire(const TopoDS_Face& face)
{
	if (face.IsNull())
		return false;
	try {
		BRepAdaptor_Surface surface(face);
		if (surface.GetType() != GeomAbs_Plane)
			return false;
		const TopoDS_Wire outer = BRepTools::OuterWire(face);
		if (outer.IsNull())
			return false;

		std::vector<gp_Pnt2d> points;
		for (BRepTools_WireExplorer explorer(outer, face);
			explorer.More(); explorer.Next()) {
			const TopoDS_Vertex vertex = explorer.CurrentVertex();
			if (vertex.IsNull())
				continue;
			double u = 0.0;
			double v = 0.0;
			ElSLib::Parameters(
				surface.Plane(), BRep_Tool::Pnt(vertex), u, v);
			if (points.empty()
				|| points.back().Distance(gp_Pnt2d(u, v)) > 1.0e-8) {
				points.emplace_back(u, v);
			}
		}
		if (points.size() > 2
			&& points.front().Distance(points.back()) <= 1.0e-8) {
			points.pop_back();
		}
		if (points.size() < 4)
			return false;

		double u_min = points.front().X();
		double u_max = u_min;
		double v_min = points.front().Y();
		double v_max = v_min;
		for (const gp_Pnt2d& point : points) {
			u_min = std::min(u_min, point.X());
			u_max = std::max(u_max, point.X());
			v_min = std::min(v_min, point.Y());
			v_max = std::max(v_max, point.Y());
		}
		const double span = std::max(u_max - u_min, v_max - v_min);
		const double epsilon = std::max(1.0e-12, span * span * 1.0e-10);
		bool positive = false;
		bool negative = false;
		for (size_t index = 0; index < points.size(); ++index) {
			const gp_Pnt2d& previous = points[
				(index + points.size() - 1) % points.size()];
			const gp_Pnt2d& current = points[index];
			const gp_Pnt2d& next = points[(index + 1) % points.size()];
			const double cross_value =
				(current.X() - previous.X()) * (next.Y() - current.Y())
				- (current.Y() - previous.Y()) * (next.X() - current.X());
			positive = positive || cross_value > epsilon;
			negative = negative || cross_value < -epsilon;
			if (positive && negative)
				return true;
		}
	} catch (const Standard_Failure&) {
	}
	return false;
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

bool triangulate_mesh_ngons_in_xy(CMesh3D& mesh)
{
	const std::vector<Vec3>& vertices = mesh.GetVertices();
	std::vector<CMesh3D::Face> result;
	result.reserve(mesh.GetFaces().size());
	constexpr double eps = 1.0e-12;
	const auto cross_2d = [&](size_t first, size_t middle, size_t last) {
		const Vec3& a = vertices[first];
		const Vec3& b = vertices[middle];
		const Vec3& c = vertices[last];
		return static_cast<double>(b.x - a.x) * (c.y - b.y)
			- static_cast<double>(b.y - a.y) * (c.x - b.x);
	};
	const auto point_in_triangle = [&](size_t point, size_t a, size_t b,
			size_t c, double orientation) {
		const double ab = cross_2d(a, b, point) * orientation;
		const double bc = cross_2d(b, c, point) * orientation;
		const double ca = cross_2d(c, a, point) * orientation;
		return ab >= -eps && bc >= -eps && ca >= -eps;
	};

	for (const CMesh3D::Face& source : mesh.GetFaces()) {
		if (source.deleted || source.corners.size() <= 4) {
			result.push_back(source);
			continue;
		}
		bool valid = true;
		for (const MeshCorner& corner : source.corners) {
			if (corner.v >= vertices.size()) {
				valid = false;
				break;
			}
		}
		if (!valid)
			return false;

		double twice_area = 0.0;
		for (size_t i = 0; i < source.corners.size(); ++i) {
			const Vec3& a = vertices[source.corners[i].v];
			const Vec3& b = vertices[source.corners[
				(i + 1) % source.corners.size()].v];
			twice_area += static_cast<double>(a.x) * b.y
				- static_cast<double>(b.x) * a.y;
		}
		if (std::fabs(twice_area) <= eps)
			return false;
		const double orientation = twice_area > 0.0 ? 1.0 : -1.0;
		std::vector<size_t> remaining(source.corners.size());
		for (size_t i = 0; i < remaining.size(); ++i)
			remaining[i] = i;

		while (remaining.size() > 3) {
			bool clipped = false;
			for (size_t i = 0; i < remaining.size(); ++i) {
				const size_t previous = remaining[
					(i + remaining.size() - 1) % remaining.size()];
				const size_t current = remaining[i];
				const size_t next = remaining[(i + 1) % remaining.size()];
				const size_t a = source.corners[previous].v;
				const size_t b = source.corners[current].v;
				const size_t c = source.corners[next].v;
				if (cross_2d(a, b, c) * orientation <= eps)
					continue;
				bool contains_vertex = false;
				for (size_t candidate : remaining) {
					if (candidate == previous || candidate == current
						|| candidate == next) {
						continue;
					}
					if (point_in_triangle(source.corners[candidate].v,
							a, b, c, orientation)) {
						contains_vertex = true;
						break;
					}
				}
				if (contains_vertex)
					continue;

				CMesh3D::Face triangle = source;
				triangle.corners = {source.corners[previous],
					source.corners[current], source.corners[next]};
				result.push_back(std::move(triangle));
				remaining.erase(remaining.begin() + i);
				clipped = true;
				break;
			}
			if (!clipped)
				return false;
		}
		CMesh3D::Face triangle = source;
		triangle.corners = {source.corners[remaining[0]],
			source.corners[remaining[1]], source.corners[remaining[2]]};
		result.push_back(std::move(triangle));
	}
	mesh.GetFaces() = std::move(result);
	return true;
}

void merge_trim_triangle_pairs_to_quads(CMesh3D& mesh)
{
	using Edge = std::pair<size_t, size_t>;
	std::vector<CMesh3D::Face>& faces = mesh.GetFaces();
	const std::vector<Vec3>& vertices = mesh.GetVertices();
	std::map<Edge, std::vector<size_t>> triangle_edges;
	for (size_t face_index = 0; face_index < faces.size(); ++face_index) {
		const CMesh3D::Face& face = faces[face_index];
		if (face.deleted || face.corners.size() != 3)
			continue;
		for (size_t corner = 0; corner < 3; ++corner) {
			const size_t first = face.corners[corner].v;
			const size_t second = face.corners[(corner + 1) % 3].v;
			if (first < vertices.size() && second < vertices.size() && first != second)
				triangle_edges[std::minmax(first, second)].push_back(face_index);
		}
	}

	struct Candidate {
		size_t first = 0;
		size_t second = 0;
		double shared_length_sq = 0.0;
	};
	std::vector<Candidate> candidates;
	for (const auto& [edge, owners] : triangle_edges) {
		if (owners.size() != 2)
			continue;
		const Vec3 delta = vertices[edge.second] - vertices[edge.first];
		candidates.push_back({owners[0], owners[1],
			static_cast<double>(dot(delta, delta))});
	}
	// A trimmed source quad is normally split along its diagonal, which is the
	// longest common edge. Prefer that diagonal over an unrelated neighbouring
	// triangle edge when more than one pairing is possible.
	std::sort(candidates.begin(), candidates.end(),
		[](const Candidate& first, const Candidate& second) {
			return first.shared_length_sq > second.shared_length_sq;
		});
	std::vector<bool> paired(faces.size(), false);
	for (const Candidate& candidate : candidates) {
		if (paired[candidate.first] || paired[candidate.second]
			|| faces[candidate.first].deleted || faces[candidate.second].deleted)
			continue;

		std::map<Edge, int> edge_counts;
		std::map<size_t, std::vector<size_t>> boundary_neighbors;
		std::map<size_t, MeshCorner> source_corners;
		for (size_t face_index : {candidate.first, candidate.second}) {
			const CMesh3D::Face& face = faces[face_index];
			for (size_t corner = 0; corner < 3; ++corner) {
				const MeshCorner& first = face.corners[corner];
				const MeshCorner& second = face.corners[(corner + 1) % 3];
				++edge_counts[std::minmax(first.v, second.v)];
				source_corners.emplace(first.v, first);
			}
		}
		for (const auto& [edge, count] : edge_counts) {
			if (count != 1)
				continue;
			boundary_neighbors[edge.first].push_back(edge.second);
			boundary_neighbors[edge.second].push_back(edge.first);
		}
		if (boundary_neighbors.size() != 4)
			continue;
		std::vector<size_t> cycle;
		cycle.reserve(4);
		size_t previous = std::numeric_limits<size_t>::max();
		size_t current = boundary_neighbors.begin()->first;
		bool valid = true;
		for (size_t corner = 0; corner < 4; ++corner) {
			cycle.push_back(current);
			const auto& neighbors = boundary_neighbors[current];
			if (neighbors.size() != 2) {
				valid = false;
				break;
			}
			const size_t next = neighbors[0] != previous
				? neighbors[0] : neighbors[1];
			previous = current;
			current = next;
		}
		if (!valid || current != cycle.front())
			continue;

		const auto signed_area = [&](const std::vector<size_t>& indices) {
			double twice_area = 0.0;
			for (size_t i = 0; i < indices.size(); ++i) {
				const Vec3& first = vertices[indices[i]];
				const Vec3& second = vertices[indices[(i + 1) % indices.size()]];
				twice_area += static_cast<double>(first.x) * second.y
					- static_cast<double>(second.x) * first.y;
			}
			return twice_area;
		};
		std::vector<size_t> reference;
		for (const MeshCorner& corner : faces[candidate.first].corners)
			reference.push_back(corner.v);
		if (signed_area(cycle) * signed_area(reference) < 0.0)
			std::reverse(cycle.begin(), cycle.end());

		CMesh3D::Face quad;
		for (size_t vertex : cycle)
			quad.corners.push_back(source_corners.at(vertex));
		faces[candidate.first] = std::move(quad);
		faces[candidate.second].deleted = true;
		paired[candidate.first] = true;
		paired[candidate.second] = true;
	}
}

// Rejected experiment: splitting a spherical triangular corner into three
// quad blocks creates a visible small "house" at the fillet junction.  Keep
// it disabled so it cannot be reconnected accidentally while the replacement
// corner topology is developed and verified against the project corpus.
#if 0
bool repair_partial_sphere_pole(CSurfaceFace* surface)
{
	if (!surface || !surface->pMesh3D
		|| surface->m_QtyU < 3 || surface->m_QtyV < 3) {
		return false;
	}
	const std::vector<Vec3>& source_vertices = surface->pMesh3D->GetVertices();
	const std::vector<UV>& source_uvs = surface->pMesh3D->GetUVs();
	const std::vector<Vec3>& source_normals = surface->pMesh3D->GetNormals();
	const size_t expected = static_cast<size_t>(surface->m_QtyU)
		* static_cast<size_t>(surface->m_QtyV);
	if (source_vertices.size() < expected || source_uvs.size() < expected
		|| source_normals.size() < expected) {
		return false;
	}

	const auto index = [surface](int u, int v) {
		return static_cast<size_t>(v * surface->m_QtyU + u);
	};
	std::array<std::vector<size_t>, 4> sides;
	for (int u = 0; u < surface->m_QtyU; ++u) {
		sides[0].push_back(index(u, 0));
		sides[1].push_back(index(u, surface->m_QtyV - 1));
	}
	for (int v = 0; v < surface->m_QtyV; ++v) {
		sides[2].push_back(index(0, v));
		sides[3].push_back(index(surface->m_QtyU - 1, v));
	}
	double scale_sq = 1.0;
	for (Vec3 vertex : source_vertices) {
		const Vec3 delta = vertex - source_vertices.front();
		scale_sq = std::max(scale_sq, static_cast<double>(dot(delta, delta)));
	}
	const double collapsed_tolerance_sq = scale_sq * 1.0e-10;
	int collapsed_side = -1;
	for (int side = 0; side < 4; ++side) {
		bool collapsed = true;
		for (size_t vertex : sides[side]) {
			const Vec3 delta = source_vertices[vertex]
				- source_vertices[sides[side].front()];
			if (static_cast<double>(dot(delta, delta))
				> collapsed_tolerance_sq) {
				collapsed = false;
				break;
			}
		}
		if (!collapsed)
			continue;
		if (collapsed_side >= 0)
			return false;
		collapsed_side = side;
	}
	if (collapsed_side < 0)
		return false;

	const int angle_points = collapsed_side < 2
		? surface->m_QtyU : surface->m_QtyV;
	const int radial_points = collapsed_side < 2
		? surface->m_QtyV : surface->m_QtyU;
	if (angle_points != radial_points || angle_points < 3
		|| (angle_points & 1) == 0)
		return false;
	const int segments = angle_points - 1;
	const int block_segments = segments / 2;

	std::vector<Vec3> vertices = source_vertices;
	std::vector<UV> uvs = source_uvs;
	std::vector<Vec3> normals = source_normals;
	std::vector<CMesh3D::Face> faces;
	faces.reserve(static_cast<size_t>(3 * block_segments * block_segments));
	double u_min = source_uvs.front().u;
	double u_max = u_min;
	double v_min = source_uvs.front().v;
	double v_max = v_min;
	for (size_t i = 0; i < expected; ++i) {
		u_min = std::min(u_min, static_cast<double>(source_uvs[i].u));
		u_max = std::max(u_max, static_cast<double>(source_uvs[i].u));
		v_min = std::min(v_min, static_cast<double>(source_uvs[i].v));
		v_max = std::max(v_max, static_cast<double>(source_uvs[i].v));
	}
	using ParamKey = std::pair<long long, long long>;
	std::map<ParamKey, size_t> param_vertices;
	const auto grid_index = [&](int angle_step, int radial_step) {
		switch (collapsed_side) {
		case 0: return index(angle_step, radial_step);
		case 1: return index(angle_step, segments - radial_step);
		case 2: return index(radial_step, angle_step);
		default: return index(segments - radial_step, angle_step);
		}
	};
	const auto vertex_at = [&](double q, double r) -> size_t {
		constexpr double key_scale = 1000000000.0;
		const ParamKey key{
			std::llround(q * key_scale), std::llround(r * key_scale)};
		const auto existing = param_vertices.find(key);
		if (existing != param_vertices.end())
			return existing->second;

		const double eps = 1.0e-9;
		if (r <= eps) {
			const size_t vertex = grid_index(0, 0);
			param_vertices.emplace(key, vertex);
			return vertex;
		}
		const double angle = std::clamp(q / r, 0.0, 1.0);
		const bool on_boundary = q <= eps || std::fabs(q - r) <= eps
			|| std::fabs(r - 1.0) <= eps;
		if (on_boundary) {
			const int angle_step = std::clamp(
				static_cast<int>(std::llround(angle * segments)), 0, segments);
			const int radial_step = std::clamp(
				static_cast<int>(std::llround(r * segments)), 0, segments);
			const size_t vertex = grid_index(angle_step, radial_step);
			param_vertices.emplace(key, vertex);
			return vertex;
		}

		double u_normalized = angle;
		double v_normalized = r;
		if (collapsed_side == 1)
			v_normalized = 1.0 - r;
		else if (collapsed_side == 2) {
			u_normalized = r;
			v_normalized = angle;
		} else if (collapsed_side == 3) {
			u_normalized = 1.0 - r;
			v_normalized = angle;
		}
		const UV uv{
			static_cast<float>(u_min + (u_max - u_min) * u_normalized),
			static_cast<float>(v_min + (v_max - v_min) * v_normalized)};
		CPoint8d point;
		if (!surface->GetPoint(uv.u, uv.v, &point))
			return std::numeric_limits<size_t>::max();
		const size_t vertex = vertices.size();
		vertices.push_back({static_cast<float>(point.x),
			static_cast<float>(point.y), static_cast<float>(point.z)});
		uvs.push_back(uv);
		normals.push_back(normalize({static_cast<float>(point.l),
			static_cast<float>(point.m), static_cast<float>(point.n)}));
		param_vertices.emplace(key, vertex);
		return vertex;
	};
	const auto append_quad = [&](std::array<size_t, 4> quad) {
		Vec3 face_normal = cross(vertices[quad[1]] - vertices[quad[0]],
			vertices[quad[2]] - vertices[quad[0]]);
		Vec3 expected_normal{};
		for (size_t vertex : quad)
			expected_normal = expected_normal + normals[vertex];
		if (dot(face_normal, expected_normal) < 0.0f)
			std::reverse(quad.begin(), quad.end());
		CMesh3D::Face face;
		for (size_t vertex : quad)
			face.corners.push_back({vertex, vertex, vertex});
		faces.push_back(std::move(face));
	};
	struct ParamPoint { double q; double r; };
	const ParamPoint apex{0.0, 0.0};
	const ParamPoint left{0.0, 1.0};
	const ParamPoint right{1.0, 1.0};
	const ParamPoint left_middle{0.0, 0.5};
	const ParamPoint base_middle{0.5, 1.0};
	const ParamPoint right_middle{0.5, 0.5};
	const ParamPoint centre{1.0 / 3.0, 2.0 / 3.0};
	const std::array<std::array<ParamPoint, 4>, 3> blocks{{
		{{apex, right_middle, centre, left_middle}},
		{{left, left_middle, centre, base_middle}},
		{{right, base_middle, centre, right_middle}}
	}};
	const auto interpolate = [](const std::array<ParamPoint, 4>& block,
			double s, double t) {
		const double w0 = (1.0 - s) * (1.0 - t);
		const double w1 = s * (1.0 - t);
		const double w2 = s * t;
		const double w3 = (1.0 - s) * t;
		return ParamPoint{
			block[0].q * w0 + block[1].q * w1
				+ block[2].q * w2 + block[3].q * w3,
			block[0].r * w0 + block[1].r * w1
				+ block[2].r * w2 + block[3].r * w3};
	};
	for (const auto& block : blocks) {
		std::vector<size_t> block_vertices(
			static_cast<size_t>((block_segments + 1) * (block_segments + 1)));
		for (int t = 0; t <= block_segments; ++t) {
			for (int s = 0; s <= block_segments; ++s) {
				const ParamPoint point = interpolate(
					block, static_cast<double>(s) / block_segments,
					static_cast<double>(t) / block_segments);
				const size_t vertex = vertex_at(point.q, point.r);
				if (vertex == std::numeric_limits<size_t>::max())
					return false;
				block_vertices[static_cast<size_t>(
					t * (block_segments + 1) + s)] = vertex;
			}
		}
		for (int t = 0; t < block_segments; ++t) {
			for (int s = 0; s < block_segments; ++s) {
				const size_t row = static_cast<size_t>(t * (block_segments + 1));
				const size_t next_row = row + block_segments + 1;
				append_quad({block_vertices[row + s],
					block_vertices[row + s + 1],
					block_vertices[next_row + s + 1],
					block_vertices[next_row + s]});
			}
		}
	}

	return surface->pMesh3D->SetGeometry(
		std::move(vertices), std::move(faces),
		std::move(uvs), std::move(normals));
}
#endif

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
	double classifier_u_min = 0.0;
	double classifier_u_max = 0.0;
	double classifier_v_min = 0.0;
	double classifier_v_max = 0.0;
	bool classifier_u_periodic = false;
	bool classifier_v_periodic = false;
	double classifier_u_period = 0.0;
	double classifier_v_period = 0.0;
	try {
		BRepTools::UVBounds(face, classifier_u_min, classifier_u_max,
			classifier_v_min, classifier_v_max);
		BRepAdaptor_Surface adaptor(face);
		classifier_u_periodic = adaptor.IsUPeriodic();
		classifier_v_periodic = adaptor.IsVPeriodic();
		classifier_u_period = classifier_u_periodic ? adaptor.UPeriod() : 0.0;
		classifier_v_period = classifier_v_periodic ? adaptor.VPeriod() : 0.0;
	} catch (const Standard_Failure&) {
		classifier_u_periodic = false;
		classifier_v_periodic = false;
	}
	const auto wrap_periodic = [](double value, double minimum,
		double period) {
		if (!(period > 0.0) || !std::isfinite(value))
			return value;
		double wrapped = minimum + std::fmod(value - minimum, period);
		if (wrapped < minimum)
			wrapped += period;
		return wrapped;
	};
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
		if (classifier_u_periodic)
			center.x = wrap_periodic(center.x,
				classifier_u_min, classifier_u_period);
		if (classifier_v_periodic)
			center.y = wrap_periodic(center.y,
				classifier_v_min, classifier_v_period);
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

bool delete_mesh_faces_outside_occt_face_3d(CMesh3D* mesh,
	CSurfaceFace* surface)
{
	if (!mesh || !surface || surface->m_Face.IsNull())
		return false;
	SurfaceUVMapping mapping(surface);
	if (!mapping.IsValid())
		return false;

	bool changed = false;
	const TopoDS_Face face = TopoDS::Face(surface->m_Face);
	const std::vector<Vec3>& vertices = mesh->GetVertices();
	std::vector<CMesh3D::Face>& mesh_faces = mesh->GetFaces();
	std::vector<bool> outside(mesh_faces.size(), false);
	for (size_t face_index = 0; face_index < mesh_faces.size(); ++face_index) {
		CMesh3D::Face& mesh_face = mesh_faces[face_index];
		if (mesh_face.deleted || mesh_face.corners.size() < 3)
			continue;
		Vec3 center{};
		size_t valid_corners = 0;
		for (const MeshCorner& corner : mesh_face.corners) {
			if (corner.v >= vertices.size())
				continue;
			center = center + vertices[corner.v];
			++valid_corners;
		}
		if (valid_corners != mesh_face.corners.size())
			continue;
		center = center * (1.0f / static_cast<float>(valid_corners));
		SurfaceUVPoint uv;
		if (!mapping.Project(center, uv))
			continue;
		try {
			BRepClass_FaceClassifier classifier(face,
				gp_Pnt2d(uv.u, uv.v), 1.0e-7, Standard_False);
			if (classifier.State() == TopAbs_OUT)
				outside[face_index] = true;
		} catch (const Standard_Failure&) {
		}
	}

	// Only peel OUT cells that are connected to an existing open boundary.
	// Numerical projection near a periodic B-Spline seam can classify an
	// isolated valid cell as OUT; deleting such a cell punches a small hole in
	// an otherwise watertight quad patch. Real wraparound tabs are attached to
	// the patch boundary and remain removable by this flood fill.
	using EdgeKey = std::pair<size_t, size_t>;
	std::map<EdgeKey, std::vector<size_t>> edge_faces;
	for (size_t face_index = 0; face_index < mesh_faces.size(); ++face_index) {
		const CMesh3D::Face& mesh_face = mesh_faces[face_index];
		if (mesh_face.deleted || mesh_face.corners.size() < 3)
			continue;
		for (size_t corner = 0; corner < mesh_face.corners.size(); ++corner) {
			const size_t first = mesh_face.corners[corner].v;
			const size_t second = mesh_face.corners[
				(corner + 1) % mesh_face.corners.size()].v;
			if (first >= vertices.size() || second >= vertices.size())
				continue;
			edge_faces[std::minmax(first, second)].push_back(face_index);
		}
	}
	std::vector<size_t> pending;
	std::vector<bool> removable(mesh_faces.size(), false);
	for (const auto& edge : edge_faces) {
		if (edge.second.size() == 1 && outside[edge.second.front()]) {
			const size_t face_index = edge.second.front();
			if (!removable[face_index]) {
				removable[face_index] = true;
				pending.push_back(face_index);
			}
		}
	}
	while (!pending.empty()) {
		const size_t face_index = pending.back();
		pending.pop_back();
		const CMesh3D::Face& mesh_face = mesh_faces[face_index];
		for (size_t corner = 0; corner < mesh_face.corners.size(); ++corner) {
			const EdgeKey edge = std::minmax(mesh_face.corners[corner].v,
				mesh_face.corners[(corner + 1) % mesh_face.corners.size()].v);
			const auto owners = edge_faces.find(edge);
			if (owners == edge_faces.end())
				continue;
			for (size_t neighbour : owners->second) {
				if (outside[neighbour] && !removable[neighbour]) {
					removable[neighbour] = true;
					pending.push_back(neighbour);
				}
			}
		}
	}
	for (size_t face_index = 0; face_index < mesh_faces.size(); ++face_index) {
		if (removable[face_index]) {
			mesh_faces[face_index].deleted = true;
			changed = true;
		}
	}
	return changed;
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
	m_PreparedTopoEdges.clear();
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
	for (TopExp_Explorer edge_explorer(m_Face, TopAbs_EDGE);
		edge_explorer.More(); edge_explorer.Next()) {
		try {
			const TopoDS_Edge edge = TopoDS::Edge(edge_explorer.Current());
			if (!edge.IsNull() && !BRep_Tool::Degenerated(edge))
				m_PreparedTopoEdges.push_back(edge);
		} catch (const Standard_Failure&) {
		}
	}
	if (m_PreparedTopoEdges.size() != plines.size())
		m_PreparedTopoEdges.clear();
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
	std::vector<TopoDS_Edge> Edges2Topo;
	Edges2.push_back(Polylines[0]);
	if (m_PreparedTopoEdges.size() == Polylines.size())
		Edges2Topo.push_back(m_PreparedTopoEdges[0]);
	bool NeedRevers = false;
	Polylines.erase(Polylines.begin());
	if (!m_PreparedTopoEdges.empty())
		m_PreparedTopoEdges.erase(m_PreparedTopoEdges.begin());
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
		if (!m_PreparedTopoEdges.empty())
			Edges2Topo.push_back(m_PreparedTopoEdges[static_cast<size_t>(j_min)]);
		if (NeedRevers)
			Polylines[j_min]->Revers();
		Polylines.erase(Polylines.begin() + j_min);
		if (!m_PreparedTopoEdges.empty())
			m_PreparedTopoEdges.erase(m_PreparedTopoEdges.begin() + j_min);
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
	if (Edges2Topo.size() == Polylines.size())
		m_PreparedTopoEdges = std::move(Edges2Topo);
	else
		m_PreparedTopoEdges.clear();

	// Keep the requested node count, but place the nodes at uniform physical
	// distances along every OCCT edge. MakePolylineByQtyKnots() is uniform in
	// spline parameter/knot space, which can create a large step jump on an
	// otherwise smooth trimming boundary. In Mesh Quadro, short circular edges
	// need their own density-driven angular count: scaling only by the longest
	// edge of the body pins a small corner fillet to the same minimum count over
	// nearly the complete Density range.
	// A closed circle is the one fidelity exception to the length rule: at low
	// density a three- or eight-sided cylinder no longer represents the source
	// geometry. Keep the exception named and isolated so it can become a tool
	// parameter later without changing the 3DCoat sizing formula above. The
	// shared OCCT edge synchronization gives the cylindrical wall the same ring.
	constexpr double pi = 3.14159265358979323846;
	constexpr double curved_edge_max_angle = pi / 4.0;
	// Independent from density_reference_quantity: this is a silhouette
	// fidelity limit of 18 degrees per sector for a closed circle/ellipse.
	constexpr double closed_circular_max_angle = pi / 10.0;
	for (int edge_index = 0;
		edge_index < static_cast<int>(Polylines.size()); ++edge_index) {
		TopoDS_Edge topo_edge;
		if (!GetPreparedTopoEdge(edge_index, topo_edge))
			continue;
		try {
			BRepAdaptor_Curve curve(topo_edge);
			const double first = curve.FirstParameter();
			const double last = curve.LastParameter();
			if (!std::isfinite(first) || !std::isfinite(last) || last <= first)
				continue;
			const int current_segments = std::max(
				1, GetPreparedPolylinePointCount(edge_index) - 1);
			const bool circular = curve.GetType() == GeomAbs_Circle
				|| curve.GetType() == GeomAbs_Ellipse;
			int fidelity_segments = 1;
			if (circular) {
				if (normalized_quadro_density) {
					const double density = 1.0 / std::max(
						static_cast<double>(Deflection), 0.0001);
					const int density_segments = static_cast<int>(std::ceil(
						(last - first) * 8.0 * density
						/ 3.14159265358979323846));
					const int closed_fidelity = curve.IsClosed()
						? static_cast<int>(std::ceil(
							(last - first) / closed_circular_max_angle))
						: 2;
					fidelity_segments = std::clamp(
						std::max(closed_fidelity, density_segments), 2, 512);
				} else {
					fidelity_segments = std::max(2,
						static_cast<int>(std::ceil(
							(last - first) / curved_edge_max_angle)));
				}
			}
			int segments = std::max(current_segments, fidelity_segments);
			// Quad sphere caps and closed trimming loops both require an even
			// circular segment count. Make the shared OCCT edge even here, before
			// adjacent surfaces copy it, instead of inserting an unmatched node.
			if (normalized_quadro_density && circular && (segments & 1) != 0)
				++segments;

			std::vector<CPoint3d> exact_points;
			exact_points.reserve(static_cast<size_t>(segments + 1));
			GCPnts_UniformAbscissa uniform(curve, segments + 1, first, last);
			for (int point_index = 0; point_index <= segments; ++point_index) {
				const double parameter = uniform.IsDone()
					? uniform.Parameter(point_index + 1)
					: first + (last - first) * point_index / segments;
				const gp_Pnt point = curve.Value(parameter);
				exact_points.emplace_back(point.X(), point.Y(), point.Z());
			}

			std::vector<CPoint3d> old_points;
			if (GetPreparedPolylinePoints(edge_index, old_points)
				&& old_points.size() >= 2) {
				const double forward = old_points.front().DistTo(
					&exact_points.front());
				const double reverse = old_points.front().DistTo(
					&exact_points.back());
				if (reverse < forward)
					std::reverse(exact_points.begin(), exact_points.end());
			}
			SetPreparedPolylinePoints(edge_index, exact_points);
		} catch (const Standard_Failure&) {
			// Keep the knot-sampled density polyline if exact resampling fails.
		}
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
	if (m_PreparedTopoEdges.size() == Polylines.size()
		&& !m_PreparedTopoEdges[static_cast<size_t>(edge_index)].IsNull()) {
		edge = m_PreparedTopoEdges[static_cast<size_t>(edge_index)];
		return true;
	}
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
	// A concave planar outer wire must be filled from its real contour.
	// Treating it as a regular UV rectangle fills back edge notches made by a
	// boolean Box/Cylinder cut, leaving an apparently solid top over the cavity.
	if (has_concave_planar_outer_wire(TopoDS::Face(m_Face)))
		return;

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
		double candidate_length = 0.0;
		for (size_t i = 1; i < candidates[candidate_index].size(); ++i) {
			const Vec3 delta = vertices[candidates[candidate_index][i]]
				- vertices[candidates[candidate_index][i - 1]];
			candidate_length += std::sqrt(static_cast<double>(dot(delta, delta)));
		}
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
		// A spherical UV pole is represented by a whole grid side whose
		// vertices all occupy the same geometric point.  A one-sided distance
		// test gives that collapsed side a perfect score for every incident
		// edge because the pole is also an edge endpoint.  Include length and
		// endpoint coverage so only the grid side spanning the complete OCCT
		// edge can be selected for seam synchronization.
		const double target_length = std::max(target->GetLength(), 1.0e-12);
		const double length_delta = candidate_length - target_length;
		score += length_delta * length_delta;
		const Vec3 target_first{
			static_cast<float>(target_points.front().x),
			static_cast<float>(target_points.front().y),
			static_cast<float>(target_points.front().z)};
		const Vec3 target_last{
			static_cast<float>(target_points.back().x),
			static_cast<float>(target_points.back().y),
			static_cast<float>(target_points.back().z)};
		const Vec3 candidate_first = vertices[candidates[candidate_index].front()];
		const Vec3 candidate_last = vertices[candidates[candidate_index].back()];
		const double same_ends = static_cast<double>(dot(
			candidate_first - target_first, candidate_first - target_first))
			+ static_cast<double>(dot(
				candidate_last - target_last, candidate_last - target_last));
		const double reversed_ends = static_cast<double>(dot(
			candidate_first - target_last, candidate_first - target_last))
			+ static_cast<double>(dot(
				candidate_last - target_first, candidate_last - target_first));
		score += std::min(same_ends, reversed_ends);
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
	if (low_poly_quadro) {
		m_LastLowPolyDensity = 1.0f / std::max(Deflection, 0.0001f);
		m_LastIslandBoundariesUV.clear();
	}
	// The new Low Poly path builds trimmed faces from the exact prepared
	// boundary in UV space. Regular faces must continue to the CNet path below:
	// BuildQuadroMesh builds them first, then copies their finished boundary
	// nodes into adjacent trimmed faces before this contour path is invoked.
	// Holes are opened by non-intersecting bridges into simple patches because
	// ContourQuadrangulator accepts only one boundary loop.
	// Keep the former regular-grid trimming path below as a compatibility
	// fallback for singular or otherwise unprojectable OCCT faces.
	size_t topological_edge_count = 0;
	for (TopExp_Explorer edge(F1, TopAbs_EDGE); edge.More(); edge.Next())
		++topological_edge_count;
	// A four-sided BSpline transition is already a regular tensor-product
	// surface even when tiny endpoint tolerances classify its boundary as
	// trimmed.  Re-projecting that boundary independently can jump between the
	// two equivalent ends of the BSpline parameter range and produces a long
	// diagonal.  Keep the synchronized four sides and build the regular CNet;
	// the trimming/classification stage below still removes any genuine excess.
	const bool four_sided_bspline_compatibility = low_poly_quadro
		&& surface_type_of(F1) == GeomAbs_BSplineSurface
		&& topological_edge_count == 4;
	if (low_poly_quadro && m_TypeMesh != REGULAR_MESH
		&& !four_sided_bspline_compatibility) {
		if (BuildFilledMeshWhithHoles(Deflection,
			psol && psol->MeshQuadroHoleSLX))
			return true;
		// SLX is an explicitly selected algorithm.  Falling through after an SLX
		// failure used to display the legacy island/CNet result under the checked
		// SLX box, which made a failed outer quadrangulation look like a wildly
		// different SLX mesh.  Preserve the failure and its diagnostic instead.
		if (psol && psol->MeshQuadroHoleSLX)
			return false;
		// On a planar face a failed island is not permission to show an
		// unrelated legacy CNet triangulation under the cached island boundaries.
		// Non-planar transition faces still need their compatibility path until
		// the UV island filler supports every singular/periodic parameterisation.
		if (surface_type_of(F1) == GeomAbs_Plane)
			return false;
	}
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
	if (four_sided_bspline_compatibility) {
		// This mesh is already the synchronized tensor-product grid in 3D.
		// Sending it through PutOnSurface/RestoreTo3D again can cross the nearly
		// closed BSpline parameter end, split a few boundary cells and leave the
		// visible three-quad tab. Classify the original grid directly instead.
		IsTrimmed = false;
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
		if (pMesh3D) {
			pMesh3D->RestoreTo3DFromUVSurface(this);
		}
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

bool CSurfaceFace::RemoveOutsideMeshFaces3D()
{
	const bool removed = delete_mesh_faces_outside_occt_face_3d(pMesh3D, this);
	IsTrimmed = removed || IsTrimmed;
	return removed;
}

bool CSurfaceFace::MakeFilledContour(const std::vector<Vec3>& contour,
	Vec3 normal, CMesh3D* quad_mesh, bool prefer_safe_quads,
	std::string* error, CMesh3D* contour_to_fill_mesh,
	std::string* quadrangulator_rejection)
{
	if (error)
		error->clear();
	if (quadrangulator_rejection)
		quadrangulator_rejection->clear();
	if (!quad_mesh) {
		if (error)
			*error = "No destination mesh.";
		return false;
	}
	if (contour.size() < 3) {
		quad_mesh->Clear();
		if (error)
			*error = "The contour has fewer than three nodes.";
		return false;
	}
	// A cylinder's native UV plane is not metric: U is an angle in radians,
	// while V is a length. Feeding that plane directly to the advancing front
	// turns an ordinary unrolled cylindrical rectangle into a very narrow strip
	// (for the Fusion fillet, about 1.57 x 30.7 instead of 18.85 x 30.7).
	// Quadrangulate the exact cylindrical development X = radius * U, Y = V,
	// then map the generated vertices back to UV before restoring the mesh to 3D.
	double metric_u_scale = 1.0;
	bool use_sphere_pole_projection = false;
	double sphere_radius = 1.0;
	double sphere_v_sign = 1.0;
	if (!m_Face.IsNull()) {
		try {
			BRepAdaptor_Surface adaptor(TopoDS::Face(m_Face));
			if (adaptor.GetType() == GeomAbs_Cylinder) {
				const double radius = adaptor.Cylinder().Radius();
				if (std::isfinite(radius) && radius > 1.0e-9)
					metric_u_scale = radius;
			} else if (adaptor.GetType() == GeomAbs_Sphere) {
				bool has_degenerated_edge = false;
				for (TopExp_Explorer edge(m_Face, TopAbs_EDGE);
					edge.More(); edge.Next()) {
					if (BRep_Tool::Degenerated(TopoDS::Edge(edge.Current()))) {
						has_degenerated_edge = true;
						break;
					}
				}
				double minimum_v = contour.front().y;
				double maximum_v = contour.front().y;
				for (Vec3 point : contour) {
					minimum_v = std::min(minimum_v,
						static_cast<double>(point.y));
					maximum_v = std::max(maximum_v,
						static_cast<double>(point.y));
				}
				const double radius = adaptor.Sphere().Radius();
				if (has_degenerated_edge
					&& (minimum_v >= -1.0e-7 || maximum_v <= 1.0e-7)
					&& std::isfinite(radius) && radius > 1.0e-9) {
					use_sphere_pole_projection = true;
					sphere_radius = radius;
					sphere_v_sign = minimum_v + maximum_v >= 0.0
						? 1.0 : -1.0;
				}
			}
		} catch (const Standard_Failure&) {
			metric_u_scale = 1.0;
			use_sphere_pole_projection = false;
		}
	}
	std::vector<Vec3> working_contour = contour;
	if (use_sphere_pole_projection) {
		for (Vec3& point : working_contour) {
			const double radial = sphere_radius
				* std::cos(static_cast<double>(point.y));
			const double u = point.x;
			point.x = static_cast<float>(radial * std::cos(u));
			point.y = static_cast<float>(radial * std::sin(u));
		}
	} else if (metric_u_scale != 1.0) {
		for (Vec3& point : working_contour)
			point.x = static_cast<float>(point.x * metric_u_scale);
	}
	const auto copy_working_mesh_to_uv = [metric_u_scale,
		use_sphere_pole_projection, sphere_radius, sphere_v_sign](
		const CMesh3D& source, CMesh3D* destination) {
		if (!destination)
			return false;
		std::vector<Vec3> vertices = source.GetVertices();
		if (use_sphere_pole_projection) {
			for (Vec3& point : vertices) {
				const double x = point.x;
				const double y = point.y;
				const double normalized_radius = std::clamp(
					std::hypot(x, y) / sphere_radius, 0.0, 1.0);
				point.x = static_cast<float>(std::atan2(y, x));
				point.y = static_cast<float>(sphere_v_sign
					* std::acos(normalized_radius));
			}
		} else if (metric_u_scale != 1.0) {
			for (Vec3& point : vertices)
				point.x = static_cast<float>(point.x / metric_u_scale);
		}
		return destination->SetGeometry(std::move(vertices), source.GetFaces(),
			source.GetUVs(), source.GetNormals());
	};
	CMesh3D triangle_mesh;
	if (!FillContorByTriangles(&triangle_mesh, working_contour, normal)) {
		quad_mesh->Clear();
		if (error)
			*error = "ContourToFill could not triangulate the island.";
		return false;
	}
	if (contour_to_fill_mesh) {
		// Keep this mesh in the actual metric XY plane supplied to the
		// quadrangulator. It is an A/B diagnostic, not a production UV mesh.
		contour_to_fill_mesh->SetGeometry(triangle_mesh.GetVertices(),
			triangle_mesh.GetFaces(), triangle_mesh.GetUVs(),
			triangle_mesh.GetNormals());
	}
	CMesh3D moving_front_mesh;
	ContourQuadrangulator quadrangulator;
	quadrangulator.CreateFromMesh(&triangle_mesh);
	quadrangulator.Quadrangulate(&moving_front_mesh);
	// Shpagin's result is authoritative.  Do not second-guess it with Dom3D
	// coverage, aspect, valence, or all-quad tests: those checks made density
	// 0.70 fall back to triangles while 0.55 and 0.80 remained quads.
	const bool shpagin_created = std::any_of(
		moving_front_mesh.GetFaces().begin(), moving_front_mesh.GetFaces().end(),
		[](const CMesh3D::Face& face) {
			return !face.deleted && face.corners.size() >= 3;
		});
	bool shpagin_inside_contour = shpagin_created;
	bool shpagin_spacing_collapsed = false;
	if (shpagin_created) {
		float minimum_x = working_contour.front().x;
		float maximum_x = working_contour.front().x;
		float minimum_y = working_contour.front().y;
		float maximum_y = working_contour.front().y;
		for (Vec3 point : working_contour) {
			minimum_x = std::min(minimum_x, point.x);
			maximum_x = std::max(maximum_x, point.x);
			minimum_y = std::min(minimum_y, point.y);
			maximum_y = std::max(maximum_y, point.y);
		}
		const double diagonal = std::hypot(
			static_cast<double>(maximum_x - minimum_x),
			static_cast<double>(maximum_y - minimum_y));
		const double tolerance = std::max(1.0e-6, diagonal * 1.0e-5);
		std::vector<double> boundary_edge_lengths;
		boundary_edge_lengths.reserve(working_contour.size());
		for (size_t index = 0; index < working_contour.size(); ++index) {
			const Vec3 first = working_contour[index];
			const Vec3 second = working_contour[
				(index + 1) % working_contour.size()];
			const double edge_length = std::hypot(
				static_cast<double>(second.x - first.x),
				static_cast<double>(second.y - first.y));
			if (edge_length > tolerance) {
				boundary_edge_lengths.push_back(edge_length);
			}
		}
		std::sort(boundary_edge_lengths.begin(), boundary_edge_lengths.end());
		const double representative_boundary_edge = boundary_edge_lengths.empty()
			? std::numeric_limits<double>::max()
			: boundary_edge_lengths[boundary_edge_lengths.size() / 2];
		const auto point_on_source_boundary = [&](Vec3 point) {
			for (size_t index = 0, previous = working_contour.size() - 1;
				index < working_contour.size(); previous = index++) {
				const Vec3 first = working_contour[previous];
				const Vec3 second = working_contour[index];
				const double edge_x = static_cast<double>(second.x - first.x);
				const double edge_y = static_cast<double>(second.y - first.y);
				const double edge_length = std::hypot(edge_x, edge_y);
				if (edge_length > 1.0e-12) {
					const double cross_value = edge_x * (point.y - first.y)
						- edge_y * (point.x - first.x);
					const double projection = (point.x - first.x) * edge_x
						+ (point.y - first.y) * edge_y;
					if (std::fabs(cross_value) <= tolerance * edge_length
						&& projection >= -tolerance * edge_length
						&& projection <= edge_length * edge_length
							+ tolerance * edge_length) {
						return true;
					}
				}
			}
			return false;
		};
		const auto point_in_source_contour = [&](Vec3 point) {
			if (point_on_source_boundary(point))
				return true;
			bool inside = false;
			for (size_t index = 0, previous = working_contour.size() - 1;
				index < working_contour.size(); previous = index++) {
				const Vec3 first = working_contour[previous];
				const Vec3 second = working_contour[index];
				if (((first.y > point.y) != (second.y > point.y))
					&& point.x < (second.x - first.x) * (point.y - first.y)
						/ (second.y - first.y) + first.x) {
					inside = !inside;
				}
			}
			return inside;
		};
		const auto& moving_vertices = moving_front_mesh.GetVertices();
		double minimum_result_edge = std::numeric_limits<double>::max();
		for (const CMesh3D::Face& face : moving_front_mesh.GetFaces()) {
			if (face.deleted || face.corners.size() < 3)
				continue;
			Vec3 face_center{};
			for (const MeshCorner& corner : face.corners) {
				if (corner.v >= moving_vertices.size()) {
					shpagin_inside_contour = false;
					break;
				}
				const Vec3 point = moving_vertices[corner.v];
				if (point.x < minimum_x - tolerance
					|| point.x > maximum_x + tolerance
					|| point.y < minimum_y - tolerance
					|| point.y > maximum_y + tolerance) {
					shpagin_inside_contour = false;
					break;
				}
				face_center = face_center + point;
			}
			if (!shpagin_inside_contour)
				break;
			face_center = face_center
				* (1.0f / static_cast<float>(face.corners.size()));
			if (!point_in_source_contour(face_center)) {
				shpagin_inside_contour = false;
				break;
			}
			for (size_t index = 0; index < face.corners.size(); ++index) {
				const Vec3 first = moving_vertices[face.corners[index].v];
				const Vec3 second = moving_vertices[face.corners[
					(index + 1) % face.corners.size()].v];
				if (!(point_on_source_boundary(first)
					&& point_on_source_boundary(second))) {
					minimum_result_edge = std::min(minimum_result_edge,
						std::hypot(static_cast<double>(second.x - first.x),
							static_cast<double>(second.y - first.y)));
				}
				for (float alpha : {0.25f, 0.5f, 0.75f}) {
					if (!point_in_source_contour(
							first + (second - first) * alpha)) {
						shpagin_inside_contour = false;
						break;
					}
				}
				if (!shpagin_inside_contour)
					break;
			}
			if (!shpagin_inside_contour)
				break;
		}
		if (shpagin_inside_contour && prefer_safe_quads
			&& std::isfinite(representative_boundary_edge)
			&& minimum_result_edge < representative_boundary_edge * 0.20) {
			shpagin_inside_contour = false;
			shpagin_spacing_collapsed = true;
		}
	}
	if (shpagin_created && shpagin_inside_contour) {
		return copy_working_mesh_to_uv(moving_front_mesh, quad_mesh);
	}
	if (quadrangulator_rejection) {
		*quadrangulator_rejection = shpagin_spacing_collapsed
			? "ContourQuadrangulator collapsed the internal patch spacing."
			: shpagin_created
			? "ContourQuadrangulator escaped the source contour."
			: "ContourQuadrangulator returned an empty mesh.";
	}
	std::vector<CPoint3d> rejected_boundary;
	rejected_boundary.reserve(working_contour.size());
	for (Vec3 point : working_contour)
		rejected_boundary.emplace_back(point.x, point.y, 0.0);
	m_LastQuadrangulationBoundariesXY.push_back(
		std::move(rejected_boundary));
	// Only an actually empty Shpagin result may fall back to ContourToFill.
	// Pair its local triangulation back into quads where the source diagonal is
	// unambiguous. This keeps simple cylindrical strips quad-dominant even when
	// a runaway advancing-front candidate is rejected by its resource guard.
	merge_trim_triangle_pairs_to_quads(triangle_mesh);
	return copy_working_mesh_to_uv(triangle_mesh, quad_mesh);
}

bool CSurfaceFace::BuildFilledMeshWhithHoles(float Deflection,
	bool use_mesh_quadro_hole_slx)
{
	// The diagnostic tool must never expose islands from an earlier build when
	// the current attempt fails before producing a new patch set.
	m_LastIslandBoundariesUV.clear();
	m_LastQuadrangulationBoundariesXY.clear();
	m_LastIslandFillError.clear();
	m_LastQuadrangulationDiagnostic.clear();

	// Boundary diagnostics. Put the required CSurfaceFace::m_ID here and
	// rebuild. Use -2 to dump every surface, -1 to disable output. Files are written to
	// C:\temp\Dom3D_Surface_<ID>_01_Prepared3D.txt ... _04_PatchesUV.txt.
	constexpr int kBoundaryDumpSurfaceId = -1;
	const bool dump_boundary = kBoundaryDumpSurfaceId == -2
		|| m_ID == kBoundaryDumpSurfaceId;
	const std::string dump_prefix = "C:\\temp\\Dom3D_Surface_"
		+ std::to_string(m_ID);
	const std::array<std::string, 4> dump_paths{
		dump_prefix + "_01_Prepared3D.txt",
		dump_prefix + "_02_Joined3D.txt",
		dump_prefix + "_03_ContoursUV.txt",
		dump_prefix + "_04_PatchesUV.txt"};
	if (dump_boundary) {
		for (const std::string& path : dump_paths) {
			std::error_code remove_error;
			std::filesystem::remove(path, remove_error);
		}
	}
	const auto dump_polyline = [&](const std::string& path,
		const std::vector<CPoint3d>& points, bool closed) {
		if (!dump_boundary || points.empty())
			return;
		CPolyline line;
		for (const CPoint3d& point : points)
			line.AddPoint(point);
		line.SetClosed(closed);
		line.printToFile(path);
	};

	if (!pMesh3D || m_Face.IsNull() || Polylines.empty()
		|| !std::isfinite(Deflection) || Deflection <= 0.0f) {
		m_LastIslandFillError = "Invalid surface, boundary, or mesh density.";
		return false;
	}

	// Prepared surface edges are individual polylines. Join copies here so the
	// synchronized boundary samples remain owned by the surface and can still
	// be used to snap neighbouring faces after this mesh has been built.
	std::vector<std::unique_ptr<CPolyline>> loop_storage;
	std::vector<CPolyline*> loose_lines;
	loop_storage.reserve(Polylines.size());
	loose_lines.reserve(Polylines.size());
	for (const CPolyline* source : Polylines) {
		if (source) {
			dump_polyline(dump_paths[0], source->GetPoints(),
				source->IsClosed());
		}
		std::unique_ptr<CPolyline> copy = copy_polyline_points(source);
		if (!copy || copy->GetPointCount() < 2)
			continue;
		loose_lines.push_back(copy.get());
		loop_storage.push_back(std::move(copy));
	}
	if (loose_lines.empty()) {
		m_LastIslandFillError = "No usable prepared boundary polylines.";
		return false;
	}

	const double world_join_tolerance = std::max(
		1.0e-7, static_cast<double>(std::max(lenEdgeMax, 1.0f)) * 1.0e-5);
	std::vector<CPolyline*> joined_loops;
	if (!CPolyline::JoinMultuLines(&loose_lines, &joined_loops,
		world_join_tolerance) || joined_loops.empty()) {
		m_LastIslandFillError = "Prepared boundary edges could not be joined into loops.";
		return false;
	}
	for (const CPolyline* loop : joined_loops) {
		if (loop)
			dump_polyline(dump_paths[1], loop->GetPoints(), true);
	}

	std::vector<std::vector<SurfacePatchPoint>> uv_contours;
	uv_contours.reserve(joined_loops.size());
	for (CPolyline* loop : joined_loops) {
		if (!loop || loop->GetPointCount() < 3)
			continue;
		if (loop->P(0)->DistTo(loop->PLast()) <= world_join_tolerance)
			loop->SetClosed(true);
		if (!loop->IsClosed() || !loop->PutOnSurface(this))
			continue;

		std::vector<SurfacePatchPoint> contour;
		contour.reserve(loop->GetPointCount());
		for (const CPoint3d& point : loop->GetPoints())
			contour.push_back({point.x, point.y});
		if (contour.size() > 1) {
			const double du = contour.front().u - contour.back().u;
			const double dv = contour.front().v - contour.back().v;
			if (du * du + dv * dv <= 1.0e-18)
				contour.pop_back();
		}
		if (contour.size() >= 3)
			uv_contours.push_back(std::move(contour));
	}
	if (uv_contours.empty()) {
		m_LastIslandFillError = "No closed UV contours were produced.";
		return false;
	}

	// A window cut across a cylinder's parameter seam is represented by one
	// finite-area loop around U=0 plus the top and bottom circles, which collapse
	// to zero-area horizontal lines in UV. The generic patch builder then mistakes
	// the cutout for the outer contour and fills the side that OCCT says is OUT.
	// Move the working seam opposite that loop, synthesize the complete periodic
	// rectangle as the outer contour, and retain the classified OUT loop as a hole.
	// This is deliberately limited to one enclosed seam-crossing cut; cylinders
	// with ordinary outer contours continue through the established path below.
	try {
		const TopoDS_Face face = TopoDS::Face(m_Face);
		BRepAdaptor_Surface adaptor(face);
		if (adaptor.GetType() == GeomAbs_Cylinder
			&& adaptor.IsUPeriodic()) {
			double face_u_min = 0.0;
			double face_u_max = 0.0;
			double face_v_min = 0.0;
			double face_v_max = 0.0;
			BRepTools::UVBounds(face, face_u_min, face_u_max,
				face_v_min, face_v_max);
			const double period = adaptor.UPeriod();
			const double coordinate_scale = std::max({
				std::fabs(face_u_max - face_u_min),
				std::fabs(face_v_max - face_v_min), 1.0});
			const double epsilon = coordinate_scale * 1.0e-8;
			const double retrace_epsilon = coordinate_scale * 1.0e-5;
			if (period > epsilon
				&& face_u_max - face_u_min >= period - epsilon * 10.0) {
				const auto same_uv = [retrace_epsilon](SurfacePatchPoint first,
					SurfacePatchPoint second) {
					return std::hypot(first.u - second.u,
						first.v - second.v) <= retrace_epsilon;
				};
				const auto remove_retraced_spikes = [&](const auto& source) {
					std::vector<SurfacePatchPoint> clean;
					clean.reserve(source.size());
					for (SurfacePatchPoint point : source) {
						if (!clean.empty() && same_uv(clean.back(), point))
							continue;
						if (clean.size() >= 2
							&& same_uv(clean[clean.size() - 2], point)) {
							clean.pop_back();
							continue;
						}
						clean.push_back(point);
					}
					return clean;
				};
				const auto signed_area = [](const auto& contour) {
					double area = 0.0;
					for (size_t index = 0; index < contour.size(); ++index) {
						const auto& first = contour[index];
						const auto& second = contour[(index + 1) % contour.size()];
						area += first.u * second.v - second.u * first.v;
					}
					return area * 0.5;
				};

				std::vector<std::vector<SurfacePatchPoint>> finite_loops;
				for (const auto& source : uv_contours) {
					auto clean = remove_retraced_spikes(source);
					if (clean.size() >= 3
						&& std::fabs(signed_area(clean)) > epsilon * epsilon) {
						finite_loops.push_back(std::move(clean));
					}
				}
				if (finite_loops.size() == 1) {
					auto hole = std::move(finite_loops.front());
					SurfacePatchPoint center{};
					for (SurfacePatchPoint point : hole) {
						center.u += point.u;
						center.v += point.v;
					}
					center.u /= static_cast<double>(hole.size());
					center.v /= static_cast<double>(hole.size());
					for (SurfacePatchPoint& point : hole)
						point.u += std::round((center.u - point.u) / period) * period;
					double hole_v_min = std::numeric_limits<double>::max();
					double hole_v_max = std::numeric_limits<double>::lowest();
					for (SurfacePatchPoint point : hole) {
						hole_v_min = std::min(hole_v_min, point.v);
						hole_v_max = std::max(hole_v_max, point.v);
					}
					double classifier_u = face_u_min
						+ std::fmod(center.u - face_u_min, period);
					if (classifier_u < face_u_min)
						classifier_u += period;
					BRepClass_FaceClassifier classifier(face,
						gp_Pnt2d(classifier_u, center.v), EPS2D,
						Standard_False);
					const bool enclosed_in_v = hole_v_min > face_v_min + epsilon
						&& hole_v_max < face_v_max - epsilon;
					if (enclosed_in_v && classifier.State() == TopAbs_OUT) {
						const double work_u_min = center.u - period * 0.5;
						const double work_u_max = center.u + period * 0.5;
						std::vector<double> metric_edge_lengths;
						metric_edge_lengths.reserve(hole.size());
						const double radius = adaptor.Cylinder().Radius();
						for (size_t index = 0; index < hole.size(); ++index) {
							const auto& first = hole[index];
							const auto& second = hole[(index + 1) % hole.size()];
							const double length = std::hypot(
								(second.u - first.u) * radius,
								second.v - first.v);
							if (length > epsilon && std::isfinite(length))
								metric_edge_lengths.push_back(length);
						}
						std::sort(metric_edge_lengths.begin(),
							metric_edge_lengths.end());
						const double boundary_step = metric_edge_lengths.empty()
							? std::max(radius * period, face_v_max - face_v_min)
							: metric_edge_lengths[metric_edge_lengths.size() / 2];
						const int u_segments = std::max(2,
							static_cast<int>(std::ceil(radius * period / boundary_step)));
						const int v_segments = std::max(2,
							static_cast<int>(std::ceil(
								(face_v_max - face_v_min) / boundary_step)));
						std::vector<SurfacePatchPoint> outer;
						outer.reserve(static_cast<size_t>(
							u_segments * 2 + v_segments * 2));
						for (int index = 0; index < u_segments; ++index) {
							const double alpha = static_cast<double>(index) / u_segments;
							outer.push_back({work_u_min
								+ (work_u_max - work_u_min) * alpha, face_v_min});
						}
						for (int index = 0; index < v_segments; ++index) {
							const double alpha = static_cast<double>(index) / v_segments;
							outer.push_back({work_u_max, face_v_min
								+ (face_v_max - face_v_min) * alpha});
						}
						for (int index = u_segments; index > 0; --index) {
							const double alpha = static_cast<double>(index) / u_segments;
							outer.push_back({work_u_min
								+ (work_u_max - work_u_min) * alpha, face_v_max});
						}
						for (int index = v_segments; index > 0; --index) {
							const double alpha = static_cast<double>(index) / v_segments;
							outer.push_back({work_u_min, face_v_min
								+ (face_v_max - face_v_min) * alpha});
						}
						uv_contours.clear();
						uv_contours.push_back(std::move(outer));
						uv_contours.push_back(std::move(hole));
					}
				}
			}
		}
	} catch (const Standard_Failure&) {
	}
	size_t topological_wire_count = 0;
	for (TopExp_Explorer wire(m_Face, TopAbs_WIRE); wire.More(); wire.Next())
		++topological_wire_count;
	const bool slx_face_has_holes = use_mesh_quadro_hole_slx
		&& topological_wire_count > 1;
	if (slx_face_has_holes && uv_contours.size() <= 1) {
		pMesh3D->Clear();
		m_LastIslandFillError = "Mesh Quadro Hole SLX: the OCCT face has "
			+ std::to_string(topological_wire_count)
			+ " boundary wires, but boundary preparation produced only "
			+ std::to_string(uv_contours.size()) + " closed UV contour(s).";
		return false;
	}
	if (dump_boundary) {
		for (const std::vector<SurfacePatchPoint>& contour : uv_contours) {
			std::vector<CPoint3d> points;
			points.reserve(contour.size());
			for (SurfacePatchPoint point : contour)
				points.emplace_back(point.u, point.v, 0.0);
			dump_polyline(dump_paths[2], points, true);
		}
	}

	// Projecting each closed loop independently can place an inner loop in a
	// neighbouring period of a cylinder, sphere or torus. Translate complete
	// loops by whole periods until their centroid lies in the outer loop; the
	// surface points represented by the parameters remain unchanged.
	SurfaceUVMapping periodic_mapping(this);
	if (uv_contours.size() > 1 && periodic_mapping.IsValid()
		&& (periodic_mapping.IsUPeriodic() || periodic_mapping.IsVPeriodic())) {
		const auto signed_uv_area = [](const std::vector<SurfacePatchPoint>& contour) {
			double area = 0.0;
			for (size_t i = 0; i < contour.size(); ++i) {
				const SurfacePatchPoint& a = contour[i];
				const SurfacePatchPoint& b = contour[(i + 1) % contour.size()];
				area += a.u * b.v - b.u * a.v;
			}
			return area * 0.5;
		};
		const size_t outer_index = static_cast<size_t>(std::distance(
			uv_contours.begin(),
			std::max_element(uv_contours.begin(), uv_contours.end(),
				[&](const auto& a, const auto& b) {
					return std::fabs(signed_uv_area(a)) < std::fabs(signed_uv_area(b));
				})));
		const auto contains = [](const std::vector<SurfacePatchPoint>& polygon,
		                           SurfacePatchPoint point) {
			bool inside = false;
			for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
				const SurfacePatchPoint& a = polygon[j];
				const SurfacePatchPoint& b = polygon[i];
				if (((a.v > point.v) != (b.v > point.v))
					&& point.u < (b.u - a.u) * (point.v - a.v) / (b.v - a.v) + a.u) {
					inside = !inside;
				}
			}
			return inside;
		};
		for (size_t contour_index = 0; contour_index < uv_contours.size(); ++contour_index) {
			if (contour_index == outer_index)
				continue;
			SurfacePatchPoint center{};
			for (SurfacePatchPoint point : uv_contours[contour_index]) {
				center.u += point.u;
				center.v += point.v;
			}
			center.u /= static_cast<double>(uv_contours[contour_index].size());
			center.v /= static_cast<double>(uv_contours[contour_index].size());
			bool aligned = contains(uv_contours[outer_index], center);
			double shift_u = 0.0;
			double shift_v = 0.0;
			for (int u_shift = -2; !aligned && u_shift <= 2; ++u_shift) {
				for (int v_shift = -2; !aligned && v_shift <= 2; ++v_shift) {
					shift_u = periodic_mapping.IsUPeriodic()
						? u_shift * periodic_mapping.UPeriod() : 0.0;
					shift_v = periodic_mapping.IsVPeriodic()
						? v_shift * periodic_mapping.VPeriod() : 0.0;
					aligned = contains(uv_contours[outer_index],
						{center.u + shift_u, center.v + shift_v});
				}
			}
			if (aligned) {
				for (SurfacePatchPoint& point : uv_contours[contour_index]) {
					point.u += shift_u;
					point.v += shift_v;
				}
			}
		}
	}

	// Mesh_Quadro_Hole_SLX: quadrangulate the complete outer domain as if
	// holes did not exist, then cut that single coherent mesh by every inner
	// contour.  TrimByPline owns the carefully tuned cell-splitting variants;
	// OCCT classification below owns the side-to-remove decision.
	if (slx_face_has_holes) {
		const auto signed_area = [](const std::vector<SurfacePatchPoint>& contour) {
			double area = 0.0;
			for (size_t i = 0; i < contour.size(); ++i) {
				const SurfacePatchPoint& first = contour[i];
				const SurfacePatchPoint& second = contour[(i + 1) % contour.size()];
				area += first.u * second.v - second.u * first.v;
			}
			return area * 0.5;
		};
		const size_t outer_index = static_cast<size_t>(std::distance(
			uv_contours.begin(),
			std::max_element(uv_contours.begin(), uv_contours.end(),
				[&](const auto& first, const auto& second) {
					return std::fabs(signed_area(first))
						< std::fabs(signed_area(second));
				})));
		const double outer_area = std::fabs(signed_area(
			uv_contours[outer_index]));
		Face2D outer_face;
		outer_face.verts.reserve(uv_contours[outer_index].size());
		for (SurfacePatchPoint point : uv_contours[outer_index])
			outer_face.verts.emplace_back(point.u, point.v);

		// TrimByPline must receive holes only.  Prepared edge joining can contain
		// a second copy of the outer wire; skipping only outer_index then sends
		// that duplicate to TrimByPline and tears the complete surface into a fan.
		std::vector<size_t> hole_indices;
		for (size_t contour_index = 0;
			contour_index < uv_contours.size(); ++contour_index) {
			if (contour_index == outer_index)
				continue;
			const double contour_area = std::fabs(signed_area(
				uv_contours[contour_index]));
			if (contour_area <= 0.0
				|| contour_area >= outer_area * (1.0 - 1.0e-6)) {
				continue;
			}
			SurfacePatchPoint centre{};
			for (SurfacePatchPoint point : uv_contours[contour_index]) {
				centre.u += point.u;
				centre.v += point.v;
			}
			centre.u /= static_cast<double>(uv_contours[contour_index].size());
			centre.v /= static_cast<double>(uv_contours[contour_index].size());
			if (ClassifyPointInFace2(outer_face,
					cVec2(centre.u, centre.v), EPS2D) == PFP_OUTSIDE) {
				continue;
			}
			hole_indices.push_back(contour_index);
		}
		std::sort(hole_indices.begin(), hole_indices.end(),
			[&](size_t first, size_t second) {
				return std::fabs(signed_area(uv_contours[first]))
					< std::fabs(signed_area(uv_contours[second]));
			});
		const size_t topological_hole_count = topological_wire_count - 1;
		if (hole_indices.size() > topological_hole_count)
			hole_indices.resize(topological_hole_count);

		std::vector<Vec3> outer_contour;
		outer_contour.reserve(uv_contours[outer_index].size());
		for (SurfacePatchPoint point : uv_contours[outer_index]) {
			outer_contour.push_back({static_cast<float>(point.u),
				static_cast<float>(point.v), 0.0f});
		}

		CMesh3D slx_mesh;
		CMesh3D contour_to_fill_mesh;
		std::string fill_error;
		// Use exactly the same Shpagin mesh producer as variant 1.  There is no
		// SLX-specific quality gate: whatever MakeFilledContour returns is passed
		// directly to TrimByPline so the current cutting defects remain visible.
		const bool quadrangulator_created = MakeFilledContour(
			outer_contour, {0.0f, 0.0f, 1.0f}, &slx_mesh,
			Deflection >= 1.5f, &fill_error, &contour_to_fill_mesh, nullptr)
			&& std::any_of(slx_mesh.GetFaces().begin(), slx_mesh.GetFaces().end(),
				[](const CMesh3D::Face& face) {
					return !face.deleted && face.corners.size() >= 3;
				});

		// Keep the exact mesh that exists immediately before TrimByPline.  This
		// makes the two stages independently inspectable: this is the raw
		// ContourQuadrangulator result, before the first TrimByPline call.
		{
			const std::filesystem::path diagnostic_dir =
				"C:\\temp\\Dom3D_Quadrangulation";
			std::error_code directory_error;
			std::filesystem::create_directories(
				diagnostic_dir, directory_error);
			if (!directory_error) {
				const long long step_milli = std::llround(
					static_cast<double>(Deflection) * 1000.0);
				const std::string stem = "Surface_" + std::to_string(m_ID)
					+ "_SLX_StepMilli_" + std::to_string(step_milli);
				const std::filesystem::path obj_path = diagnostic_dir /
					(stem + (quadrangulator_created ? "_BeforeHoles.obj"
						: "_Empty_ContourToFill.obj"));
				CMesh3D& diagnostic_mesh = quadrangulator_created
					? slx_mesh : contour_to_fill_mesh;
				if (diagnostic_mesh.ExportToObj(obj_path.string()))
					m_LastQuadrangulationDiagnostic = obj_path.string();
			}
		}

		if (quadrangulator_created) {
			// A coarse SLX background cannot be cut reliably by a much finer hole
			// polyline: several consecutive contour nodes then fall into one cell and
			// TrimByPline has to create a long fan.  Move that scale transition into a
			// compact conforming collar.  The background is cut by the collar's outer
			// ring; the exact OCCT hole remains its inner ring.
			struct SlxHoleCollar {
				size_t contour_index = 0;
				std::vector<std::vector<SurfacePatchPoint>> rings;
				bool snap_cut_to_background_cells = false;
				bool cut_matches_requested_outer = true;
				bool used = false;
			};
			std::vector<SlxHoleCollar> slx_collars;
			const auto polygon_contains = [](const std::vector<SurfacePatchPoint>& polygon,
				SurfacePatchPoint point) {
				Face2D face;
				face.verts.reserve(polygon.size());
				for (SurfacePatchPoint vertex : polygon)
					face.verts.emplace_back(vertex.u, vertex.v);
				return ClassifyPointInFace2(
					face, cVec2(point.u, point.v), EPS2D) != PFP_OUTSIDE;
			};
			const auto median_edge_length = [](const std::vector<SurfacePatchPoint>& ring) {
				std::vector<double> lengths;
				lengths.reserve(ring.size());
				for (size_t i = 0; i < ring.size(); ++i) {
					const SurfacePatchPoint& a = ring[i];
					const SurfacePatchPoint& b = ring[(i + 1) % ring.size()];
					const double length = std::hypot(b.u - a.u, b.v - a.v);
					if (length > 1.0e-12 && std::isfinite(length))
						lengths.push_back(length);
				}
				if (lengths.empty())
					return 0.0;
				std::sort(lengths.begin(), lengths.end());
				return lengths[lengths.size() / 2];
			};
			const auto ring_signed_area = [](const std::vector<SurfacePatchPoint>& ring) {
				double area = 0.0;
				for (size_t i = 0; i < ring.size(); ++i) {
					const SurfacePatchPoint& a = ring[i];
					const SurfacePatchPoint& b = ring[(i + 1) % ring.size()];
					area += a.u * b.v - b.u * a.v;
				}
				return area * 0.5;
			};
			const auto point_segment_distance = [](SurfacePatchPoint point,
				SurfacePatchPoint a, SurfacePatchPoint b) {
				const double du = b.u - a.u;
				const double dv = b.v - a.v;
				const double length_sq = du * du + dv * dv;
				if (length_sq <= 1.0e-24)
					return std::hypot(point.u - a.u, point.v - a.v);
				const double t = std::clamp(((point.u - a.u) * du
					+ (point.v - a.v) * dv) / length_sq, 0.0, 1.0);
				return std::hypot(point.u - (a.u + du * t),
					point.v - (a.v + dv * t));
			};
			const auto offset_ring = [](const std::vector<SurfacePatchPoint>& ring,
				double distance, std::vector<SurfacePatchPoint>& result) {
				result.clear();
				if (ring.size() < 3 || distance <= 0.0)
					return false;
				result.reserve(ring.size());
				const auto cross_2d = [](double ax, double ay,
					double bx, double by) { return ax * by - ay * bx; };
				// Keep the vertex displacement equal to the requested collar step.
				// This is a bounded bevel at sharp corners and avoids moving an
				// eight-sided round hole farther than the proven radial variant did.
				constexpr double miter_limit = 1.0;
				for (size_t i = 0; i < ring.size(); ++i) {
					const SurfacePatchPoint& previous = ring[
						(i + ring.size() - 1) % ring.size()];
					const SurfacePatchPoint& current = ring[i];
					const SurfacePatchPoint& next = ring[(i + 1) % ring.size()];
					double previous_du = current.u - previous.u;
					double previous_dv = current.v - previous.v;
					double next_du = next.u - current.u;
					double next_dv = next.v - current.v;
					const double previous_length = std::hypot(previous_du, previous_dv);
					const double next_length = std::hypot(next_du, next_dv);
					if (previous_length <= 1.0e-12 || next_length <= 1.0e-12)
						return false;
					previous_du /= previous_length;
					previous_dv /= previous_length;
					next_du /= next_length;
					next_dv /= next_length;

					// The hole ring is CCW, therefore its exterior is to the right
					// of every directed edge. Intersect the two shifted edge lines;
					// unlike a centroid ray this remains local for angular contours.
					const double previous_normal_u = previous_dv;
					const double previous_normal_v = -previous_du;
					const double next_normal_u = next_dv;
					const double next_normal_v = -next_du;
					const double first_u = current.u + previous_normal_u * distance;
					const double first_v = current.v + previous_normal_v * distance;
					const double second_u = current.u + next_normal_u * distance;
					const double second_v = current.v + next_normal_v * distance;
					const double denominator = cross_2d(
						previous_du, previous_dv, next_du, next_dv);
					double candidate_u = 0.5 * (first_u + second_u);
					double candidate_v = 0.5 * (first_v + second_v);
					if (std::fabs(denominator) > 1.0e-10) {
						const double t = cross_2d(second_u - first_u,
							second_v - first_v, next_du, next_dv) / denominator;
						candidate_u = first_u + previous_du * t;
						candidate_v = first_v + previous_dv * t;
					}
					double miter_u = candidate_u - current.u;
					double miter_v = candidate_v - current.v;
					const double miter_length = std::hypot(miter_u, miter_v);
					if (!std::isfinite(miter_length) || miter_length <= 1.0e-12)
						return false;
					const double maximum_miter = distance * miter_limit;
					if (miter_length > maximum_miter) {
						miter_u *= maximum_miter / miter_length;
						miter_v *= maximum_miter / miter_length;
						candidate_u = current.u + miter_u;
						candidate_v = current.v + miter_v;
					}
					result.push_back({candidate_u, candidate_v, true});
				}
				return true;
			};
			const auto ring_is_simple = [](const std::vector<SurfacePatchPoint>& ring) {
				const auto orientation = [](SurfacePatchPoint a,
					SurfacePatchPoint b, SurfacePatchPoint c) {
					return (b.u - a.u) * (c.v - a.v)
						- (b.v - a.v) * (c.u - a.u);
				};
				for (size_t i = 0; i < ring.size(); ++i) {
					const size_t i_next = (i + 1) % ring.size();
					for (size_t j = i + 1; j < ring.size(); ++j) {
						const size_t j_next = (j + 1) % ring.size();
						if (i_next == j || j_next == i)
							continue;
						const double first_a = orientation(ring[i], ring[i_next], ring[j]);
						const double first_b = orientation(ring[i], ring[i_next], ring[j_next]);
						const double second_a = orientation(ring[j], ring[j_next], ring[i]);
						const double second_b = orientation(ring[j], ring[j_next], ring[i_next]);
						if (first_a * first_b < -1.0e-18
							&& second_a * second_b < -1.0e-18) {
							return false;
						}
					}
				}
				return true;
			};

			double background_step = 0.0;
			{
				std::vector<double> mesh_edges;
				for (const CMesh3D::Face& face : slx_mesh.GetFaces()) {
					if (face.deleted || face.corners.size() < 3)
						continue;
					for (size_t i = 0; i < face.corners.size(); ++i) {
						const size_t first = face.corners[i].v;
						const size_t second = face.corners[(i + 1) % face.corners.size()].v;
						if (first >= slx_mesh.GetVertices().size()
							|| second >= slx_mesh.GetVertices().size()) {
							continue;
						}
						const Vec3 delta = slx_mesh.GetVertices()[second]
							- slx_mesh.GetVertices()[first];
						const double length = std::sqrt(static_cast<double>(dot(delta, delta)));
						if (length > 1.0e-12 && std::isfinite(length))
							mesh_edges.push_back(length);
					}
				}
				if (!mesh_edges.empty()) {
					std::sort(mesh_edges.begin(), mesh_edges.end());
					background_step = mesh_edges[mesh_edges.size() / 2];
				}
			}

			// SLX keeps the exact hole as the inner edge of a transition collar and
			// asks TrimByPline to cut only its coarser outer edge.
			for (size_t contour_index : hole_indices) {
				std::vector<SurfacePatchPoint> hole = uv_contours[contour_index];
				if (ring_signed_area(hole) < 0.0)
					std::reverse(hole.begin(), hole.end());
				const double hole_step = median_edge_length(hole);
				if (hole.size() < 4 || hole_step <= 0.0
					|| background_step <= hole_step * 1.35) {
					continue;
				}
				SurfacePatchPoint center{};
				for (SurfacePatchPoint point : hole) {
					center.u += point.u;
					center.v += point.v;
				}
				center.u /= static_cast<double>(hole.size());
				center.v /= static_cast<double>(hole.size());
				double mean_radius = 0.0;
				double clearance = std::numeric_limits<double>::max();
				for (SurfacePatchPoint point : hole) {
					mean_radius += std::hypot(point.u - center.u, point.v - center.v);
					for (size_t obstacle_index = 0;
						obstacle_index < uv_contours.size(); ++obstacle_index) {
						if (obstacle_index == contour_index)
							continue;
						const auto& obstacle = uv_contours[obstacle_index];
						for (size_t edge = 0; edge < obstacle.size(); ++edge) {
							clearance = std::min(clearance,
								point_segment_distance(point, obstacle[edge],
									obstacle[(edge + 1) % obstacle.size()]));
						}
					}
				}
				mean_radius /= static_cast<double>(hole.size());
				if (mean_radius <= 1.0e-12)
					continue;
				const bool small_hole = mean_radius <= background_step;
				// The collar is a separate mesh. Its outer contour cuts the SLX
				// background, then the collar is appended and welded to that cut.
				// A hole whose radius fits in one background edge needs a complete
				// radial mesh step.  Classifying it by 0.40 * background_step caused
				// the visible 0.20 -> 0.25 discontinuity: the radius cap suddenly
				// reduced the collar to roughly one third of the surrounding edges.
				const double radius_width_limit = small_hole
					? std::numeric_limits<double>::max() : mean_radius * 1.25;
				const double collar_width = std::min({
					background_step * (small_hole ? 1.0 : 0.75),
					radius_width_limit,
					clearance * (small_hole ? 0.45 : 0.30)});
				if (!std::isfinite(collar_width)
					|| collar_width < hole_step * 0.45) {
					continue;
				}
				const int ring_count = collar_width >= hole_step * 1.1 ? 2 : 1;
				SlxHoleCollar collar;
				collar.contour_index = contour_index;
				collar.snap_cut_to_background_cells = small_hole;
				collar.cut_matches_requested_outer = !small_hole;
				collar.rings.push_back(hole);
				bool valid = true;
				for (int ring = 1; ring <= ring_count && valid; ++ring) {
					std::vector<SurfacePatchPoint> expanded;
					const double offset = collar_width
						* static_cast<double>(ring) / ring_count;
					valid = offset_ring(hole, offset, expanded)
						&& ring_is_simple(expanded)
						&& ring_signed_area(expanded) > 0.0;
					for (SurfacePatchPoint candidate : expanded) {
						if (!polygon_contains(uv_contours[outer_index], candidate)) {
							valid = false;
							break;
						}
						for (size_t other : hole_indices) {
							if (other != contour_index
								&& polygon_contains(uv_contours[other], candidate)) {
								valid = false;
								break;
							}
						}
						if (!valid)
							break;
					}
					if (valid)
						collar.rings.push_back(std::move(expanded));
				}
				if (valid && collar.rings.size() >= 2) {
					// Keep all exact nodes on the inner circular boundary, but let
					// TrimByPline see a coarser outer collar. The zipper below is
					// explicitly able to connect unequal node counts; sending all 20+
					// circle nodes into one coarse background cell recreates the fan
					// that the collar is meant to prevent.
					double minimum_radius = std::numeric_limits<double>::max();
					double maximum_radius = 0.0;
					for (SurfacePatchPoint point : hole) {
						const double radius = std::hypot(
							point.u - center.u, point.v - center.v);
						minimum_radius = std::min(minimum_radius, radius);
						maximum_radius = std::max(maximum_radius, radius);
					}
					if (hole.size() >= 16 && maximum_radius > 1.0e-12
						&& minimum_radius / maximum_radius >= 0.95) {
						const auto& dense_outer = collar.rings.back();
						double outer_perimeter = 0.0;
						for (size_t point = 0; point < dense_outer.size(); ++point) {
							const SurfacePatchPoint& first = dense_outer[point];
							const SurfacePatchPoint& second = dense_outer[
								(point + 1) % dense_outer.size()];
							outer_perimeter += std::hypot(
								second.u - first.u, second.v - first.v);
						}
						// Match the outer collar edge length to the local SLX edge
						// instead of inheriting the dense sampling of the exact hole.
						int coarse_count = static_cast<int>(std::lround(
							outer_perimeter / background_step));
						coarse_count = std::max(coarse_count, 8);
						coarse_count = std::min(
							coarse_count, static_cast<int>(dense_outer.size()));
						if (coarse_count < static_cast<int>(dense_outer.size())) {
							std::vector<SurfacePatchPoint> coarse_outer;
							coarse_outer.reserve(static_cast<size_t>(coarse_count));
							double traversed = 0.0;
							size_t edge = 0;
							for (int point = 0; point < coarse_count; ++point) {
								const double target = outer_perimeter * point / coarse_count;
								while (edge + 1 < dense_outer.size()) {
									const SurfacePatchPoint& first = dense_outer[edge];
									const SurfacePatchPoint& second = dense_outer[
										(edge + 1) % dense_outer.size()];
									const double edge_length = std::hypot(
										second.u - first.u, second.v - first.v);
									if (traversed + edge_length >= target)
										break;
									traversed += edge_length;
									++edge;
								}
								const SurfacePatchPoint& first = dense_outer[edge];
								const SurfacePatchPoint& second = dense_outer[
									(edge + 1) % dense_outer.size()];
								const double edge_length = std::hypot(
									second.u - first.u, second.v - first.v);
								const double alpha = edge_length > 1.0e-12
									? std::clamp((target - traversed) / edge_length,
										0.0, 1.0) : 0.0;
								coarse_outer.push_back({
									first.u + (second.u - first.u) * alpha,
									first.v + (second.v - first.v) * alpha, true});
							}
							collar.rings.back() = std::move(coarse_outer);
						}
					}
					slx_collars.push_back(std::move(collar));
				}
			}

			std::vector<Face2D> loop_faces;
			loop_faces.reserve(uv_contours.size());
			for (const auto& contour : uv_contours) {
				Face2D face;
				face.verts.reserve(contour.size());
				for (SurfacePatchPoint point : contour)
					face.verts.emplace_back(point.u, point.v);
				loop_faces.push_back(std::move(face));
			}
			const cVec2 keep_uv = choose_boundary_keep_point(
				&slx_mesh, loop_faces, outer_index);
			const CPoint3d keep_point(keep_uv.x, keep_uv.y, 0.0);
			for (size_t contour_index : hole_indices) {
				const SlxHoleCollar* collar = nullptr;
				for (SlxHoleCollar& candidate : slx_collars) {
					if (candidate.contour_index == contour_index) {
						collar = &candidate;
						candidate.used = true;
						break;
					}
				}
				const auto& trim_contour = collar
					? collar->rings.back() : uv_contours[contour_index];
				if (collar && collar->snap_cut_to_background_cells) {
					Face2D removal_polygon;
					for (SurfacePatchPoint point : trim_contour)
						removal_polygon.verts.emplace_back(point.u, point.v);
					SurfacePatchPoint center{};
					for (SurfacePatchPoint point : collar->rings.front()) {
						center.u += point.u;
						center.v += point.v;
					}
					center.u /= static_cast<double>(collar->rings.front().size());
					center.v /= static_cast<double>(collar->rings.front().size());
					for (CMesh3D::Face& face : slx_mesh.GetFaces()) {
						if (face.deleted || face.corners.size() < 3)
							continue;
						Face2D cell;
						cVec2 cell_center{};
						bool valid = true;
						for (const MeshCorner& corner : face.corners) {
							if (corner.v >= slx_mesh.GetVertices().size()) {
								valid = false;
								break;
							}
							const Vec3 vertex = slx_mesh.GetVertices()[corner.v];
							cell.verts.emplace_back(vertex.x, vertex.y);
							cell_center.x += vertex.x;
							cell_center.y += vertex.y;
						}
						if (!valid)
							continue;
						cell_center.x /= static_cast<double>(cell.verts.size());
						cell_center.y /= static_cast<double>(cell.verts.size());
						bool intersects_collar = ClassifyPointInFace2(cell,
								cVec2(center.u, center.v), EPS2D) != PFP_OUTSIDE
							|| ClassifyPointInFace2(removal_polygon,
								cell_center, EPS2D) != PFP_OUTSIDE;
						for (const cVec2& vertex : cell.verts) {
							intersects_collar = intersects_collar
								|| ClassifyPointInFace2(removal_polygon,
									vertex, EPS2D) != PFP_OUTSIDE;
						}
						for (SurfacePatchPoint point : trim_contour) {
							intersects_collar = intersects_collar
								|| ClassifyPointInFace2(cell,
									cVec2(point.u, point.v), EPS2D) != PFP_OUTSIDE;
						}
						if (intersects_collar) {
							face.deleted = true;
						}
					}
				} else {
					CPolyline hole;
					for (SurfacePatchPoint point : trim_contour)
						hole.AddPoint(CPoint3d(point.u, point.v, 0.0));
					hole.SetClosed(true);
					slx_mesh.TrimByPline(&hole, keep_point);
				}
			}
			const bool trim_created_triangles = std::any_of(
				slx_mesh.GetFaces().begin(), slx_mesh.GetFaces().end(),
				[](const CMesh3D::Face& face) {
					return !face.deleted && face.corners.size() == 3;
				});
			{
				const std::filesystem::path diagnostic_dir =
					"C:\\temp\\Dom3D_Quadrangulation";
				std::error_code directory_error;
				std::filesystem::create_directories(
					diagnostic_dir, directory_error);
				if (!directory_error) {
					const long long step_milli = std::llround(
						static_cast<double>(Deflection) * 1000.0);
					const std::filesystem::path obj_path = diagnostic_dir /
						("Surface_" + std::to_string(m_ID)
							+ "_SLX_StepMilli_" + std::to_string(step_milli)
							+ "_AfterHoles.obj");
					if (slx_mesh.ExportToObj(obj_path.string()))
						m_LastQuadrangulationDiagnostic = obj_path.string();
				}
			}

			// Stitch the exact inner rings to the real boundary produced by
			// TrimByPline.  The cutter is allowed to omit requested outer-ring nodes;
			// a zipper transition then absorbs that node-count mismatch locally with
			// quads where directions coincide and triangles everywhere else.
			const auto stitch_slx_collars = [&](CMesh3D& cut_mesh) {
				if (slx_collars.empty())
					return true;
				bool collar_merge_ok = true;
				std::vector<Vec3> merged_vertices;
				std::vector<CMesh3D::Face> merged_faces;
				const double cut_weld_tolerance = std::max(
					background_step * 1.0e-6, 1.0e-7);
				const auto welded_cut_vertex = [&](Vec3 requested) {
					for (size_t index = 0; index < merged_vertices.size(); ++index) {
						const Vec3 delta = merged_vertices[index] - requested;
						if (static_cast<double>(dot(delta, delta))
							<= cut_weld_tolerance * cut_weld_tolerance) {
							return index;
						}
					}
					merged_vertices.push_back(requested);
					return merged_vertices.size() - 1;
				};
				for (const CMesh3D::Face& face : cut_mesh.GetFaces()) {
					if (face.deleted || face.corners.size() < 3)
						continue;
					CMesh3D::Face welded_face = face;
					for (MeshCorner& corner : welded_face.corners) {
						if (corner.v >= cut_mesh.GetVertices().size()) {
							collar_merge_ok = false;
							break;
						}
						corner.v = welded_cut_vertex(cut_mesh.GetVertices()[corner.v]);
						corner.uv = 0;
						corner.n = 0;
					}
					if (!collar_merge_ok)
						break;
					merged_faces.push_back(std::move(welded_face));
				}
				std::map<std::pair<size_t, size_t>, int> edge_use;
				for (const CMesh3D::Face& face : merged_faces) {
					for (size_t corner = 0; corner < face.corners.size(); ++corner) {
						const size_t first = face.corners[corner].v;
						const size_t second = face.corners[
							(corner + 1) % face.corners.size()].v;
						++edge_use[std::minmax(first, second)];
					}
				}
				std::map<size_t, std::vector<size_t>> boundary_neighbors;
				for (const auto& edge : edge_use) {
					if (edge.second != 1)
						continue;
					boundary_neighbors[edge.first.first].push_back(edge.first.second);
					boundary_neighbors[edge.first.second].push_back(edge.first.first);
				}
				std::set<std::pair<size_t, size_t>> visited_boundary_edges;
				std::vector<std::vector<size_t>> boundary_loops;
				for (const auto& entry : boundary_neighbors) {
					for (size_t first_next : entry.second) {
						const auto first_edge = std::minmax(entry.first, first_next);
						if (visited_boundary_edges.count(first_edge) != 0)
							continue;
						std::vector<size_t> loop;
						size_t previous = std::numeric_limits<size_t>::max();
						size_t current = entry.first;
						size_t next = first_next;
						for (size_t guard = 0;
							guard <= boundary_neighbors.size() + 1; ++guard) {
							loop.push_back(current);
							visited_boundary_edges.insert(std::minmax(current, next));
							previous = current;
							current = next;
							if (current == loop.front())
								break;
							const auto neighbors = boundary_neighbors.find(current);
							if (neighbors == boundary_neighbors.end()
								|| neighbors->second.size() != 2) {
								loop.clear();
								break;
							}
							next = neighbors->second[0] == previous
								? neighbors->second[1] : neighbors->second[0];
						}
						if (loop.size() >= 3 && current == loop.front())
							boundary_loops.push_back(std::move(loop));
					}
				}
				const auto vertex_point = [&](size_t index) {
					return SurfacePatchPoint{
						merged_vertices[index].x, merged_vertices[index].y, true};
				};
				const auto append_face = [&](std::initializer_list<size_t> indices) {
					CMesh3D::Face face;
					for (size_t index : indices)
						face.corners.push_back({index, 0, 0});
					merged_faces.push_back(std::move(face));
				};
				for (const SlxHoleCollar& collar : slx_collars) {
					if (!collar.used)
						continue;
					const size_t ring_size = collar.rings.front().size();
					const std::vector<SurfacePatchPoint>& requested_outer =
						collar.rings.back();
					size_t best_loop = boundary_loops.size();
					double best_loop_score = std::numeric_limits<double>::max();
					double best_loop_max_distance =
						std::numeric_limits<double>::max();
					for (size_t loop_index = 0;
						loop_index < boundary_loops.size(); ++loop_index) {
						double score = 0.0;
						for (SurfacePatchPoint requested : requested_outer) {
							double point_score = std::numeric_limits<double>::max();
							const auto& loop = boundary_loops[loop_index];
							for (size_t edge = 0; edge < loop.size(); ++edge) {
								point_score = std::min(point_score,
									point_segment_distance(requested,
										vertex_point(loop[edge]),
										vertex_point(loop[(edge + 1) % loop.size()])));
							}
							score += point_score * point_score;
						}
						score /= static_cast<double>(requested_outer.size());
						double loop_max_distance = 0.0;
						for (size_t vertex_index : boundary_loops[loop_index]) {
							double vertex_distance =
								std::numeric_limits<double>::max();
							const SurfacePatchPoint vertex = vertex_point(vertex_index);
							for (size_t edge = 0; edge < requested_outer.size(); ++edge) {
								vertex_distance = std::min(vertex_distance,
									point_segment_distance(vertex, requested_outer[edge],
										requested_outer[(edge + 1) % requested_outer.size()]));
							}
							loop_max_distance = std::max(loop_max_distance, vertex_distance);
						}
						if (score < best_loop_score) {
							best_loop_score = score;
							best_loop_max_distance = loop_max_distance;
							best_loop = loop_index;
						}
					}
					const double loop_match_distance =
						collar.cut_matches_requested_outer
							? background_step * 0.8 : background_step * 3.0;
					if (best_loop == boundary_loops.size()
						|| best_loop_score
							> loop_match_distance * loop_match_distance
						|| best_loop_max_distance > loop_match_distance) {
						collar_merge_ok = false;
						break;
					}
					std::vector<size_t> outer_indices = boundary_loops[best_loop];
					double boundary_loop_area = 0.0;
					for (size_t i = 0; i < outer_indices.size(); ++i) {
						const SurfacePatchPoint a = vertex_point(outer_indices[i]);
						const SurfacePatchPoint b = vertex_point(
							outer_indices[(i + 1) % outer_indices.size()]);
						boundary_loop_area += a.u * b.v - b.u * a.v;
					}
					if (boundary_loop_area < 0.0)
						std::reverse(outer_indices.begin(), outer_indices.end());

					// The requested outer ring is represented by the cutter boundary.
					// Insert only exact and optional intermediate rings here.
					// TrimByPline already owns the requested outer ring when the
					// direct cut is clean. The complete-cell fallback has a different
					// boundary, so include the outer collar ring and add one more
					// welded transition from it to the retained background cells.
					const size_t mesh_ring_count = collar.cut_matches_requested_outer
						? collar.rings.size() - 1 : collar.rings.size();
					std::vector<std::vector<size_t>> ring_indices(mesh_ring_count);
					for (size_t ring = 0; ring < mesh_ring_count; ++ring) {
						ring_indices[ring].reserve(ring_size);
						for (SurfacePatchPoint point : collar.rings[ring]) {
							const size_t index = merged_vertices.size();
							merged_vertices.push_back({static_cast<float>(point.u),
								static_cast<float>(point.v), 0.0f});
							ring_indices[ring].push_back(index);
						}
					}
					for (size_t ring = 0;
						ring + 1 < ring_indices.size(); ++ring) {
						const std::vector<size_t>& inner_ring = ring_indices[ring];
						const std::vector<size_t>& outer_ring = ring_indices[ring + 1];
						if (inner_ring.size() == outer_ring.size()) {
							for (size_t i = 0; i < inner_ring.size(); ++i) {
								const size_t next = (i + 1) % inner_ring.size();
								append_face({inner_ring[i], outer_ring[i],
									outer_ring[next], inner_ring[next]});
							}
							continue;
						}
						// A large node-count reduction (for example 20 -> 8) cannot
						// be tiled safely by collapsing pairs into kite quads: those
						// quads overlap. Walk both normalized perimeters instead and
						// emit a quad only at coincident advances, otherwise one short
						// local triangle. This is the explicit collar topology used by
						// the old eight-sided transition.
						const auto cumulative_parameters = [&](const std::vector<size_t>& indices) {
							std::vector<double> parameters(indices.size() + 1, 0.0);
							for (size_t i = 0; i < indices.size(); ++i) {
								const Vec3 delta = merged_vertices[
									indices[(i + 1) % indices.size()]]
									- merged_vertices[indices[i]];
								parameters[i + 1] = parameters[i] + std::sqrt(
									static_cast<double>(dot(delta, delta)));
							}
							if (parameters.back() > 1.0e-12) {
								for (double& parameter : parameters)
									parameter /= parameters.back();
							}
							return parameters;
						};
						const std::vector<double> inner_walk =
							cumulative_parameters(inner_ring);
						const std::vector<double> outer_walk =
							cumulative_parameters(outer_ring);
						size_t inner_walk_index = 0;
						size_t outer_walk_index = 0;
						while (inner_walk_index < inner_ring.size()
							|| outer_walk_index < outer_ring.size()) {
							const size_t inner_current = inner_ring[
								inner_walk_index % inner_ring.size()];
							const size_t outer_current = outer_ring[
								outer_walk_index % outer_ring.size()];
							const double next_inner = inner_walk_index < inner_ring.size()
								? inner_walk[inner_walk_index + 1]
								: std::numeric_limits<double>::infinity();
							const double next_outer = outer_walk_index < outer_ring.size()
								? outer_walk[outer_walk_index + 1]
								: std::numeric_limits<double>::infinity();
							if (std::fabs(next_inner - next_outer) <= 1.0e-8) {
								append_face({inner_current, outer_current,
									outer_ring[(outer_walk_index + 1) % outer_ring.size()],
									inner_ring[(inner_walk_index + 1) % inner_ring.size()]});
								++inner_walk_index;
								++outer_walk_index;
							} else if (next_inner < next_outer) {
								append_face({inner_current, outer_current,
									inner_ring[(inner_walk_index + 1) % inner_ring.size()]});
								++inner_walk_index;
							} else {
								append_face({inner_current, outer_current,
									outer_ring[(outer_walk_index + 1) % outer_ring.size()]});
								++outer_walk_index;
							}
						}
						continue;
					}

					std::vector<size_t>& inner_indices = ring_indices.back();
					size_t inner_start = 0;
					size_t outer_start = 0;
					double best_start_distance = std::numeric_limits<double>::max();
					for (size_t i = 0; i < inner_indices.size(); ++i) {
						for (size_t j = 0; j < outer_indices.size(); ++j) {
							const Vec3 delta = merged_vertices[inner_indices[i]]
								- merged_vertices[outer_indices[j]];
							const double distance = static_cast<double>(dot(delta, delta));
							if (distance < best_start_distance) {
								best_start_distance = distance;
								inner_start = i;
								outer_start = j;
							}
						}
					}
					std::rotate(inner_indices.begin(),
						inner_indices.begin() + inner_start, inner_indices.end());
					std::rotate(outer_indices.begin(),
						outer_indices.begin() + outer_start, outer_indices.end());
					const auto perimeter_parameters = [&](const std::vector<size_t>& indices) {
						std::vector<double> parameters(indices.size() + 1, 0.0);
						double perimeter = 0.0;
						for (size_t i = 0; i < indices.size(); ++i) {
							const Vec3 delta = merged_vertices[indices[(i + 1) % indices.size()]]
								- merged_vertices[indices[i]];
							perimeter += std::sqrt(static_cast<double>(dot(delta, delta)));
							parameters[i + 1] = perimeter;
						}
						if (perimeter > 1.0e-12) {
							for (double& parameter : parameters)
								parameter /= perimeter;
						}
						return parameters;
					};
					const std::vector<double> inner_parameters =
						perimeter_parameters(inner_indices);
					const std::vector<double> outer_parameters =
						perimeter_parameters(outer_indices);
					const size_t inner_count = inner_indices.size();
					const size_t outer_count = outer_indices.size();
					// Counts are intentionally unrestricted (for example 9 against 14).
					// Walk both normalized perimeters and keep every surplus advance as
					// one short local triangle instead of forcing an even node count.
					size_t inner = 0;
					size_t outer = 0;
					while (inner < inner_count || outer < outer_count) {
						const size_t inner_current = inner_indices[inner % inner_count];
						const size_t outer_current = outer_indices[outer % outer_count];
						const double next_inner = inner < inner_count
							? inner_parameters[inner + 1]
							: std::numeric_limits<double>::infinity();
						const double next_outer = outer < outer_count
							? outer_parameters[outer + 1]
							: std::numeric_limits<double>::infinity();
						if (std::fabs(next_inner - next_outer) <= 1.0e-8) {
							append_face({inner_current, outer_current,
								outer_indices[(outer + 1) % outer_count],
								inner_indices[(inner + 1) % inner_count]});
							++inner;
							++outer;
						} else if (next_inner < next_outer) {
							append_face({inner_current, outer_current,
								inner_indices[(inner + 1) % inner_count]});
							++inner;
						} else {
							append_face({inner_current, outer_current,
								outer_indices[(outer + 1) % outer_count]});
							++outer;
						}
					}
				}
				if (collar_merge_ok) {
					collar_merge_ok = cut_mesh.SetGeometry(
						std::move(merged_vertices), std::move(merged_faces));
				}
				return collar_merge_ok;
			};
			// The outer collar contour owns the cut. Add the prebuilt collar to the
			// retained SLX mesh and weld on that exact boundary; triangle pairs made
			// by TrimByPline are restored to quads after the merge below.
			bool collar_merge_ok = !trim_created_triangles
				&& stitch_slx_collars(slx_mesh);
			if (!collar_merge_ok) {
				slx_mesh.Clear();
				if (MakeFilledContour(outer_contour, {0.0f, 0.0f, 1.0f},
						&slx_mesh, Deflection >= 1.5f, nullptr, nullptr, nullptr)) {
					// Adaptive fallback: remove complete background cells around each
					// feature. Their existing edges form the irregular outer contour shown
					// by the one-row collar variant and require no fragile coarse-cell cut.
					for (const SlxHoleCollar& collar : slx_collars) {
						if (!collar.used)
							continue;
						Face2D removal_polygon;
						for (SurfacePatchPoint point : collar.rings.back())
							removal_polygon.verts.emplace_back(point.u, point.v);
						SurfacePatchPoint center{};
						for (SurfacePatchPoint point : collar.rings.front()) {
							center.u += point.u;
							center.v += point.v;
						}
						center.u /= static_cast<double>(collar.rings.front().size());
						center.v /= static_cast<double>(collar.rings.front().size());
						for (CMesh3D::Face& face : slx_mesh.GetFaces()) {
							if (face.deleted || face.corners.size() < 3)
								continue;
							Face2D cell;
							cVec2 cell_center{};
							bool valid_cell = true;
							for (const MeshCorner& corner : face.corners) {
								if (corner.v >= slx_mesh.GetVertices().size()) {
									valid_cell = false;
									break;
								}
								const Vec3 vertex = slx_mesh.GetVertices()[corner.v];
								cell.verts.emplace_back(vertex.x, vertex.y);
								cell_center.x += vertex.x;
								cell_center.y += vertex.y;
							}
							if (!valid_cell)
								continue;
							cell_center.x /= static_cast<double>(cell.verts.size());
							cell_center.y /= static_cast<double>(cell.verts.size());
							const bool remove_cell = ClassifyPointInFace2(cell,
									cVec2(center.u, center.v), EPS2D) != PFP_OUTSIDE
								|| ClassifyPointInFace2(removal_polygon,
									cell_center, EPS2D) != PFP_OUTSIDE;
							if (remove_cell) {
								face.deleted = true;
							}
						}
					}
					collar_merge_ok = stitch_slx_collars(slx_mesh);
				}
			}
			if (!collar_merge_ok) {
				slx_mesh.Clear();
				if (MakeFilledContour(outer_contour, {0.0f, 0.0f, 1.0f},
						&slx_mesh, Deflection >= 1.5f, nullptr, nullptr, nullptr)) {
					for (size_t contour_index : hole_indices) {
						CPolyline hole;
						for (SurfacePatchPoint point : uv_contours[contour_index])
							hole.AddPoint(CPoint3d(point.u, point.v, 0.0));
						hole.SetClosed(true);
						slx_mesh.TrimByPline(&hole, keep_point);
					}
				}
			}

			// A collar stitch may be topologically valid yet fail to contribute its
			// annular strip (the remaining area then matches the coarser outer cut,
			// not the exact hole). Detect that case in the planar UV domain and rerun
			// the same SLX cutter directly on the exact prepared contours. Dense
			// circles can produce N-gons here; they are ear-clipped just below.
			const auto active_area_xy = [](const CMesh3D& mesh) {
				double area = 0.0;
				for (const CMesh3D::Face& face : mesh.GetFaces()) {
					if (face.deleted || face.corners.size() < 3)
						continue;
					double twice_area = 0.0;
					bool valid = true;
					for (size_t i = 0; i < face.corners.size(); ++i) {
						const size_t first = face.corners[i].v;
						const size_t second = face.corners[
							(i + 1) % face.corners.size()].v;
						if (first >= mesh.GetVertices().size()
							|| second >= mesh.GetVertices().size()) {
							valid = false;
							break;
						}
						const Vec3& a = mesh.GetVertices()[first];
						const Vec3& b = mesh.GetVertices()[second];
						twice_area += static_cast<double>(a.x) * b.y
							- static_cast<double>(b.x) * a.y;
					}
					if (valid)
						area += std::fabs(twice_area) * 0.5;
				}
				return area;
			};
			double expected_uv_area = std::fabs(signed_area(
				uv_contours[outer_index]));
			for (size_t contour_index : hole_indices)
				expected_uv_area -= std::fabs(signed_area(
					uv_contours[contour_index]));
			const double stitched_uv_area = active_area_xy(slx_mesh);
			if (expected_uv_area > 1.0e-9
				&& std::fabs(stitched_uv_area - expected_uv_area)
					> expected_uv_area * 0.01) {
				const std::filesystem::path stitched_path =
					std::filesystem::path("C:\\temp\\Dom3D_Quadrangulation") /
					("Surface_" + std::to_string(m_ID)
						+ "_SLX_StitchedAreaMismatch.obj");
				slx_mesh.ExportToObj(stitched_path.string());
				slx_mesh.Clear();
				if (MakeFilledContour(outer_contour, {0.0f, 0.0f, 1.0f},
						&slx_mesh, Deflection >= 1.5f,
						nullptr, nullptr, nullptr)) {
					for (size_t contour_index : hole_indices) {
						CPolyline hole;
						for (SurfacePatchPoint point : uv_contours[contour_index])
							hole.AddPoint(CPoint3d(point.u, point.v, 0.0));
						hole.SetClosed(true);
						slx_mesh.TrimByPline(&hole, keep_point);
					}
				}
			}

			// Trimming can leave a disconnected remnant on the rejected side of a
			// periodic cut. Keep only the component containing the previously
			// classified surface point before pairing the split cells.
			slx_mesh.KeepConnectedComponentAt(keep_point);
			// Restore source cells which TrimByPline split into adjacent triangle
			// pairs before handling the rarer N-gon remainder.
			merge_trim_triangle_pairs_to_quads(slx_mesh);
			// A dense circular contour can cut several times through one coarse
			// background cell, leaving a simple N-gon. Ear-clip only those N-gons;
			// keep every trim vertex and leave triangles/quads untouched.
			if (!triangulate_mesh_ngons_in_xy(slx_mesh))
				slx_mesh.Clear();
			const bool has_active_face = std::any_of(
					slx_mesh.GetFaces().begin(), slx_mesh.GetFaces().end(),
					[](const CMesh3D::Face& face) {
						return !face.deleted && face.corners.size() >= 3;
					});
			if (has_active_face) {
					if (m_Face.Orientation() == TopAbs_REVERSED) {
						for (CMesh3D::Face& face : slx_mesh.GetFaces()) {
							if (!face.deleted)
								std::reverse(face.corners.begin(), face.corners.end());
						}
					}
					// TrimByPline creates vertices and corners, but its new corner UV and
					// normal indices do not belong to the pre-trim attribute arrays.
					// Supplying those stale arrays makes SetGeometry reject the direct
					// trim result. Rebuild attributes from the returned topology instead.
					if (pMesh3D->SetGeometry(slx_mesh.GetVertices(),
							slx_mesh.GetFaces())
						&& pMesh3D->RestoreTo3DFromUVSurface(this)) {
						// The two sides of a periodic UV development represent the same
						// 3D seam. Weld after restoration so the SLX result remains one
						// connected surface mesh rather than two coincident components.
						std::unique_ptr<CMesh3D> welded = CMesh3D::CreateWelded(
							std::vector<const CMesh3D*>{pMesh3D});
						if (welded) {
							pMesh3D->SetGeometry(welded->GetVertices(),
								welded->GetFaces(), welded->GetUVs(),
								welded->GetNormals());
						}
						m_LastIslandBoundariesUV.clear();
						for (const auto& contour : uv_contours) {
							std::vector<CPoint3d> boundary;
							boundary.reserve(contour.size());
							for (SurfacePatchPoint point : contour)
								boundary.emplace_back(point.u, point.v, 0.0);
							m_LastIslandBoundariesUV.push_back(std::move(boundary));
						}
						IsTrimmed = true;
						IsInitMesh = true;
						return true;
					}
			}
		}
		pMesh3D->Clear();
		m_LastIslandFillError = quadrangulator_created
			? "Mesh Quadro Hole SLX could not produce an active trimmed mesh."
			: "Mesh Quadro Hole SLX: " + (fill_error.empty()
				? std::string("ContourQuadrangulator returned an empty mesh.")
				: fill_error);
		return false;
	}

	// A small hole beside a coarse outer grid creates a severe scale jump for
	// the island quadrangulator. Build one or two conforming quad collars first,
	// then let the patch builder work with the larger outer collar contour. The
	// real trimming contour remains untouched as the inner edge of the collar.
	struct HoleCollar {
		std::vector<std::vector<SurfacePatchPoint>> rings;
	};
	std::vector<HoleCollar> hole_collars;
	// Protect a small trimming loop with a compact regular quad collar.  The
	// four directional islands are built from its outer ring; the real hole
	// contour remains the inner ring and therefore stays exact and conforming.
	constexpr bool kUsePreSplitHoleCollars = true;
	if (kUsePreSplitHoleCollars && uv_contours.size() > 1
		&& BRepAdaptor_Surface(TopoDS::Face(m_Face)).GetType()
			== GeomAbs_Plane) {
		const auto signed_area = [](const std::vector<SurfacePatchPoint>& contour) {
			double result = 0.0;
			for (size_t i = 0; i < contour.size(); ++i) {
				const SurfacePatchPoint& a = contour[i];
				const SurfacePatchPoint& b = contour[(i + 1) % contour.size()];
				result += a.u * b.v - b.u * a.v;
			}
			return result * 0.5;
		};
		const auto ray_segment_distance = [](SurfacePatchPoint origin,
			double direction_u, double direction_v,
			SurfacePatchPoint a, SurfacePatchPoint b) {
			const double segment_u = b.u - a.u;
			const double segment_v = b.v - a.v;
			const double denominator = direction_u * segment_v
				- direction_v * segment_u;
			if (std::fabs(denominator) <= 1.0e-14)
				return std::numeric_limits<double>::max();
			const double relative_u = a.u - origin.u;
			const double relative_v = a.v - origin.v;
			const double ray_distance = (relative_u * segment_v
				- relative_v * segment_u) / denominator;
			const double segment_parameter = (relative_u * direction_v
				- relative_v * direction_u) / denominator;
			if (ray_distance <= 1.0e-9
				|| segment_parameter < -1.0e-9
				|| segment_parameter > 1.0 + 1.0e-9) {
				return std::numeric_limits<double>::max();
			}
			return ray_distance;
		};
		const auto contains = [](const std::vector<SurfacePatchPoint>& polygon,
			SurfacePatchPoint point) {
			bool inside = false;
			for (size_t i = 0, j = polygon.size() - 1;
				i < polygon.size(); j = i++) {
				const SurfacePatchPoint& a = polygon[j];
				const SurfacePatchPoint& b = polygon[i];
				if (((a.v > point.v) != (b.v > point.v))
					&& point.u < (b.u - a.u) * (point.v - a.v)
						/ (b.v - a.v) + a.u) {
					inside = !inside;
				}
			}
			return inside;
		};

		const std::vector<std::vector<SurfacePatchPoint>> source_contours =
			uv_contours;
		const size_t outer_index = static_cast<size_t>(std::distance(
			source_contours.begin(),
			std::max_element(source_contours.begin(), source_contours.end(),
				[&](const auto& first, const auto& second) {
					return std::fabs(signed_area(first))
						< std::fabs(signed_area(second));
				})));
		std::vector<double> outer_edges;
		const auto& outer = source_contours[outer_index];
		outer_edges.reserve(outer.size());
		for (size_t i = 0; i < outer.size(); ++i) {
			outer_edges.push_back(std::hypot(
				outer[(i + 1) % outer.size()].u - outer[i].u,
				outer[(i + 1) % outer.size()].v - outer[i].v));
		}
		std::sort(outer_edges.begin(), outer_edges.end());
		const double outer_step = outer_edges.empty() ? 0.0
			: outer_edges[std::min(
				outer_edges.size() - 1,
				static_cast<size_t>(outer_edges.size() * 2 / 3))];
		const double geometry_epsilon = std::max(outer_step * 1.0e-6, 1.0e-9);
		for (size_t contour_index = 0;
			contour_index < source_contours.size(); ++contour_index) {
			if (contour_index == outer_index
				|| source_contours[contour_index].size() < 4
				|| outer_step <= geometry_epsilon) {
				continue;
			}
			std::vector<SurfacePatchPoint> hole = source_contours[contour_index];
			if (signed_area(hole) < 0.0)
				std::reverse(hole.begin(), hole.end());
			SurfacePatchPoint center{};
			for (SurfacePatchPoint point : hole) {
				center.u += point.u;
				center.v += point.v;
			}
			center.u /= static_cast<double>(hole.size());
			center.v /= static_cast<double>(hole.size());
			double mean_radius = 0.0;
			bool convex = true;
			double turn_sign = 0.0;
			for (size_t i = 0; i < hole.size(); ++i) {
				mean_radius += std::hypot(
					hole[i].u - center.u, hole[i].v - center.v);
				const SurfacePatchPoint& a = hole[i];
				const SurfacePatchPoint& b = hole[(i + 1) % hole.size()];
				const SurfacePatchPoint& c = hole[(i + 2) % hole.size()];
				const double turn = (b.u - a.u) * (c.v - b.v)
					- (b.v - a.v) * (c.u - b.u);
				if (std::fabs(turn) <= geometry_epsilon)
					continue;
				if (turn_sign == 0.0)
					turn_sign = turn;
				else if (turn * turn_sign < 0.0)
					convex = false;
			}
			mean_radius /= static_cast<double>(hole.size());
			if (!convex || mean_radius <= geometry_epsilon
				|| mean_radius > outer_step * 1.5) {
				continue;
			}

			std::vector<double> directional_widths(hole.size(), 0.0);
			double mean_width = 0.0;
			double maximum_width = 0.0;
			// A tiny hole must not reserve a collar several global cells wide.
			// Keep the complete local cluster within both the surface sizing field
			// and a radius-relative envelope.  Clearance below additionally makes
			// the collar contract near another trim boundary.
			const double step_width_limit = outer_step * 0.75;
			const double radius_width_limit = mean_radius * 1.5;
			for (size_t point_index = 0;
				point_index < hole.size(); ++point_index) {
				const SurfacePatchPoint point = hole[point_index];
				const double du = point.u - center.u;
				const double dv = point.v - center.v;
				const double radius = std::hypot(du, dv);
				const double direction_u = du / radius;
				const double direction_v = dv / radius;
				double collision_distance =
					std::numeric_limits<double>::max();
				for (size_t other = 0;
					other < source_contours.size(); ++other) {
					if (other == contour_index)
						continue;
					const auto& obstacle = source_contours[other];
					for (size_t edge = 0; edge < obstacle.size(); ++edge) {
						collision_distance = std::min(collision_distance,
							ray_segment_distance(point,
								direction_u, direction_v,
								obstacle[edge],
								obstacle[(edge + 1) % obstacle.size()]));
					}
				}
				const double clearance_width = std::isfinite(collision_distance)
					? collision_distance * 0.30
					: std::numeric_limits<double>::max();
				directional_widths[point_index] = std::min({
					step_width_limit, radius_width_limit, clearance_width});
				mean_width += directional_widths[point_index];
				maximum_width = std::max(maximum_width,
					directional_widths[point_index]);
			}
			if (source_contours.size() > 2 && !directional_widths.empty()) {
				// Several islands must keep mutually predictable, convex collars;
				// independent directional stretching can complicate the bridges
				// between neighbouring holes. Reserve the asymmetric collar for
				// the single small-hole case near an outer boundary.
				const double uniform_width = *std::min_element(
					directional_widths.begin(), directional_widths.end());
				std::fill(directional_widths.begin(), directional_widths.end(),
					uniform_width);
				mean_width = uniform_width * directional_widths.size();
				maximum_width = uniform_width;
			}
			mean_width /= static_cast<double>(directional_widths.size());
			if (mean_width < std::min(outer_step * 0.25,
				mean_radius * 0.35)) {
				continue;
			}
			// Retain two compact transition rings when their combined width is
			// meaningful for this feature.  In a tight gap the clearance cap makes
			// the width small and a single ring is safer.
			const double two_ring_width = std::min(
				outer_step * 0.5, mean_radius);
			const int ring_count = maximum_width >= two_ring_width ? 2 : 1;
			HoleCollar collar;
			collar.rings.push_back(hole);
			bool valid = true;
			for (int ring = 1; ring <= ring_count; ++ring) {
				std::vector<SurfacePatchPoint> expanded;
				expanded.reserve(hole.size());
				for (size_t point_index = 0;
					point_index < hole.size(); ++point_index) {
					const SurfacePatchPoint point = hole[point_index];
					const double du = point.u - center.u;
					const double dv = point.v - center.v;
					const double radius = std::hypot(du, dv);
					const double offset = directional_widths[point_index]
						* ring / ring_count;
					SurfacePatchPoint expanded_point{
						point.u + du / radius * offset,
						point.v + dv / radius * offset,
						true};
					if (!contains(outer, expanded_point)) {
						valid = false;
						break;
					}
					for (size_t other = 0;
						valid && other < source_contours.size(); ++other) {
						if (other != contour_index && other != outer_index
							&& contains(source_contours[other], expanded_point)) {
							valid = false;
						}
					}
					expanded.push_back(expanded_point);
				}
				for (size_t i = 0; valid && i < expanded.size(); ++i) {
					const SurfacePatchPoint& a = expanded[i];
					const SurfacePatchPoint& b = expanded[(i + 1) % expanded.size()];
					for (double alpha : {0.25, 0.5, 0.75}) {
						const SurfacePatchPoint sample{
							a.u + (b.u - a.u) * alpha,
							a.v + (b.v - a.v) * alpha, true};
						if (!contains(outer, sample)) {
							valid = false;
							break;
						}
						for (size_t other = 0;
							valid && other < source_contours.size(); ++other) {
							if (other != contour_index && other != outer_index
								&& contains(source_contours[other], sample)) {
								valid = false;
							}
						}
					}
				}
				if (!valid)
					break;
				collar.rings.push_back(std::move(expanded));
			}
			if (!valid || collar.rings.size() < 2)
				continue;
			uv_contours[contour_index] = collar.rings.back();
			hole_collars.push_back(std::move(collar));
		}
	}

	std::vector<std::vector<SurfacePatchPoint>> patches;
	std::string patch_error;
	if (!BuildSurfacePatchesWithoutHoles(uv_contours, patches, &patch_error)
		|| patches.empty()) {
		m_LastIslandFillError = patch_error.empty()
			? "Island splitting produced no patches." : patch_error;
		return false;
	}
	// Preserve the exact island contours as soon as the bridge builder has
	// produced them.  They are diagnostic input to the quadrangulator, so they
	// must remain available even when a later mesh/restore stage fails and the
	// caller falls back to the legacy trimming path.
	m_LastIslandBoundariesUV.clear();
	m_LastIslandBoundariesUV.reserve(patches.size());
	for (const std::vector<SurfacePatchPoint>& patch : patches) {
		std::vector<CPoint3d> boundary;
		boundary.reserve(patch.size());
		for (SurfacePatchPoint point : patch)
			boundary.emplace_back(point.u, point.v, 0.0);
		m_LastIslandBoundariesUV.push_back(std::move(boundary));
	}
	if (dump_boundary) {
		for (const std::vector<SurfacePatchPoint>& patch : patches) {
			std::vector<CPoint3d> points;
			points.reserve(patch.size());
			for (SurfacePatchPoint point : patch)
				points.emplace_back(point.u, point.v, 0.0);
			dump_polyline(dump_paths[3], points, true);
		}
	}

	std::vector<Vec3> merged_vertices;
	std::vector<CMesh3D::Face> merged_faces;
	// Every island is quadrangulated independently, but the result belongs to
	// one OCCT face.  Reuse UV vertices on shared patch/collar boundaries while
	// assembling the result; merely appending each patch with an offset leaves
	// coincident vertices disconnected and turns every patch into a mesh island.
	double uv_coordinate_scale = 1.0;
	for (const auto& contour : uv_contours) {
		for (SurfacePatchPoint point : contour) {
			uv_coordinate_scale = std::max(uv_coordinate_scale,
				std::max(std::fabs(point.u), std::fabs(point.v)));
		}
	}
	const double patch_weld_tolerance = std::max(1.0e-7,
		uv_coordinate_scale * std::numeric_limits<float>::epsilon() * 8.0);
	std::multimap<float, size_t> merged_vertices_by_u;
	const auto welded_patch_vertex = [&](Vec3 requested) {
		const float min_u = static_cast<float>(
			static_cast<double>(requested.x) - patch_weld_tolerance);
		const float max_u = static_cast<float>(
			static_cast<double>(requested.x) + patch_weld_tolerance);
		for (auto candidate = merged_vertices_by_u.lower_bound(min_u);
			candidate != merged_vertices_by_u.end()
				&& candidate->first <= max_u; ++candidate) {
			const Vec3 delta = merged_vertices[candidate->second] - requested;
			if (static_cast<double>(dot(delta, delta))
				<= patch_weld_tolerance * patch_weld_tolerance) {
				return candidate->second;
			}
		}
		const size_t index = merged_vertices.size();
		merged_vertices.push_back(requested);
		merged_vertices_by_u.emplace(requested.x, index);
		return index;
	};
	for (const HoleCollar& collar : hole_collars) {
		if (collar.rings.size() < 2)
			continue;
		const size_t ring_size = collar.rings.front().size();
		std::vector<std::vector<size_t>> ring_indices;
		ring_indices.reserve(collar.rings.size());
		for (const auto& ring : collar.rings) {
			std::vector<size_t> indices;
			indices.reserve(ring.size());
			for (SurfacePatchPoint point : ring) {
				indices.push_back(welded_patch_vertex({
					static_cast<float>(point.u),
					static_cast<float>(point.v), 0.0f}));
			}
			ring_indices.push_back(std::move(indices));
		}
		for (size_t ring = 0; ring + 1 < collar.rings.size(); ++ring) {
			for (size_t i = 0; i < ring_size; ++i) {
				const size_t next = (i + 1) % ring_size;
				CMesh3D::Face face;
				face.corners = {
					{ring_indices[ring][i], 0, 0},
					{ring_indices[ring + 1][i], 0, 0},
					{ring_indices[ring + 1][next], 0, 0},
					{ring_indices[ring][next], 0, 0}};
				merged_faces.push_back(std::move(face));
			}
		}
	}
	const bool guard_cylinder_patch_spacing = patches.size() > 1
		&& !use_mesh_quadro_hole_slx
		&& BRepAdaptor_Surface(TopoDS::Face(m_Face)).GetType()
			== GeomAbs_Cylinder;
	for (size_t patch_index = 0; patch_index < patches.size(); ++patch_index) {
		const std::vector<SurfacePatchPoint>& patch = patches[patch_index];
		std::vector<Vec3> contour;
		contour.reserve(patch.size());
		for (SurfacePatchPoint point : patch) {
			contour.push_back({static_cast<float>(point.u),
			                   static_cast<float>(point.v), 0.0f});
		}

		CMesh3D patch_mesh;
		CMesh3D contour_to_fill_mesh;
		std::string fill_error;
		std::string quadrangulator_rejection;
		if (!MakeFilledContour(contour, {0.0f, 0.0f, 1.0f}, &patch_mesh,
			guard_cylinder_patch_spacing, &fill_error, &contour_to_fill_mesh,
			&quadrangulator_rejection)) {
			m_LastIslandFillError = "Surface " + std::to_string(m_ID)
				+ ", island " + std::to_string(patch_index + 1) + "/"
				+ std::to_string(patches.size()) + ", nodes "
				+ std::to_string(contour.size()) + ": " + fill_error;
#if defined(_WIN32)
			OutputDebugStringA(("Dom3D Low Poly: " + m_LastIslandFillError
				+ "\n").c_str());
#endif
			return false;
		}
		if (guard_cylinder_patch_spacing
			&& quadrangulator_rejection
				== "ContourQuadrangulator collapsed the internal patch spacing.") {
			// The independent island fronts have bunched into visually doubled
			// rows. Rebuild this cylinder through the single-background SLX path;
			// all other faces and all well-spaced cylinder patches stay unchanged.
			return BuildFilledMeshWhithHoles(Deflection, true);
		}

		// Preserve the exact input whenever the ported ContourQuadrangulator is
		// rejected. Importing this OBJ into 3DCoat gives its quadrangulator the
		// same triangles and therefore exactly the same open boundary. This makes
		// an A/B comparison distinguish a port difference from contour/data prep.
		if (!quadrangulator_rejection.empty()) {
			const std::filesystem::path diagnostic_dir =
				"C:\\temp\\Dom3D_Quadrangulation";
			std::error_code directory_error;
			std::filesystem::create_directories(
				diagnostic_dir, directory_error);
			if (!directory_error) {
				const long long density_milli = std::llround(
					static_cast<double>(Deflection) * 1000.0);
				const std::string stem = "Surface_" + std::to_string(m_ID)
					+ "_Island_" + std::to_string(patch_index + 1)
					+ "_StepMilli_" + std::to_string(density_milli);
				const std::filesystem::path obj_path =
					diagnostic_dir / (stem + "_ContourToFill.obj");
				const std::filesystem::path contour_path =
					diagnostic_dir / (stem + "_Contour.txt");
				const std::filesystem::path reason_path =
					diagnostic_dir / (stem + "_Reason.txt");
				const bool obj_written = contour_to_fill_mesh.ExportToObj(
					obj_path.string());
				std::ofstream contour_stream(contour_path,
					std::ios::out | std::ios::trunc);
				if (contour_stream) {
					const auto same_point = [](Vec3 lhs, Vec3 rhs) {
						const Vec3 delta = lhs - rhs;
						return dot(delta, delta) <= 1.0e-10f;
					};
					const bool already_closed = contour.size() > 1
						&& same_point(contour.front(), contour.back());
					const size_t output_count = contour.size()
						+ (already_closed || contour.empty() ? 0 : 1);
					contour_stream << "POLYLINE\n" << output_count << "\n";
					contour_stream << std::fixed << std::setprecision(4);
					const auto write_point = [&contour_stream](Vec3 point) {
						contour_stream << std::setw(10) << point.x << " "
							<< std::setw(10) << point.y << " "
							<< std::setw(10) << point.z << "\n";
					};
					for (Vec3 point : contour) {
						write_point(point);
					}
					if (!contour.empty() && !already_closed) {
						write_point(contour.front());
					}
				}
				std::ofstream reason_stream(reason_path,
					std::ios::out | std::ios::trunc);
				if (reason_stream)
					reason_stream << quadrangulator_rejection << '\n';
				if (obj_written && contour_stream) {
					m_LastQuadrangulationDiagnostic = obj_path.string();
#if defined(_WIN32)
					OutputDebugStringA(("Dom3D quadrangulation A/B input: "
						+ m_LastQuadrangulationDiagnostic + "\n").c_str());
#endif
				}
			}
		}

		std::vector<size_t> patch_vertex_indices(
			patch_mesh.GetVertices().size());
		for (size_t index = 0; index < patch_mesh.GetVertices().size(); ++index) {
			patch_vertex_indices[index] = welded_patch_vertex(
				patch_mesh.GetVertices()[index]);
		}
		for (const CMesh3D::Face& source_face : patch_mesh.GetFaces()) {
			if (source_face.deleted || source_face.corners.size() < 3)
				continue;
			CMesh3D::Face face = source_face;
			for (MeshCorner& corner : face.corners) {
				if (corner.v >= patch_vertex_indices.size()) {
					face.corners.clear();
					break;
				}
				corner.v = patch_vertex_indices[corner.v];
				corner.n = corner.v;
				corner.uv = corner.v;
			}
			if (face.corners.size() < 3)
				continue;
			// A tolerance weld can collapse a very short boundary edge.  Remove
			// duplicate adjacent corners instead of keeping a zero-area polygon.
			face.corners.erase(std::unique(face.corners.begin(), face.corners.end(),
				[](const MeshCorner& first, const MeshCorner& second) {
					return first.v == second.v;
				}), face.corners.end());
			if (face.corners.size() > 1
				&& face.corners.front().v == face.corners.back().v) {
				face.corners.pop_back();
			}
			if (face.corners.size() < 3)
				continue;
			merged_faces.push_back(std::move(face));
		}
	}
	if (merged_vertices.empty() || merged_faces.empty())
		return false;

	if (m_Face.Orientation() == TopAbs_REVERSED) {
		for (CMesh3D::Face& face : merged_faces)
			std::reverse(face.corners.begin(), face.corners.end());
	}
	if (!pMesh3D->SetGeometry(std::move(merged_vertices),
		std::move(merged_faces), {}, {})) {
		return false;
	}

	// ContourQuadrangulator works on the unbounded supporting UV plane.  With
	// a coarse circular contour it can occasionally close the advancing front
	// by one large quad whose centre lies in a hole (or beyond the outer wire).
	// Reject such faces while vertices still contain UV coordinates; after
	// RestoreTo3DFromUVSurface the same quad would become a visibly floating
	// patch on the infinite continuation of the OCCT surface.
	delete_mesh_faces_outside_occt_face(pMesh3D, this);
	const bool has_active_face = std::any_of(
		pMesh3D->GetFaces().begin(), pMesh3D->GetFaces().end(),
		[](const CMesh3D::Face& face) {
			return !face.deleted && face.corners.size() >= 3;
		});
	if (!has_active_face) {
		pMesh3D->Clear();
		return false;
	}
	if (!pMesh3D->RestoreTo3DFromUVSurface(this)) {
		pMesh3D->Clear();
		return false;
	}
	IsTrimmed = m_TypeMesh != REGULAR_MESH || uv_contours.size() > 1;
	IsInitMesh = true;
	return true;
}

bool CSurfaceFace::CreateLastIslandBoundaryPolylines(
	std::vector<std::unique_ptr<CPolyline>>& boundaries)
{
	boundaries.clear();
	if (m_Face.IsNull() || m_LastIslandBoundariesUV.empty())
		return false;

	boundaries.reserve(m_LastIslandBoundariesUV.size());
	for (const std::vector<CPoint3d>& uv_boundary : m_LastIslandBoundariesUV) {
		if (uv_boundary.size() < 3)
			continue;
		auto line = std::make_unique<CPolyline>();
		for (const CPoint3d& uv : uv_boundary) {
			CPoint8d point;
			if (!GetPoint(uv.x, uv.y, &point)) {
				boundaries.clear();
				return false;
			}
			line->AddPoint(CPoint3d(point.x, point.y, point.z));
		}
		line->SetClosed(true);
		boundaries.push_back(std::move(line));
	}
	return !boundaries.empty();
}

bool CSurfaceFace::CreateLastQuadrangulationBoundaryPolylines(
	std::vector<std::unique_ptr<CPolyline>>& boundaries) const
{
	boundaries.clear();
	boundaries.reserve(m_LastQuadrangulationBoundariesXY.size());
	for (const std::vector<CPoint3d>& xy_boundary
		: m_LastQuadrangulationBoundariesXY) {
		if (xy_boundary.size() < 3)
			continue;
		auto line = std::make_unique<CPolyline>();
		for (const CPoint3d& point : xy_boundary)
			line->AddPoint(point);
		line->SetClosed(true);
		boundaries.push_back(std::move(line));
	}
	return !boundaries.empty();
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
