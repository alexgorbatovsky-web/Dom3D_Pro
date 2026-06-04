#pragma once

#include "CAlfaObject.h"

#include <iosfwd>
#include <string>
#include <vector>

class CPolyline : public CAlfaObject {
public:
    CPolyline();
    explicit CPolyline(std::string name);

    const std::vector<CurvePoint>& GetPoints() const;
    std::vector<CurvePoint>& GetPoints();

    bool IsEmpty() const;
    size_t GetPointCount() const;
    void Clear();
    void AddPoint(CurvePoint point);
    bool InsertPoint(size_t index, CurvePoint point);
    bool SetPoint(size_t index, CurvePoint point);
    bool RemovePoint(size_t index);
    bool HitTestPoint(CurvePoint point, float tolerance, size_t& point_index) const;
    void Render3d(bool selected) const override;
    void Render3d(bool selected, bool has_selected_point, size_t selected_point_index) const override;
    void Render2d(float center_x, float center_y, float scale) const override;
    bool HitTest(CurvePoint point, float tolerance) const override;
    void Translate(Vec3 delta) override;
    void Rotate(Vec3 center, Vec3 axis, float angle) override;
    void Scale(Vec3 center, Vec3 axis, float factor) override;
    bool GetBounds(Vec3& min_point, Vec3& max_point) const override;
    void Edit(NativeWindowHandle parent_window) override;

    bool Save(std::ostream& stream) const override;
    bool Load(std::istream& stream);

private:
    float DistanceToSegment(CurvePoint point, CurvePoint start, CurvePoint end) const;
    void DrawPointBox(CurvePoint point, bool selected, bool point_selected) const;

    std::vector<CurvePoint> points_;
};
