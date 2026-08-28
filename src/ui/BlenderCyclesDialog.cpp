#include "BlenderCyclesDialog.h"

#include "../render/BlenderCyclesRenderer.h"
#include "RenderLightEditorDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
#include <QUrl>

#include <algorithm>

BlenderCyclesDialog::BlenderCyclesDialog(RenderScene scene,
                                         QSize initial_image_size,
                                         QWidget* parent)
    : QDialog(parent), scene_(std::move(scene)),
      initial_image_size_(initial_image_size) {
    setWindowTitle("Blender Cycles Render");
    setAttribute(Qt::WA_DeleteOnClose);
    resize(1080, 820);

    auto* layout = new QVBoxLayout(this);
    auto* summary = new QLabel(QString(
        "Current scene: %1 objects, %2 triangles, %3 materials")
        .arg(scene_.meshes.size()).arg(scene_.TriangleCount())
        .arg(scene_.materials.size()), this);
    layout->addWidget(summary);

    auto* form = new QFormLayout;
    auto add_path_row = [this, form](const QString& label,
                                     QLineEdit*& edit,
                                     auto callback) {
        auto* row = new QWidget(this);
        auto* row_layout = new QHBoxLayout(row);
        row_layout->setContentsMargins(0, 0, 0, 0);
        edit = new QLineEdit(row);
        auto* browse = new QPushButton("...", row);
        browse->setFixedWidth(38);
        row_layout->addWidget(edit, 1);
        row_layout->addWidget(browse);
        connect(browse, &QPushButton::clicked, this, callback);
        form->addRow(label, row);
    };
    add_path_row("Blender", blender_path_, [this]() { BrowseBlender(); });
    blender_path_->setText(BlenderCyclesRenderer::FindBlender());

    preset_ = new QComboBox(this);
    preset_->addItems({"Preview", "Final", "Custom"});
    form->addRow("Quality", preset_);
    connect(preset_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &BlenderCyclesDialog::ApplyPreset);

    lighting_preset_ = new QComboBox(this);
    lighting_preset_->addItems({"Interior", "Exterior", "Studio"});
    lighting_preset_->setToolTip(
        "Interior: window and ceiling light. Exterior: environment only. "
        "Studio: neutral softboxes.");
    form->addRow("Lighting", lighting_preset_);

    auto* light_mode_row = new QWidget(this);
    auto* light_mode_layout = new QHBoxLayout(light_mode_row);
    light_mode_layout->setContentsMargins(0, 0, 0, 0);
    light_mode_ = new QComboBox(light_mode_row);
    light_mode_->addItems({"Auto", "Customize"});
    customize_lights_ = new QPushButton("Sources...", light_mode_row);
    light_mode_layout->addWidget(light_mode_, 1);
    light_mode_layout->addWidget(customize_lights_);
    form->addRow("Light sources", light_mode_row);
    connect(customize_lights_, &QPushButton::clicked,
            this, &BlenderCyclesDialog::EditLights);
    connect(light_mode_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int index) { customize_lights_->setEnabled(index == 1); });

    width_ = new QSpinBox(this);
    height_ = new QSpinBox(this);
    for (QSpinBox* spin : {width_, height_}) {
        spin->setRange(64, 16384);
        spin->setSingleStep(64);
    }
    width_->setValue(initial_image_size.isValid()
        ? std::clamp(initial_image_size.width(), 64, 16384) : 1280);
    height_->setValue(initial_image_size.isValid()
        ? std::clamp(initial_image_size.height(), 64, 16384) : 720);
    resolution_scale_ = new QComboBox(this);
    for (const int percent : {200, 170, 130, 100, 50, 30}) {
        resolution_scale_->addItem(QString::number(percent) + "%", percent);
    }
    resolution_scale_->setCurrentIndex(resolution_scale_->findData(100));
    resolution_scale_->setToolTip(
        "Scale the output resolution without changing the base width and height");
    output_size_ = new QLabel(this);
    form->addRow("Width", width_);
    form->addRow("Height", height_);
    form->addRow("Resolution scale", resolution_scale_);
    form->addRow("Output size", output_size_);
    connect(width_, qOverload<int>(&QSpinBox::valueChanged),
            this, [this]() { UpdateResolutionSummary(); });
    connect(height_, qOverload<int>(&QSpinBox::valueChanged),
            this, [this]() { UpdateResolutionSummary(); });
    connect(resolution_scale_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this]() { UpdateResolutionSummary(); });
    UpdateResolutionSummary();

    samples_ = new QSpinBox(this);
    samples_->setRange(1, 16384);
    noise_threshold_ = new QDoubleSpinBox(this);
    noise_threshold_->setRange(0.0, 1.0);
    noise_threshold_->setDecimals(3);
    noise_threshold_->setSingleStep(0.01);
    denoise_ = new QCheckBox("Enabled", this);
    denoise_->setChecked(true);
    device_ = new QComboBox(this);
    device_->addItems({"Auto", "GPU", "CPU"});
    exposure_ = new QDoubleSpinBox(this);
    exposure_->setRange(-5.0, 5.0);
    exposure_->setDecimals(1);
    exposure_->setSingleStep(0.1);
    environment_strength_ = new QDoubleSpinBox(this);
    environment_strength_->setRange(0.0, 5.0);
    environment_strength_->setDecimals(2);
    environment_strength_->setSingleStep(0.05);
    interior_light_strength_ = new QDoubleSpinBox(this);
    interior_light_strength_->setRange(0.0, 5.0);
    interior_light_strength_->setDecimals(2);
    interior_light_strength_->setSingleStep(0.1);
    form->addRow("Samples", samples_);
    form->addRow("Noise Threshold", noise_threshold_);
    form->addRow("Denoise", denoise_);
    form->addRow("Device", device_);
    form->addRow("Exposure", exposure_);
    form->addRow("Environment", environment_strength_);
    form->addRow("Interior light", interior_light_strength_);

    add_path_row("Output PNG", output_path_, [this]() { BrowseOutput(); });
    output_path_->setText(QDir(
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation))
        .filePath("Dom3D_Cycles.png"));
    last_render_path_ = QSettings("Dom3D", "Dom3D_Pro")
        .value("render/cycles/lastOutputPath").toString();
    if (QFileInfo::exists(last_render_path_)) {
        result_pixmap_.load(last_render_path_);
    }
    layout->addLayout(form);

    progress_ = new QProgressBar(this);
    progress_->setRange(0, 100);
    progress_->setValue(0);
    progress_->setFormat("%p%");
    status_ = new QLabel("Ready", this);
    layout->addWidget(progress_);
    layout->addWidget(status_);

    preview_ = new QLabel(this);
    preview_->setMinimumHeight(340);
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setStyleSheet("QLabel { background:#111; border:1px solid #444; }");
    preview_->setText("Cycles result will appear here");
    layout->addWidget(preview_, 1);

    log_ = new QPlainTextEdit(this);
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(4000);
    log_->setMaximumHeight(145);
    layout->addWidget(log_);

    auto* buttons = new QDialogButtonBox(this);
    render_button_ = buttons->addButton("Render", QDialogButtonBox::AcceptRole);
    restore_defaults_button_ = buttons->addButton(
        "Restore Defaults", QDialogButtonBox::ActionRole);
    restore_defaults_button_->setToolTip(
        "Restore the built-in render settings.");
    view_button_ = buttons->addButton("View Result", QDialogButtonBox::ActionRole);
    cancel_button_ = buttons->addButton("Cancel Render", QDialogButtonBox::RejectRole);
    auto* close = buttons->addButton(QDialogButtonBox::Close);
    cancel_button_->setEnabled(false);
    view_button_->setEnabled(!result_pixmap_.isNull());
    connect(render_button_, &QPushButton::clicked, this, &BlenderCyclesDialog::StartRender);
    connect(restore_defaults_button_, &QPushButton::clicked, this, [this]() {
        RestoreDefaults();
        status_->setText("Built-in render settings restored");
    });
    connect(view_button_, &QPushButton::clicked, this, &BlenderCyclesDialog::ViewResult);
    connect(cancel_button_, &QPushButton::clicked, this, [this]() {
        if (renderer_) renderer_->Cancel();
    });
    connect(close, &QPushButton::clicked, this, &QDialog::close);
    // Keep the primary render controls directly below the settings. Large
    // previews and the Blender log may extend beyond a short display, but the
    // user must always be able to start or cancel a render.
    layout->insertWidget(2, buttons);
    ApplyPreset(0);
    LoadSettings();
    connect(lighting_preset_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &BlenderCyclesDialog::ApplyLightingPreset);
    QTimer::singleShot(0, this, &BlenderCyclesDialog::UpdatePreview);
}

