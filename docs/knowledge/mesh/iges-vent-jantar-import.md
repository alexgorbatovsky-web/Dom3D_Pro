# Vent_Jantar: excessive IGES import tessellation

Reproducer: `Vent_Jantar.igs` (1,481,084 bytes, 155 independent surfaces).
Run `CatalogImportTests --benchmark-iges <path>`; set `DOM3D_PROFILE_IGES=1`
for read and per-surface timing. `--render-iges <path> <png>` renders the
actual imported objects through the viewport.

Release baseline: 321.767 s, 155 objects / surfaces, 21,177,968 mesh faces.
After correction: 0.604 s, 155 objects / surfaces, 16,131 mesh faces.
These measurements cover import and display mesh generation, not file-dialog
interaction or first-frame presentation. Reading the IGES itself took 0.012 s.

`CSurfaceSet` enables `MeshQuadro` by default. `IgesIO` passed a scale-aware
linear deflection to `BuldMesh`, but the selected Quadro pipeline interprets
its argument as normalized mesh density. The resulting grids were enormous.
IGES import now explicitly chooses display meshing (`MeshQuadro=false`),
which matches `ComputeIgesMeshDeflection`. CAD geometry stays intact; dense
Quadro generation remains an explicit later operation.

The initial suspicion about unused hybrid boundary preparation was rejected:
this input did not use the hybrid path at all. No changes to hybrid boundary
preparation were retained.

Benchmark validation checks that every imported surface has a nonempty mesh,
finite vertices, and uses display mode. Related checks: IgesShapeCollectorTests,
ParametricSmartHybrid, SurfacePatchWelding.
