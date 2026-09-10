# Shell_Bag: strips across the opening

Fixture: `tests/data/mesh-regression/Shell_Bag.dom3d`, copied unchanged from
`C:/Users/Alex/Desktop/Shell_Bag.dom3d`.
SHA256: `DA21F0480F74C11291FF2670B8C7710644407C19E13023E5E6D9D8FD25742BC1`.

The upper BSpline rim was snapped to a prepared closed boundary in the wrong
direction. Closed endpoints cannot distinguish winding. Although the welded
mesh remained manifold and closed, rim quads crossed the opening: at density
0.50 the longest rim edge was 262.867 instead of 29.0858.

`snap_structured_mesh_to_prepared_edges` now minimizes whole-ring distances over
both windings and cyclic phases. It resolves source vertices against an immutable
copy to prevent earlier moves from changing later nearest-vertex matches.

`ShellRimQuadro` checks densities 0.25, 0.50, 0.70, 1.00 and repeat 0.50:
rim quads and their centers must stay outside the opening; surfaces must remain
connected and the welded shell must be manifold and closed. All pass, as do
PillowCadQuadro, FrameCadQuadro, WireCircularCaps and RevolveAllFilletedQuadro.
OpenGL verification: `output/shell-rim/fixed-0.50-0.png`.

The different `Bags_Mesh/Shell_Bag.dom3d` contains two bodies named Shell.
The first passes these checks; the second still has a separate seam mismatch
(28 boundary edges at density 0.50). This change does not claim to fix that body.

The user-confirmed `Bags_Mesh/Revolve.dom3d` rebuilds at density 0.70 with 43x3
vertices on both revolved fillets (84 quads each), without the reported dense
strip. Its saved Solid is hidden and a previously generated Low Poly Welded
mesh is visible. The original file is unchanged. The current rebuilt geometry
has two rows across each fillet; the saved mesh has 43 distinct upper-fillet
height levels (42 rows). Regenerating the Low Poly object is necessary: rebuilding
the source solid does not replace an existing independent mesh object.
The rebuilt geometry
was inspected via a preview copy with the Solid made visible:
`output/revolve-radius/current-0.70-0.png`.