void BlenderCyclesDialog::EditLights() {
    RenderLightEditorDialog dialog(custom_lights_, this);
    if (dialog.exec() != QDialog::Accepted) return;
    custom_lights_ = dialog.Lights();
    QSettings settings("Dom3D", "Dom3D_Pro");
    RenderLightEditorDialog::Save(settings, custom_lights_);
}

BlenderCyclesDialog::~BlenderCyclesDialog() {
    if (renderer_) renderer_->Cancel();
}

void BlenderCyclesDialog::ApplyPreset(int index) {
    if (index == 0) {
        samples_->setValue(64);
        noise_threshold_->setValue(0.05);
        denoise_->setChecked(true);
    } else if (index == 1) {
        samples_->setValue(512);
        noise_threshold_->setValue(0.01);
        denoise_->setChecked(true);
    }
}

void BlenderCyclesDialog::ApplyLightingPreset(int index) {
    if (index == 0) {
        exposure_->setValue(-1.0);
        environment_strength_->setValue(0.15);
        interior_light_strength_->setValue(0.45);
    } else if (index == 1) {
        exposure_->setValue(0.0);
        environment_strength_->setValue(1.0);
        // A closed architectural room cannot rely on the world environment
        // alone. Keep a neutral daylight fill so Exterior remains usable for
        // interiors with windows instead of rendering almost completely black.
        interior_light_strength_->setValue(0.30);
    } else {
        exposure_->setValue(-0.2);
        environment_strength_->setValue(0.15);
        interior_light_strength_->setValue(0.8);
    }
}

