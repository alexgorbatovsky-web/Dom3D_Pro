#include "BooleanDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

BooleanDialog::BooleanDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Tool Options");
    setModal(true);
    setMinimumWidth(260);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* label = new QLabel("Type", this);
    layout->addWidget(label);

    combo_ = new QComboBox(this);
    combo_->addItem("Add");        // Union
    combo_->addItem("Subtract");   // Cut
    combo_->addItem("Intersect");  // Common
    layout->addWidget(combo_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &BooleanDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &BooleanDialog::reject);
    layout->addWidget(buttons);

    setLayout(layout);
}

void BooleanDialog::SetSelectedOperation(Operation operation) {
    if (!combo_) {
        return;
    }
    combo_->setCurrentIndex(static_cast<int>(operation));
}

BooleanDialog::Operation BooleanDialog::SelectedOperation() const {
    const int idx = combo_ ? combo_->currentIndex() : 0;
    switch (idx) {
    case 1: return Operation::Cut;
    case 2: return Operation::Common;
    case 0:
    default: return Operation::Union;
    }
}

