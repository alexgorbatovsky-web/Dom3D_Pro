#include "ConstraintVertLine.h"

#include "SmartLine.h"

CConstraintVertLine::CConstraintVertLine(std::size_t line_index)
    : CConstraint(line_index) {
}

ConstraintType CConstraintVertLine::GetType() const {
    return ConstraintType::Vertical;
}

bool CConstraintVertLine::Apply(CSmartLine& sketch) const {
    CLinkLine* line = sketch.GetLine(GetLineIndex());
    if (!line) {
        return false;
    }
    CPoint3d end = line->GetEnd();
    end.x = line->GetStart().x;
    line->SetEnd(end);
    sketch.ConnectAdjacentLines(GetLineIndex());
    return true;
}

std::unique_ptr<CConstraint> CConstraintVertLine::Clone() const {
    return std::make_unique<CConstraintVertLine>(GetLineIndex());
}
