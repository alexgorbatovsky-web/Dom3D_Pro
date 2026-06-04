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
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <Standard_Failure.hxx>

namespace {
bool apply_shape_transform(TopoDS_Shape& shape, const gp_Trsf& transform)
{
	if (shape.IsNull())
		return false;

	try {
		BRepBuilderAPI_Transform builder(shape, transform, true);
		if (!builder.IsDone())
			return false;
		shape = builder.Shape();
		return !shape.IsNull();
	} catch (const Standard_Failure&) {
		return false;
	}
}
}

//cList<TopoDS_Shape> m_shapes;
//void Step(char* text);

bool IsEqual(double val1, double val2, float delta)
{
	if (fabs(val1 - val2) < delta)
		return true;
	return false;
}


//=======================================


ParametricFunction::ParametricFunction()
{
	m_ID = 0;
	Name = "";
}

ParametricFunction::ParametricFunction(int ID, std::string name)
{
	m_ID = ID;
	Name = name;
}

ParametricFunction::ParametricFunction(const char* name, std::function<void()> onClicki)
{
//	text = name;
//	onClick = onClicki;
	Name = name;
}

int CSolid::prevTime =0;
int CSolid::m_DimensId = 0;
//int CSolid::Oper_ID;
//===================
CSolid* CSolid::pCSolidTool=NULL;
int CSolid::NumReadFile =0;
bool CSolid::DoSmooth = false;


CSolid::CSolid()
{
	Alloc();
}

CSolid::CSolid(TopoDS_Shape& shape)
{
	Alloc();
	m_Shape = shape;
}

CSolid::~CSolid()
{
	Clear();
}

void CSolid::Alloc()
{
/*	Editing = false;
	ptchDensity = 0.25;
	MatrixT.SetIdentity();
	MatrixRot.SetIdentity();
	MatrixScal.SetIdentity();
	MatrixSumm.SetIdentity();
	ScaleKoeff = 1.0;*/

	IsSurfaceInit = false;
	IsInitEdges = false;
//	ptchDensityOld = ptchDensity;
	MeshQuadro = false;
//	Selected = false;
	EditingSolid = NULL;
	Surface_ID = 0;
	FastMesh = false;
	IsEmpty = true;
	SetName("Solid");
	NeedUpdateSculpt = false;
	MeshQuadroX = false;
	DrawNet = false;
	AngDeflection = 0.25;
	m_pSolid = NULL;
	m_IsParametric = false;
	DoWeld = false;
	NumSurfPrint = 0;
	FillMesh = false;
	FastMeshQuadro = false;
	DimensVisible = true;
	BinData = NULL;
}


CSurfaceFace* CSolid::GetSurfaceFace(int indx)
{
	if (indx < 0 || indx >= m_Surfaces.size())
		return NULL;
	return m_Surfaces[indx];
}

const CSurfaceFace* CSolid::GetSurfaceFace(int indx) const
{
	if (indx < 0 || indx >= static_cast<int>(m_Surfaces.size()))
		return NULL;
	return m_Surfaces[static_cast<size_t>(indx)];
}

bool CSolid::HitTestEdgeScreen(DomPoint point,
                               const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                               float tolerance,
                               int& surface_index,
                               int& edge_index) const
{
	bool found = false;
	int best_surface = -1;
	int best_edge = -1;

	for (int i = 0; i < static_cast<int>(m_Surfaces.size()); ++i) {
		const CSurfaceFace* surface = m_Surfaces[static_cast<size_t>(i)];
		if (!surface)
			continue;

		int local_edge = -1;
		if (surface->HitTestEdgeScreen(point, world_to_screen, tolerance, local_edge)) {
			best_surface = i;
			best_edge = local_edge;
			found = true;
		}
	}

	if (found) {
		surface_index = best_surface;
		edge_index = best_edge;
	}
	return found;
}

void CSolid::SetSelectedEdge(int surface_index, int edge_index)
{
	m_SelectedEdges.clear();
	AddSelectedEdge(surface_index, edge_index);
}

