#pragma once
#include "../CAlfaObject.h"

#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <AIS_Shape.hxx>

#include "SurfaceFace.h"

#include <functional>
#include <iosfwd>
#include <string>
#include <utility>
#include <vector>

class QDomElement;
class QString;
class QXmlStreamWriter;
class CPolyline;
class CSurface;
class CSplineCurve;
class TopoDS_Face;
class Poly_Triangulation;
class CMesh3D_XL;

enum TypeParamsFunction {
	FUNCT_SOLID_BOX,
	FUNCT_SOLID_CYLINDER,
	FUNCT_SOLID_BOOLEAN,
	FUNC_SOLID_FILLET,
	FUNC_SOLID_CHAMFER,
	FUNC_SOLID_SPHERE,
	FUNC_SOLID_TORUS,
	FUNC_SOLID_SEWING_FACE,
	FUNC_SOLID_THICK,
	FUNC_SOLID_PRISM,
	FUNC_SOLID_EXTRUDE,
	FUNC_SOLID_SWEPT,
	FUNC_SOLID_TRANSFORM,
	FUNC_SOLID_DRAFT,

};

class SolidTool;

class ParametricFunction 
{
public:
	SolidTool* m_Tool;//Operation
	int m_ID;
	std::string Name;
	std::string ToolId;
	std::vector<ParametricParameterValue> Parameters;
	std::vector<int> CreatedSurfaceIndices;
	ParametricFunction();
	ParametricFunction(int ID, std::string name);
	ParametricFunction(const char* text, std::function<void()> onClick = nullptr);
	ParametricFunction(std::string tool_id, std::string name, std::vector<ParametricParameterValue> parameters);

//	SERIALIZE_LATER();
};

class CDimens;
class CSolid :public CAlfaObject {

//	cList<CPolyline*> m_Edges;
	Poly_Triangulation* m_Mesh;
	std::vector<CSurfaceFace*> m_Surfaces;
	std::vector<ParametricFunction*> m_OperatonTree;
	std::vector<CSolid*> m_BooleanTools;//tools used for this solid


public:
	virtual const char* GetID() { return "CSolid"; }
	virtual const char* GetHint() { return "CSolid_HINT"; }
	virtual bool			PresentInRetopoTools() { return true; }
	virtual const char* GetPrevTool() { return "SpineTool"; }//applies only for uv/retopo rooms
	virtual bool			NeedBrushMipmaps() { return false; }
	virtual bool			PickAveragePos() { return false; }
	virtual bool			PickCurrentPos() { return false; }
	virtual bool			PickEmptySpace() { return true; }
	virtual bool AllowRadisRMBControl() { return false; }

	static SolidDisplayMode GetDisplayMode();
	static void SetDisplayMode(SolidDisplayMode mode);
	static bool IsSurfaceTransparencyEnabled();
	static void SetSurfaceTransparencyEnabled(bool enabled);

	void Render3d(bool selected) const override;
	void Render2d(float center_x, float center_y, float scale) const override;
	bool HitTest(CurvePoint point, float tolerance) const override;
	bool Save(std::ostream& stream) const override;
	bool Save(QXmlStreamWriter& xml, QString& error) const;
	static std::unique_ptr<CSolid> Load(const QDomElement& object_element, QString& error);
	std::unique_ptr<CAlfaObject> Clone() const override;
	void Translate(Vec3 delta) override;
	void Rotate(Vec3 center, Vec3 axis, float angle) override;
	void Scale(Vec3 center, Vec3 axis, float factor) override;
	void Mirror(Vec3 plane_point, Vec3 plane_normal) override;
	void PreviewTranslate(Vec3 delta);
	void PreviewRotate(Vec3 center, Vec3 axis, float angle);
	void PreviewScale(Vec3 center, Vec3 axis, float factor);
	bool ReverseNormals();
	bool GetBounds(Vec3& min_point, Vec3& max_point) const override;

	CSolid();
	CSolid(TopoDS_Shape& shape);
	void Alloc();
	virtual ~CSolid();
	void Clear();
	int GetNumOperations() const { return static_cast<int>(m_OperatonTree.size()); }
	const ParametricFunction* GetOperation(int index) const;
	ParametricFunction* GetOperation(int index);
	const std::vector<ParametricFunction*>& GetOperationTree() const { return m_OperatonTree; }
	void ClearOperationTree();
	void SetParametricOperation(size_t operation_index,
	                            std::string tool_id,
	                            std::string name,
	                            std::vector<ParametricParameterValue> parameters,
	                            std::vector<int> created_surface_indices = {});
	bool RemoveParametricOperation(size_t operation_index);
	void CopyOperationTreeFrom(const CSolid& source);
	size_t AddBooleanToolCopy(const CSolid& tool);
	size_t AddBooleanTool(std::unique_ptr<CSolid> tool);
	const CSolid* GetBooleanTool(size_t tool_index) const;
	size_t GetBooleanToolCount() const { return m_BooleanTools.size(); }
	void ClearBooleanTools();

