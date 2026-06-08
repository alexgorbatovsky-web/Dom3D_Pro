#include "SurfaceFace.h"
#include "Solid.h"
#include "SolidTool.h"


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
#include <TopAbs_Orientation.hxx>
#include <TopLoc_Location.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <GCPnts_UniformAbscissa.hxx>
#include <Standard_Failure.hxx>

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
}


CSurfaceFace::~CSurfaceFace()
{
	for (CSplineCurve* edge : m_Edges)
		delete edge;
	m_Edges.clear();
	m_TopoEdges.clear();
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

}

CSurfaceFace::CSurfaceFace(TopoDS_Shape shape)
{
	Alloc();
	m_Face = shape;
}


bool CSurfaceFace::BuldMeshTriangle(float Deflection, float AngDeflection)
{
//	PrepareEdges(Deflection);
	TopLoc_Location aLoc;
	const TopoDS_Face theFace = TopoDS::Face(m_Face);
	const Standard_Real    theLinDeflection = Deflection;
	const Standard_Real    theAngDeflection = AngDeflection;
	BRepMesh_IncrementalMesh bim(theFace, theLinDeflection, false, theAngDeflection, false);
	bim.Perform();
	const Handle(Poly_Triangulation)& aTriangulation = BRep_Tool::Triangulation(theFace, aLoc, Poly_MeshPurpose_NONE);
	if (aTriangulation.IsNull())
		return false;

	std::vector<Vec3> vertices;
	vertices.reserve(static_cast<size_t>(aTriangulation->NbNodes()));
	const gp_Trsf& transform = aLoc.Transformation();
	for (Standard_Integer nodeIndex = 1; nodeIndex <= aTriangulation->NbNodes(); ++nodeIndex) {
		gp_Pnt point = aTriangulation->Node(nodeIndex);
		point.Transform(transform);
		vertices.push_back({
			static_cast<float>(point.X()),
			static_cast<float>(point.Y()),
			static_cast<float>(point.Z())
		});
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
	if (!pMesh3D->SetGeometry(std::move(vertices), std::move(faces))) {
		return false;
	}

	IsInitMesh = true;
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
		return surface.GetType() == GeomAbs_Plane;
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
		if (surface.GetType() != GeomAbs_Plane)
			return false;

		const gp_Dir direction = surface.Plane().Axis().Direction();
		const float sign = face.Orientation() == TopAbs_REVERSED ? -1.0f : 1.0f;
		normal = normalize({static_cast<float>(direction.X()) * sign,
		                    static_cast<float>(direction.Y()) * sign,
		                    static_cast<float>(direction.Z()) * sign});
		return dot(normal, normal) > 0.000001f;
	} catch (const Standard_Failure&) {
		return false;
	}
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

	try {
		const TopoDS_Face face = TopoDS::Face(m_Face);
		for (TopExp_Explorer edge_explorer(face, TopAbs_EDGE); edge_explorer.More(); edge_explorer.Next()) {
			const TopoDS_Edge edge = TopoDS::Edge(edge_explorer.Current());
			if (edge.IsNull())
				continue;

			BRepAdaptor_Curve curve(edge);
			const Standard_Real first = curve.FirstParameter();
			const Standard_Real last = curve.LastParameter();
			if (last <= first)
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
		}
	} catch (const Standard_Failure&) {
		for (CSplineCurve* edge : m_Edges)
			delete edge;
		m_Edges.clear();
		m_TopoEdges.clear();
		return false;
	}

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
