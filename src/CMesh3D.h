#pragma once

#include "CAlfaObject.h"
#include "CPolyline.h"

#include <iosfwd>
#include <functional>
#include <string>
#include <vector>

class CMesh3D : public CAlfaObject {
public:
    using Face = std::vector<size_t>;

    CMesh3D();
    explicit CMesh3D(std::string name);

    const std::vector<Vec3>& GetVertices() const;
    const std::vector<Face>& GetFaces() const;
    const std::vector<UV>& GetUVs() const;
    const std::vector<Vec3>& GetNormals() const;
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

private:
    void Clear();
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
