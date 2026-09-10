# Solid centerlines

`CSolid` owns persistent `SolidCenterline` records: a stable ID, rotation/hole/path kind,
world-space double-precision points, and a closed flag. These are independent of mesh
edges and remain available when the display mesh is regenerated.

Cylinder, sphere, torus, revolve and polyhedron builders generate rotation axes.
Hole operations use distinct operation IDs and honor blind depth. Wire stores its
actual sweep spine, sampled with a 0.01 model-unit deflection. Replaying an operation
replaces its lines instead of accumulating duplicates.

Solid transformations and clones carry the lines with the geometry. Boolean results
inherit operand lines; a cutter's rotation axis becomes a hole axis on subtraction.
Fillet rebuilds preserve the source lines.

Project XML stores a versioned `centerlines` element alongside geometry, including
packed geometry references and saved boolean tools. Legacy parametric projects
recover missing axes from supported creation/hole/transform histories on load,
without rebuilding their BRep. Unrecognized imported geometry is not inferred from
its display triangulation.

Drafting projects this geometry separately into `centerLines` on each model view.
Lines use a 0.18 mm dash-dot pen; each open endpoint extends by 5% of the projected
path length. Axial cylinder projections cross the circle and extend by 5% of its
diameter at each end. Unmatched axial projections retain a small center mark.
Hidden-edge visibility does not control axes. Version 2 regenerates previously
cached centerlines using these proportional extensions.
Opening a sheet revalidates its axes even with a current cache version: an empty
cache may predate recovered solid metadata. Loading stored hole axes also reconciles
their span with coaxial cylindrical walls, so old face picks do not truncate them
after an extrusion edit.

The 3D viewport draws the same lines in an overlay after opaque/transparent
geometry: a thin dash-dot line in the solid's color (selection color when selected),
with 5% end extensions. Hidden objects/layers contribute neither drawing nor line
search targets. Straight centerlines participate in rotation-axis line picking;
creation/measurement point capture includes endpoints, straight-axis midpoints and
nearest points on the path. Preview transforms use transient coordinates and leave
saved axis coordinates unchanged until commit.
Centerlines participate in model signatures, refresh, sheet persistence and export;
they do not change the geometry center used by dimension references.

Regression: `ctest --test-dir build -C Release -R SolidCenterlines --output-on-failure`.
The test covers creation, hole edits, history replay, transforms, cloning, boolean
subtraction, save/load, legacy recovery and rendered/PDF drawing output.
