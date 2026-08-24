#pragma once

#include "CMesh3D.h"

#include <vector>

// Standalone Dom3D port of the front-expansion ContourQuadrangulator from
// 3DCoat. The input mesh must be triangulated and have an open boundary.
bool Build3DCoatQuadrangulation(const std::vector<Vec3>& vertices,
                                const std::vector<CMesh3D::Face>& triangles,
                                CMesh3D* result);

