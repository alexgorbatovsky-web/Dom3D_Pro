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
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_GTrsf.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <Standard_Failure.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>

#include <QByteArray>
#include <QDir>
#include <QDomElement>
#include <QFile>
#include <QString>
#include <QTemporaryFile>
#include <QXmlStreamWriter>

#include <limits>
#include <algorithm>
#include <memory>

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

bool apply_shape_transform(TopoDS_Shape& shape, const gp_GTrsf& transform)
{
	if (shape.IsNull())
		return false;

	try {
		BRepBuilderAPI_GTransform builder(shape, transform, true);
		if (!builder.IsDone())
			return false;
		shape = builder.Shape();
		return !shape.IsNull();
	} catch (const Standard_Failure&) {
		return false;
	}
}

std::vector<ParametricParameterValue> transform_parameters(int type,
                                                           Vec3 delta,
                                                           Vec3 center,
                                                           Vec3 axis,
                                                           double angle,
                                                           double factor)
{
	return {
		{"type", static_cast<double>(type)},
		{"dx", delta.x},
		{"dy", delta.y},
		{"dz", delta.z},
		{"center.x", center.x},
		{"center.y", center.y},
		{"center.z", center.z},
		{"axis.x", axis.x},
		{"axis.y", axis.y},
		{"axis.z", axis.z},
		{"angle", angle},
		{"factor", factor}
	};
}

bool point_in_screen_triangle(DomPoint point,
                              DomPoint a,
                              DomPoint b,
                              DomPoint c,
                              float& weight_a,
                              float& weight_b,
                              float& weight_c)
{
	const float px = static_cast<float>(point.x);
	const float py = static_cast<float>(point.y);
	const float ax = static_cast<float>(a.x);
	const float ay = static_cast<float>(a.y);
	const float bx = static_cast<float>(b.x);
	const float by = static_cast<float>(b.y);
	const float cx = static_cast<float>(c.x);
	const float cy = static_cast<float>(c.y);
	const float v0x = bx - ax;
	const float v0y = by - ay;
	const float v1x = cx - ax;
	const float v1y = cy - ay;
	const float v2x = px - ax;
	const float v2y = py - ay;
	const float denominator = v0x * v1y - v1x * v0y;
	if (std::fabs(denominator) <= 0.0001f)
		return false;

	weight_b = (v2x * v1y - v1x * v2y) / denominator;
	weight_c = (v0x * v2y - v2x * v0y) / denominator;
	weight_a = 1.0f - weight_b - weight_c;
	return weight_a >= -0.0001f && weight_b >= -0.0001f && weight_c >= -0.0001f;
}

bool save_solid_step(const CSolid& solid, QByteArray& step_data, QString& error)
{
	if (solid.m_Shape.IsNull()) {
		error = "Solid has no BRep shape.";
		return false;
	}

	QTemporaryFile temp_file(QDir::tempPath() + "/Dom3DProjectSolidXXXXXX.step");
	temp_file.setAutoRemove(true);
	if (!temp_file.open()) {
		error = temp_file.errorString();
		return false;
	}
	const QString temp_path = temp_file.fileName();
	temp_file.close();

	try {
		STEPControl_Writer writer;
		const IFSelect_ReturnStatus transfer_status = writer.Transfer(solid.m_Shape, STEPControl_AsIs);
		if (transfer_status != IFSelect_RetDone) {
			error = "Could not transfer solid shape to STEP.";
			return false;
		}
		const IFSelect_ReturnStatus write_status = writer.Write(temp_path.toLocal8Bit().constData());
		if (write_status != IFSelect_RetDone) {
			error = "Could not write temporary STEP data.";
			return false;
		}
	} catch (const Standard_Failure& failure) {
		error = failure.GetMessageString();
		if (error.isEmpty()) {
			error = "OpenCascade failed while saving solid STEP data.";
		}
		return false;
	}

	QFile step_file(temp_path);
	if (!step_file.open(QIODevice::ReadOnly)) {
		error = step_file.errorString();
		return false;
	}
	step_data = step_file.readAll();
	return !step_data.isEmpty();
}

