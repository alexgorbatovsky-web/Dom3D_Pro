#pragma once

#include "CAlfaObject.h"
#include "CPolyline.h"
#include "Line2D.h"
#include <iosfwd>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

class CSurfaceFace;
class CPolyline;

struct Edge {
    int v1;
    int v2;
    Edge() { v1 = v2 = 0; }
    Edge(int vi1, int vi2) { v1 = vi1; v2 = vi2; if (v1 > v2) std::swap(v1, v2); }
    bool operator == (const Edge&) const;
    Edge& operator =(const Edge& ed);
};

inline bool Edge::operator == (const Edge& e2) const {
    if (v1 != e2.v1)
        return false;
    return v2 == e2.v2;
}

inline Edge& Edge::operator = (const Edge& src) {
    if (&src == this)
        return *this;
    v1 = src.v1;
    v2 = src.v2;
    return *this;
}


struct MeshCorner {
    size_t v = 0;
    size_t n = 0;
    size_t uv = 0;
};

struct MeshFace {
    Vec3 normal{};
    std::vector<MeshCorner> corners;
    bool selected = false;
    int id = 0;
    bool deleted = false;
    int sourceFaceId = -1;
	CPoint3d pm;
	int edgeIndex = -1; 
	bool m_Trimmed = false;
    MeshFace() = default;
    MeshFace(std::initializer_list<size_t> vertex_indices);
};
struct TrimFaceData
{
    int FaceID;
    bool m_Trimmed;
    int VariantCut;
    int v1;
    int v2;
    CPoint3d pm;
    int edgeIndex;
    int vertexToMove;
    cVec2 moveTarget;
};
struct IndAndDist {
    int ind;// indx  pLine
    double dist;
    int vertInd; // indx  vertex in mesh
    bool needMove = true; // need to move vertex
    MeshFace* pf;
};
struct DataToMoveVerts {
    IndAndDist IndAndDistArr[2];
    MeshFace* pf;
};

class CMesh3D : public CAlfaObject {
public:
    using Face = MeshFace;

    CMesh3D();
    explicit CMesh3D(std::string name);

    const std::vector<Vec3>& GetVertices() const;
    std::vector<Vec3>& GetVertices();
    const std::vector<Face>& GetFaces() const;
    std::vector<Face>& GetFaces();
    const std::vector<UV>& GetUVs() const;
    const std::vector<Vec3>& GetNormals() const;
    static size_t GetFaceVertexIndex(const Face& face, size_t i);
    static void SetFaceVertexIndex(Face& face, size_t i, size_t v);
    static size_t FaceVertexCount(const Face& face);
    bool PutOnSurface(CSurfaceFace* surface);
    bool RestoreTo3DFromUVSurface(CSurfaceFace* surface);

    bool ExportToObj(const std::string& name) const;
    void Clear();
    bool CreateFromBoundary(CPolyline* bond, float Density);
    bool SetGeometry(std::vector<Vec3> vertices, std::vector<Face> faces);
    bool SetGeometry(std::vector<Vec3> vertices, std::vector<Face> faces, std::vector<UV> uvs);
    bool SetGeometry(std::vector<Vec3> vertices,
                     std::vector<Face> faces,
                     std::vector<UV> uvs,
                     std::vector<Vec3> normals);
    void GeneratePlanarUVs();

    void Render();
    void Render3d(bool selected) const override;
    void RenderFaces(bool selected, bool offset_fill = false, const Material* material_override = nullptr) const;
    void RenderWire(bool selected, bool draw_on_top = false, const Color* color_override = nullptr) const;
    void Render2d(float center_x, float center_y, float scale) const override;
    static Material material_Defailt;
    static float GetWireOpacity();
    static void SetWireOpacity(float opacity);
    static MeshDisplayMode GetDisplayMode();
    static void SetDisplayMode(MeshDisplayMode mode);
    bool HitTest(CurvePoint point, float tolerance) const override;
    bool HitTestMeshScreen(DomPoint point,
                           const std::function<bool(Vec3, DomPoint&, float&)>& project_world,
                           float& depth) const;
    std::unique_ptr<CAlfaObject> Clone() const override;
    void Translate(Vec3 delta) override;
    void Rotate(Vec3 center, Vec3 axis, float angle) override;
    void Scale(Vec3 center, Vec3 axis, float factor) override;
    void Mirror(Vec3 plane_point, Vec3 plane_normal) override;
    bool GetBounds(Vec3& min_point, Vec3& max_point) const override;
    bool Save(std::ostream& stream) const override;
    bool Load(std::istream& stream);
    bool Create(CPolyline* pline, CVector3d dir, float dist);
    bool TrimByPline(CPolyline* pLine, CPoint3d pc);
    bool TrimByPlineTest(CPolyline* pLine, CPoint3d pc);
    bool KeepConnectedComponentAt(CPoint3d pc);
    bool SplitFaceByPoint(int face_index, int ind1, int ind2, const cVec2& pm);
    bool SplitFaceByPointVar4(int face_index, int ind1, int ind2, const cVec2& pm);
    bool SplitFaceByPointVar3(int face_index, int ind1, int ind2, const cVec2& pm);
    int MakeFace(std::vector < size_t> indV);
    bool PrepareAndMoveVertexToTrimLine(CPolyline* pLine, std::vector<DataToMoveVerts*>& Data);
    bool FindVertexToMove(CPolyline* pLine, DataToMoveVerts* data);
    bool SplitFaceByVar5(int face_index, int v1, int edgeIndex, cVec2& pm);
    bool SplitFaceByVar6(int face_index, int v1, int edgeIndex, cVec2& pm);
    bool SplitFaceByVar7(int face_index, int v1, int edgeIndex);
    bool SplitFaceByVar8(int face_index, int vertexToMove, cVec2 moveTarget);
    int FindFirstFace3d(Edge ed);
    int FindSecondCFace3d(int first_face_index, Edge ed);

private:
    bool IsValidFace(const Face& face, size_t vertex_count) const;
    Vec3 FaceNormal(const Face& face) const;
    bool PointInFace2d(CurvePoint point, const Face& face) const;
    float DistanceToSegment2d(CurvePoint point, Vec3 start, Vec3 end) const;

    std::vector<Vec3> vertices_;
    std::vector<UV> uvs_;
    std::vector<Vec3> normals_;
    std::vector<Face> faces_;
    static float s_WireOpacity;
    static MeshDisplayMode s_DisplayMode;
};
