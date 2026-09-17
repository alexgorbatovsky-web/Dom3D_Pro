#pragma once
#include <QDialog>
#include "../mesh/ImageRelief.h"
class CSolid;
class ImageReliefDialog : public QDialog {
public:
    ImageReliefDialog(const CSolid& solid, QWidget* parent = nullptr, int surface_index = -1);
    void SetImage(const QImage& image);
    void reject() override;
    std::unique_ptr<CMesh3D> TakeMesh() { return std::move(result_.mesh); }
    bool HideSource() const;
private:
    image_relief::Result result_;
    class QCheckBox* hide_source_ = nullptr;
    std::function<void(const QImage&)> image_changed_;
    std::function<void()> cancel_build_;
    bool busy_ = false;
};
