#pragma once

#include "../render/RenderScene.h"

#include <QDialog>

#include <vector>

class QSettings;

class RenderLightEditorDialog final : public QDialog {
public:
    explicit RenderLightEditorDialog(std::vector<RenderLight> lights,
                                     QWidget* parent = nullptr);
    ~RenderLightEditorDialog() override;

    std::vector<RenderLight> Lights() const;

    static std::vector<RenderLight> Defaults(const RenderScene& scene);
    static std::vector<RenderLight> Load(
        QSettings& settings, const std::vector<RenderLight>& defaults);
    static void Save(QSettings& settings,
                     const std::vector<RenderLight>& lights);

private:
    struct Controls;
    std::vector<Controls*> controls_;
};
