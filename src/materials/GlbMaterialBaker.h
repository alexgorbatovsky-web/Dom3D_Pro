#pragma once
#include <QImage>
struct RenderMesh;
struct Material;
struct GlbBakedMaps { QImage color, normal, orm, emission; };
// Rasterizes the viewport's procedural shaders over a per-triangle UV atlas.
// Updates only the export snapshot, never the document geometry/materials.
GlbBakedMaps BakeGlbMaterial(RenderMesh& mesh, const Material& material,
    const QImage& color = {}, const QImage& normal = {}, const QImage& emission = {});
