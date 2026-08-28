#include "NativeRaytraceDialog.h"
#include "RenderLightEditorDialog.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <memory>

namespace {
struct NativeRenderResult {
    QImage image;
    QString error;
    QString output_path;
    bool success = false;
};
}

NativeRaytraceDialog::NativeRaytraceDialog(
    RenderScene scene, QSize initial_image_size, QWidget* parent)
    : QDialog(parent), scene_(std::move(scene)),
      initial_image_size_(initial_image_size) {
    setWindowTitle("Dom3D Native Raytrace");
    setAttribute(Qt::WA_DeleteOnClose);
    resize(980, 760);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(QString(
        "Original Dom3D ray tracer: %1 objects, %2 triangles, %3 materials")
        .arg(scene_.meshes.size()).arg(scene_.TriangleCount())
        .arg(scene_.materials.size()), this));

    auto* form = new QFormLayout;
    auto* size_row = new QWidget(this);
    auto* size_layout = new QHBoxLayout(size_row);
    size_layout->setContentsMargins(0, 0, 0, 0);
    width_ = new QSpinBox(size_row);
    height_ = new QSpinBox(size_row);
    for (QSpinBox* value : {width_, height_}) {
        value->setRange(64, 16384);
        value->setSingleStep(64);
    }
    width_->setValue(initial_image_size.isValid()
        ? std::clamp(initial_image_size.width(), 64, 16384) : 1280);
    height_->setValue(initial_image_size.isValid()
        ? std::clamp(initial_image_size.height(), 64, 16384) : 720);
    size_layout->addWidget(width_);
    size_layout->addWidget(new QLabel("×", size_row));
    size_layout->addWidget(height_);
    form->addRow("Image size", size_row);

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
            this, &NativeRaytraceDialog::EditLights);
    connect(light_mode_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int index) { customize_lights_->setEnabled(index == 1); });

    depth_ = new QSpinBox(this);
    depth_->setRange(1, 16);
    depth_->setValue(4);
    anti_alias_ = new QSpinBox(this);
    anti_alias_->setRange(1, 8);
    anti_alias_->setValue(2);
    passes_ = new QSpinBox(this);
    passes_->setRange(1, 10);
    passes_->setValue(5);
    threads_ = new QSpinBox(this);
    threads_->setRange(0, 256);
    threads_->setSpecialValueText("Auto");
    threads_->setValue(0);
    lights_ = new QSpinBox(this);
    lights_->setRange(1, 8);
    lights_->setValue(3);
    soft_shadow_samples_ = new QSpinBox(this);
    soft_shadow_samples_->setRange(1, 16);
    soft_shadow_samples_->setValue(4);
    light_strength_ = new QDoubleSpinBox(this);
    light_strength_->setRange(0.0, 5.0);
    light_strength_->setSingleStep(0.05);
    light_strength_->setValue(0.62);
    light_size_ = new QDoubleSpinBox(this);
    light_size_->setRange(0.0, 30.0);
    light_size_->setSuffix(" %");
    light_size_->setSingleStep(1.0);
    light_size_->setValue(6.0);
    shadow_density_ = new QDoubleSpinBox(this);
    shadow_density_->setRange(0.0, 100.0);
    shadow_density_->setSuffix(" %");
    shadow_density_->setSingleStep(5.0);
    shadow_density_->setValue(100.0);
    ambient_ = new QDoubleSpinBox(this);
    ambient_->setRange(0.0, 1.0);
    ambient_->setSingleStep(0.02);
    ambient_->setValue(0.10);
    exposure_ = new QDoubleSpinBox(this);
    exposure_->setRange(-4.0, 6.0);
    exposure_->setSuffix(" EV");
    exposure_->setSingleStep(0.25);
    exposure_->setValue(1.25);
    form->addRow("Reflection depth", depth_);
    form->addRow("Antialiasing", anti_alias_);
    form->addRow("Progressive passes", passes_);
    form->addRow("CPU threads", threads_);
    form->addRow("Interior lights", lights_);
    form->addRow("Soft shadow samples", soft_shadow_samples_);
    form->addRow("Light strength", light_strength_);
    form->addRow("Light size", light_size_);
    form->addRow("Shadow density", shadow_density_);
    form->addRow("Ambient", ambient_);
    form->addRow("Exposure", exposure_);

    auto* output_row = new QWidget(this);
    auto* output_layout = new QHBoxLayout(output_row);
    output_layout->setContentsMargins(0, 0, 0, 0);
    output_path_ = new QLineEdit(output_row);
    auto* browse = new QPushButton("...", output_row);
    browse->setFixedWidth(38);
    output_layout->addWidget(output_path_, 1);
    output_layout->addWidget(browse);
    form->addRow("Output PNG", output_row);
    connect(browse, &QPushButton::clicked,
            this, &NativeRaytraceDialog::BrowseOutput);
    output_path_->setText(QDir(
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation))
        .filePath("Dom3D_NativeRaytrace.png"));
    layout->addLayout(form);

    progress_ = new QProgressBar(this);
    progress_->setRange(0, 100);
    progress_->setValue(0);
    progress_->setFormat("%p%");
    status_ = new QLabel("Ready", this);
    layout->addWidget(progress_);
    layout->addWidget(status_);
    preview_ = new QLabel("Native raytrace result will appear here", this);
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setMinimumHeight(320);
    preview_->setStyleSheet(
        "QLabel { background:#111; border:1px solid #444; }");
    layout->addWidget(preview_, 1);

    auto* buttons = new QDialogButtonBox(this);
    render_button_ = buttons->addButton("Render", QDialogButtonBox::AcceptRole);
    restore_defaults_button_ = buttons->addButton(
        "Restore Defaults", QDialogButtonBox::ActionRole);
    restore_defaults_button_->setToolTip(
        "Restore the built-in render settings.");
    view_button_ = buttons->addButton("View Result", QDialogButtonBox::ActionRole);
    cancel_button_ = buttons->addButton("Cancel Render", QDialogButtonBox::RejectRole);
    auto* close = buttons->addButton(QDialogButtonBox::Close);
    view_button_->setEnabled(false);
    cancel_button_->setEnabled(false);
    connect(render_button_, &QPushButton::clicked,
            this, &NativeRaytraceDialog::StartRender);
    connect(restore_defaults_button_, &QPushButton::clicked, this, [this]() {
        RestoreDefaults();
        status_->setText("Built-in render settings restored");
    });
    connect(view_button_, &QPushButton::clicked,
            this, &NativeRaytraceDialog::ViewResult);
    connect(cancel_button_, &QPushButton::clicked, this, [this]() {
        cancel_.store(true, std::memory_order_relaxed);
        status_->setText("Canceling...");
    });
    connect(close, &QPushButton::clicked, this, &QDialog::close);
    layout->addWidget(buttons);
    LoadSettings();
    connect(this, &NativeRaytraceDialog::RenderProgress, this,
            [this](int percent, const QImage& preview, const QString& stage) {
        progress_->setValue(std::clamp(percent, 0, 100));
        status_->setText(QString("%1 — %2%").arg(stage).arg(percent));
        if (!preview.isNull()) {
            result_pixmap_ = QPixmap::fromImage(preview);
            UpdatePreview();
        }
    });
}

