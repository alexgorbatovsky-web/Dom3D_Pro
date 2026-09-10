#pragma once
#include <QString>
#include <string>

inline QString RevolveInputHelp(const std::string& tool) {
    if (tool == "SurfaceRevolve")
        return "Surface Revolve: select one open or closed 2D/3D Polyline, Spline or Sketch; choose axis and angle. Ends remain uncapped.";
    return "Solid Revolve: select one open or closed Sketch or XY-planar 2D/3D Polyline. Open ends close to the axis; choose an axis in the profile plane. Standalone splines are supported by Surface Revolve.";
}

inline QString RevolveFailureHelp(const std::string& tool) {
    if (tool == "SurfaceRevolve")
        return "Surface Revolve: cannot rotate this curve. Check for zero-length segments or segments lying on the rotation axis. " + RevolveInputHelp(tool);
    return "Solid Revolve: cannot build the volume. Choose an axis in the profile plane; the profile must not cross it or intersect itself. " + RevolveInputHelp(tool);
}
