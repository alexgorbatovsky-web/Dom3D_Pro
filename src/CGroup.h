#pragma once

#include "CAlfaObject.h"

#include <vector>

class CGroup : public CAlfaObject {
public:
    explicit CGroup(std::string name = "Group");
    CGroup(std::string name, std::vector<unsigned long> element_ids);

    const std::vector<unsigned long>& GetElementIds() const;
    void SetElementIds(std::vector<unsigned long> element_ids);
    bool Contains(unsigned long object_id) const;

    void Render3d(bool selected) const override;
    void Render2d(float center_x, float center_y, float scale) const override;
    bool HitTest(CurvePoint point, float tolerance) const override;
    bool Save(std::ostream& stream) const override;
    std::unique_ptr<CAlfaObject> Clone() const override;
    void Translate(Vec3 delta) override;
    void Rotate(Vec3 center, Vec3 axis, float angle) override;
    void Scale(Vec3 center, Vec3 axis, float factor) override;
    void Mirror(Vec3 plane_point, Vec3 plane_normal) override;
    bool GetBounds(Vec3& min_point, Vec3& max_point) const override;

    void PreviewTranslate(Vec3 delta);
    void PreviewRotate(Vec3 center, Vec3 axis, float angle);
    void PreviewScale(Vec3 center, Vec3 axis, float factor);
    virtual bool CommitTranslate(Vec3 delta);
    virtual bool CommitRotate(Vec3 center, Vec3 axis, float angle);
    virtual bool CommitScale(Vec3 center, Vec3 axis, float factor);

    void SetLayer(unsigned long id);
    void SetVisible(bool visible) override;
    void SetColor(Color color) override;
    void SetColor(unsigned long col_set);

private:
    std::vector<unsigned long> m_Elem;
};
