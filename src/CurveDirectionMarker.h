#pragma once

#include "OpenGLCompat.h"
#include "Point3d.h"

#include <algorithm>
#include <cmath>

inline void DrawCurveStartArrow(const CPoint3d& start,
                                const CPoint3d& next,
                                double curve_span) {
    double dx = next.x - start.x;
    double dy = next.y - start.y;
    double dz = next.z - start.z;
    const double tangent_length = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (tangent_length <= 1.0e-12 || curve_span <= 1.0e-12) return;
    dx /= tangent_length;
    dy /= tangent_length;
    dz /= tangent_length;

    const double size = curve_span * 0.045;
    const CPoint3d tip(
        start.x + dx * size,
        start.y + dy * size,
        start.z + dz * size);
    const CPoint3d base(
        tip.x - dx * size * 0.58,
        tip.y - dy * size * 0.58,
        tip.z - dz * size * 0.58);

    double ax = std::abs(dx) < 0.8 ? 1.0 : 0.0;
    double ay = std::abs(dx) < 0.8 ? 0.0 : 1.0;
    double az = 0.0;
    double px = dy * az - dz * ay;
    double py = dz * ax - dx * az;
    double pz = dx * ay - dy * ax;
    const double perpendicular_length = std::sqrt(px * px + py * py + pz * pz);
    if (perpendicular_length <= 1.0e-12) return;
    px /= perpendicular_length;
    py /= perpendicular_length;
    pz /= perpendicular_length;
    const double qx = dy * pz - dz * py;
    const double qy = dz * px - dx * pz;
    const double qz = dx * py - dy * px;
    const double radius = size * 0.24;

    const CPoint3d ring[4] = {
        {base.x + px * radius, base.y + py * radius, base.z + pz * radius},
        {base.x + qx * radius, base.y + qy * radius, base.z + qz * radius},
        {base.x - px * radius, base.y - py * radius, base.z - pz * radius},
        {base.x - qx * radius, base.y - qy * radius, base.z - qz * radius}};

    glColor3f(1.0f, 0.82f, 0.05f);
    glBegin(GL_TRIANGLES);
    for (int index = 0; index < 4; ++index) {
        const CPoint3d& first = ring[index];
        const CPoint3d& second = ring[(index + 1) % 4];
        glVertex3f(static_cast<float>(tip.x), static_cast<float>(tip.y),
                   static_cast<float>(tip.z));
        glVertex3f(static_cast<float>(first.x), static_cast<float>(first.y),
                   static_cast<float>(first.z));
        glVertex3f(static_cast<float>(second.x), static_cast<float>(second.y),
                   static_cast<float>(second.z));
    }
    glEnd();
    glLineWidth(2.5f);
    glBegin(GL_LINES);
    glVertex3f(static_cast<float>(start.x), static_cast<float>(start.y),
               static_cast<float>(start.z));
    glVertex3f(static_cast<float>(tip.x), static_cast<float>(tip.y),
               static_cast<float>(tip.z));
    glEnd();
}
