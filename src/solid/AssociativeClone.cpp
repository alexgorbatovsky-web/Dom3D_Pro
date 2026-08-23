#include "AssociativeClone.h"

#include <BRepBuilderAPI_GTransform.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <cmath>

CAssociativeClone::CAssociativeClone() = default;

CAssociativeClone::CAssociativeClone(TopoDS_Shape shape, unsigned long source_id)
    : CSolid(shape), source_id_(source_id) {
}

unsigned long CAssociativeClone::GetSourceId() const { return source_id_; }
void CAssociativeClone::SetSourceId(unsigned long source_id) { source_id_ = source_id; }
const gp_GTrsf& CAssociativeClone::GetPlacement() const { return placement_; }
void CAssociativeClone::SetPlacement(const gp_GTrsf& placement) { placement_ = placement; }

void CAssociativeClone::Prepend(const gp_Trsf& transform) {
    gp_GTrsf affine(transform);
    placement_.PreMultiply(affine);
}

bool CAssociativeClone::RebuildFromSource(const CSolid& source) {
    BRepBuilderAPI_GTransform builder(source.m_Shape, placement_, true);
    if (!builder.IsDone() || builder.Shape().IsNull()) {
        return false;
    }
    Clear();
    m_Shape = builder.Shape();
    InitSurfaces();
    ReBuldMesh();
    return true;
}

std::unique_ptr<CAlfaObject> CAssociativeClone::Clone() const {
    auto copy = std::make_unique<CAssociativeClone>(m_Shape, source_id_);
    copy->placement_ = placement_;
    copy->SetName(GetName() + " Copy");
    copy->SetGroupName(GetGroupName());
    copy->SetVisible(IsVisible());
    copy->SetColor(GetColor());
    copy->SetMaterial(GetMaterial());
    copy->SetMaterialId(GetMaterialId());
    copy->m_LayerID = m_LayerID;
    copy->InitSurfaces();
    copy->EnsureRenderMesh();
    return copy;
}

void CAssociativeClone::Translate(Vec3 delta) {
    gp_Trsf transform;
    transform.SetTranslation(gp_Vec(delta.x, delta.y, delta.z));
    Prepend(transform);
    CSolid::Translate(delta);
}

void CAssociativeClone::Rotate(Vec3 center, Vec3 axis, float angle) {
    const Vec3 unit = normalize(axis);
    if (std::fabs(angle) <= 0.000001f || dot(unit, unit) <= 0.000001f) return;
    gp_Trsf transform;
    transform.SetRotation(gp_Ax1(gp_Pnt(center.x, center.y, center.z),
                                 gp_Dir(unit.x, unit.y, unit.z)), angle);
    Prepend(transform);
    CSolid::Rotate(center, axis, angle);
}

void CAssociativeClone::Scale(Vec3 center, Vec3 axis, float factor) {
    if (dot(axis, axis) <= 0.000001f && factor > 0.000001f) {
        gp_Trsf transform;
        transform.SetScale(gp_Pnt(center.x, center.y, center.z), factor);
        Prepend(transform);
    }
    CSolid::Scale(center, axis, factor);
}

void CAssociativeClone::Mirror(Vec3 plane_point, Vec3 plane_normal) {
    const Vec3 unit = normalize(plane_normal);
    if (dot(unit, unit) <= 0.000001f) return;
    gp_Trsf transform;
    transform.SetMirror(gp_Ax2(gp_Pnt(plane_point.x, plane_point.y, plane_point.z),
                               gp_Dir(unit.x, unit.y, unit.z)));
    Prepend(transform);
    CSolid::Mirror(plane_point, plane_normal);
}
