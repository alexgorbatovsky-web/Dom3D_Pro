#pragma once

#include "CMesh3D.h"

#include <memory>
#include <string>

enum class ReferenceImageAxis { X, Y, Z };

class CReferenceImage final : public CMesh3D {
public:
    CReferenceImage();
    explicit CReferenceImage(std::string name);

    static std::unique_ptr<CReferenceImage> Create(
        const std::string& image_path,
        ReferenceImageAxis axis,
        float width,
        float height);

    void Render3d(bool selected) const override;
    std::unique_ptr<CAlfaObject> Clone() const override;
};
