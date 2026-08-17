#include "LinkLineHor.h"

CLinkLineHor::CLinkLineHor(CPoint3d start, CPoint3d end)
    : CLinkLine(start, end) {
    EnforceGeometry();
}

CLinkLineHor::CLinkLineHor(CPoint3d* start, CPoint3d* end)
    : CLinkLineHor(start ? *start : CPoint3d(), end ? *end : CPoint3d()) {
}

LinkLineType CLinkLineHor::GetType() const {
    return LinkLineType::Horizontal;
}

std::unique_ptr<CLinkLine> CLinkLineHor::Clone() const {
    auto result = std::make_unique<CLinkLineHor>(GetStart(), GetEnd());
    result->SetID(GetID());
    return result;
}

void CLinkLineHor::EnforceGeometry() {
    CPoint3d* end = P(1);
    end->y = GetStart().y;
}
