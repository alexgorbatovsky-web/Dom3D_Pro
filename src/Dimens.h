#pragma once

#include "Point3d.h"

#include <string>
#include <cstddef>

struct DimensionGeometry3D {
    CPoint3d source_start;
    CPoint3d source_end;
    CPoint3d dimension_start;
    CPoint3d dimension_end;
    CPoint3d extension_start;
    CPoint3d extension_end;
    CPoint3d text_position;
};

class CDimens {
public:
    CDimens() = default;
    CDimens(std::string parameter_id, std::string label, double value);
    virtual ~CDimens() = default;

    const std::string& GetParameterId() const;
    const std::string& GetLabel() const;
    double GetValue() const;
    void SetValue(double value);
    bool IsVisible() const;
    void SetVisible(bool visible);

protected:
    std::string parameter_id_;
    std::string label_;
    double value_ = 0.0;
    bool visible_ = true;
};

class CDimens3D final : public CDimens {
public:
    CDimens3D() = default;
    CDimens3D(CPoint3d start,
              CPoint3d end,
              CPoint3d offset_direction,
              double offset,
              std::string parameter_id,
              std::string label,
              double value);

    void SetPoints(CPoint3d start, CPoint3d end);
    void SetOffset(CPoint3d direction, double distance);
    const CPoint3d& GetStart() const;
    const CPoint3d& GetEnd() const;
    const CPoint3d& GetOffsetDirection() const;
    double GetOffsetDistance() const;
    void SetActive(bool active);
    bool IsActive() const;
    void SetSourceIndex(size_t source_index);
    size_t GetSourceIndex() const;
    DimensionGeometry3D GetGeometry(double extension_overshoot = 0.0) const;
    double GetMeasuredLength() const;

private:
    CPoint3d start_;
    CPoint3d end_;
    CPoint3d offset_direction_{0.0, 1.0, 0.0};
    double offset_ = 0.0;
    bool active_ = false;
    size_t source_index_ = 0;
};
