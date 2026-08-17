#include "BlenderCyclesDialog.h"

#include "../render/BlenderCyclesRenderer.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
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
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

BlenderCyclesDialog::BlenderCyclesDialog(RenderScene scene, QWidget* parent)
    : QDialog(parent), scene_(std::move(scene)) {
    setWindowTitle("Blender Cycles Render");
    setAttribute(Qt::WA_DeleteOnClose);
    resize(900, 760);

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
    form->addRow("Preset", preset_);
    connect(preset_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &BlenderCyclesDialog::ApplyPreset);

    auto* size_row = new QWidget(this);
    auto* size_layout = new QHBoxLayout(size_row);
    size_layout->setContentsMargins(0, 0, 0, 0);
    width_ = new QSpinBox(size_row);
    height_ = new QSpinBox(size_row);
    for (QSpinBox* spin : {width_, height_}) {
        spin->setRange(64, 16384);
        spin->setSingleStep(64);
    }
    width_->setValue(1280);
    height_->setValue(720);
    size_layout->addWidget(width_);
    size_layout->addWidget(new QLabel("×", size_row));
    size_layout->addWidget(height_);
    form->addRow("Image size", size_row);

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
    form->addRow("Samples", samples_);
    form->addRow("Noise Threshold", noise_threshold_);
    form->addRow("Denoise", denoise_);
    form->addRow("Device", device_);

    add_path_row("Output PNG", output_path_, [this]() { BrowseOutput(); });
    output_path_->setText(QDir(
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation))
        .filePath("Dom3D_Cycles.png"));
    layout->addLayout(form);

    progress_ = new QProgressBar(this);
    progress_->setRange(0, 1);
    progress_->setValue(0);
    status_ = new QLabel("Ready", this);
    layout->addWidget(progress_);
    layout->addWidget(status_);

    preview_ = new QLabel(this);
    preview_->setMinimumHeight(260);
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
    cancel_button_ = buttons->addButton("Cancel Render", QDialogButtonBox::RejectRole);
    auto* close = buttons->addButton(QDialogButtonBox::Close);
    cancel_button_->setEnabled(false);
    connect(render_button_, &QPushButton::clicked, this, &BlenderCyclesDialog::StartRender);
    connect(cancel_button_, &QPushButton::clicked, this, [this]() {
        if (renderer_) renderer_->Cancel();
    });
    connect(close, &QPushButton::clicked, this, &QDialog::close);
    layout->addWidget(buttons);
    ApplyPreset(0);
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

RenderSettings BlenderCyclesDialog::CurrentSettings() const {
    RenderSettings settings;
    settings.width = width_->value();
    settings.height = height_->value();
    settings.samples = samples_->value();
    settings.noise_threshold = noise_threshold_->value();
    settings.denoise = denoise_->isChecked();
    settings.output_file = output_path_->text().trimmed();
    settings.device = device_->currentIndex() == 1
        ? RenderSettings::Device::GPU
        : device_->currentIndex() == 2
            ? RenderSettings::Device::CPU : RenderSettings::Device::Auto;
    return settings;
}

void BlenderCyclesDialog::StartRender() {
    if (renderer_) renderer_->deleteLater();
    renderer_ = new BlenderCyclesRenderer(blender_path_->text().trimmed(), this);
    log_->clear();
    connect(renderer_, &BlenderCyclesRenderer::OutputReceived,
            log_, &QPlainTextEdit::appendPlainText);
    connect(renderer_, &BlenderCyclesRenderer::RenderStarted, this, [this]() {
        SetRendering(true);
        status_->setText("Blender Cycles is rendering...");
    });
    connect(renderer_, &BlenderCyclesRenderer::RenderFinished,
            this, [this](const QString& path, double seconds) {
        SetRendering(false);
        const QPixmap image(path);
        preview_->setPixmap(image.scaled(
            preview_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
    QString error;
    if (!renderer_->StartRender(scene_, CurrentSettings(), &error)) {
        SetRendering(false);
        QMessageBox::warning(this, "Blender Cycles", error);
    }
}

void BlenderCyclesDialog::SetRendering(bool rendering) {
    render_button_->setEnabled(!rendering);
    cancel_button_->setEnabled(rendering);
    progress_->setRange(0, rendering ? 0 : 1);
    if (!rendering) progress_->setValue(1);
}
