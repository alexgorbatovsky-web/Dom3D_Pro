#pragma once

#include "../render/RenderScene.h"

#include <QDialog>

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

class BlenderCyclesDialog final : public QDialog {
    Q_OBJECT
public:
    explicit BlenderCyclesDialog(RenderScene scene, QWidget* parent = nullptr);
    ~BlenderCyclesDialog() override;

private:
    void ApplyPreset(int index);
    void BrowseBlender();
    void BrowseOutput();
    void StartRender();
    void SetRendering(bool rendering);
    RenderSettings CurrentSettings() const;

    RenderScene scene_;
    BlenderCyclesRenderer* renderer_ = nullptr;
    QLineEdit* blender_path_ = nullptr;
    QComboBox* preset_ = nullptr;
    QSpinBox* width_ = nullptr;
    QSpinBox* height_ = nullptr;
    QSpinBox* samples_ = nullptr;
    QDoubleSpinBox* noise_threshold_ = nullptr;
    QComboBox* device_ = nullptr;
    QCheckBox* denoise_ = nullptr;
    QLineEdit* output_path_ = nullptr;
    QPushButton* render_button_ = nullptr;
    QPushButton* cancel_button_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* preview_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QPlainTextEdit* log_ = nullptr;
};

