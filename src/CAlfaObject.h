#pragma once

#include "Common.h"

#include <iosfwd>
#include <cstddef>
#include <string>






class CAlfaObject {
public:
    virtual ~CAlfaObject() = default;

    virtual void Render3d(bool selected) const = 0;
    virtual void Render3d(bool selected, bool has_selected_point, size_t selected_point_index) const;
    virtual void Render2d(float center_x, float center_y, float scale) const = 0;
    virtual bool HitTest(CurvePoint point, float tolerance) const = 0;
    virtual bool Save(std::ostream& stream) const = 0;
    virtual void Translate(Vec3 delta) = 0;
    virtual void Rotate(Vec3 center, Vec3 axis, float angle) = 0;
    virtual void Scale(Vec3 center, Vec3 axis, float factor) = 0;
    virtual bool GetBounds(Vec3& min_point, Vec3& max_point) const = 0;
    virtual void Edit(NativeWindowHandle parent_window);

    const std::string& GetName() const;
    void SetName(std::string name);

    Color GetColor() const;
    void SetColor(Color color);
    Material GetMaterial() const;
    void SetMaterial(Material material);

protected:
    CAlfaObject();
    explicit CAlfaObject(std::string name);

private:
    std::string name_;
    Material material_;
};
