#pragma once

#include "CAlfaDoc.h"

class CView2d {
public:
    void DrawPreview(const CAlfaDoc& document, float x, float y, float width, float height) const;

private:
    void DrawFrame(float x, float y, float width, float height) const;
};
