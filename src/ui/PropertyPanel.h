#pragma once

#include "ToolRegistry.h"

#include <QWidget>
#include <QString>

class QFormLayout;

class PropertyPanel : public QWidget {
    Q_OBJECT

public:
    explicit PropertyPanel(QWidget* parent = nullptr);

    void Clear();
    void SetActiveObject(const ActiveParametricObject& active_object);
    void UpdateParameterValue(const std::string& parameter_id, double value);
    void SetCatalogParameterValue(const std::string& parameter_id, double value);
    void FocusParameter(const std::string& parameter_id);
    const ActiveParametricObject& ActiveObject() const;

signals:
    void ParametersChanged();
    void Accepted();
    void Canceled();
    void CatalogSelectionRequested(QString parameter_id, bool product);
    void CatalogOrientationHelpRequested(QString parameter_id, bool product);

private:
    void RebuildForm();

    QFormLayout* form_ = nullptr;
    ActiveParametricObject active_object_;
};
