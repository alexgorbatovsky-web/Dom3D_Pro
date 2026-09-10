#include "SurfaceUVMapping.h"

#include "Common.h"
#include "solid/SurfaceFace.h"

#include <BRep_Tool.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepTools.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <Geom_Surface.hxx>
#include <Standard_Failure.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <cmath>
#include <mutex>
#include <utility>

void Step(const char* text);

class SurfaceUVMappingImpl {
public:
    Handle(Geom_Surface) surface;
    gp_Trsf world_to_surface;
    bool valid = false;
    bool u_periodic = false;
    bool v_periodic = false;
    double u_period = 0.0;
    double v_period = 0.0;
    bool bounded_projection = false;
    double u_min = 0.0, u_max = 0.0, v_min = 0.0, v_max = 0.0;
};

namespace {
double unwrap_periodic(double value, double reference, double period) {
    if (!(period > 0.0) || !std::isfinite(period)) {
        return value;
    }
    return value + std::round((reference - value) / period) * period;
}
}

SurfaceUVMapping::SurfaceUVMapping(const CSurfaceFace* surface)
    : impl_(std::make_unique<SurfaceUVMappingImpl>()) {
    if (!surface || surface->m_Face.IsNull()) {
        return;
    }

    try {
        const TopoDS_Face face = TopoDS::Face(surface->m_Face);
        TopLoc_Location location;
        impl_->surface = BRep_Tool::Surface(face, location);
        if (impl_->surface.IsNull()) {
            return;
        }

        impl_->world_to_surface = location.Transformation().Inverted();
        impl_->u_periodic = impl_->surface->IsUPeriodic();
        impl_->v_periodic = impl_->surface->IsVPeriodic();
        impl_->u_period = impl_->u_periodic ? impl_->surface->UPeriod() : 0.0;
        impl_->v_period = impl_->v_periodic ? impl_->surface->VPeriod() : 0.0;
        // A revolved fillet can have several stationary projections on its
        // supporting surface. Search the actual face domain: an unrestricted
        // projector can return the opposite side of the generating arc.
        impl_->bounded_projection = BRepAdaptor_Surface(face).GetType()
            == GeomAbs_SurfaceOfRevolution;
        if (impl_->bounded_projection)
            BRepTools::UVBounds(face, impl_->u_min, impl_->u_max,
                               impl_->v_min, impl_->v_max);
        impl_->valid = true;
    } catch (const Standard_Failure&) {
    }
}

SurfaceUVMapping::~SurfaceUVMapping() = default;
SurfaceUVMapping::SurfaceUVMapping(SurfaceUVMapping&&) noexcept = default;
SurfaceUVMapping& SurfaceUVMapping::operator=(SurfaceUVMapping&&) noexcept = default;

bool SurfaceUVMapping::IsValid() const {
    return impl_ && impl_->valid;
}

bool SurfaceUVMapping::Project(Vec3 point, SurfaceUVPoint& uv) const {
    if (!IsValid()) {
        return false;
    }
    try {
        gp_Pnt projected_point(point.x, point.y, point.z);
        projected_point.Transform(impl_->world_to_surface);
        // Keep the projector alive for the process lifetime. Some OCCT builds
        // have been seen to crash while destroying this helper after projection.
        static auto* projector = new GeomAPI_ProjectPointOnSurf;
        static std::mutex projector_mutex;
        const std::lock_guard<std::mutex> lock(projector_mutex);
        gp_Pnt dummy(0, 0, 0);
        if (impl_->bounded_projection)
            // Float boundary samples can lie just beyond a parameter bound.
            // Tree search also avoids the wrong stationary solution returned
            // by the gradient solver for revolved, trimmed generating curves.
            projector->Init(impl_->surface, impl_->u_min - 1.0e-5, impl_->u_max + 1.0e-5,
                            impl_->v_min - 1.0e-5, impl_->v_max + 1.0e-5,
                            1.0e-9, Extrema_ExtAlgo_Tree);
        else
            projector->Init(dummy, impl_->surface);
        projector->Perform(projected_point);
        if (projector->NbPoints() < 1) {
            return false;
        }
        Standard_Real u = 0.0;
        Standard_Real v = 0.0;
        projector->LowerDistanceParameters(u, v);
        if (!std::isfinite(u) || !std::isfinite(v)) {
            return false;
        }
        uv = {u, v};
        return true;
    } catch (const Standard_Failure&) {
        return false;
    }
    return true;
}

SurfaceUVPoint SurfaceUVMapping::UnwrapNear(SurfaceUVPoint uv, SurfaceUVPoint reference) const {
    if (IsUPeriodic()) {
        uv.u = unwrap_periodic(uv.u, reference.u, UPeriod());
    }
    if (IsVPeriodic()) {
        uv.v = unwrap_periodic(uv.v, reference.v, VPeriod());
    }
    return uv;
}

bool SurfaceUVMapping::IsUPeriodic() const {
    return IsValid() && impl_->u_periodic;
}

bool SurfaceUVMapping::IsVPeriodic() const {
    return IsValid() && impl_->v_periodic;
}

double SurfaceUVMapping::UPeriod() const {
    return IsValid() ? impl_->u_period : 0.0;
}

double SurfaceUVMapping::VPeriod() const {
    return IsValid() ? impl_->v_period : 0.0;
}
