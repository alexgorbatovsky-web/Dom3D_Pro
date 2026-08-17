#include "ConstraintHorLine.h"

#include "SmartLine.h"

CConstraintHorLine::CConstraintHorLine(std::size_t line_index)
    : CConstraint(line_index) {
}

ConstraintType CConstraintHorLine::GetType() const {
    return ConstraintType::Horizontal;
}

bool CConstraintHorLine::Apply(CSmartLine& sketch) const {
    CLinkLine* line = sketch.GetLine(GetLineIndex());
    if (!line) {
        return false;
    }
    CPoint3d end = line->GetEnd();
    end.y = line->GetStart().y;
    line->SetEnd(end);
    sketch.ConnectAdjacentLines(GetLineIndex());
    return true;
}

std::unique_ptr<CConstraint> CConstraintHorLine::Clone() const {
    return std::make_unique<CConstraintHorLine>(GetLineIndex());
}