void BlenderCyclesDialog::BrowseBlender() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Choose blender.exe", QFileInfo(blender_path_->text()).absolutePath(),
        "Blender (blender.exe);;Executables (*.exe);;All files (*.*)");
    if (!path.isEmpty()) blender_path_->setText(path);
}

void BlenderCyclesDialog::BrowseOutput() {
    QString path = QFileDialog::getSaveFileName(
        this, "Cycles output", output_path_->text(), "PNG image (*.png)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".png", Qt::CaseInsensitive)) path += ".png";
    output_path_->setText(path);
}

int BlenderCyclesDialog::ResolutionScalePercent() const {
    return resolution_scale_ ? resolution_scale_->currentData().toInt() : 100;
}

void BlenderCyclesDialog::UpdateResolutionSummary() {
    if (!width_ || !height_ || !output_size_) return;
    const int scale = ResolutionScalePercent();
    const int output_width = std::max(16, width_->value() * scale / 100);
    const int output_height = std::max(16, height_->value() * scale / 100);
    output_size_->setText(QString("%1 × %2 pixels")
        .arg(output_width).arg(output_height));
}

RenderSettings BlenderCyclesDialog::CurrentSettings() const {
    RenderSettings settings;
    const int resolution_scale = ResolutionScalePercent();
    settings.width = std::max(16, width_->value() * resolution_scale / 100);
    settings.height = std::max(16, height_->value() * resolution_scale / 100);
    settings.samples = samples_->value();
    settings.noise_threshold = noise_threshold_->value();
    settings.denoise = denoise_->isChecked();
    settings.output_file = output_path_->text().trimmed();
    settings.device = device_->currentIndex() == 1
        ? RenderSettings::Device::GPU
        : device_->currentIndex() == 2
            ? RenderSettings::Device::CPU : RenderSettings::Device::Auto;
    settings.lighting_preset = lighting_preset_->currentIndex() == 1
        ? RenderSettings::LightingPreset::Exterior
        : lighting_preset_->currentIndex() == 2
            ? RenderSettings::LightingPreset::Studio
            : RenderSettings::LightingPreset::Interior;
    settings.view_transform = "AgX";
    settings.look = "Medium High Contrast";
    settings.exposure = exposure_->value();
    settings.environment_strength = environment_strength_->value();
    settings.interior_light_strength = interior_light_strength_->value();
    settings.light_mode = light_mode_->currentIndex() == 1
        ? RenderSettings::LightMode::Customize : RenderSettings::LightMode::Auto;
    settings.custom_lights = custom_lights_;
    return settings;
}

void BlenderCyclesDialog::RestoreDefaults() {
    blender_path_->setText(BlenderCyclesRenderer::FindBlender());
    preset_->setCurrentIndex(0);
    ApplyPreset(0);
    lighting_preset_->setCurrentIndex(0);
    ApplyLightingPreset(0);
    light_mode_->setCurrentIndex(0);
    customize_lights_->setEnabled(false);
    custom_lights_ = RenderLightEditorDialog::Defaults(scene_);
    width_->setValue(initial_image_size_.isValid()
        ? std::clamp(initial_image_size_.width(), 64, 16384) : 1280);
    height_->setValue(initial_image_size_.isValid()
        ? std::clamp(initial_image_size_.height(), 64, 16384) : 720);
    resolution_scale_->setCurrentIndex(resolution_scale_->findData(100));
    device_->setCurrentIndex(0);
    output_path_->setText(QDir(
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation))
        .filePath("Dom3D_Cycles.png"));
}

