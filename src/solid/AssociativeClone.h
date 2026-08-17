#pragma once

#include "Solid.h"

#include <gp_GTrsf.hxx>

class CAssociativeClone : public CSolid {
public:
    CAssociativeClone();
    CAssociativeClone(TopoDS_Shape shape, unsigned long source_id);

    unsigned long GetSourceId() const;
    void SetSourceId(unsigned long source_id);
    const gp_GTrsf& GetPlacement() const;
    void SetPlacement(const gp_GTrsf& placement);
    bool RebuildFromSource(const CSolid& source);

    std::unique_ptr<CAlfaObject> Clone() const override;
    void Translate(Vec3 delta) override;
    void Rotate(Vec3 center, Vec3 axis, float angle) override;
    void Scale(Vec3 center, Vec3 axis, float factor) override;
    void Mirror(Vec3 plane_point, Vec3 plane_normal) override;

private:
    void Prepend(const gp_Trsf& transform);

    unsigned long source_id_ = 0;
    gp_GTrsf placement_;
};