NativeRaytraceDialog::~NativeRaytraceDialog() {
    cancel_.store(true, std::memory_order_relaxed);
    if (render_thread_) render_thread_->wait();
}

void NativeRaytraceDialog::EditLights() {
    RenderLightEditorDialog dialog(custom_lights_, this);
    if (dialog.exec() != QDialog::Accepted) return;
    custom_lights_ = dialog.Lights();
    QSettings settings("Dom3D", "Dom3D_Pro");
    RenderLightEditorDialog::Save(settings, custom_lights_);
}

void NativeRaytraceDialog::RestoreDefaults() {
    width_->setValue(initial_image_size_.isValid()
        ? std::clamp(initial_image_size_.width(), 64, 16384) : 1280);
    height_->setValue(initial_image_size_.isValid()
        ? std::clamp(initial_image_size_.height(), 64, 16384) : 720);
    depth_->setValue(4);
    anti_alias_->setValue(2);
    passes_->setValue(5);
    threads_->setValue(0);
    lights_->setValue(3);
    soft_shadow_samples_->setValue(4);
    light_strength_->setValue(0.62);
    light_size_->setValue(6.0);
    shadow_density_->setValue(100.0);
    ambient_->setValue(0.10);
    exposure_->setValue(1.25);
    light_mode_->setCurrentIndex(0);
    customize_lights_->setEnabled(false);
    custom_lights_ = RenderLightEditorDialog::Defaults(scene_);
    output_path_->setText(QDir(
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation))
        .filePath("Dom3D_NativeRaytrace.png"));
}