void BlenderCyclesDialog::LoadSettings() {
    QSettings settings("Dom3D", "Dom3D_Pro");
    const int legacy_lighting_preset = settings.value(
        "render/lightingPreset", lighting_preset_->currentIndex()).toInt();
    const double legacy_exposure = settings.value(
        "render/exposure", exposure_->value()).toDouble();
    const double legacy_environment_strength = settings.value(
        "render/environmentStrength", environment_strength_->value()).toDouble();
    const double legacy_interior_light_strength = settings.value(
        "render/interiorLightStrength", interior_light_strength_->value()).toDouble();
    const int legacy_light_mode = settings.value(
        "render/lightMode", light_mode_->currentIndex()).toInt();
    settings.beginGroup("render/cycles");
    blender_path_->setText(settings.value("blenderPath", blender_path_->text()).toString());
    preset_->setCurrentIndex(std::clamp(
        settings.value("qualityPreset", preset_->currentIndex()).toInt(), 0, 2));
    width_->setValue(settings.value("width", width_->value()).toInt());
    height_->setValue(settings.value("height", height_->value()).toInt());
    const int scale_index = resolution_scale_->findData(
        settings.value("resolutionScale", 100).toInt());
    resolution_scale_->setCurrentIndex(scale_index >= 0 ? scale_index
                                                        : resolution_scale_->findData(100));
    samples_->setValue(settings.value("samples", samples_->value()).toInt());
    noise_threshold_->setValue(
        settings.value("noiseThreshold", noise_threshold_->value()).toDouble());
    denoise_->setChecked(settings.value("denoise", denoise_->isChecked()).toBool());
    device_->setCurrentIndex(std::clamp(
        settings.value("device", device_->currentIndex()).toInt(), 0, 2));
    const int lighting_index = std::clamp(
        settings.value("lightingPreset", legacy_lighting_preset).toInt(), 0, 2);
    lighting_preset_->setCurrentIndex(lighting_index);
    ApplyLightingPreset(lighting_index);
    exposure_->setValue(settings.value("exposure", legacy_exposure).toDouble());
    environment_strength_->setValue(
        settings.value("environmentStrength", legacy_environment_strength).toDouble());
    interior_light_strength_->setValue(
        settings.value("interiorLightStrength", legacy_interior_light_strength).toDouble());
    light_mode_->setCurrentIndex(std::clamp(
        settings.value("lightMode", legacy_light_mode).toInt(), 0, 1));
    QString output_path = settings.value(
        "outputPath", output_path_->text()).toString();
    if (QFileInfo(output_path).fileName().compare(
            "Dom3D_NativeRaytrace.png", Qt::CaseInsensitive) == 0) {
        output_path = QDir(
            QStandardPaths::writableLocation(QStandardPaths::PicturesLocation))
            .filePath("Dom3D_Cycles.png");
    }
    output_path_->setText(output_path);
    if (lighting_index == 1 && interior_light_strength_->value() <= 0.0) {
        interior_light_strength_->setValue(0.30);
    }
    settings.endGroup();

    custom_lights_ = RenderLightEditorDialog::Load(
        settings, RenderLightEditorDialog::Defaults(scene_));
    customize_lights_->setEnabled(light_mode_->currentIndex() == 1);
}

