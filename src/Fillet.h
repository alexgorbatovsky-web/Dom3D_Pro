#pragma once

#include "Point3d.h"

#include <cstddef>
#include <vector>

class CLinkLine;

struct FilletGeometry {
    CPoint3d tangent_on_first;
    CPoint3d tangent_on_second;
    CPoint3d center;
    double first_parameter = 1.0;
    double second_parameter = 0.0;
    double signed_angle = 0.0;
    bool valid = false;
};

class CFillet {
public:
    CFillet(std::size_t first_line_index, double radius);
    CFillet(std::size_t first_line_index, std::size_t second_line_index, double radius);

    std::size_t GetFirstLineIndex() const;
    std::size_t GetSecondLineIndex() const;
    double GetRadius() const;
    bool SetRadius(double radius);

    FilletGeometry Calculate(const CLinkLine& first, const CLinkLine& second) const;
    std::vector<CPoint3d> Sample(const CLinkLine& first,
                                 const CLinkLine& second,
                                 std::size_t minimum_segments = 6) const;

private:
    std::size_t first_line_index_;
    std::size_t second_line_index_;
    double radius_;
};
