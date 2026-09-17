# Ball And Box: sphere / periodic fillet seam

Fixture: `tests/data/mesh-regression/Ball_And_Box.dom3d`, copied unchanged from
`C:/Users/Alex/Documents/Dom3D Pro/Ball_And_Box.dom3d`.
SHA-256: `9F633AE0739299C16FDAB92778EE2C5602A3B8C1C8DF8ECD196A961B545496AF`.

At density .35, the four-sided periodic B-spline fillet originally had 20 rows
across its short profile (400 quads). Projecting the duplicate seam into UV
gave ambiguous edge directions and the fallback paired a seam with a rim.
Pair duplicated topological seam edges together and the two remaining rims
together before using projected directions.

The clipped cube-sphere creates 40 boundary nodes while the regular strip
initially uses 20. Carry the actual spherical boundary through the natural
parameter direction of the periodic fillet, preserving the independently
chosen cross-strip row count. Give the resulting outer ring to the adjoining
planar face as its master boundary. This applies only to a four-edge B-spline
strip with a repeated seam, a shared spherical edge, and a single sphere mesh
loop projecting to a natural strip boundary.

At .35 the corrected strip has two rows / 80 quads. The welded body reports
zero boundary edges, compared with 80 before either correction.
`BallAndBoxQuadro` checks repeated rebuilds at .25, .35, .5, .7, 1, .35,
closed manifold seams, non-folded quad strip cells and the low-density budget.

Validation: BallAndBoxQuadro, PillowCadQuadro, FrameCadQuadro,
RevolveAllFilletedQuadro, ShellRimQuadro, TorusPlaneCutQuadro and
TwoSketchCornerNormals pass. SpherePoleQuadro reports a sphere/cap boundary
count mismatch both with this change and with the original HEAD Solid.cpp;
that independent pre-existing failure is not addressed here.
