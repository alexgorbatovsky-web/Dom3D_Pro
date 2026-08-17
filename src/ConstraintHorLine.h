#pragma once

#include "Constraint.h"

class CConstraintHorLine final : public CConstraint {
public:
    explicit CConstraintHorLine(std::size_t line_index);

    ConstraintType GetType() const override;
    bool Apply(CSmartLine& sketch) const override;
    std::unique_ptr<CConstraint> Clone() const override;
};
