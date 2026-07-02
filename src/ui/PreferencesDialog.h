#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QRadioButton;
class QSpinBox;

class PreferencesDialog : public QDialog {
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget* parent = nullptr);

signals:
    void SettingsApplied();

private:
    QWidget* CreateProjectPage();
    QWidget* CreateModelingPage();
    QWidget* CreatePlaceholderPage(const QString& text);
    void LoadSettings();
    void ApplySettings();

    QComboBox* length_unit_ = nullptr;
    QDoubleSpinBox* tolerance_modeling_ = nullptr;
    QCheckBox* delete_loop_ = nullptr;
    QRadioButton* offset_corner_ = nullptr;
    QRadioButton* offset_radius_ = nullptr;
    QSpinBox* angle_alignment_ = nullptr;
    QCheckBox* spiral_ = nullptr;
    QSpinBox* spiral_value_ = nullptr;
    QRadioButton* parametrization_none_ = nullptr;
    QRadioButton* parametrization_ortho_ = nullptr;
    QRadioButton* parametrization_max_template_ = nullptr;
    QCheckBox* change_group_ = nullptr;
    QCheckBox* two_d_drag_auto_ = nullptr;
    QCheckBox* control_intersections_ = nullptr;
    QCheckBox* gizmo_3d_enable_ = nullptr;
};
