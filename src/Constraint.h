#pragma once

#include <cstddef>
#include <memory>

class CSmartLine;

enum class ConstraintType {
    Horizontal,
    Vertical,
    TangentAtStart,
    TangentAtEnd
};

class CConstraint {
public:
    explicit CConstraint(std::size_t line_index);
    virtual ~CConstraint() = default;

    std::size_t GetLineIndex() const;
    virtual ConstraintType GetType() const = 0;
    virtual bool Apply(CSmartLine& sketch) const = 0;
    virtual std::unique_ptr<CConstraint> Clone() const = 0;

private:
    std::size_t line_index_;
};
