#pragma once

#include "ToolRegistry.h"

#include <QWidget>

class QFormLayout;

class PropertyPanel : public QWidget {
    Q_OBJECT

public:
    explicit PropertyPanel(QWidget* parent = nullptr);

    void Clear();
    void SetActiveObject(const ActiveParametricObject& active_object);
    const ActiveParametricObject& ActiveObject() const;

signals:
    void ParametersChanged();
    void Accepted();
    void Canceled();

private:
    void RebuildForm();

    QFormLayout* form_ = nullptr;
    ActiveParametricObject active_object_;
};
