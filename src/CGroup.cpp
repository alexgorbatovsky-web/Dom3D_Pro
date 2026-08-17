#include "CGroup.h"

#include "CAlfaDoc.h"
#include "solid/Solid.h"

#include <algorithm>
#include <limits>
#include <ostream>

namespace {
CAlfaObject* group_element(unsigned long id) {
    CAlfaDoc* document = GetAlfaDoc();
    return document ? document->FindObjectById(id) : nullptr;
}

const CAlfaObject* const_group_element(unsigned long id) {
    const CAlfaDoc* document = GetAlfaDoc();
    return document ? document->FindObjectById(id) : nullptr;
}

bool group_is_registered(const CGroup* group) {
    const CAlfaDoc* document = GetAlfaDoc();
    return document && group && group->m_id != 0
        && document->FindObjectById(group->m_id) == group;
}
}

CGroup::CGroup(std::string name)
    : CAlfaObject(std::move(name)) {
}

CGroup::CGroup(std::string name, std::vector<unsigned long> element_ids)
    : CAlfaObject(std::move(name)), m_Elem(std::move(element_ids)) {
    std::sort(m_Elem.begin(), m_Elem.end());
    m_Elem.erase(std::remove(m_Elem.begin(), m_Elem.end(), 0UL), m_Elem.end());
    m_Elem.erase(std::unique(m_Elem.begin(), m_Elem.end()), m_Elem.end());
}

const std::vector<unsigned long>& CGroup::GetElementIds() const {
    return m_Elem;
}

void CGroup::SetElementIds(std::vector<unsigned long> element_ids) {
    m_Elem = std::move(element_ids);
    std::sort(m_Elem.begin(), m_Elem.end());
    m_Elem.erase(std::remove(m_Elem.begin(), m_Elem.end(), 0UL), m_Elem.end());
    m_Elem.erase(std::unique(m_Elem.begin(), m_Elem.end()), m_Elem.end());
}

bool CGroup::Contains(unsigned long object_id) const {
    return std::find(m_Elem.begin(), m_Elem.end(), object_id) != m_Elem.end();
}

void CGroup::Render3d(bool selected) const {
    if (!selected) {
        return;
    }
    for (unsigned long id : m_Elem) {
        const CAlfaObject* object = const_group_element(id);
        if (object && object != this && object->IsVisible()) {
            const CAlfaDoc* document = GetAlfaDoc();
            const size_t index = document
                ? document->FindObjectIndexById(id)
                : static_cast<size_t>(-1);
            if (document && document->IsObjectSelectionHighlighted(index)) {
                continue;
            }
            object->Render3d(true);
        }
    }
}

void CGroup::Render2d(float center_x, float center_y, float scale) const {
    (void)center_x;
    (void)center_y;
    (void)scale;
}

bool CGroup::HitTest(CurvePoint point, float tolerance) const {
    for (auto it = m_Elem.rbegin(); it != m_Elem.rend(); ++it) {
        const CAlfaObject* object = const_group_element(*it);
        if (object && object != this && object->IsVisible() && object->HitTest(point, tolerance)) {
            return true;
        }
    }
    return false;
}

bool CGroup::Save(std::ostream& stream) const {
    stream << "Group " << m_id << ' ' << m_Elem.size();
    for (unsigned long id : m_Elem) {
        stream << ' ' << id;
    }
    stream << '\n';
    return static_cast<bool>(stream);
}

std::unique_ptr<CAlfaObject> CGroup::Clone() const {
    auto copy = std::make_unique<CGroup>(GetName() + " Copy", m_Elem);
    copy->SetGroupName(GetGroupName());
    copy->SetVisible(IsVisible());
    copy->CAlfaObject::SetColor(GetColor());
    copy->SetMaterial(GetMaterial());
    copy->SetMaterialId(GetMaterialId());
    copy->m_LayerID = m_LayerID;
    return copy;
}

void CGroup::Translate(Vec3 delta) {
    for (unsigned long id : m_Elem) {
        CAlfaObject* object = group_element(id);
        if (object && object != this) {
            object->Translate(delta);
        }
    }
}

void CGroup::Rotate(Vec3 center, Vec3 axis, float angle) {
    for (unsigned long id : m_Elem) {
        CAlfaObject* object = group_element(id);
        if (object && object != this) {
            object->Rotate(center, axis, angle);
        }
    }
}

void CGroup::Scale(Vec3 center, Vec3 axis, float factor) {
    for (unsigned long id : m_Elem) {
        CAlfaObject* object = group_element(id);
        if (object && object != this) {
            object->Scale(center, axis, factor);
        }
    }
}

void CGroup::Mirror(Vec3 plane_point, Vec3 plane_normal) {
    for (unsigned long id : m_Elem) {
        CAlfaObject* object = group_element(id);
        if (object && object != this) {
            object->Mirror(plane_point, plane_normal);
        }
    }
}

