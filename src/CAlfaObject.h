#pragma once

#include "Common.h"
#include "Material.h"

#include <iosfwd>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>


struct ParametricParameterValue {
    std::string id;
    double value = 0.0;
};

class CAlfaObject {
public:
    virtual ~CAlfaObject() = default;

    unsigned long m_col = 0;
    bool m_selected = false;
	unsigned long m_id = 0;

    virtual void Render3d(bool selected) const = 0;
    virtual void Render3d(bool selected, bool has_selected_point, size_t selected_point_index) const;
    virtual void Render2d(float center_x, float center_y, float scale) const = 0;
    virtual bool HitTest(CurvePoint point, float tolerance) const = 0;
    virtual bool Save(std::ostream& stream) const = 0;
    virtual std::unique_ptr<CAlfaObject> Clone() const;
    virtual void Translate(Vec3 delta) = 0;
    virtual void Rotate(Vec3 center, Vec3 axis, float angle) = 0;
    virtual void Scale(Vec3 center, Vec3 axis, float factor) = 0;
    virtual void Mirror(Vec3 plane_point, Vec3 plane_normal);
    virtual bool GetBounds(Vec3& min_point, Vec3& max_point) const = 0;
    virtual void Edit(NativeWindowHandle parent_window);

    const std::string& GetName() const;
    void SetName(std::string name);
    const std::string& GetGroupName() const;
    void SetGroupName(std::string group_name);
    bool IsVisible() const;
    void SetVisible(bool visible);

    Color GetColor() const;
    void SetColor(Color color);
    Material GetMaterial() const;
    void SetMaterial(Material material);
    unsigned long GetMaterialId() const;
    void SetMaterialId(unsigned long id);
    bool IsParametric() const;
    const std::string& GetParametricToolId() const;
    const std::vector<ParametricParameterValue>& GetParametricParameters() const;
    void SetParametricDefinition(std::string tool_id, std::vector<ParametricParameterValue> parameters);
    void ClearParametricDefinition();

protected:
    CAlfaObject();
    explicit CAlfaObject(std::string name);

private:
    std::string name_;
    std::string group_name_;
    Material material_;
    unsigned long material_id_ = 0;
    bool visible_ = true;
    std::string parametric_tool_id_;
    std::vector<ParametricParameterValue> parametric_parameters_;
};