void NativeRaytraceDialog::LoadSettings() {
    QSettings settings("Dom3D", "Dom3D_Pro");
    const int legacy_light_mode = settings.value(
        "render/lightMode", light_mode_->currentIndex()).toInt();
    settings.beginGroup("render/native");
    width_->setValue(settings.value("width", width_->value()).toInt());
    height_->setValue(settings.value("height", height_->value()).toInt());
    depth_->setValue(settings.value("reflectionDepth", depth_->value()).toInt());
    anti_alias_->setValue(settings.value("antialiasing", anti_alias_->value()).toInt());
    passes_->setValue(settings.value("progressivePasses", passes_->value()).toInt());
    threads_->setValue(settings.value("threads", threads_->value()).toInt());
    lights_->setValue(settings.value("interiorLights", lights_->value()).toInt());
    soft_shadow_samples_->setValue(
        settings.value("softShadowSamples", soft_shadow_samples_->value()).toInt());
    light_strength_->setValue(
        settings.value("lightStrength", light_strength_->value()).toDouble());
    light_size_->setValue(settings.value("lightSize", light_size_->value()).toDouble());
    shadow_density_->setValue(
        settings.value("shadowDensity", shadow_density_->value()).toDouble());
    ambient_->setValue(settings.value("ambient", ambient_->value()).toDouble());
    exposure_->setValue(settings.value("exposure", exposure_->value()).toDouble());
    light_mode_->setCurrentIndex(std::clamp(
        settings.value("lightMode", legacy_light_mode).toInt(), 0, 1));
    output_path_->setText(settings.value("outputPath", output_path_->text()).toString());
    settings.endGroup();

    custom_lights_ = RenderLightEditorDialog::Load(
        settings, RenderLightEditorDialog::Defaults(scene_));
    customize_lights_->setEnabled(light_mode_->currentIndex() == 1);

    last_render_path_ = settings.value("render/lastOutputPath").toString();
    if (QFileInfo::exists(last_render_path_)) {
        result_pixmap_.load(last_render_path_);
        view_button_->setEnabled(!result_pixmap_.isNull());
        UpdatePreview();
    }
}

void NativeRaytraceDialog::SaveSettings() {
    QSettings settings("Dom3D", "Dom3D_Pro");
    settings.beginGroup("render/native");
    settings.setValue("width", width_->value());
    settings.setValue("height", height_->value());
    settings.setValue("reflectionDepth", depth_->value());
    settings.setValue("antialiasing", anti_alias_->value());
    settings.setValue("progressivePasses", passes_->value());
    settings.setValue("threads", threads_->value());
    settings.setValue("interiorLights", lights_->value());
    settings.setValue("softShadowSamples", soft_shadow_samples_->value());
    settings.setValue("lightStrength", light_strength_->value());
    settings.setValue("lightSize", light_size_->value());
    settings.setValue("shadowDensity", shadow_density_->value());
    settings.setValue("ambient", ambient_->value());
    settings.setValue("exposure", exposure_->value());
    settings.setValue("lightMode", light_mode_->currentIndex());
    settings.setValue("outputPath", output_path_->text().trimmed());
    settings.endGroup();
    RenderLightEditorDialog::Save(settings, custom_lights_);
}

