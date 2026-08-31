#pragma once
#include "../CMesh3D.h"
#include "../Net.h"

#include <TopoDS_Shape.hxx>
#include <TopoDS_Edge.hxx>
#include <AIS_Shape.hxx>

#include <functional>
#include <memory>
#include <string>
#include <vector>


class CPolyline;
class CPoint8d;
class CSurface;
class CSplineCurve;
class CVector;
class TopoDS_Face;
class Poly_Triangulation;
class CMesh3D_XL;
class CFreeSurface;
class CSolid;

enum TypeMesh {
	REGULAR_MESH,
	TRIMMED_MESH,
	FILLED_MESH,
	CLUSTERS_MESH,
	FILLED_AND_TRIMMED_MESH,
};

enum TypeGeom {
	COMMON_SURF,
	PLANES_SURF,
	CYLINDERS_SURF,
	CONES_SURF,
	SPHERES_SURF,
	TORUS_SURF,
	REVOLUTION_SURF,
};

struct SurfaceTextureTransform {
	float offset_u = 0.0f;
	float offset_v = 0.0f;
	float scale_u = 1.0f;
	float scale_v = 1.0f;
	float rotation_degrees = 0.0f;
	bool fit_to_surface = false;
};

struct SurfaceMaterialOverride {
	bool enabled = false;
	unsigned long material_id = 0;
	Material material{};
	bool coating_enabled = false;
	unsigned long coating_material_id = 0;
	Material coating_material{};
};

// Builds the visible surface material.  A coating is a shader layer over the
// substrate, not transparent solid geometry: its alpha controls how strongly
// the film colour covers the base while the resulting face keeps the base
// opacity.
Material ComposeSurfaceMaterial(
	const Material& substrate,
	const SurfaceMaterialOverride& override_data);

class CSurfaceFace {
public:
	CSurfaceFace();
	CSurfaceFace(TopoDS_Shape shape);
	void Alloc();
	virtual ~CSurfaceFace();

