#include "Constraint.h"

CConstraint::CConstraint(std::size_t line_index)
    : line_index_(line_index) {
}

std::size_t CConstraint::GetLineIndex() const {
    return line_index_;
}
