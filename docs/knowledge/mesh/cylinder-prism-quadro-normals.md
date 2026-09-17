# Cylinder/prism Quadro: corner attributes during boundary synchronization

Fixture: `tests/data/mesh-regression/Cylinder_Min_Prizm_BAD_Quadro.dom3d`.

At density 0.5 the cylindrical face had correct cell winding, but 552 corner normals differed from its analytic CAD normal. Newly inserted quad strips displayed abrupt colour bands.

`CMesh3D::SynchronizeBoundaryVertices` constructed `MeshCorner` with UV and normal indices reversed. It also interpolated attributes only when their array length matched the position count, although mesh corners support independent attribute indices. The insertion now assigns the named fields and appends interpolated attributes using the source corner indices independently.

CTest `CylinderPrismQuadroNormals` checks a quad strip with shared UVs and independently indexed normals, then checks the fixture at densities 0.25, 0.5, 0.75, 1.0 and repeated 0.5. Normals are compared with the oriented analytic cylinder; cell winding is checked separately. The fix does not move vertices or change cell connectivity.

## Conforming window and circular seams

The subsequent seam audit found 120 unmatched edges at density 0.5. The cylinder used 26 angular segments while the adjacent fillet used 30; inserting the fillet's nodes after caps were built propagated extra strips to the other end without updating its cap.

`CylinderWindowMesh.h` constructs one angular chart through the two window corners for a cylinder with a coaxial circular fillet and a four-edge planar window. The chart keeps the fillet's angular count and the window wall's edge counts. The two window profiles are evaluated on their CAD curves. Cylinder, fillet, window walls and circular caps receive the same boundary nodes. Mesh and prepared-boundary changes are rolled back if the resulting welded body is not closed. Unsupported configurations retain their existing meshing path.

Circular CapRetopo uses the exact CAD circle centre, rather than the biased average of nonuniform boundary samples. This preserves its radial inner rings.

The regression additionally checks oriented two-owner edges, folded/collapsed polygons, normals, and repeated rebuilding at densities 0.1, 0.25, 0.5, 0.75, 1.0, 0.5 on original, rotated and mirrored geometry. At 0.5 the cylinder has 238 quads with zero unmatched edges (previously 348 cells and 120 unmatched edges).
