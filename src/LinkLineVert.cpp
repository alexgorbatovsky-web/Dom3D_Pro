#include "LinkLineVert.h"

CLinkLineVert::CLinkLineVert(CPoint3d start, CPoint3d end)
    : CLinkLine(start, end) {
    EnforceGeometry();
}

CLinkLineVert::CLinkLineVert(CPoint3d* start, CPoint3d* end)
    : CLinkLineVert(start ? *start : CPoint3d(), end ? *end : CPoint3d()) {
}

LinkLineType CLinkLineVert::GetType() const {
    return LinkLineType::Vertical;
}

std::unique_ptr<CLinkLine> CLinkLineVert::Clone() const {
    auto result = std::make_unique<CLinkLineVert>(GetStart(), GetEnd());
    result->SetID(GetID());
    return result;
}

void CLinkLineVert::EnforceGeometry() {
    CPoint3d* end = P(1);
    end->x = GetStart().x;
}
