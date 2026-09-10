#pragma once
#include "Point3d.h"
#include <cmath>
#include <cstdint>
#include <functional>
#include <stdexcept>

// Authoritative sketch geometry has no coordinate normal to its plane.
struct SketchPoint {
    double u = 0, v = 0;
    SketchPoint() = default;
    SketchPoint(CPoint3d p) : u(p.x), v(p.y) {
        if (!std::isfinite(u) || !std::isfinite(v)) throw std::invalid_argument("Non-finite sketch point");
    }
    operator CPoint3d() const { return {u,v,0}; }
};
struct SketchRevisions {
    std::uint64_t geometry = 0, topology = 0, placement = 0;
};
enum SketchChange : unsigned { SketchGeometryChanged=1, SketchTopologyChanged=2, SketchPlacementChanged=4 };
struct SketchChanges {
    SketchRevisions revisions;
    unsigned pending = 0;
    int depth = 0;
    std::uint64_t generation = 0;
    std::function<void(unsigned,SketchRevisions)> notify;
    void Mark(unsigned kind) { ++generation; pending |= kind; if (!depth) Publish(); }
    void Publish() {
        const unsigned kind = pending; pending = 0;
        if (!kind) return;
        if (kind & SketchGeometryChanged) ++revisions.geometry;
        if (kind & SketchTopologyChanged) ++revisions.topology;
        if (kind & SketchPlacementChanged) ++revisions.placement;
        if (notify) notify(kind,revisions);
    }
};
struct SketchNode { SketchPoint point; std::size_t id = 0; };
