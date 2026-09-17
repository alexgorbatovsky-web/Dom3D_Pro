# Box_Min_Box_And_Drafts: CAD vertex tolerances

Fixture: `tests/data/mesh-regression/Box_Min_Box_And_Drafts.dom3d`.
The supplied density 0.6 failed on planar face 44 of 50, with
`No closed UV contours were produced` and triangle fallback in Low Poly.

The endpoint discrepancy reaches 0.002454 mm, within shared CAD vertex
tolerances of 0.00258–0.00271 mm, but exceeds the contour join tolerance
(0.001016 mm). Replaying just the face also failed; an isolated control
replacing curve endpoints with their shared CAD vertices filled correctly.

`SetPreparedPolylinePoints` now canonicalizes endpoints to the corresponding
topological vertices only within their recorded tolerances plus float
roundoff. Endpoint orientation is matched geometrically. Partial edge/ring
endpoints away from the CAD vertices remain unchanged. No global contour
join tolerance is increased. This also applies when a neighbouring surface
donates a new boundary row or an edge is resampled.

At density 0.6, face 44 builds 222 quads with no out-of-face cell centres.
`BoxDraftsQuadro` checks densities 0.4, 0.6, 0.8, repeated 0.6, both SLX
settings at 0.6, face area/coverage and welded-body edge incidence.

All 13 related regression tests pass. This also resolves the previously
failing `BoxMinBoxFilledQuadro` test, which had the same endpoint-tolerance
problem. Both fixtures retain closed welded meshes across tested densities.
