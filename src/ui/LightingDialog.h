#pragma once

#include <QDialog>

class QDoubleSpinBox;
class QCheckBox;
class QComboBox;

class LightingDialog : public QDialog {
    Q_OBJECT

public:
    explicit LightingDialog(QWidget* parent = nullptr);

signals:
    void LightingChanged();

private:
    QDoubleSpinBox* AddControl(const QString& text_id,
                               const QString& label,
                               const QString& tooltip_id,
                               const QString& tooltip,
                               double minimum,
                               double maximum,
                               double step);
    void LoadSettings();
    void PopulateEnvironments();
    void BrowseEnvironment();
    void ConnectLiveUpdates();
    void SaveAndApply();
    void ResetDefaults();

    class QFormLayout* form_ = nullptr;
    QDoubleSpinBox* light_x_ = nullptr;
    QDoubleSpinBox* light_y_ = nullptr;
    QDoubleSpinBox* light_z_ = nullptr;
    QDoubleSpinBox* ambient_ = nullptr;
    QDoubleSpinBox* wrap_light_ = nullptr;
    QDoubleSpinBox* diffuse_ = nullptr;
    QDoubleSpinBox* specular_ = nullptr;
    QDoubleSpinBox* shininess_scale_ = nullptr;
    QDoubleSpinBox* rim_ = nullptr;
    QDoubleSpinBox* gamma_ = nullptr;
    QCheckBox* environment_enabled_ = nullptr;
    QComboBox* environment_combo_ = nullptr;
    QDoubleSpinBox* environment_strength_ = nullptr;
    QDoubleSpinBox* environment_rotation_ = nullptr;
};