std::unique_ptr<CSolid> load_solid_step(const QByteArray& step_data, QString& error)
{
	QTemporaryFile temp_file(QDir::tempPath() + "/Dom3DProjectSolidXXXXXX.step");
	temp_file.setAutoRemove(true);
	if (!temp_file.open()) {
		error = temp_file.errorString();
		return {};
	}
	if (temp_file.write(step_data) != step_data.size()) {
		error = temp_file.errorString();
		return {};
	}
	temp_file.flush();

	try {
		STEPControl_Reader reader;
		const IFSelect_ReturnStatus read_status = reader.ReadFile(temp_file.fileName().toLocal8Bit().constData());
		if (read_status != IFSelect_RetDone) {
			error = "Could not read STEP data from XML project.";
			return {};
		}

		const Standard_Integer transferred = reader.TransferRoots();
		if (transferred <= 0) {
			error = "STEP data does not contain transferable shapes.";
			return {};
		}

		TopoDS_Shape shape = reader.OneShape();
		if (shape.IsNull()) {
			error = "STEP data does not contain a valid shape.";
			return {};
		}

		auto solid = std::make_unique<CSolid>(shape);
		solid->InitSurfaces();
		solid->InitEdges();
		solid->BuldMesh(0.1f);
		return solid;
	} catch (const Standard_Failure& failure) {
		error = failure.GetMessageString();
		if (error.isEmpty()) {
			error = "OpenCascade failed while loading solid STEP data.";
		}
		return {};
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
	 m_Tool = nullptr;
	 m_ID = 0;
	 Name = "";
}

ParametricFunction::ParametricFunction(int ID, std::string name)
{
	 m_Tool = nullptr;
	 m_ID = ID;
	 Name = name;
}

ParametricFunction::ParametricFunction(const char* name, std::function<void()> onClicki)
{
//	text = name;
//	onClick = onClicki;
	(void)onClicki;
	 m_Tool = nullptr;
	 m_ID = 0;
	 Name = name;
}

ParametricFunction::ParametricFunction(std::string tool_id, std::string name, std::vector<ParametricParameterValue> parameters)
{
	 m_Tool = nullptr;
	 m_ID = 0;
	 Name = std::move(name);
	 ToolId = std::move(tool_id);
	 Parameters = std::move(parameters);
}
int CSolid::prevTime =0;
int CSolid::m_DimensId = 0;
//int CSolid::Oper_ID;
//===================
CSolid* CSolid::pCSolidTool=NULL;
int CSolid::NumReadFile =0;
bool CSolid::DoSmooth = false;
SolidDisplayMode CSolid::s_DisplayMode = SolidDisplayMode::SurfacesAndEdges;
bool CSolid::s_SurfaceTransparencyEnabled = false;


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
	ClearOperationTree();
	ClearBooleanTools();
	Clear();
}

const ParametricFunction* CSolid::GetOperation(int index) const
{
	if (index < 0 || index >= static_cast<int>(m_OperatonTree.size()))
		return nullptr;
	return m_OperatonTree[static_cast<size_t>(index)];
}

ParametricFunction* CSolid::GetOperation(int index)
{
	if (index < 0 || index >= static_cast<int>(m_OperatonTree.size()))
		return nullptr;
	return m_OperatonTree[static_cast<size_t>(index)];
}

void CSolid::ClearOperationTree()
{
	for (ParametricFunction* operation : m_OperatonTree) {
		delete operation;
	}
	m_OperatonTree.clear();
	ClearParametricDefinition();
}

void CSolid::SetParametricOperation(size_t operation_index,
                                    std::string tool_id,
                                    std::string name,
                                    std::vector<ParametricParameterValue> parameters,
                                    std::vector<int> created_surface_indices)
{
	if (operation_index > m_OperatonTree.size()) {
		operation_index = m_OperatonTree.size();
	}
	ParametricFunction* operation = new ParametricFunction(tool_id, name, parameters);
	operation->CreatedSurfaceIndices = std::move(created_surface_indices);
	if (operation_index == m_OperatonTree.size()) {
		m_OperatonTree.push_back(operation);
	} else {
		delete m_OperatonTree[operation_index];
		m_OperatonTree[operation_index] = operation;
	}
	if (!m_OperatonTree.empty() && m_OperatonTree[0]) {
		SetParametricDefinition(m_OperatonTree[0]->ToolId, m_OperatonTree[0]->Parameters);
	}
}

