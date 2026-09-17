# Plasticity F-1: open donor boundary

The imported F-1 STEP body has 14 valid CAD faces. Low Poly / Mesh Quadro
previously built 12 surfaces; planar faces 3 and 6 fell back to triangles.
Their island filler reported `No closed UV contours were produced.`

The four-sided planar donor (face 11) generated a structured net corner
approximately 2.05 model units beyond its CAD vertex. Synchronizing that row
into the adjacent faces opened their prepared contours. The STEP topology
itself was closed; increasing the contour-joining tolerance would mask the
incorrect mesh boundary.

The Low Poly structured-mesh passes now snap four-sided planar donors to
their prepared CAD edge samples, using the existing structured-boundary
alignment also used by compatible spline patches. Already trimmed island
meshes are excluded from tensor-row snapping and donation: retained grid
dimensions alone do not make their vertex arrays structured nets. This happens before both
the count synchronization pass and the final transfer to trimmed neighbours.
Open donor rows must also cover both endpoints of the prepared CAD edge.
This prevents a guessed row on a distorted spline net from substituting a
different edge, which previously broke face 13 at density 1.00.

Regression: `PlasticityImport` imports `Plasticity-F1.stp` directly and checks
all 14 surfaces at densities 0.10, 0.50, 1.00 and 0.50 again. It requires quad
cells on every surface and verifies that the prepared edge endpoints remain
at CAD vertices. Coarse contours may retain individual closing triangles;
this is distinct from replacing a failed surface with triangle tessellation.

Verified counts: density 0.10 produces 110 quads and 4 triangles; density 0.50
produces 2515 quads and no triangles; density 1.00 produces 10938 quads and
no triangles. All cases mesh all 14 surfaces without per-surface fallback.
