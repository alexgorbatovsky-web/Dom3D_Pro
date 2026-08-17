#pragma once

#include "RenderScene.h"

#include <QString>

class IExternalRenderer {
public:
    virtual ~IExternalRenderer() = default;
    virtual QString Name() const = 0;
    virtual bool IsAvailable(QString* error = nullptr) const = 0;
    virtual bool StartRender(const RenderScene& scene,
                             const RenderSettings& settings,
                             QString* error = nullptr) = 0;
    virtual void Cancel() = 0;
};

