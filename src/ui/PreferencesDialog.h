#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
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
    QWidget* CreatePicturePage();
    QWidget* CreatePlaceholderPage(const QString& text);
    void UpdateNavigationPreview();
    void LoadSettings();
    void ApplySettings();

    QComboBox* length_unit_ = nullptr;
    QComboBox* number_separator_ = nullptr;
    QCheckBox* auto_save_ = nullptr;
    QSpinBox* auto_save_time_ = nullptr;
    QDoubleSpinBox* tolerance_modeling_ = nullptr;
    QCheckBox* delete_loop_ = nullptr;
    QRadioButton* offset_corner_ = nullptr;
    QRadioButton* offset_radius_ = nullptr;
    QSpinBox* angle_alignment_ = nullptr;
    QCheckBox* snapping_enabled_ = nullptr;
    QSpinBox* capture_distance_ = nullptr;
    QRadioButton* parametrization_none_ = nullptr;
    QRadioButton* parametrization_ortho_ = nullptr;
    QRadioButton* parametrization_max_template_ = nullptr;
    QCheckBox* groups_enabled_ = nullptr;
    QCheckBox* two_d_drag_auto_ = nullptr;
    QCheckBox* control_intersections_ = nullptr;
    QCheckBox* gizmo_3d_enable_ = nullptr;
    QComboBox* navigation_preset_ = nullptr;
    QLabel* navigation_orbit_ = nullptr;
    QLabel* navigation_pan_ = nullptr;
    QLabel* navigation_zoom_ = nullptr;
};
