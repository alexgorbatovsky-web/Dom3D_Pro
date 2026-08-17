#pragma once

namespace TransformGizmoGeometry {

constexpr float kPi = 3.14159265358979323846f;

// Distance from the selected-object anchor to the axis-sensitive handles.
constexpr float kAxisDistanceScale = 2.0f;

// Screen-plane move handle at the common axis origin.  The whole disk is
// interactive, not just the rendered circumference.
constexpr float kMoveCenterRingRadiusScale = 0.28f;

// Keep the rotation handles well outside the object and axis handles.
// The first enlargement was 1.5; visual comparison with 3DCoat called for
// another 1.7 increase relative to that current size.
constexpr float kRotationArcRadiusScale = 0.46f * 1.5f * 1.7f;

// A centered 120-degree arc: 30 .. 150 degrees.
constexpr float kRotationArcStart = kPi / 6.0f;
constexpr float kRotationArcSweep = 2.0f * kPi / 3.0f;

}