bool CSolid::RemoveParametricOperation(size_t operation_index)
{
	if (operation_index >= m_OperatonTree.size())
		return false;
	delete m_OperatonTree[operation_index];
	m_OperatonTree.erase(m_OperatonTree.begin() + static_cast<std::vector<ParametricFunction*>::difference_type>(operation_index));
	if (!m_OperatonTree.empty() && m_OperatonTree[0]) {
		SetParametricDefinition(m_OperatonTree[0]->ToolId, m_OperatonTree[0]->Parameters);
	} else {
		ClearParametricDefinition();
	}
	return true;
}
void CSolid::CopyOperationTreeFrom(const CSolid& source)
{
	ClearOperationTree();
	ClearBooleanTools();
	for (const ParametricFunction* operation : source.GetOperationTree()) {
		if (!operation)
			continue;
		SetParametricOperation(m_OperatonTree.size(),
		                      operation->ToolId,
		                      operation->Name,
		                      operation->Parameters,
		                      operation->CreatedSurfaceIndices);
	}
	for (const CSolid* tool : source.m_BooleanTools) {
		if (tool) {
			AddBooleanToolCopy(*tool);
		}
	}
}

size_t CSolid::AddBooleanToolCopy(const CSolid& tool)
{
	auto copy = tool.Clone();
	auto* solid_copy = dynamic_cast<CSolid*>(copy.release());
	if (!solid_copy) {
		return m_BooleanTools.size();
	}
	m_BooleanTools.push_back(solid_copy);
	return m_BooleanTools.size() - 1;
}

size_t CSolid::AddBooleanTool(std::unique_ptr<CSolid> tool)
{
	if (!tool) {
		return m_BooleanTools.size();
	}
	m_BooleanTools.push_back(tool.release());
	return m_BooleanTools.size() - 1;
}

const CSolid* CSolid::GetBooleanTool(size_t tool_index) const
{
	if (tool_index >= m_BooleanTools.size()) {
		return nullptr;
	}
	return m_BooleanTools[tool_index];
}

void CSolid::ClearBooleanTools()
{
	for (CSolid* tool : m_BooleanTools) {
		delete tool;
	}
	m_BooleanTools.clear();
}

SolidDisplayMode CSolid::GetDisplayMode()
{
	return s_DisplayMode;
}

void CSolid::SetDisplayMode(SolidDisplayMode mode)
{
	s_DisplayMode = mode;
}

bool CSolid::IsSurfaceTransparencyEnabled()
{
	return s_SurfaceTransparencyEnabled;
}

