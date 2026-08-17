#pragma once

#include <cstddef>
#include <memory>

class CSurfaceFace;
class SurfaceUVMappingImpl;
struct Vec3;

struct SurfaceUVPoint {
    double u = 0.0;
    double v = 0.0;
};

class SurfaceUVMapping {
public:
    explicit SurfaceUVMapping(const CSurfaceFace* surface);
    ~SurfaceUVMapping();

    SurfaceUVMapping(SurfaceUVMapping&&) noexcept;
    SurfaceUVMapping& operator=(SurfaceUVMapping&&) noexcept;

    SurfaceUVMapping(const SurfaceUVMapping&) = delete;
    SurfaceUVMapping& operator=(const SurfaceUVMapping&) = delete;

    bool IsValid() const;
    bool Project(Vec3 point, SurfaceUVPoint& uv) const;
    SurfaceUVPoint UnwrapNear(SurfaceUVPoint uv, SurfaceUVPoint reference) const;
    bool IsUPeriodic() const;
    bool IsVPeriodic() const;
    double UPeriod() const;
    double VPeriod() const;

private:
    std::unique_ptr<SurfaceUVMappingImpl> impl_;
};
