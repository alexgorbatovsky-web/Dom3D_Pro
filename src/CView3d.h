#pragma once

#include "CAlfaDoc.h"

class CPoint3d;

class CView3d {
public:
    double m_Objmat[4][4];
	int m_ProjType;

    void Draw(const CAlfaDoc& document,
              Vec3 camera_eye,
              Vec3 camera_forward,
              bool xy_plane_grid = false,
              bool show_grid = true,
              float grid_size = kDefaultSceneSize,
              float grid_step = 100.0f,
              int grid_subdivisions = 20) const;
    void Project(CPoint3d* wp, CPoint3d* win);

private:
    void DrawGrid(bool xy_plane_grid,
                  float grid_size,
                  float grid_step,
                  int grid_subdivisions) const;
    void DrawRoom() const;
    void DrawObjects(const CAlfaDoc& document,
                     Vec3 camera_eye,
                     Vec3 camera_forward) const;
    void DrawBox(float x, float y, float z, float w, float h, float d, float r, float g, float b) const;
};
