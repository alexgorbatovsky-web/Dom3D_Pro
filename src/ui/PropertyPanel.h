#pragma once

#include "ToolRegistry.h"

#include <QWidget>
#include <QString>

class QFormLayout;

class PropertyPanel : public QWidget {
    Q_OBJECT

public:
    explicit PropertyPanel(QWidget* parent = nullptr,
                           bool additional_parameters = false);

    void Clear();
    void SetActiveObject(const ActiveParametricObject& active_object);
    void UpdateParameterValue(const std::string& parameter_id, double value);
    void SetMaterialParameterValue(const std::string& parameter_id,
                                   double value,
                                   const std::string& material_name);
    void SetCatalogParameterValue(const std::string& parameter_id, double value);
    void FocusParameter(const std::string& parameter_id);
    const ActiveParametricObject& ActiveObject() const;

signals:
    void ParametersChanged();
    void MaterialParameterChanged(const QString& parameter_id);
    void Applied();
    void Accepted();
    void Canceled();
    void CatalogSelectionRequested(QString parameter_id, bool product);
    void CatalogOrientationHelpRequested(QString parameter_id, bool product);
    void MaterialLibraryRequested(QString parameter_id);

private:
    void RebuildForm();

    QFormLayout* form_ = nullptr;
    ActiveParametricObject active_object_;
    bool additional_parameters_ = false;
};
