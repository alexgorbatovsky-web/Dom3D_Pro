# Rounded opening collars

Planar faces with a single inner wire composed of straight edges and circular
arcs now receive an explicit one-row quad collar even when the opening is
larger than the small-hole threshold. The sampled contour must be convex.
Offsets follow local outward angle bisectors rather than rays from the center,
so the row follows straight walls and rounded corners. A uniform width is
limited by the background step and clearance to the outer boundary.

Ordinary Quadro uses the explicit collar / island-fill path. SLX uses its
filled background and hole cutter, with a two-ring construction: the inner
quad row remains intact and the outer ring defines the cut. Its width budget
is 1.25 background steps, capped by clearance, leaving room for the transition
to the actual cutter boundary. The presence of cutter triangles alone no
longer rejects this rounded-opening result; stitching and area validation
remain active. Other hole types retain their existing paths. Multiple openings
are not included in this new rounded-opening recognition yet.

Fixture `tests/data/mesh-regression/Box_Min_Filleted_Box.dom3d` is copied
unchanged from `Desktop/Notes_2022/Dom-3D_Pro/Bags_Mesh`.
`RoundedOpeningCollar` exercises .2, .35, .5, .7, 1, .35 with SLX off/on,
checks welded manifold closure and a non-folded quad adjoining every prepared
opening boundary edge. It also requires the selected SLX path to complete and
compares mesh area with the CAD face to detect loss/overlap. A successful test
with the SLX checkbox is insufficient unless the cutter actually produced the
mesh (`UsedSlxHoleCut`). Local previews are in `output/rounded-opening`.

RoundedOpeningCollar, BallAndBoxQuadro, SmallHoleSlxCollar,
HoleLowDensityCollar, FourHoleCollars and BoxMinBoxFilledQuadro pass.
BezierCollarWelding reports a cell-count mismatch both before and after this
change; confirmed by reversing only the rounded-collar patch and rebuilding.
