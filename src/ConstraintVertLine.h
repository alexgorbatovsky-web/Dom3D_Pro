#pragma once

#include "Constraint.h"

class CConstraintVertLine final : public CConstraint {
public:
    explicit CConstraintVertLine(std::size_t line_index);

    ConstraintType GetType() const override;
    bool Apply(CSmartLine& sketch) const override;
    std::unique_ptr<CConstraint> Clone() const override;
};
