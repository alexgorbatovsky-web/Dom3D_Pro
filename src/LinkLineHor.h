#pragma once

#include "LinkLine.h"

class CLinkLineHor final : public CLinkLine {
public:
    CLinkLineHor(CPoint3d start, CPoint3d end);
    CLinkLineHor(CPoint3d* start, CPoint3d* end);

    LinkLineType GetType() const override;
    std::unique_ptr<CLinkLine> Clone() const override;
    void EnforceGeometry() override;
};
