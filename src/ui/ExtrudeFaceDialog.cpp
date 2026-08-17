#include "ExtrudeFaceDialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>

ExtrudeFaceDialog::ExtrudeFaceDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Extrude Face Options");
    setModal(true);
    setMinimumWidth(280);

    auto* layout = new QFormLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setHorizontalSpacing(14);
    layout->setVerticalSpacing(8);

    taper_angle_ = new QDoubleSpinBox(this);
    taper_angle_->setRange(-89.0, 89.0);
    taper_angle_->setDecimals(2);
    taper_angle_->setSingleStep(1.0);
    taper_angle_->setSuffix(" deg");
    taper_angle_->setValue(0.0);

    taper_slider_ = new QSlider(Qt::Horizontal, this);
    taper_slider_->setRange(-8900, 8900);
    taper_slider_->setSingleStep(1);
    taper_slider_->setPageStep(100);
    taper_slider_->setValue(0);
    taper_slider_->setToolTip(
        "Taper Angle: drag; arrow keys — 0.01 deg, Page Up/Down — 1 deg");

    auto* taper_controls = new QWidget(this);
    auto* taper_layout = new QVBoxLayout(taper_controls);
    taper_layout->setContentsMargins(0, 0, 0, 0);
    taper_layout->setSpacing(4);
    taper_layout->addWidget(taper_angle_);
    taper_layout->addWidget(taper_slider_);
    layout->addRow("Taper Angle", taper_controls);

    connect(taper_angle_, &QDoubleSpinBox::valueChanged,
            this, [this](double value) {
        const QSignalBlocker blocker(taper_slider_);
        taper_slider_->setValue(
            static_cast<int>(std::lround(value * 100.0)));
    });
    connect(taper_slider_, &QSlider::valueChanged,
            this, [this](int value) {
        const QSignalBlocker blocker(taper_angle_);
        taper_angle_->setValue(static_cast<double>(value) / 100.0);
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &ExtrudeFaceDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &ExtrudeFaceDialog::reject);
    layout->addRow(buttons);
}

double ExtrudeFaceDialog::TaperAngle() const {
    return taper_angle_ ? taper_angle_->value() : 0.0;
}