void CSolid::AddSelectedEdge(int surface_index, int edge_index)
{
	if (surface_index < 0 || edge_index < 0)
		return;
	const auto edge_ref = std::make_pair(surface_index, edge_index);
	if (std::find(m_SelectedEdges.begin(), m_SelectedEdges.end(), edge_ref) == m_SelectedEdges.end())
		m_SelectedEdges.push_back(edge_ref);
}

void CSolid::RemoveSelectedEdge(int surface_index, int edge_index)
{
	const auto edge_ref = std::make_pair(surface_index, edge_index);
	auto found = std::find(m_SelectedEdges.begin(), m_SelectedEdges.end(), edge_ref);
	if (found != m_SelectedEdges.end())
		m_SelectedEdges.erase(found);
}

void CSolid::ClearSelectedEdge()
{
	m_SelectedEdges.clear();
}

bool CSolid::HasSelectedEdge() const
{
	return !m_SelectedEdges.empty();
}

const TopoDS_Edge* CSolid::GetSelectedTopoEdge() const
{
	if (m_SelectedEdges.empty())
		return nullptr;
	const auto edge_ref = m_SelectedEdges.front();
	const CSurfaceFace* surface = GetSurfaceFace(edge_ref.first);
	return surface ? surface->GetTopoEdge(edge_ref.second) : nullptr;
}

std::vector<TopoDS_Edge> CSolid::GetSelectedTopoEdges() const
{
	std::vector<TopoDS_Edge> edges;
	for (const auto& edge_ref : m_SelectedEdges) {
		const CSurfaceFace* surface = GetSurfaceFace(edge_ref.first);
		const TopoDS_Edge* edge = surface ? surface->GetTopoEdge(edge_ref.second) : nullptr;
		if (edge && !edge->IsNull())
			edges.push_back(*edge);
	}
	return edges;
}

std::vector<TopoDS_Edge> CSolid::GetAllTopoEdges() const
{
	std::vector<TopoDS_Edge> edges;
	for (const CSurfaceFace* surface : m_Surfaces) {
		if (!surface)
			continue;
		for (int edge_index = 0; edge_index < surface->GetEdgeCount(); ++edge_index) {
			const TopoDS_Edge* edge = surface->GetTopoEdge(edge_index);
			if (edge && !edge->IsNull())
				edges.push_back(*edge);
		}
	}
	return edges;
}


//void GetPointFromCurve(TopoDS_Edge& ed, int gtystep, CPolyline* pl);

bool CSolid::InitEdges()
{
	if (!IsSurfaceInit && !InitSurfaces())
		return false;

	bool ok = true;
	for (CSurfaceFace* surface : m_Surfaces) {
		if (surface && !surface->InitEdges()) {
			ok = false;
		}
	}
	IsInitEdges = ok;
	return ok;
}

bool CSolid::InitSurfaces()
{
	for (int i = 0; i < m_Surfaces.size(); i++)
		delete m_Surfaces[i];
	m_Surfaces.clear();
	Surface_ID = 0;
	for (TopExp_Explorer aExpFace(m_Shape, TopAbs_FACE); aExpFace.More(); aExpFace.Next())
	{
		TopoDS_Face curFace = TopoDS::Face(aExpFace.Current());
		CSurfaceFace* surf = new CSurfaceFace(curFace);
		surf->m_ID = Surface_ID++;
		surf->lenEdgeMax = lenEdgeMax;
		m_Surfaces.push_back(surf);
	}
	IsSurfaceInit = true;
	return true;
}

bool CSolid::BuldMesh(float Deflection)
{
	if (!IsSurfaceInit)
		return false;
	if (!MeshQuadro) {
		for (int i = 0; i < m_Surfaces.size(); i++) {
			m_Surfaces[i]->BuldMeshTriangle(Deflection, AngDeflection);
		}		
		return true;
	}

	for (int i = 0; i < m_Surfaces.size(); i++)
		m_Surfaces[i]->TypeGeom = m_TypeGeom;

	// Prepare edges of Surfaces
//	for (int i = 0; i < m_Surfaces.size(); i++)
//		m_Surfaces[i]->PrepareEdges(Deflection);

	return true;
}


