#pragma once
#include "../CMesh3D.h"

#include <TopoDS_Shape.hxx>
#include <TopoDS_Edge.hxx>
#include <AIS_Shape.hxx>

#include <functional>
#include <vector>


class CPolyline;
class CSurface;
class CSplineCurve;
class TopoDS_Face;
class Poly_Triangulation;
class CMesh3D_XL;
class CFreeSurface;

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
};

class CSurfaceFace {
public:
	CSurfaceFace();
	CSurfaceFace(TopoDS_Shape shape);
	void Alloc();
	virtual ~CSurfaceFace();

	bool BuldMeshTriangle(float Deflection, float AngDeflection);
	bool InitEdges();
	bool BuldMesh(float Deflection, bool MeshQuadro);
	bool IsPlanar() const;
	bool GetCenterAndNormal(Vec3& center, Vec3& normal) const;
	void RenderEdges(bool selected, const std::vector<int>& selected_edge_indices = {}) const;
	void PreviewTranslate(Vec3 delta);
	void PreviewRotate(Vec3 center, Vec3 axis, float angle);
	void PreviewScale(Vec3 center, Vec3 axis, float factor);
	bool HitTestEdgeScreen(DomPoint point,
	                       const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
	                       float tolerance,
	                       int& edge_index) const;
	const TopoDS_Edge* GetTopoEdge(int edge_index) const;
	bool GetEdgeEndpoints(int edge_index, Vec3& start, Vec3& end) const;
	int GetEdgeCount() const { return static_cast<int>(m_Edges.size()); }

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


protected:
	std::vector<CPolyline*> Polylines;
	std::vector<CSplineCurve*> BoundSpl;
	std::vector<CSplineCurve*> m_Edges;
	std::vector<TopoDS_Edge> m_TopoEdges;
	std::vector<CPolyline*> TrimLine;
	std::vector<CPolyline*> TrimLine2D;
	std::vector<CPolyline*> LinesJoin;
	std::vector<CPolyline*> TrimLine2DCpy;
	CPolyline* m_BoundLine;
	bool IsInitEdges;
	Poly_Triangulation* m_Mesh;

	void UpdateRGB();
};



