#pragma once

#include <QDialog>

class QDoubleSpinBox;
class QSlider;

class ExtrudeFaceDialog : public QDialog {
    Q_OBJECT
public:
    explicit ExtrudeFaceDialog(QWidget* parent = nullptr);

    double TaperAngle() const;
    void SetCurvedFace(bool curved);

private:
    QDoubleSpinBox* taper_angle_ = nullptr;
    QSlider* taper_slider_ = nullptr;
};
