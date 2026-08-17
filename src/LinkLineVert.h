#pragma once

#include "LinkLine.h"

class CLinkLineVert final : public CLinkLine {
public:
    CLinkLineVert(CPoint3d start, CPoint3d end);
    CLinkLineVert(CPoint3d* start, CPoint3d* end);

    LinkLineType GetType() const override;
    std::unique_ptr<CLinkLine> Clone() const override;
    void EnforceGeometry() override;
};
