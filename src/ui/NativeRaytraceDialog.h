#pragma once

#include "../render/NativeRaytraceRenderer.h"

#include <QDialog>
#include <QPixmap>

#include <atomic>

class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QDoubleSpinBox;
class QThread;
class QComboBox;

class NativeRaytraceDialog final : public QDialog {
    Q_OBJECT
public:
    explicit NativeRaytraceDialog(RenderScene scene,
                                  QSize initial_image_size = {},
                                  QWidget* parent = nullptr);
    ~NativeRaytraceDialog() override;

private:
    void StartRender();
    void BrowseOutput();
    void ViewResult();
    void EditLights();
    void UpdatePreview();
    void SetRendering(bool rendering);
    void RestoreDefaults();
    void LoadSettings();
    void SaveSettings();

signals:
    void RenderProgress(int percent, QImage preview, QString stage);

private:

    RenderScene scene_;
    QSize initial_image_size_;
    QSpinBox* width_ = nullptr;
    QSpinBox* height_ = nullptr;
    QSpinBox* depth_ = nullptr;
    QSpinBox* anti_alias_ = nullptr;
    QSpinBox* passes_ = nullptr;
    QSpinBox* threads_ = nullptr;
    QSpinBox* lights_ = nullptr;
    QComboBox* light_mode_ = nullptr;
    QPushButton* customize_lights_ = nullptr;
    std::vector<RenderLight> custom_lights_;
    QSpinBox* soft_shadow_samples_ = nullptr;
    QDoubleSpinBox* light_strength_ = nullptr;
    QDoubleSpinBox* light_size_ = nullptr;
    QDoubleSpinBox* shadow_density_ = nullptr;
    QDoubleSpinBox* ambient_ = nullptr;
    QDoubleSpinBox* exposure_ = nullptr;
    QLineEdit* output_path_ = nullptr;
    QPushButton* render_button_ = nullptr;
    QPushButton* restore_defaults_button_ = nullptr;
    QPushButton* cancel_button_ = nullptr;
    QPushButton* view_button_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* preview_ = nullptr;
    QPixmap result_pixmap_;
    QString last_render_path_;
    QThread* render_thread_ = nullptr;
    std::atomic_bool cancel_{false};
};
