#pragma once

#include "CAlfaObject.h"

#include <iosfwd>
#include <string>
#include <vector>

class CCadCurve3D : public CAlfaObject {
public:
    CCadCurve3D();
    explicit CCadCurve3D(std::string name);
    CCadCurve3D(std::string name, std::vector<Vec3> points);

    const std::vector<Vec3>& GetPoints() const;
    std::vector<Vec3>& GetPoints();
    void SetPoints(std::vector<Vec3> points);
    bool IsEmpty() const;

    void Render3d(bool selected) const override;
    void Render2d(float center_x, float center_y, float scale) const override;
    bool HitTest(CurvePoint point, float tolerance) const override;
    bool Save(std::ostream& stream) const override;
    std::unique_ptr<CAlfaObject> Clone() const override;
    void Translate(Vec3 delta) override;
    void Rotate(Vec3 center, Vec3 axis, float angle) override;
    void Scale(Vec3 center, Vec3 axis, float factor) override;
    bool GetBounds(Vec3& min_point, Vec3& max_point) const override;

private:
    float DistanceToSegmentXZ(CurvePoint point, Vec3 start, Vec3 end) const;

    std::vector<Vec3> points_;
};
