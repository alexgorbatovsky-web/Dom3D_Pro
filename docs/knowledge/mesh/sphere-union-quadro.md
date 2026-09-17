# Sphere union: physical grid spacing and shared CAD boundaries

Fixtures copied unchanged from `C:/Users/Alex/Desktop/Notes_2022/Dom-3D_Pro/Bags_Mesh/`:

- `Ball_Plus_4_Ball.dom3d`: SHA-256 `1FE5D5911D8BFDF2D5819C1E708C747AF222131879AC2B05684937E705553AC2`.
- `Bal_Plus_Ball.dom3d`: SHA-256 `91D39C1469DA351E94EDCA6E9EF493595B75010541DC28BD26449C5B8BC88A93`.
- `Ball_Plus_4_Ball_Filleted.dom3d`: SHA-256 `8E8B820549932925C2918FF60BA5180C87BA365F0084BA94124DB8C997730ABE`.

At density 0.50 the old five-ball union had 222 open mesh edges after welding.
The cube-sphere builder selected subdivision counts independently of radius.
Both sides clipped their own grid against the CAD face, producing different
intersection samples. One small sphere is itself divided into two CAD faces.

`SphereIntersectionMesh.h` is private implementation included by `Solid.cpp`
after its cube-sphere clipping helpers. The new transaction handles closed
bodies consisting of forward, direct spherical faces, optionally joined by
rectangular BSpline fillet patches. Fillets must have four isoparametric sides
and two rims adjoining spheres. Other mixed bodies, standalone spheres,
cavity/mirrored orientations and SLX continue through the previous paths.

The largest sphere establishes physical spacing; the other subdivisions scale
with radius, with a four-segment curvature floor. Draft cube grids contribute
boundary sample demands. Shared CAD edge identity determines adjacency;
supporting circles combine split arcs of those shared edges. CAD endpoints
are retained when a cut cell crosses an arc junction. One frozen sample list
is evaluated on each circle and consumed by both final faces. The identification
budget for legacy float/UV clipping is separate from node deduplication and
does not act as a welding tolerance.

Boundary polygons receive the shared samples and are split into convex quads
where possible, otherwise triangles. The unaffected background quad grid stays
intact. This is a conforming, predominantly quad mesh, not an all-quad collar
or a regular strip of equal-length seam segments. Very short boundary edges
can remain. Curved cuts that would fold a coarse transition trigger a bounded
retry with a finer background grid (at most eight additional subdivision
increments). A collapsed clipping sliver or a failed exact seam audit also
triggers this bounded retry; no failed draft is published. Unresolved cases
return an explicit failure.

The transaction removes collapsed cells and unused background vertices,
preserves spherical normals and periodic UVs, and validates exact float XYZ
incidence and opposite edge orientations before publishing any face. It does
not use `CreateWelded` tolerance to prove seam closure.

`SphereUnionQuadro` and `TwoSpheresQuadro` test densities .25, .35, .45, .50,
.65, .70, 1.0, .50 and .25. Checks cover exact oriented shared edges, a single
connected component, nondegenerate triangles, nonfolded quads, only triangle/
quad cells, a predominantly quad result, CAD area agreement, spherical point
residuals, comparable median interior edge lengths and deterministic repeat
builds. Mixed-body regressions include BallAndBoxQuadro, BallAndCylinderQuadro,
BoolAndFillQuadro, PillowCadQuadro and ShellRimQuadro.

At .50 the five-ball fixture now has zero boundary edges and zero boundary
loops. Diagnostic JSON, welded OBJ, a rendered mesh preview and the validation
log are under `output/ball-four/`.

The filleted fixture has 14 CAD faces, including split spherical faces, full
periodic collars, and very short BSpline patches. Its original density .50
mesh had 278 open edges and five boundary loops. Several apparent circular
rims are actually BSpline CAD curves. Those curves are evaluated directly;
the mesher does not replace them with fitted circles. Closest-point searches
wrap across closed-curve parameter seams. A clipped cell spanning several
small CAD arcs follows a path through their common endpoints.

Fillets receive rectangular grids on their original CAD surfaces, with the
same physical sizing target and a curvature floor (at most .25 radians of
accumulated boundary tangent turn per requested interval). Their boundary demands
join the sphere demands before master nodes are frozen. Periodic seams are
identified explicitly. Boundary cells are split with the local CAD normal;
surface normals and UVs are retained. Exact incidence is checked across all
14 faces before any mesh is published. Short edges and transition triangles
remain possible, as with the non-filleted union.

`FilletedSphereUnionQuadro` also checks surface point residuals against the
original CAD surfaces, area agreement, exact oriented closure, cell quality,
the sphere spacing ratio, connectivity and repeated density builds.

At density .50 the filleted fixture has 1,501 quads and 398 transition
triangles, zero open edges, zero boundary loops, and no cell centroids outside
their CAD faces. The sphere median interior spacing ratio is 1.0528. Diagnostics,
OBJ meshes and the rendered preview are under `output/ball-fillet/`.
