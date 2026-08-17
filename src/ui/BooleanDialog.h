#pragma once

#include <QDialog>

class QComboBox;

class BooleanDialog : public QDialog {
    Q_OBJECT
public:
    enum class Operation {
        Union = 0,
        Cut = 1,
        Common = 2
    };

    explicit BooleanDialog(QWidget* parent = nullptr);

    void SetSelectedOperation(Operation operation);
    Operation SelectedOperation() const;

private:
    QComboBox* combo_ = nullptr;
};
