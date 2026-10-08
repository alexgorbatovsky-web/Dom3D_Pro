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
    CopyCenterlinesFrom(source);
    TransformCenterlines(placement_);
    InitSurfaces();
    ReBuldMesh();
    return true;
}

std::unique_ptr<CAlfaObject> CAssociativeClone::Clone() const {
    auto copy = std::make_unique<CAssociativeClone>(m_Shape, source_id_);
    copy->placement_ = placement_;
    copy->CopyCenterlinesFrom(*this);
    copy->SetName(GetName() + " Copy");
    copy->SetGroupName(GetGroupName());
    copy->SetFrozen(IsFrozen());
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
    if (factor <= 0.000001f || std::fabs(factor - 1.0f) <= 0.000001f) return;
    const Vec3 unit = normalize(axis);
    const bool uniform = dot(unit, unit) <= 0.000001f;
    const double u[] = {unit.x, unit.y, unit.z};
    const double c[] = {center.x, center.y, center.z};
    gp_GTrsf transform;
    for (int row = 0; row < 3; ++row) {
        double offset = c[row];
        for (int col = 0; col < 3; ++col) {
            const double value = (row == col ? 1.0 : 0.0)
                + (factor - 1.0) * (uniform ? (row == col ? 1.0 : 0.0) : u[row] * u[col]);
            transform.SetValue(row + 1, col + 1, value);
            offset -= value * c[col];
        }
        transform.SetValue(row + 1, 4, offset);
    }
    placement_.PreMultiply(transform);
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

bool CAssociativeClone::CommitPreviewTranslate(Vec3 delta, bool record_operation) {
    if (!CSolid::CommitPreviewTranslate(delta, record_operation)) return false;
    gp_Trsf transform;
    transform.SetTranslation(gp_Vec(delta.x, delta.y, delta.z));
    Prepend(transform);
    return true;
}

bool CAssociativeClone::CommitPreviewRotate(Vec3 center, Vec3 axis, float angle) {
    if (!CSolid::CommitPreviewRotate(center, axis, angle)) return false;
    if (std::fabs(angle) <= 0.000001f) return true;
    const Vec3 unit = normalize(axis);
    gp_Trsf transform;
    transform.SetRotation(gp_Ax1(gp_Pnt(center.x, center.y, center.z),
                                gp_Dir(unit.x, unit.y, unit.z)), angle);
    Prepend(transform);
    return true;
}

bool CAssociativeClone::ApplyAffineTransform(const std::array<double, 16>& matrix) {
    if (!CSolid::ApplyAffineTransform(matrix)) return false;
    gp_GTrsf transform;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 4; ++col)
            transform.SetValue(row + 1, col + 1, matrix[row * 4 + col]);
    placement_.PreMultiply(transform);
    return true;
}

std::unique_ptr<CAssociativeClone> CAssociativeClone::FromSource(const CSolid& source) {
    auto copy = std::make_unique<CAssociativeClone>(source.m_Shape, source.m_id);
    copy->CopyCenterlinesFrom(source);
    copy->SetName(source.GetName() + " Linked Copy");
    copy->SetGroupName(source.GetGroupName());
    copy->SetVisible(source.IsVisible());
    copy->SetColor(source.GetColor());
    copy->SetMaterial(source.GetMaterial());
    copy->SetMaterialId(source.GetMaterialId());
    copy->m_LayerID = source.m_LayerID;
    copy->InitSurfaces();
    copy->ReBuldMesh();
    return copy;
}
