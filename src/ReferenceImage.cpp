#include "ReferenceImage.h"

#include <utility>

CReferenceImage::CReferenceImage()
    : CMesh3D("Reference Image") {
}

CReferenceImage::CReferenceImage(std::string name)
    : CMesh3D(std::move(name)) {
}

std::unique_ptr<CReferenceImage> CReferenceImage::Create(
    const std::string& image_path,
    ReferenceImageAxis axis,
    float width,
    float height) {
    if (image_path.empty() || width <= 0.0f || height <= 0.0f) {
        return {};
    }

    auto image = std::make_unique<CReferenceImage>();
    const Vec3 u = axis == ReferenceImageAxis::X
        ? Vec3{0.0f, 1.0f, 0.0f}
        : Vec3{1.0f, 0.0f, 0.0f};
    const Vec3 v = axis == ReferenceImageAxis::Z
        ? Vec3{0.0f, 1.0f, 0.0f}
        : Vec3{0.0f, 0.0f, 1.0f};
    const Vec3 half_u = u * (width * 0.5f);
    const Vec3 half_v = v * (height * 0.5f);
    std::vector<Vec3> vertices{
        half_u * -1.0f + half_v * -1.0f,
        half_u + half_v * -1.0f,
        half_u + half_v,
        half_u * -1.0f + half_v
    };
    std::vector<UV> uvs{
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}
    };
    CMesh3D::Face face;
    face.corners = {{0, 0, 0}, {1, 1, 1}, {2, 2, 2}, {3, 3, 3}};
    std::vector<CMesh3D::Face> faces;
    faces.push_back(std::move(face));
    if (!image->SetGeometry(
            std::move(vertices), std::move(faces), std::move(uvs))) {
        return {};
    }

    Material material = Material::DefaultTexture();
    material.name = "Reference Image";
    material.diffuse = {1.0f, 1.0f, 1.0f};
    material.ambient = {1.0f, 1.0f, 1.0f};
    material.specular = 0.0f;
    material.shininess = 4.0f;
    material.alpha = 1.0f;
    material.texture_fit_to_surface = true;
    material.color_texture_path = image_path;
    image->SetMaterial(material);
    image->SetColor({0.12f, 1.0f, 0.18f});
    return image;
}

void CReferenceImage::Render3d(bool selected) const {
    const Material material = GetMaterial();
    const bool zebra_target = CMesh3D::IsZebraAnalysisTarget();
    CMesh3D::SetZebraAnalysisTarget(false);
    RenderFaces(false, false, &material, false, true);
    CMesh3D::SetZebraAnalysisTarget(zebra_target);
    if (selected) {
        const Color selection{0.12f, 1.0f, 0.18f};
        RenderWire(true, true, &selection);
    }
}

std::unique_ptr<CAlfaObject> CReferenceImage::Clone() const {
    auto copy = std::make_unique<CReferenceImage>(GetName() + " Copy");
    copy->SetGeometry(GetVertices(), GetFaces(), GetUVs(), GetNormals());
    copy->SetGroupName(GetGroupName());
    copy->SetVisible(IsVisible());
    copy->SetColor(GetColor());
    copy->SetMaterial(GetMaterial());
    copy->SetMaterialId(GetMaterialId());
    return copy;
}
