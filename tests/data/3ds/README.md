# Legacy 3DS normal regression

Generated two-triangle fixtures without smoothing-group chunks. Shared positions
use distinct vertex indices, as at a UV seam. With the 40-degree threshold,
crease10, crease30 and crease39 must smooth the four shared corners;
crease41 and crease50 must retain all six flat corner normals.

Run `CatalogImportTests --check-3ds-normals <fixture> smooth|flat <output.dom3d>`.
Use `smooth` for crease10/30/39 and `flat` for crease41/50. The same diagnostic accepts a
private real-world 3DS and saves its imported meshes for visual inspection.
