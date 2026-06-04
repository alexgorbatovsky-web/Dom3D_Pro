#pragma once

#include "CAlfaObject.h"
#include "CPolyline.h"

#include <iosfwd>
#include <string>
#include <vector>

class CMesh3D : public CAlfaObject {
public:
    using Face = std::vector<size_t>;

    CMesh3D();
    explicit CMesh3D(std::string name);

    const std::vector<Vec3>& GetVertices() const;
    const std::vector<Face>& GetFaces() const;
    bool SetGeometry(std::vector<Vec3> vertices, std::vector<Face> faces);

    void Render();
    void Render3d(bool selected) const override;
    void Render2d(float center_x, float center_y, float scale) const override;
    bool HitTest(CurvePoint point, float tolerance) const override;
    void Translate(Vec3 delta) override;
    void Rotate(Vec3 center, Vec3 axis, float angle) override;
    void Scale(Vec3 center, Vec3 axis, float factor) override;
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
    std::vector<Face> faces_;
};
