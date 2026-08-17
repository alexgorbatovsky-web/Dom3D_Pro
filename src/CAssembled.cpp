#include "CAssembled.h"

#include "CAlfaDoc.h"
#include "CMesh3D.h"
#include "solid/AssociativeClone.h"
#include "solid/Solid.h"

#include <algorithm>
#include <cmath>
#include <ostream>
#include <set>
#include <utility>

namespace {
using Matrix = CAssembled::TransformMatrix;

Matrix identity_matrix() {
    return {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0};
}

Matrix multiply(const Matrix& left, const Matrix& right) {
    Matrix result{};
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            for (int inner = 0; inner < 4; ++inner) {
                result[static_cast<size_t>(row * 4 + column)] +=
                    left[static_cast<size_t>(row * 4 + inner)]
                    * right[static_cast<size_t>(inner * 4 + column)];
            }
        }
    }
    return result;
}

Matrix translation_matrix(Vec3 delta) {
    Matrix result = identity_matrix();
    result[3] = delta.x;
    result[7] = delta.y;
    result[11] = delta.z;
    return result;
}

Matrix affine_about_center(const Matrix& linear, Vec3 center) {
    Matrix result = linear;
    result[3] = center.x
        - (linear[0] * center.x + linear[1] * center.y + linear[2] * center.z);
    result[7] = center.y
        - (linear[4] * center.x + linear[5] * center.y + linear[6] * center.z);
    result[11] = center.z
        - (linear[8] * center.x + linear[9] * center.y + linear[10] * center.z);
    return result;
}

Matrix rotation_matrix(Vec3 center, Vec3 axis, float angle) {
    const Vec3 n = normalize(axis);
    if (dot(n, n) <= 1.0e-12f || std::fabs(angle) <= 1.0e-9f) {
        return identity_matrix();
    }
    const double c = std::cos(static_cast<double>(angle));
    const double s = std::sin(static_cast<double>(angle));
    const double k = 1.0 - c;
    Matrix result = identity_matrix();
    result[0] = c + k * n.x * n.x;
    result[1] = k * n.x * n.y - s * n.z;
    result[2] = k * n.x * n.z + s * n.y;
    result[4] = k * n.y * n.x + s * n.z;
    result[5] = c + k * n.y * n.y;
    result[6] = k * n.y * n.z - s * n.x;
    result[8] = k * n.z * n.x - s * n.y;
    result[9] = k * n.z * n.y + s * n.x;
    result[10] = c + k * n.z * n.z;
    return affine_about_center(result, center);
}

Matrix scale_matrix(Vec3 center, Vec3 axis, float factor) {
    const Vec3 n = normalize(axis);
    Matrix result = identity_matrix();
    if (dot(n, n) <= 1.0e-12f) {
        result[0] = factor;
        result[5] = factor;
        result[10] = factor;
    } else {
        const double k = static_cast<double>(factor) - 1.0;
        result[0] = 1.0 + k * n.x * n.x;
        result[1] = k * n.x * n.y;
        result[2] = k * n.x * n.z;
        result[4] = k * n.y * n.x;
        result[5] = 1.0 + k * n.y * n.y;
        result[6] = k * n.y * n.z;
        result[8] = k * n.z * n.x;
        result[9] = k * n.z * n.y;
        result[10] = 1.0 + k * n.z * n.z;
    }
    return affine_about_center(result, center);
}

Matrix mirror_matrix(Vec3 point, Vec3 normal) {
    const Vec3 n = normalize(normal);
    if (dot(n, n) <= 1.0e-12f) {
        return identity_matrix();
    }
    Matrix result = identity_matrix();
    result[0] = 1.0 - 2.0 * n.x * n.x;
    result[1] = -2.0 * n.x * n.y;
    result[2] = -2.0 * n.x * n.z;
    result[4] = -2.0 * n.y * n.x;
    result[5] = 1.0 - 2.0 * n.y * n.y;
    result[6] = -2.0 * n.y * n.z;
    result[8] = -2.0 * n.z * n.x;
    result[9] = -2.0 * n.z * n.y;
    result[10] = 1.0 - 2.0 * n.z * n.z;
    return affine_about_center(result, point);
}

gp_GTrsf to_occ_transform(const Matrix& matrix) {
    gp_GTrsf result;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 4; ++column) {
            result.SetValue(
                row + 1,
                column + 1,
                matrix[static_cast<size_t>(row * 4 + column)]);
        }
    }
    return result;
}

struct ClonePlacementSnapshot {
    CAssociativeClone* clone = nullptr;
    gp_GTrsf placement;
};