void BlenderCyclesDialog::SaveSettings() {
    QSettings settings("Dom3D", "Dom3D_Pro");
    settings.beginGroup("render/cycles");
    settings.setValue("blenderPath", blender_path_->text().trimmed());
    settings.setValue("qualityPreset", preset_->currentIndex());
    settings.setValue("width", width_->value());
    settings.setValue("height", height_->value());
    settings.setValue("resolutionScale", ResolutionScalePercent());
    settings.setValue("samples", samples_->value());
    settings.setValue("noiseThreshold", noise_threshold_->value());
    settings.setValue("denoise", denoise_->isChecked());
    settings.setValue("device", device_->currentIndex());
    settings.setValue("lightingPreset", lighting_preset_->currentIndex());
    settings.setValue("exposure", exposure_->value());
    settings.setValue("environmentStrength", environment_strength_->value());
    settings.setValue("interiorLightStrength", interior_light_strength_->value());
    settings.setValue("lightMode", light_mode_->currentIndex());
    settings.setValue("outputPath", output_path_->text().trimmed());
    settings.endGroup();
    RenderLightEditorDialog::Save(settings, custom_lights_);
}

void BlenderCyclesDialog::StartRender() {
    if (renderer_) renderer_->deleteLater();
    renderer_ = new BlenderCyclesRenderer(blender_path_->text().trimmed(), this);
    log_->clear();
    connect(renderer_, &BlenderCyclesRenderer::OutputReceived,
            log_, &QPlainTextEdit::appendPlainText);
    connect(renderer_, &BlenderCyclesRenderer::RenderStarted, this, [this]() {
        SetRendering(true);
        status_->setText("Starting Blender Cycles — 0%");
    });
    connect(renderer_, &BlenderCyclesRenderer::RenderProgress, this,
            [this](int percent, const QString& stage) {
        progress_->setValue(std::clamp(percent, 0, 100));
        status_->setText(QString("%1 — %2%").arg(stage).arg(percent));
    });
    connect(renderer_, &BlenderCyclesRenderer::PreviewUpdated, this,
            [this](const QString& path) {
        QPixmap preview(path);
        if (preview.isNull()) return;
        result_pixmap_ = preview;
        UpdatePreview();
    });
    connect(renderer_, &BlenderCyclesRenderer::RenderFinished,
            this, [this](const QString& path, double seconds) {
        SetRendering(false);
        progress_->setValue(100);
        last_render_path_ = path;
        QSettings("Dom3D", "Dom3D_Pro")
            .setValue("render/cycles/lastOutputPath", path);
        result_pixmap_.load(path);
        UpdatePreview();
        view_button_->setEnabled(!result_pixmap_.isNull());
        status_->setText(QString("Finished in %1 s — %2")
                         .arg(seconds, 0, 'f', 1).arg(path));
    });
    connect(renderer_, &BlenderCyclesRenderer::RenderFailed,
            this, [this](const QString& error, const QString& diagnostic) {
        SetRendering(false);
        status_->setText("Render failed");
        QMessageBox::critical(this, "Blender Cycles",
            error + "\n\nDiagnostics retained in:\n" + diagnostic);
    });
    connect(renderer_, &BlenderCyclesRenderer::RenderCanceled, this, [this]() {
        SetRendering(false);
        status_->setText("Render canceled");
    });
    const RenderSettings settings = CurrentSettings();
    SaveSettings();
    QString error;
    if (!renderer_->StartRender(scene_, settings, &error)) {
        SetRendering(false);
        QMessageBox::warning(this, "Blender Cycles", error);
    }
}

void BlenderCyclesDialog::ViewResult() {
    const QString path = !last_render_path_.isEmpty()
        ? last_render_path_ : output_path_->text().trimmed();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        QMessageBox::information(
            this, "Render Result", "There is no completed render to view yet.");
        return;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
        QMessageBox::warning(
            this, "Render Result", QString("Could not open:\n%1").arg(path));
    }
}

void BlenderCyclesDialog::UpdatePreview() {
    if (result_pixmap_.isNull() || !preview_) {
        return;
    }
    const QSize available = preview_->contentsRect().size();
    if (available.width() <= 0 || available.height() <= 0) {
        return;
    }
    preview_->setPixmap(result_pixmap_.scaled(
        available, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void BlenderCyclesDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    UpdatePreview();
}

void BlenderCyclesDialog::SetRendering(bool rendering) {
    render_button_->setEnabled(!rendering);
    restore_defaults_button_->setEnabled(!rendering);
    cancel_button_->setEnabled(rendering);
    progress_->setRange(0, 100);
    progress_->setValue(rendering ? 0 : progress_->value());
}
