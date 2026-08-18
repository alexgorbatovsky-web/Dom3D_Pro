#include "SurfaceSet.h"

CSurfaceSet::CSurfaceSet()
    : CSolid() {
    SetName("Surface Set");
    // Standalone surfaces benefit from the regular UV net used by the old
    // Dom-3D CSurface::UpdateFace path. BuildTrimmingMesh keeps the UV grid
    // for natural, untrimmed bounds and falls back to contour trimming when
    // the face is trimmed.
    MeshQuadro = true;
}

CSurfaceSet::CSurfaceSet(TopoDS_Shape& shape)
    : CSolid(shape) {
    SetName("Surface Set");
    MeshQuadro = true;
}

void CSurfaceSet::Render3d(bool selected) const {
    if (GetParametricToolId() != "PlaneTool") {
        CSolid::Render3d(selected);
        return;
    }

    // Reference planes are construction helpers, not ordinary shaded
    // surfaces.  Keep them readable from both sides and translucent so the
    // model remains visible through the plane.
    Material fill = GetMaterial();
    fill.diffuse = selected
        ? Color{0.38f, 0.52f, 0.88f}
        : Color{0.30f, 0.42f, 0.74f};
    fill.ambient = {0.12f, 0.16f, 0.30f};
    fill.alpha = selected ? 0.30f : 0.20f;
    fill.specular = 0.0f;
    fill.shininess = 4.0f;
    fill.color_texture_path.clear();
    fill.light_texture_path.clear();
    fill.bump_texture_path.clear();

    const Color border = selected
        ? Color{0.58f, 0.76f, 1.0f}
        : Color{0.42f, 0.56f, 0.90f};
    for (int index = 0; index < GetNumSurfaces(); ++index) {
        const CSurfaceFace* surface = GetSurfaceFace(index);
        if (!surface) {
            continue;
        }
        if (surface->pMesh3D) {
            surface->pMesh3D->RenderFaces(
                false, true, &fill, false, true);
        }
        surface->RenderOutline(border, {}, true, selected);
    }
}

std::unique_ptr<CAlfaObject> CSurfaceSet::Clone() const {
    TopoDS_Shape shape_copy = m_Shape;
    auto copy = std::make_unique<CSurfaceSet>(shape_copy);
    copy->SetName(GetName() + " Copy");
    copy->SetGroupName(GetGroupName());
    copy->SetVisible(IsVisible());
    copy->SetColor(GetColor());
    copy->SetMaterial(GetMaterial());
    copy->SetMaterialId(GetMaterialId());
    copy->SetParametricDefinition(GetParametricToolId(), GetParametricParameters());
    copy->CopyOperationTreeFrom(*this);
    copy->InitSurfaces();
    for (int i = 0; i < GetNumSurfaces(); ++i) {
        const CSurfaceFace* source = GetSurfaceFace(i);
        if (!source) {
            continue;
        }
        copy->SetSurfaceTextureTransform(i, source->TextureTransform);
        if (source->MaterialOverride.enabled) {
            copy->SetSurfaceMaterial(i, source->MaterialOverride.material);
        }
        if (source->MaterialOverride.coating_enabled) {
            copy->SetSurfaceCoating(i, source->MaterialOverride.coating_material);
        }
    }
    copy->ReBuldMesh();
    return copy;
}
