#pragma once

#include "CAlfaObject.h"
#include "Point3d.h"

#include <string>
#include <functional>
#include <vector>

class CDrawingText final : public CAlfaObject {
public:
    CDrawingText(std::string text,
                 CPoint3d insertion,
                 double height,
                 double rotation_degrees = 0.0,
                 std::string font_family = "Arial");

    const std::string& GetText() const;
    double GetHeight() const;
    double GetRotationDegrees() const;
    const CPoint3d& GetInsertion() const;
    const std::string& GetFontFamily() const;
    double GetLineAdvance() const;
    void SetText(std::string text);
    void SetHeight(double height);
    void SetRotationDegrees(double rotation_degrees);
    void SetInsertion(CPoint3d insertion);
    void SetFontFamily(std::string font_family);

    void Render3d(bool selected) const override;
    void Render2d(float center_x, float center_y, float scale) const override;
    bool HitTest(CurvePoint point, float tolerance) const override;
    bool HitTestScreen(
        DomPoint point,
        const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
        float tolerance) const;
    bool Save(std::ostream& stream) const override;
    std::unique_ptr<CAlfaObject> Clone() const override;
    void Translate(Vec3 delta) override;
    void Rotate(Vec3 center, Vec3 axis, float angle) override;
    void Scale(Vec3 center, Vec3 axis, float factor) override;
    bool GetBounds(Vec3& min_point, Vec3& max_point) const override;

private:
    void EnsureGeometry() const;
    CPoint3d LocalToWorld(double x, double y) const;

    std::string text_;
    CPoint3d insertion_;
    double height_ = 1.0;
    double rotation_degrees_ = 0.0;
    std::string font_family_ = "Arial";
    mutable bool geometry_dirty_ = true;
    mutable std::vector<std::vector<CPoint3d>> contours_;
};
