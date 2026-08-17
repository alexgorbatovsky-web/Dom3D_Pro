#pragma once

#include "Solid.h"

class CSurfaceSet : public CSolid {
public:
    CSurfaceSet();
    explicit CSurfaceSet(TopoDS_Shape& shape);
    std::unique_ptr<CAlfaObject> Clone() const override;
    void Render3d(bool selected) const override;

    const char* GetID() override { return "CSurfaceSet"; }
    const char* GetHint() override { return "CSurfaceSet_HINT"; }
    bool PresentInRetopoTools() override { return true; }
    bool ForceWireframeDisplay() const override { return false; }
};
