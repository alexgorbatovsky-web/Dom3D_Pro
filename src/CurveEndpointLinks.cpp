#include "CAlfaDoc.h"
#include "CBSpline.h"
#include "CPolyline.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace {
const std::vector<CPoint3d>* points(const CAlfaObject* object) {
    if (const auto* curve = dynamic_cast<const CPolyline*>(object))
        return !curve->IsClosed() ? &curve->GetPoints() : nullptr;
    if (const auto* curve = dynamic_cast<const CBSpline*>(object))
        return !curve->IsClosed() ? &curve->GetPoints() : nullptr;
    return nullptr;
}
double distance(const CPoint3d& a, const CPoint3d& b) {
    return std::hypot(std::hypot(a.x-b.x, a.y-b.y), a.z-b.z);
}
using End = std::pair<unsigned long, bool>;
End first(const CAlfaDoc::CurveEndpointLink& link) { return {link.first_id, link.first_end}; }
End second(const CAlfaDoc::CurveEndpointLink& link) { return {link.second_id, link.second_end}; }
}

std::vector<CAlfaDoc::CurveEndpointLink> CAlfaDoc::FindTouchingCurveEnds(double tolerance) const {
    std::vector<CurveEndpointLink> result;
    if (!std::isfinite(tolerance) || tolerance < 0) return result;
    for (size_t i = 0; i < selected_object_indices_.size(); ++i) {
        const size_t ai = selected_object_indices_[i];
        if (ai >= objects_.size()) continue;
        const auto* a = points(objects_[ai].get());
        if (!a || a->size() < 2) continue;
        for (size_t j = i+1; j < selected_object_indices_.size(); ++j) {
            const size_t bi = selected_object_indices_[j];
            if (bi >= objects_.size()) continue;
            const auto* b = points(objects_[bi].get());
            if (!b || b->size() < 2) continue;
            for (bool ae : {false, true}) for (bool be : {false, true}) {
                const auto& ap = ae ? a->back() : a->front();
                const auto& bp = be ? b->back() : b->front();
                if (distance(ap, bp) <= tolerance)
                    result.push_back({objects_[ai]->m_id, objects_[bi]->m_id, ae, be, ap});
            }
        }
    }
    return result;
}

int CAlfaDoc::LinkTouchingCurveEnds(double tolerance) {
    EnsureObjectIds();
    int added = 0;
    for (const auto& candidate : FindTouchingCurveEnds(tolerance)) {
        const bool exists = std::any_of(curve_endpoint_links_.begin(), curve_endpoint_links_.end(),
            [&](const auto& link) { return (first(link) == first(candidate) && second(link) == second(candidate))
                || (first(link) == second(candidate) && second(link) == first(candidate)); });
        if (!exists) { curve_endpoint_links_.push_back(candidate); ++added; }
    }
    SynchronizeCurveEndpointLinks();
    return added;
}

int CAlfaDoc::UnlinkSelectedCurveEnds() {
    std::set<unsigned long> selected;
    for (size_t index : selected_object_indices_)
        if (index < objects_.size() && objects_[index]) selected.insert(objects_[index]->m_id);
    const size_t before = curve_endpoint_links_.size();
    curve_endpoint_links_.erase(std::remove_if(curve_endpoint_links_.begin(), curve_endpoint_links_.end(),
        [&](const auto& link) { return selected.count(link.first_id) && selected.count(link.second_id); }),
        curve_endpoint_links_.end());
    return static_cast<int>(before - curve_endpoint_links_.size());
}

std::vector<unsigned long> CAlfaDoc::SynchronizeCurveEndpointLinks() {
    const auto valid = [&](unsigned long id) {
        const auto* p = points(FindObjectById(id)); return p && p->size() >= 2;
    };
    curve_endpoint_links_.erase(std::remove_if(curve_endpoint_links_.begin(), curve_endpoint_links_.end(),
        [&](const auto& l) { return !valid(l.first_id) || !valid(l.second_id); }), curve_endpoint_links_.end());
    std::set<End> visited;
    std::set<unsigned long> changed;
    for (const auto& seed : curve_endpoint_links_) {
        if (visited.count(first(seed))) continue;
        std::vector<End> group{first(seed)};
        std::vector<size_t> edges;
        for (size_t i=0; i<group.size(); ++i) {
            visited.insert(group[i]);
            for (size_t j=0; j<curve_endpoint_links_.size(); ++j) {
                const auto& l = curve_endpoint_links_[j];
                if (first(l) != group[i] && second(l) != group[i]) continue;
                if (std::find(edges.begin(), edges.end(), j) == edges.end()) edges.push_back(j);
                const End other = first(l) == group[i] ? second(l) : first(l);
                if (std::find(group.begin(), group.end(), other) == group.end()) group.push_back(other);
            }
        }
        CPoint3d target = seed.position;
        for (const End& end : group) {
            const auto* p = points(FindObjectById(end.first));
            const auto& current = end.second ? p->back() : p->front();
            if (distance(current, seed.position) > 1.e-9) { target = current; break; }
        }
        for (const End& end : group) {
            CAlfaObject* object = FindObjectById(end.first);
            const auto* p = points(object);
            const size_t index = end.second ? p->size()-1 : 0;
            if (distance((*p)[index], target) <= 1.e-9) continue;
            if (auto* curve = dynamic_cast<CPolyline*>(object)) curve->SetPoint(index, target);
            else if (auto* curve = dynamic_cast<CBSpline*>(object)) curve->SetPoint(index, target);
            changed.insert(end.first);
        }
        for (size_t index : edges) curve_endpoint_links_[index].position = target;
    }
    return {changed.begin(), changed.end()};
}