void CSolid::SetSurfaceTransparencyEnabled(bool enabled)
{
	s_SurfaceTransparencyEnabled = enabled;
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

bool CSolid::HitTestMeshScreen(DomPoint point,
                               const std::function<bool(Vec3, DomPoint&, float&)>& project_world,
                               float& depth) const
{
	bool found = false;
	float best_depth = std::numeric_limits<float>::max();

	for (const CSurfaceFace* surface : m_Surfaces) {
		if (!surface || !surface->pMesh3D)
			continue;

		const std::vector<Vec3>& vertices = surface->pMesh3D->GetVertices();
		const std::vector<CMesh3D::Face>& faces = surface->pMesh3D->GetFaces();
		for (const CMesh3D::Face& face : faces) {
			if (face.size() < 3)
				continue;

			for (size_t i = 1; i + 1 < face.size(); ++i) {
				const size_t indices[] = {face[0], face[i], face[i + 1]};
				if (indices[0] >= vertices.size() || indices[1] >= vertices.size() || indices[2] >= vertices.size())
					continue;

				DomPoint screen[3]{};
				float vertex_depth[3]{};
				bool projected = true;
				for (int j = 0; j < 3; ++j) {
					if (!project_world(vertices[indices[j]], screen[j], vertex_depth[j])) {
						projected = false;
						break;
					}
				}
				if (!projected)
					continue;

				float weight_a = 0.0f;
				float weight_b = 0.0f;
				float weight_c = 0.0f;
				if (!point_in_screen_triangle(point, screen[0], screen[1], screen[2], weight_a, weight_b, weight_c))
					continue;

				const float hit_depth = weight_a * vertex_depth[0] + weight_b * vertex_depth[1] + weight_c * vertex_depth[2];
				if (hit_depth > 0.0f && hit_depth < best_depth) {
					best_depth = hit_depth;
					found = true;
				}
			}
		}
	}

	if (found) {
		depth = best_depth;
	}
	return found;
}

bool CSolid::HitTestFaceScreen(DomPoint point,
                               const std::function<bool(Vec3, DomPoint&, float&)>& project_world,
                               bool planar_only,
                               int& surface_index,
                               float& depth) const
{
	bool found = false;
	int best_surface = -1;
	float best_depth = std::numeric_limits<float>::max();

	for (int surface_i = 0; surface_i < static_cast<int>(m_Surfaces.size()); ++surface_i) {
		const CSurfaceFace* surface = m_Surfaces[static_cast<size_t>(surface_i)];
		if (!surface || !surface->pMesh3D || (planar_only && !surface->IsPlanar()))
			continue;

		const std::vector<Vec3>& vertices = surface->pMesh3D->GetVertices();
		const std::vector<CMesh3D::Face>& faces = surface->pMesh3D->GetFaces();
		for (const CMesh3D::Face& face : faces) {
			if (face.size() < 3)
				continue;

			for (size_t i = 1; i + 1 < face.size(); ++i) {
				const size_t indices[] = {face[0], face[i], face[i + 1]};
				if (indices[0] >= vertices.size() || indices[1] >= vertices.size() || indices[2] >= vertices.size())
					continue;

				DomPoint screen[3]{};
				float vertex_depth[3]{};
				bool projected = true;
				for (int j = 0; j < 3; ++j) {
					if (!project_world(vertices[indices[j]], screen[j], vertex_depth[j])) {
						projected = false;
						break;
					}
				}
				if (!projected)
					continue;

				float weight_a = 0.0f;
				float weight_b = 0.0f;
				float weight_c = 0.0f;
				if (!point_in_screen_triangle(point, screen[0], screen[1], screen[2], weight_a, weight_b, weight_c))
					continue;

				const float hit_depth = weight_a * vertex_depth[0] + weight_b * vertex_depth[1] + weight_c * vertex_depth[2];
				if (hit_depth > 0.0f && hit_depth < best_depth) {
					best_depth = hit_depth;
					best_surface = surface_i;
					found = true;
				}
			}
		}
	}

	if (found) {
		surface_index = best_surface;
		depth = best_depth;
	}
	return found;
}

bool CSolid::GetFaceCenterAndNormal(int surface_index, Vec3& center, Vec3& normal) const
{
	const CSurfaceFace* surface = GetSurfaceFace(surface_index);
	return surface && surface->GetCenterAndNormal(center, normal);
}

TopoDS_Face CSolid::GetTopoFace(int surface_index) const
{
	const CSurfaceFace* surface = GetSurfaceFace(surface_index);
	if (!surface || surface->m_Face.IsNull())
		return {};
	return TopoDS::Face(surface->m_Face);
}

void CSolid::SetSelectedEdge(int surface_index, int edge_index)
{
	m_SelectedEdges.clear();
	m_SelectedFaceIndex = -1;
	AddSelectedEdge(surface_index, edge_index);
}

void CSolid::AddSelectedEdge(int surface_index, int edge_index)
{
	if (surface_index < 0 || edge_index < 0)
		return;
	m_SelectedFaceIndex = -1;
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

void CSolid::SetSelectedFace(int surface_index)
{
	m_SelectedEdges.clear();
	m_SelectedFaceIndices.clear();
	if (surface_index >= 0)
		m_SelectedFaceIndices.push_back(surface_index);
	m_SelectedFaceIndex = surface_index;
}

void CSolid::AddSelectedFace(int surface_index)
{
	if (surface_index < 0)
		return;
	m_SelectedEdges.clear();
	if (std::find(m_SelectedFaceIndices.begin(), m_SelectedFaceIndices.end(), surface_index) == m_SelectedFaceIndices.end())
		m_SelectedFaceIndices.push_back(surface_index);
	m_SelectedFaceIndex = surface_index;
}

void CSolid::RemoveSelectedFace(int surface_index)
{
	auto existing = std::find(m_SelectedFaceIndices.begin(), m_SelectedFaceIndices.end(), surface_index);
	if (existing == m_SelectedFaceIndices.end())
		return;
	m_SelectedFaceIndices.erase(existing);
	m_SelectedFaceIndex = m_SelectedFaceIndices.empty() ? -1 : m_SelectedFaceIndices.back();
}

void CSolid::ClearSelectedFace()
{
	m_SelectedFaceIndices.clear();
	m_SelectedFaceIndex = -1;
}

bool CSolid::SetSurfaceTextureTransform(int surface_index, const SurfaceTextureTransform& transform)
{
	CSurfaceFace* surface = GetSurfaceFace(surface_index);
	if (!surface)
		return false;
	surface->TextureTransform = transform;
	return true;
}

bool CSolid::SetSelectedSurfaceTextureTransform(const SurfaceTextureTransform& transform)
{
	bool changed = false;
	for (int surface_index : m_SelectedFaceIndices)
		changed = SetSurfaceTextureTransform(surface_index, transform) || changed;
	return changed;
}

std::vector<int> CSolid::FindCreatedSurfaceIndices(const TopoDS_Shape& previous_shape) const
{
	std::vector<TopoDS_Face> previous_faces;
	if (!previous_shape.IsNull()) {
		for (TopExp_Explorer explorer(previous_shape, TopAbs_FACE); explorer.More(); explorer.Next())
			previous_faces.push_back(TopoDS::Face(explorer.Current()));
	}

	std::vector<int> result;
	for (int i = 0; i < GetNumSurfaces(); ++i) {
		const TopoDS_Face current_face = GetTopoFace(i);
		if (current_face.IsNull())
			continue;
		const bool existed = std::any_of(previous_faces.begin(), previous_faces.end(),
			[&current_face](const TopoDS_Face& previous_face) {
				return current_face.IsSame(previous_face);
			});
		if (!existed)
			result.push_back(i);
	}
	return result;
}

void CSolid::SetOperationHighlightedSurfaces(std::vector<int> surface_indices)
{
	m_OperationHighlightedSurfaceIndices = std::move(surface_indices);
}

void CSolid::ClearOperationHighlightedSurfaces()
{
	m_OperationHighlightedSurfaceIndices.clear();
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
	return GetTopoEdgesByRefs(m_SelectedEdges);
}

std::vector<TopoDS_Edge> CSolid::GetTopoEdgesByRefs(const std::vector<std::pair<int, int>>& edge_refs) const
{
	std::vector<TopoDS_Edge> edges;
	for (const auto& edge_ref : edge_refs) {
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
	std::vector<SurfaceTextureTransform> texture_transforms;
	texture_transforms.reserve(m_Surfaces.size());
	for (const CSurfaceFace* surface : m_Surfaces)
		texture_transforms.push_back(surface ? surface->TextureTransform : SurfaceTextureTransform{});

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
		if (static_cast<size_t>(surf->m_ID) < texture_transforms.size())
			surf->TextureTransform = texture_transforms[static_cast<size_t>(surf->m_ID)];
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
		if (m_Shape.IsNull())
			return false;
		try {
			BRepTools::Clean(m_Shape);
			const Standard_Real linear_deflection =
				std::max<Standard_Real>(Deflection, 0.0001);
			const Standard_Real angular_deflection =
				std::clamp<Standard_Real>(AngDeflection, 0.01, 1.0);
			BRepMesh_IncrementalMesh mesher(
				m_Shape,
				linear_deflection,
				false,
				angular_deflection,
				true);
			mesher.Perform();
			if (!mesher.IsDone())
				return false;
		} catch (const Standard_Failure&) {
			return false;
		}

		bool ok = true;
		for (int i = 0; i < m_Surfaces.size(); i++) {
			if (!m_Surfaces[i] || !m_Surfaces[i]->BuldMeshTriangle(Deflection, AngDeflection))
				ok = false;
		}
		return ok;
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
	const SolidDisplayMode mode = GetDisplayMode();
	const MeshDisplayMode mesh_mode = CMesh3D::GetDisplayMode();
	const bool draw_faces = mode == SolidDisplayMode::SurfacesAndEdges || mode == SolidDisplayMode::SurfacesAndRaisedMesh;
	const bool draw_mesh = mode == SolidDisplayMode::MeshOnly || mode == SolidDisplayMode::SurfacesAndRaisedMesh;
	const bool draw_edges = mode != SolidDisplayMode::MeshOnly;
	Material surface_material = GetMaterial();
	if (mesh_mode == MeshDisplayMode::SurfaceGray) {
		surface_material.diffuse = {0.62f, 0.69f, 0.75f};
		surface_material.alpha = 1.0f;
		surface_material.specular = 0.24f;
		surface_material.shininess = 42.0f;
	}
	if (IsSurfaceTransparencyEnabled()) {
		surface_material.alpha = std::min(surface_material.alpha, 0.62f);
	}
	const Color solid_color = surface_material.diffuse;
	const auto material_for_surface = [&surface_material](const CSurfaceFace& surface) {
		Material material = surface_material;
		material.texture_offset_u += surface.TextureTransform.offset_u;
		material.texture_offset_v += surface.TextureTransform.offset_v;
		material.texture_scale_u *= surface.TextureTransform.scale_u;
		material.texture_scale_v *= surface.TextureTransform.scale_v;
		material.texture_rotation_degrees += surface.TextureTransform.rotation_degrees;
		return material;
	};

	if (mode == SolidDisplayMode::Wireframe) {
		for (int i = 0; i < static_cast<int>(m_Surfaces.size()); ++i) {
			CSurfaceFace* surface = m_Surfaces[static_cast<size_t>(i)];
			if (!surface) {
				continue;
			}
			std::vector<int> selected_edges;
			for (const auto& edge_ref : m_SelectedEdges) {
				if (edge_ref.first == i) {
					selected_edges.push_back(edge_ref.second);
				}
			}
			surface->RenderEdges(selected, selected_edges);
		}
		return;
	}

	if (mesh_mode == MeshDisplayMode::Wire) {
		for (CSurfaceFace* surface : m_Surfaces) {
			if (surface && surface->pMesh3D) {
				surface->pMesh3D->RenderWire(selected, true, &solid_color);
			}
		}
		return;
	}

	if (draw_faces) {
		for (CSurfaceFace* surface : m_Surfaces) {
			if (surface && surface->pMesh3D) {
				const Material face_material = material_for_surface(*surface);
				surface->pMesh3D->RenderFaces(selected, draw_edges || mode == SolidDisplayMode::SurfacesAndRaisedMesh, &face_material);
			}
		}
		for (int selected_face_index : m_SelectedFaceIndices) {
			if (selected_face_index < 0 || selected_face_index >= static_cast<int>(m_Surfaces.size()))
				continue;
			CSurfaceFace* surface = m_Surfaces[static_cast<size_t>(selected_face_index)];
			if (surface && surface->pMesh3D) {
				Material selection_material = material_for_surface(*surface);
				selection_material.color_texture_path.clear();
				selection_material.light_texture_path.clear();
				selection_material.bump_texture_path.clear();
				selection_material.alpha = 1.0f;
				surface->pMesh3D->RenderFaces(true, false, &selection_material);
			}
		}
		for (int highlighted_surface_index : m_OperationHighlightedSurfaceIndices) {
			if (highlighted_surface_index < 0 || highlighted_surface_index >= static_cast<int>(m_Surfaces.size()))
				continue;
			CSurfaceFace* surface = m_Surfaces[static_cast<size_t>(highlighted_surface_index)];
			if (surface && surface->pMesh3D) {
				Material highlight_material = material_for_surface(*surface);
				highlight_material.color_texture_path.clear();
				highlight_material.light_texture_path.clear();
				highlight_material.bump_texture_path.clear();
				highlight_material.alpha = 1.0f;
				surface->pMesh3D->RenderFaces(true, false, &highlight_material);
			}
		}
	}

	if (draw_mesh) {
		for (CSurfaceFace* surface : m_Surfaces) {
			if (surface && surface->pMesh3D) {
				surface->pMesh3D->RenderWire(selected, mode == SolidDisplayMode::SurfacesAndRaisedMesh, &solid_color);
			}
		}
	}

	if (!draw_edges)
		return;

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

bool CSolid::Save(QXmlStreamWriter& xml, QString& error) const
{
	QByteArray step_data;
	if (!save_solid_step(*this, step_data, error)) {
		return false;
	}

	xml.writeStartElement("geometry");
	xml.writeAttribute("kind", "brep-step");
	xml.writeAttribute("encoding", "base64");
	xml.writeCharacters(QString::fromLatin1(step_data.toBase64()));
	xml.writeEndElement();
	return true;
}

std::unique_ptr<CSolid> CSolid::Load(const QDomElement& object_element, QString& error)
{
	const QDomElement geometry = object_element.firstChildElement("geometry");
	if (geometry.isNull()) {
		error = "Missing XML element 'geometry'.";
		return {};
	}
	if (geometry.attribute("kind") != "brep-step" || geometry.attribute("encoding") != "base64") {
		error = "Unsupported solid geometry encoding.";
		return {};
	}

	const QByteArray step_data = QByteArray::fromBase64(geometry.text().toLatin1());
	if (step_data.isEmpty()) {
		error = "Solid STEP data is empty.";
		return {};
	}
	return load_solid_step(step_data, error);
}

std::unique_ptr<CAlfaObject> CSolid::Clone() const
{
	TopoDS_Shape shape_copy = m_Shape;
	auto copy = std::make_unique<CSolid>(shape_copy);
	copy->SetName(GetName() + " Copy");
	copy->SetGroupName(GetGroupName());
	copy->SetVisible(IsVisible());
	copy->SetMaterial(GetMaterial());
	copy->SetMaterialId(GetMaterialId());
	copy->SetParametricDefinition(GetParametricToolId(), GetParametricParameters());
	if (GetNumOperations() > 0) {
		copy->CopyOperationTreeFrom(*this);
	}
	copy->InitSurfaces();
	for (int i = 0; i < GetNumSurfaces(); ++i) {
		const CSurfaceFace* source_surface = GetSurfaceFace(i);
		if (source_surface)
			copy->SetSurfaceTextureTransform(i, source_surface->TextureTransform);
	}
	copy->InitEdges();
	copy->BuldMesh(0.1f);
	return copy;
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
	if (!m_OperatonTree.empty()) {
		SetParametricOperation(m_OperatonTree.size(),
		                      "SolidTransform",
		                      "Move",
		                      transform_parameters(0, delta, {}, {}, 0.0, 1.0));
	}
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
	if (!m_OperatonTree.empty()) {
		SetParametricOperation(m_OperatonTree.size(),
		                      "SolidTransform",
		                      "Rotate",
		                      transform_parameters(1, {}, center, unit_axis, angle, 1.0));
	}
}

void CSolid::Scale(Vec3 center, Vec3 axis, float factor)
{
	if (factor <= 0.000001f || std::fabs(factor - 1.0f) <= 0.000001f)
		return;

	const Vec3 unit_axis = normalize(axis);
	if (dot(unit_axis, unit_axis) <= 0.000001f) {
		gp_Trsf transform;
		transform.SetScale(gp_Pnt(center.x, center.y, center.z), factor);
		if (!apply_shape_transform(m_Shape, transform))
			return;
	} else {
		const float k = factor - 1.0f;
		const float m00 = 1.0f + k * unit_axis.x * unit_axis.x;
		const float m01 = k * unit_axis.x * unit_axis.y;
		const float m02 = k * unit_axis.x * unit_axis.z;
		const float m10 = k * unit_axis.y * unit_axis.x;
		const float m11 = 1.0f + k * unit_axis.y * unit_axis.y;
		const float m12 = k * unit_axis.y * unit_axis.z;
		const float m20 = k * unit_axis.z * unit_axis.x;
		const float m21 = k * unit_axis.z * unit_axis.y;
		const float m22 = 1.0f + k * unit_axis.z * unit_axis.z;

		gp_GTrsf transform;
		transform.SetValue(1, 1, m00);
		transform.SetValue(1, 2, m01);
		transform.SetValue(1, 3, m02);
		transform.SetValue(1, 4, center.x - (m00 * center.x + m01 * center.y + m02 * center.z));
		transform.SetValue(2, 1, m10);
		transform.SetValue(2, 2, m11);
		transform.SetValue(2, 3, m12);
		transform.SetValue(2, 4, center.y - (m10 * center.x + m11 * center.y + m12 * center.z));
		transform.SetValue(3, 1, m20);
		transform.SetValue(3, 2, m21);
		transform.SetValue(3, 3, m22);
		transform.SetValue(3, 4, center.z - (m20 * center.x + m21 * center.y + m22 * center.z));
		if (!apply_shape_transform(m_Shape, transform))
			return;
	}

	ClearSelectedEdge();
	InitSurfaces();
	InitEdges();
	BuldMesh(0.1f);
	if (!m_OperatonTree.empty()) {
		SetParametricOperation(m_OperatonTree.size(),
		                      "SolidTransform",
		                      "Scale",
		                      transform_parameters(2, {}, center, unit_axis, 0.0, factor));
	}
}

void CSolid::Mirror(Vec3 plane_point, Vec3 plane_normal)
{
	const Vec3 unit_normal = normalize(plane_normal);
	if (dot(unit_normal, unit_normal) <= 0.000001f)
		return;

	gp_Trsf transform;
	transform.SetMirror(gp_Ax2(
		gp_Pnt(plane_point.x, plane_point.y, plane_point.z),
		gp_Dir(unit_normal.x, unit_normal.y, unit_normal.z)));
	if (!apply_shape_transform(m_Shape, transform))
		return;

	ClearSelectedEdge();
	InitSurfaces();
	InitEdges();
	BuldMesh(0.1f);
	if (!m_OperatonTree.empty()) {
		SetParametricOperation(m_OperatonTree.size(),
		                      "SolidTransform",
		                      "Mirror",
		                      transform_parameters(3, {}, plane_point, unit_normal, 0.0, -1.0));
	}
}

void CSolid::PreviewTranslate(Vec3 delta)
{
	for (CSurfaceFace* surface : m_Surfaces) {
		if (surface)
			surface->PreviewTranslate(delta);
	}
}

void CSolid::PreviewRotate(Vec3 center, Vec3 axis, float angle)
{
	for (CSurfaceFace* surface : m_Surfaces) {
		if (surface)
			surface->PreviewRotate(center, axis, angle);
	}
}

void CSolid::PreviewScale(Vec3 center, Vec3 axis, float factor)
{
	for (CSurfaceFace* surface : m_Surfaces) {
		if (surface)
			surface->PreviewScale(center, axis, factor);
	}
}

bool CSolid::ReverseNormals()
{
	if (m_Shape.IsNull())
		return false;

	m_Shape.Reverse();
	ClearSelectedEdge();
	ClearSelectedFace();
	InitSurfaces();
	InitEdges();
	BuldMesh(0.1f);
	return true;
}

bool CSolid::GetBounds(Vec3& min_point, Vec3& max_point) const
{
	bool has_mesh_bounds = false;
	for (const CSurfaceFace* surface : m_Surfaces) {
		if (!surface || !surface->pMesh3D) {
			continue;
		}

		Vec3 mesh_min{};
		Vec3 mesh_max{};
		if (!surface->pMesh3D->GetBounds(mesh_min, mesh_max)) {
			continue;
		}

		if (!has_mesh_bounds) {
			min_point = mesh_min;
			max_point = mesh_max;
			has_mesh_bounds = true;
		} else {
			min_point.x = std::min(min_point.x, mesh_min.x);
			min_point.y = std::min(min_point.y, mesh_min.y);
			min_point.z = std::min(min_point.z, mesh_min.z);
			max_point.x = std::max(max_point.x, mesh_max.x);
			max_point.y = std::max(max_point.y, mesh_max.y);
			max_point.z = std::max(max_point.z, mesh_max.z);
		}
	}
	if (has_mesh_bounds) {
		return true;
	}

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