void collect_group_descendants(const CGroup& group,
                               CAlfaDoc& document,
                               std::set<unsigned long>& descendant_ids,
                               std::vector<CAlfaObject*>& descendants) {
    for (unsigned long id : group.GetElementIds()) {
        if (!descendant_ids.insert(id).second) {
            continue;
        }
        CAlfaObject* object = document.FindObjectById(id);
        if (!object || object == &group) {
            continue;
        }
        descendants.push_back(object);
        if (const auto* child_group = dynamic_cast<const CGroup*>(object)) {
            collect_group_descendants(
                *child_group, document, descendant_ids, descendants);
        }
    }
}

std::vector<ClonePlacementSnapshot> capture_internal_clone_placements(
    const CGroup& group) {
    CAlfaDoc* document = GetAlfaDoc();
    if (!document) {
        return {};
    }

    std::set<unsigned long> descendant_ids;
    std::vector<CAlfaObject*> descendants;
    collect_group_descendants(
        group, *document, descendant_ids, descendants);

    std::vector<ClonePlacementSnapshot> snapshots;
    for (CAlfaObject* object : descendants) {
        auto* clone = dynamic_cast<CAssociativeClone*>(object);
        if (clone
            && descendant_ids.find(clone->GetSourceId())
                != descendant_ids.end()) {
            snapshots.push_back({clone, clone->GetPlacement()});
        }
    }
    return snapshots;
}

void preserve_internal_clone_relationships(
    const std::vector<ClonePlacementSnapshot>& snapshots,
    const Matrix& matrix) {
    const gp_GTrsf transform = to_occ_transform(matrix);
    if (transform.IsSingular()) {
        return;
    }
    const gp_GTrsf inverse = transform.Inverted();
    for (const ClonePlacementSnapshot& snapshot : snapshots) {
        if (!snapshot.clone) {
            continue;
        }
        // Both the source and its clone received the assembly transform T.
        // Their relative placement therefore becomes T * P * inverse(T),
        // rather than T * P (which would apply T twice on the next rebuild).
        gp_GTrsf placement = snapshot.placement;
        placement.PreMultiply(transform);
        placement.Multiply(inverse);
        snapshot.clone->SetPlacement(placement);
    }
}

void apply_matrix_to_object(CAlfaObject* object, const Matrix& matrix) {
    if (!object) {
        return;
    }
    if (auto* assembly = dynamic_cast<CAssembled*>(object)) {
        assembly->SetAssemblyTransform(
            multiply(matrix, assembly->GetAssemblyTransform()));
        CAlfaDoc* document = GetAlfaDoc();
        if (!document) {
            return;
        }
        for (unsigned long id : assembly->GetElementIds()) {
            CAlfaObject* child = document->FindObjectById(id);
            if (child != object) {
                apply_matrix_to_object(child, matrix);
            }
        }
    } else if (auto* group = dynamic_cast<CGroup*>(object)) {
        CAlfaDoc* document = GetAlfaDoc();
        if (!document) {
            return;
        }
        for (unsigned long id : group->GetElementIds()) {
            CAlfaObject* child = document->FindObjectById(id);
            if (child != object) {
                apply_matrix_to_object(child, matrix);
            }
        }
    } else if (auto* solid = dynamic_cast<CSolid*>(object)) {
        solid->ApplyAffineTransform(matrix);
    } else if (auto* mesh = dynamic_cast<CMesh3D*>(object)) {
        mesh->ApplyAffineTransform(matrix);
    }
}
}

CAssembled::CAssembled(std::string name)
    : CGroup(std::move(name)) {
}

CAssembled::CAssembled(std::string name, std::vector<unsigned long> element_ids)
    : CGroup(std::move(name), std::move(element_ids)) {
}

CAssembled::~CAssembled() {
    ClearDimensions();
}

const std::vector<CDimens3D*>& CAssembled::GetDimensions() const {
    return m_dimens;
}

void CAssembled::AddDimension(const CDimens3D& dimension) {
    m_dimens.push_back(new CDimens3D(dimension));
}

void CAssembled::ClearDimensions() {
    for (CDimens3D* dimension : m_dimens) {
        delete dimension;
    }
    m_dimens.clear();
}

std::uint8_t CAssembled::GetDrawParam() const {
    return m_DrawParam;
}

void CAssembled::SetDrawParam(std::uint8_t draw_param) {
    m_DrawParam = draw_param;
}

unsigned long CAssembled::GetIdDim() const {
    return m_IdDim;
}

void CAssembled::SetIdDim(unsigned long id) {
    m_IdDim = id;
}

const CAssembled::TransformMatrix& CAssembled::GetAssemblyTransform() const {
    return m_AssemblyTransform;
}

void CAssembled::SetAssemblyTransform(const TransformMatrix& transform) {
    m_AssemblyTransform = transform;
}

bool CAssembled::HasAssemblyTransform() const {
    const Matrix identity = identity_matrix();
    for (size_t index = 0; index < identity.size(); ++index) {
        if (std::fabs(m_AssemblyTransform[index] - identity[index]) > 1.0e-10) {
            return true;
        }
    }
    return false;
}

