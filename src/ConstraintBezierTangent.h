#pragma once

#include "Constraint.h"

class CConstraintBezierTangent final : public CConstraint {
public:
    CConstraintBezierTangent(std::size_t line_index, bool at_start);

    ConstraintType GetType() const override;
    bool Apply(CSmartLine& sketch) const override;
    std::unique_ptr<CConstraint> Clone() const override;

private:
    bool at_start_ = true;
};