	bool BuldMeshTriangle(float Deflection, float AngDeflection);
	bool CreateRuled(CSplineCurve* gener, CVector& dir, double dist);
	bool InitEdges();
	bool InitEdges3DCoat();
	bool BuldMesh(float Deflection, bool MeshQuadro);
	bool IsPlanar() const;
	bool IsSpherical() const;
	bool GetCenterAndNormal(Vec3& center, Vec3& normal) const;
	bool GetPoint(double U, double V, CPoint8d* pnt);
	void RenderEdges(const Color& color,
	                 const std::vector<int>& selected_edge_indices = {},
	                 bool draw_regular_edges = true,
	                 bool surface_selected = false) const;
	void RenderContour(const Color& color, bool surface_selected = false) const;
	void RenderOutline(const Color& color,
	                   const std::vector<int>& selected_edge_indices = {},
	                   bool draw_regular_edges = true,
	                   bool surface_selected = false) const;
	void PreviewTranslate(Vec3 delta);
	void PreviewRotate(Vec3 center, Vec3 axis, float angle);
	void PreviewScale(Vec3 center, Vec3 axis, float factor);
	bool CommitPreviewTranslate(Vec3 delta);
	bool CommitPreviewRotate(Vec3 center, Vec3 axis, float angle);
	bool HitTestEdgeScreen(DomPoint point,
	                       const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
	                       float tolerance,
	                       int& edge_index,
	                       float* screen_distance = nullptr) const;
	const TopoDS_Edge* GetTopoEdge(int edge_index) const;
	bool GetEdgeEndpoints(int edge_index, Vec3& start, Vec3& end) const;
	bool GetEdgePolylinePoints(int edge_index, std::vector<Vec3>& points) const;
	int GetEdgeCount() const { return static_cast<int>(m_Edges.size()); }
	void PrepareEdges(float Deflection, bool normalized_quadro_density = false);
	int GetPreparedPolylineCount() const;
	int GetPreparedPolylinePointCount(int edge_index) const;
	bool GetPreparedPolylineEndpoints(int edge_index, Vec3& start, Vec3& end) const;
	bool GetPreparedPolylinePoints(int edge_index, std::vector<CPoint3d>& points) const;
	bool GetPreparedTopoEdge(int edge_index, TopoDS_Edge& edge) const;
	bool SetPreparedPolylinePointCount(int edge_index, int point_count);
	bool SetPreparedPolylinePoints(int edge_index, const std::vector<CPoint3d>& points);
	void UpdateMeshTypeFromBoundary();
	bool GetRegularMeshBoundaryPoints(int edge_index, std::vector<CPoint3d>& points) const;
	void DumpPreparedPolylinesToScene() const;
	bool BuildTrimmingMesh(CSolid* psol, float Deflection);
	bool RemoveOutsideMeshFaces3D();
	bool MakeFilledContour(const std::vector<Vec3>& contour, Vec3 normal,
		CMesh3D* quad_mesh, bool prefer_safe_quads = false,
		std::string* error = nullptr,
		CMesh3D* contour_to_fill_mesh = nullptr,
		std::string* quadrangulator_rejection = nullptr);
	bool BuildFilledMeshWhithHoles(float Deflection,
		bool use_mesh_quadro_hole_slx = false);
	float GetLastLowPolyDensity() const { return m_LastLowPolyDensity; }
	const std::string& GetLastIslandFillError() const {
		return m_LastIslandFillError;
	}
	const std::string& GetLastQuadrangulationDiagnostic() const {
		return m_LastQuadrangulationDiagnostic;
	}
	bool CreateLastIslandBoundaryPolylines(
		std::vector<std::unique_ptr<CPolyline>>& boundaries);
	bool CreateLastQuadrangulationBoundaryPolylines(
		std::vector<std::unique_ptr<CPolyline>>& boundaries) const;
	bool MakeQuadMeshFromBoundary(float density,
	                              CMesh3D* quad_mesh,
	                              std::vector<Vec3>* triangulation_boundary = nullptr);
	void GetEdges(std::vector<CPolyline*>& plines);
	bool IsBoundLine(CPolyline* line);

	CMesh3D* pMesh3D;
	int m_ID;
	float lenEdgeMax;
//	CNet* m_Net;
	TopoDS_Shape m_Face;
	bool IsInitMesh;
	static int m_QtyMin;
	bool IsTrimmed;
	int TypeGeom;
	bool Closed;
	int m_TypeMesh;

	double Umin, Umax;
	double Vmin, Vmax;
	Vec3 Norm0;
	Vec3 m_p0;
	int m_QtyU;
	int m_QtyV;
	bool IsSelected;
	SurfaceTextureTransform TextureTransform;
	SurfaceMaterialOverride MaterialOverride;
	float m_LastLowPolyDensity = 0.0f;
	std::vector<std::vector<CPoint3d>> m_LastIslandBoundariesUV;
	std::vector<std::vector<CPoint3d>> m_LastQuadrangulationBoundariesXY;
	std::string m_LastIslandFillError;
	std::string m_LastQuadrangulationDiagnostic;


protected:
	std::vector<CPolyline*> Polylines;
	std::vector<CSplineCurve*> BoundSpl;
	std::vector<CSplineCurve*> m_Edges;
	std::vector<TopoDS_Edge> m_TopoEdges;
	std::vector<TopoDS_Edge> m_PreparedTopoEdges;
	std::vector<CPolyline*> TrimLine;
	std::vector<CPolyline*> TrimLine2D;
	std::vector<CPolyline*> LinesJoin;
	std::vector<CPolyline*> TrimLine2DCpy;
	CPolyline* m_BoundLine;
	bool IsInitEdges;
	Poly_Triangulation* m_Mesh;
	CNet* m_Net;

	void UpdateRGB();


};