bool CGroup::GetBounds(Vec3& min_point, Vec3& max_point) const {
    bool found = false;
    min_point = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()};
    max_point = {
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()};
    for (unsigned long id : m_Elem) {
        const CAlfaObject* object = const_group_element(id);
        Vec3 child_min{};
        Vec3 child_max{};
        if (!object || object == this || !object->GetBounds(child_min, child_max)) {
            continue;
        }
        min_point.x = std::min(min_point.x, child_min.x);
        min_point.y = std::min(min_point.y, child_min.y);
        min_point.z = std::min(min_point.z, child_min.z);
        max_point.x = std::max(max_point.x, child_max.x);
        max_point.y = std::max(max_point.y, child_max.y);
        max_point.z = std::max(max_point.z, child_max.z);
        found = true;
    }
    return found;
}

void CGroup::PreviewTranslate(Vec3 delta) {
    for (unsigned long id : m_Elem) {
        CAlfaObject* object = group_element(id);
        if (auto* group = dynamic_cast<CGroup*>(object); group && group != this) {
            group->PreviewTranslate(delta);
        } else if (auto* solid = dynamic_cast<CSolid*>(object)) {
            solid->PreviewTranslate(delta);
        } else if (object && object != this) {
            object->Translate(delta);
        }
    }
}

void CGroup::PreviewRotate(Vec3 center, Vec3 axis, float angle) {
    for (unsigned long id : m_Elem) {
        CAlfaObject* object = group_element(id);
        if (auto* group = dynamic_cast<CGroup*>(object); group && group != this) {
            group->PreviewRotate(center, axis, angle);
        } else if (auto* solid = dynamic_cast<CSolid*>(object)) {
            solid->PreviewRotate(center, axis, angle);
        } else if (object && object != this) {
            object->Rotate(center, axis, angle);
        }
    }
}

void CGroup::PreviewScale(Vec3 center, Vec3 axis, float factor) {
    for (unsigned long id : m_Elem) {
        CAlfaObject* object = group_element(id);
        if (auto* group = dynamic_cast<CGroup*>(object); group && group != this) {
            group->PreviewScale(center, axis, factor);
        } else if (auto* solid = dynamic_cast<CSolid*>(object)) {
            solid->PreviewScale(center, axis, factor);
        } else if (object && object != this) {
            object->Scale(center, axis, factor);
        }
    }
}

bool CGroup::CommitTranslate(Vec3 delta) {
    bool changed = false;
    for (unsigned long id : m_Elem) {
        CAlfaObject* object = group_element(id);
        if (auto* group = dynamic_cast<CGroup*>(object); group && group != this) {
            changed = group->CommitTranslate(delta) || changed;
        } else if (auto* solid = dynamic_cast<CSolid*>(object)) {
            changed = solid->CommitPreviewTranslate(delta) || changed;
        } else if (object && object != this) {
            // Non-solid children are transformed during preview because they
            // do not keep a separate display-only shape. Mark the commit so
            // an assembly can persist the same matrix without applying it twice.
            changed = true;
        }
    }
    return changed;
}

bool CGroup::CommitRotate(Vec3 center, Vec3 axis, float angle) {
    bool changed = false;
    for (unsigned long id : m_Elem) {
        CAlfaObject* object = group_element(id);
        if (auto* group = dynamic_cast<CGroup*>(object); group && group != this) {
            changed = group->CommitRotate(center, axis, angle) || changed;
        } else if (auto* solid = dynamic_cast<CSolid*>(object)) {
            solid->Rotate(center, axis, angle);
            changed = true;
        } else if (object && object != this) {
            changed = true;
        }
    }
    return changed;
}

bool CGroup::CommitScale(Vec3 center, Vec3 axis, float factor) {
    bool changed = false;
    for (unsigned long id : m_Elem) {
        CAlfaObject* object = group_element(id);
        if (auto* group = dynamic_cast<CGroup*>(object); group && group != this) {
            changed = group->CommitScale(center, axis, factor) || changed;
        } else if (auto* solid = dynamic_cast<CSolid*>(object)) {
            solid->Scale(center, axis, factor);
            changed = true;
        } else if (object && object != this) {
            changed = true;
        }
    }
    return changed;
}

void CGroup::SetLayer(unsigned long id) {
    m_LayerID = static_cast<int>(id);
    for (unsigned long element_id : m_Elem) {
        CAlfaObject* object = group_element(element_id);
        if (object && object != this) {
            if (auto* group = dynamic_cast<CGroup*>(object)) {
                group->SetLayer(id);
            } else {
                object->m_LayerID = static_cast<int>(id);
            }
        }
    }
}

void CGroup::SetVisible(bool visible) {
    CAlfaObject::SetVisible(visible);
    if (!group_is_registered(this)) {
        return;
    }
    for (unsigned long id : m_Elem) {
        CAlfaObject* object = group_element(id);
        if (object && object != this) {
            object->SetVisible(visible);
        }
    }
}

void CGroup::SetColor(Color color) {
    CAlfaObject::SetColor(color);
    if (!group_is_registered(this)) {
        return;
    }
    for (unsigned long id : m_Elem) {
        CAlfaObject* object = group_element(id);
        if (object && object != this) {
            object->SetColor(color);
        }
    }
}

void CGroup::SetColor(unsigned long col_set) {
    const Color color{
        static_cast<float>(col_set & 0xffUL) / 255.0f,
        static_cast<float>((col_set >> 8) & 0xffUL) / 255.0f,
        static_cast<float>((col_set >> 16) & 0xffUL) / 255.0f};
    SetColor(color);
}
