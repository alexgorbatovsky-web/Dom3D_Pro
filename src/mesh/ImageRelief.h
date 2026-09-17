#pragma once
#include "../CMesh3D.h"
#include <QImage>
#include <TopoDS_Shape.hxx>
#include <functional>

namespace image_relief {
struct Options {
    double spacing = 2.0; // maximum base triangle edge length, millimetres
    double height = 2.0; // additive relief; never cuts into the source wall
    int axis = 2; // world X, Y or Z through the bounding-box centre
    int repeat_u = 12;
    int repeat_v = 1;
    double rotation_degrees = 0.0;
    double lower_percent = 10.0;
    double upper_percent = 95.0;
    double fade_mm = 3.0;
    bool invert = false;
    size_t max_triangles = 2000000;
    int face_index = -1; // TopExp face index; -1 retains legacy whole-body mapping
    bool preview = false; // bounded coarse mesh, no CAD midpoint projections
    bool allow_open = false;
};
struct Result {
    std::unique_ptr<CMesh3D> mesh;
    std::string error;
    size_t triangles = 0;
    double max_base_edge = 0;
    bool closed = false;
};
// The input shape and its render triangulation are never mutated.
// Selected non-revolution faces use their own surface coordinates.
bool UsesSurfaceMapping(const TopoDS_Shape&, int face_index);
// Progress returns false to cancel.
Result Build(const TopoDS_Shape&, const QImage&, const Options&,
             const std::function<bool(const char*)>& progress = {});
} // namespace image_relief
