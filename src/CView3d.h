#pragma once

#include "CAlfaDoc.h"

class CPoint3d;

class CView3d {
public:
    double m_Objmat[4][4];
	int m_ProjType;

    void Draw(const CAlfaDoc& document, bool xy_plane_grid = false, bool show_grid = true) const;
    void Project(CPoint3d* wp, CPoint3d* win);

private:
    void DrawGrid(bool xy_plane_grid) const;
    void DrawRoom() const;
    void DrawObjects(const CAlfaDoc& document) const;
    void DrawBox(float x, float y, float z, float w, float h, float d, float r, float g, float b) const;
};
