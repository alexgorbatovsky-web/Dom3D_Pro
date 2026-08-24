#pragma once

#include "../render/RenderScene.h"

#include <QDialog>
#include <QPixmap>

class BlenderCyclesRenderer;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QResizeEvent;

class BlenderCyclesDialog final : public QDialog {
    Q_OBJECT
public:
    explicit BlenderCyclesDialog(RenderScene scene,
                                 QSize initial_image_size = {},
                                 QWidget* parent = nullptr);
    ~BlenderCyclesDialog() override;

private:
    void ApplyPreset(int index);
    void ApplyLightingPreset(int index);
    void BrowseBlender();
    void BrowseOutput();
    void EditLights();
    void StartRender();
    void ViewResult();
    void UpdatePreview();
    void SetRendering(bool rendering);
    void RestoreDefaults();
    void LoadSettings();
    void SaveSettings();
    RenderSettings CurrentSettings() const;

protected:
    void resizeEvent(QResizeEvent* event) override;

    RenderScene scene_;
    QSize initial_image_size_;
    BlenderCyclesRenderer* renderer_ = nullptr;
    QLineEdit* blender_path_ = nullptr;
    QComboBox* preset_ = nullptr;
    QComboBox* lighting_preset_ = nullptr;
    QComboBox* light_mode_ = nullptr;
    QPushButton* customize_lights_ = nullptr;
    std::vector<RenderLight> custom_lights_;
    QSpinBox* width_ = nullptr;
    QSpinBox* height_ = nullptr;
    QSpinBox* samples_ = nullptr;
    QDoubleSpinBox* noise_threshold_ = nullptr;
    QComboBox* device_ = nullptr;
    QDoubleSpinBox* exposure_ = nullptr;
    QDoubleSpinBox* environment_strength_ = nullptr;
    QDoubleSpinBox* interior_light_strength_ = nullptr;
    QCheckBox* denoise_ = nullptr;
    QLineEdit* output_path_ = nullptr;
    QPushButton* render_button_ = nullptr;
    QPushButton* restore_defaults_button_ = nullptr;
    QPushButton* view_button_ = nullptr;
    QPushButton* cancel_button_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* preview_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QPlainTextEdit* log_ = nullptr;
    QPixmap result_pixmap_;
    QString last_render_path_;
};
