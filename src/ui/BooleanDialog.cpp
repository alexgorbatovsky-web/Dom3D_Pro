#include "BooleanDialog.h"

#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>

BooleanDialog::BooleanDialog(QWidget* parent)
    : QDialog(parent, Qt::Tool) {
    setObjectName("BooleanToolOptions");
    setWindowTitle("Tool Options");
    setModal(false);
    setMinimumWidth(260);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* label = new QLabel("Type", this);
    layout->addWidget(label);

    combo_ = new QComboBox(this);
    combo_->setObjectName("BooleanOperationType");
    combo_->addItem("Add");        // Union
    combo_->addItem("Subtract");   // Cut
    combo_->addItem("Intersect");  // Common
    layout->addWidget(combo_);

    connect(combo_, &QComboBox::currentIndexChanged, this, [this]() {
        emit SelectedOperationChanged(SelectedOperation());
    });

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