	int GetNumSurfaces() const { return static_cast<int>(m_Surfaces.size()); }
	CSurfaceFace* GetSurfaceFace(int indx);
	const CSurfaceFace* GetSurfaceFace(int indx) const;
	bool HitTestEdgeScreen(DomPoint point,
	                       const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
	                       float tolerance,
	                       int& surface_index,
	                       int& edge_index) const;
	bool HitTestMeshScreen(DomPoint point,
	                       const std::function<bool(Vec3, DomPoint&, float&)>& project_world,
	                       float& depth) const;
	bool HitTestFaceScreen(DomPoint point,
	                       const std::function<bool(Vec3, DomPoint&, float&)>& project_world,
	                       bool planar_only,
	                       int& surface_index,
	                       float& depth) const;
	bool GetFaceCenterAndNormal(int surface_index, Vec3& center, Vec3& normal) const;
	TopoDS_Face GetTopoFace(int surface_index) const;
	void SetSelectedEdge(int surface_index, int edge_index);
	void AddSelectedEdge(int surface_index, int edge_index);
	void RemoveSelectedEdge(int surface_index, int edge_index);
	void ClearSelectedEdge();
	void SetSelectedFace(int surface_index);
	void AddSelectedFace(int surface_index);
	void RemoveSelectedFace(int surface_index);
	void ClearSelectedFace();
	bool HasSelectedFace() const { return m_SelectedFaceIndex >= 0; }
	int GetSelectedFaceIndex() const { return m_SelectedFaceIndex; }
	const std::vector<int>& GetSelectedFaceIndices() const { return m_SelectedFaceIndices; }
	bool SetSurfaceTextureTransform(int surface_index, const SurfaceTextureTransform& transform);
	bool SetSelectedSurfaceTextureTransform(const SurfaceTextureTransform& transform);
	std::vector<int> FindCreatedSurfaceIndices(const TopoDS_Shape& previous_shape) const;
	void SetOperationHighlightedSurfaces(std::vector<int> surface_indices);
	void ClearOperationHighlightedSurfaces();
	const std::vector<std::pair<int, int>>& GetSelectedEdgeRefs() const { return m_SelectedEdges; }
	bool HasSelectedEdge() const;
	int GetSelectedSurfaceIndex() const { return m_SelectedEdges.empty() ? -1 : m_SelectedEdges.front().first; }
	int GetSelectedEdgeIndex() const { return m_SelectedEdges.empty() ? -1 : m_SelectedEdges.front().second; }
	const TopoDS_Edge* GetSelectedTopoEdge() const;
	std::vector<TopoDS_Edge> GetSelectedTopoEdges() const;
	std::vector<TopoDS_Edge> GetTopoEdgesByRefs(const std::vector<std::pair<int, int>>& edge_refs) const;
	std::vector<TopoDS_Edge> GetAllTopoEdges() const;

	static CSolid* pCSolidTool;
	CSolid* instance() { return pCSolidTool; }
	bool InitEdges();
	bool InitSurfaces();
	bool BuldMesh(float Deflection);



	bool IsInitEdges;
	TopoDS_Shape m_Shape;
	int Surface_ID;

	bool IsSurfaceInit;
	float lenEdgeMax;
	bool DrawNet;
	bool MeshQuadro;
	bool MeshQuadroOld;
	int m_TypeGeom;

	bool FastMesh;
	bool FillMesh;
	bool FastMeshQuadro;
//	HashSummator OldHash;
	char* BinData;
	int LenData;
	bool IsEmpty;
	bool NeedUpdateSculpt;
static	int NumReadFile;
	static SolidDisplayMode s_DisplayMode;
	static bool s_SurfaceTransparencyEnabled;
	bool MeshQuadroX;
	float AngDeflection;
	bool m_IsParametric;
	float ptchDensityOld;
	CSolid* EditingSolid;
	CSolid* m_pSolid;
	static int prevTime;
//	Matrix4D MatrixSumm;
	bool DoWeld;
	static bool DoSmooth;
	int NumSurfPrint;
	int QtySurf;
	bool DimensVisible;
	static int m_DimensId;
//	cList <CDimens*> m_Dimens;

	std::vector<std::pair<int, int>> m_SelectedEdges;
	std::vector<int> m_SelectedFaceIndices;
	std::vector<int> m_OperationHighlightedSurfaceIndices;
	int m_SelectedFaceIndex = -1;

//	SERIALIZE_LATER();

private:
/*
	bool SaveTopoDS_ShapeToStepFile();
	bool LoadTopoDS_ShapeFromStepFile();
	virtual void LoadData(int ChunkIdx, BinStream& BS);
	virtual void SaveData(int ChunkIdx, BinStream& BS);
	virtual int GetNumSaveChunks() { return 1; }
	virtual DWORD GetSaveMagic(int ChunkIdx) { return MakeMagic("SLD0"); }
	void PrepareClastersSurfs();
	void PrintInfoSurf();
	void FindRingPatches();
	void AnalizRingPatch(cList<RingPatch>& rp);
	void AnalizRingPatches();*/
};


