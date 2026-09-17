# Mixed Low Poly after a partial Quadro result

The Low Poly dialog now keeps successful per-surface meshes and fills only
failed or empty surfaces with OCCT triangles. `IsInitMesh` must be true and
the mesh must contain live polygons: provisional cells left by a failed
quadrangulator are not treated as success. This completion is local to the
dialog; `CSolid::BuildQuadroMesh` retains its existing success/failure contract.

The preview reports retained Quadro surfaces and fallback surface counts.
Its tooltip lists fallback face numbers (one-based), grouped by body. Create
asks whether to accept the mixed preview, with Cancel as the default. Cancel
adds no mesh and leaves the preview available. If any surface is still absent,
creation is blocked. The export helper also rejects incomplete surface meshes
instead of silently skipping them.

Successful Quadro output can itself contain transition triangles; the counts
describe the algorithm used per CAD surface, not a promise of 100% quad cells.
Completion preserves successful meshes exactly. It does not introduce a new
shared-node mesher for the triangle/quad interface or claim manifold closure.

`MixedLowPolyCompletion` covers intact Quadro, a simulated partial failure
with provisional cells, preservation of a successful face, and rejection of
a face whose CAD geometry is missing. The optional CLI fixture argument runs
the same completion on `Imported STEP 4` at density .20 with SLX enabled:

```
CatalogImportTests --test-mixed-low-poly tests/data/mesh-regression/Hairdryer.dom3d
```

Verified on 2026-09-11: the test passes; the fixture reports 80 retained
surfaces, 5 triangle fallback surfaces and 0 missing surfaces. Release
CatalogImportTests and Dom3D_Pro builds succeeded. This fixture check covers
surface completeness, not a new manifold validation of the mixed interfaces.