void NativeRaytraceDialog::StartRender() {
    if (render_thread_) return;
    NativeRaytraceSettings settings;
    settings.width = width_->value();
    settings.height = height_->value();
    settings.reflection_depth = depth_->value();
    settings.anti_alias_level = anti_alias_->value();
    settings.progressive_passes = passes_->value();
    settings.thread_count = threads_->value();
    settings.light_count = lights_->value();
    settings.soft_shadow_samples = soft_shadow_samples_->value();
    settings.light_strength = light_strength_->value();
    settings.light_radius_fraction = light_size_->value() / 100.0;
    settings.shadow_density = shadow_density_->value() / 100.0;
    settings.ambient_strength = ambient_->value();
    settings.exposure_ev = exposure_->value();
    settings.light_mode = light_mode_->currentIndex() == 1
        ? RenderSettings::LightMode::Customize : RenderSettings::LightMode::Auto;
    settings.custom_lights = custom_lights_;
    const QString output = output_path_->text().trimmed();
    if (output.isEmpty()) {
        QMessageBox::warning(this, "Native Raytrace", "Choose an output PNG file.");
        return;
    }
    SaveSettings();
    auto result = std::make_shared<NativeRenderResult>();
    result->output_path = output;
    cancel_.store(false, std::memory_order_relaxed);
    SetRendering(true);
    render_thread_ = QThread::create([this, result, settings]() {
        result->success = NativeRaytraceRenderer::Render(
            scene_, settings, &result->image, &result->error, &cancel_,
            [this](int percent, const QImage& preview, const QString& stage) {
                emit RenderProgress(percent, preview, stage);
            });
        if (result->success && !result->image.save(result->output_path, "PNG")) {
            result->success = false;
            result->error = "Could not save the PNG file.";
        }
    });
    connect(render_thread_, &QThread::finished, this, [this, result]() {
        render_thread_->deleteLater();
        render_thread_ = nullptr;
        SetRendering(false);
        if (!result->success) {
            status_->setText(result->error);
            return;
        }
        last_render_path_ = result->output_path;
        result_pixmap_ = QPixmap::fromImage(result->image);
        progress_->setValue(100);
        QSettings("Dom3D", "Dom3D_Pro").setValue(
            "render/lastOutputPath", last_render_path_);
        status_->setText(QString("Finished — %1 threads")
            .arg(threads_->value() == 0
                ? QThread::idealThreadCount() : threads_->value()));
        view_button_->setEnabled(true);
        UpdatePreview();
    });
    render_thread_->start();
}

void NativeRaytraceDialog::SetRendering(bool rendering) {
    render_button_->setEnabled(!rendering);
    restore_defaults_button_->setEnabled(!rendering);
    cancel_button_->setEnabled(rendering);
    progress_->setRange(0, 100);
    if (rendering) progress_->setValue(0);
    status_->setText(rendering ? "Compiling octree and tracing on all CPU cores..."
                               : "Ready");
}

void NativeRaytraceDialog::BrowseOutput() {
    QString path = QFileDialog::getSaveFileName(
        this, "Native raytrace output", output_path_->text(),
        "PNG image (*.png)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".png", Qt::CaseInsensitive)) path += ".png";
    output_path_->setText(path);
}

void NativeRaytraceDialog::ViewResult() {
    if (!last_render_path_.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(last_render_path_));
    }
}

void NativeRaytraceDialog::UpdatePreview() {
    if (result_pixmap_.isNull()) return;
    preview_->setPixmap(result_pixmap_.scaled(
        preview_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