void CSolid::Clear()
{
//	for (int i = 0; i < m_Edges.Count(); i++)
//		delete m_Edges[i];
//	m_Edges.Clear();
	for (int i = 0; i < m_Surfaces.size(); i++)
		delete m_Surfaces[i];
	m_Surfaces.clear();
	IsSurfaceInit = false;
	IsInitEdges = false;
}

void CSolid::Render3d(bool selected) const
{
	for (CSurfaceFace* surface : m_Surfaces) {
		if (surface && surface->pMesh3D) {
			surface->pMesh3D->Render3d(selected);
		}
	}
	for (int i = 0; i < static_cast<int>(m_Surfaces.size()); ++i) {
		CSurfaceFace* surface = m_Surfaces[static_cast<size_t>(i)];
		if (surface) {
			std::vector<int> selected_edges;
			for (const auto& edge_ref : m_SelectedEdges) {
				if (edge_ref.first == i)
					selected_edges.push_back(edge_ref.second);
			}
			surface->RenderEdges(selected, selected_edges);
		}
	}
}

void CSolid::Render2d(float center_x, float center_y, float scale) const
{
	for (CSurfaceFace* surface : m_Surfaces) {
		if (surface && surface->pMesh3D) {
			surface->pMesh3D->Render2d(center_x, center_y, scale);
		}
	}
}

bool CSolid::HitTest(CurvePoint point, float tolerance) const
{
	for (CSurfaceFace* surface : m_Surfaces) {
		if (surface && surface->pMesh3D && surface->pMesh3D->HitTest(point, tolerance)) {
			return true;
		}
	}

	return false;
}

bool CSolid::Save(std::ostream& stream) const
{
	stream << "Solid \"" << GetName() << "\" " << m_Surfaces.size() << "\n";
	return static_cast<bool>(stream);
}

void CSolid::Translate(Vec3 delta)
{
	gp_Trsf transform;
	transform.SetTranslation(gp_Vec(delta.x, delta.y, delta.z));
	if (!apply_shape_transform(m_Shape, transform))
		return;
	ClearSelectedEdge();
	InitSurfaces();
	InitEdges();
	BuldMesh(0.1f);
}

void CSolid::Rotate(Vec3 center, Vec3 axis, float angle)
{
	const Vec3 unit_axis = normalize(axis);
	if (std::fabs(angle) <= 0.000001f || dot(unit_axis, unit_axis) <= 0.000001f)
		return;

	gp_Trsf transform;
	transform.SetRotation(
		gp_Ax1(
			gp_Pnt(center.x, center.y, center.z),
			gp_Dir(unit_axis.x, unit_axis.y, unit_axis.z)),
		angle);
	if (!apply_shape_transform(m_Shape, transform))
		return;
	ClearSelectedEdge();
	InitSurfaces();
	InitEdges();
	BuldMesh(0.1f);
}

void CSolid::Scale(Vec3 center, Vec3 axis, float factor)
{
	(void)axis;
	if (factor <= 0.000001f || std::fabs(factor - 1.0f) <= 0.000001f)
		return;

	gp_Trsf transform;
	transform.SetScale(gp_Pnt(center.x, center.y, center.z), factor);
	if (!apply_shape_transform(m_Shape, transform))
		return;
	ClearSelectedEdge();
	InitSurfaces();
	InitEdges();
	BuldMesh(0.1f);
}

bool CSolid::GetBounds(Vec3& min_point, Vec3& max_point) const
{
	if (m_Shape.IsNull()) {
		return false;
	}

	Bnd_Box box;
	BRepBndLib::Add(m_Shape, box);
	if (box.IsVoid()) {
		return false;
	}

	Standard_Real xmin = 0.0;
	Standard_Real ymin = 0.0;
	Standard_Real zmin = 0.0;
	Standard_Real xmax = 0.0;
	Standard_Real ymax = 0.0;
	Standard_Real zmax = 0.0;
	box.Get(xmin, ymin, zmin, xmax, ymax, zmax);
	min_point = {static_cast<float>(xmin), static_cast<float>(ymin), static_cast<float>(zmin)};
	max_point = {static_cast<float>(xmax), static_cast<float>(ymax), static_cast<float>(zmax)};
	return true;
}