void CAssembled::ResetAssemblyTransform() {
    m_AssemblyTransform = identity_matrix();
}

void CAssembled::ApplyStoredTransformToElements() const {
    if (!HasAssemblyTransform()) {
        return;
    }
    CAlfaDoc* document = GetAlfaDoc();
    if (!document) {
        return;
    }
    const auto clone_placements = capture_internal_clone_placements(*this);
    for (unsigned long id : GetElementIds()) {
        CAlfaObject* object = document->FindObjectById(id);
        if (object != this) {
            apply_matrix_to_object(object, m_AssemblyTransform);
        }
    }
    preserve_internal_clone_relationships(
        clone_placements, m_AssemblyTransform);
}

void CAssembled::Translate(Vec3 delta) {
    const Matrix transform = translation_matrix(delta);
    const auto clone_placements = capture_internal_clone_placements(*this);
    CGroup::Translate(delta);
    preserve_internal_clone_relationships(clone_placements, transform);
    m_AssemblyTransform = multiply(transform, m_AssemblyTransform);
}

void CAssembled::Rotate(Vec3 center, Vec3 axis, float angle) {
    const Matrix transform = rotation_matrix(center, axis, angle);
    const auto clone_placements = capture_internal_clone_placements(*this);
    CGroup::Rotate(center, axis, angle);
    preserve_internal_clone_relationships(clone_placements, transform);
    m_AssemblyTransform = multiply(transform, m_AssemblyTransform);
}

void CAssembled::Scale(Vec3 center, Vec3 axis, float factor) {
    const Matrix transform = scale_matrix(center, axis, factor);
    const auto clone_placements = capture_internal_clone_placements(*this);
    CGroup::Scale(center, axis, factor);
    preserve_internal_clone_relationships(clone_placements, transform);
    m_AssemblyTransform = multiply(transform, m_AssemblyTransform);
}

void CAssembled::Mirror(Vec3 plane_point, Vec3 plane_normal) {
    const Matrix transform = mirror_matrix(plane_point, plane_normal);
    const auto clone_placements = capture_internal_clone_placements(*this);
    CGroup::Mirror(plane_point, plane_normal);
    preserve_internal_clone_relationships(clone_placements, transform);
    m_AssemblyTransform = multiply(transform, m_AssemblyTransform);
}

bool CAssembled::CommitTranslate(Vec3 delta) {
    const Matrix transform = translation_matrix(delta);
    const auto clone_placements = capture_internal_clone_placements(*this);
    const bool changed = CGroup::CommitTranslate(delta);
    if (changed) {
        preserve_internal_clone_relationships(clone_placements, transform);
        m_AssemblyTransform = multiply(transform, m_AssemblyTransform);
    }
    return changed;
}

bool CAssembled::CommitRotate(Vec3 center, Vec3 axis, float angle) {
    const Matrix transform = rotation_matrix(center, axis, angle);
    const auto clone_placements = capture_internal_clone_placements(*this);
    const bool changed = CGroup::CommitRotate(center, axis, angle);
    if (changed) {
        preserve_internal_clone_relationships(clone_placements, transform);
        m_AssemblyTransform = multiply(transform, m_AssemblyTransform);
    }
    return changed;
}

bool CAssembled::CommitScale(Vec3 center, Vec3 axis, float factor) {
    const Matrix transform = scale_matrix(center, axis, factor);
    const auto clone_placements = capture_internal_clone_placements(*this);
    const bool changed = CGroup::CommitScale(center, axis, factor);
    if (changed) {
        preserve_internal_clone_relationships(clone_placements, transform);
        m_AssemblyTransform = multiply(transform, m_AssemblyTransform);
    }
    return changed;
}

bool CAssembled::Save(std::ostream& stream) const {
    stream << "Assembly " << m_id << ' ' << GetElementIds().size();
    for (unsigned long id : GetElementIds()) {
        stream << ' ' << id;
    }
    stream << ' ' << static_cast<unsigned int>(m_DrawParam) << ' ' << m_IdDim << '\n';
    return static_cast<bool>(stream);
}

std::unique_ptr<CAlfaObject> CAssembled::Clone() const {
    auto copy = std::make_unique<CAssembled>(GetName() + " Copy", GetElementIds());
    copy->SetGroupName(GetGroupName());
    copy->CAlfaObject::SetVisible(IsVisible());
    copy->CAlfaObject::SetColor(GetColor());
    copy->SetMaterial(GetMaterial());
    copy->SetMaterialId(GetMaterialId());
    copy->m_LayerID = m_LayerID;
    copy->m_DrawParam = m_DrawParam;
    copy->m_IdDim = m_IdDim;
    copy->m_AssemblyTransform = m_AssemblyTransform;
    for (const CDimens3D* dimension : m_dimens) {
        if (dimension) {
            copy->AddDimension(*dimension);
        }
    }
    return copy;
}
