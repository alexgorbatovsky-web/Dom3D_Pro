# Bool_And_Fill: trimmed four-edge fillet transitions

Fixture: `tests/data/mesh-regression/Bool_And_Fill.dom3d` (69 faces).
The operation history includes a radius 19.1 edge fillet, a Boolean cut,
and radius 1 fillets on all edges.

At density 0.5, the original mesher lost the top planar face's UV loop and
accepted eight out-of-face cells on the large cylindrical fillet. Both faces
could be filled successfully in isolation. Four-edge B-spline transitions
(faces 1 and 4) were being forced through the rectangular-grid compatibility
path despite their non-isoparametric pcurves. Exporting their rectangular
boundary rows changed endpoints on neighbouring faces, opening those loops.

`CSurfaceFace::HasRectangularUVBoundary` checks whether the CAD pcurves can be
assigned to four distinct sides of their UV box before allowing the structured
B-spline compatibility path. A bowed edge may stay within its side's half of
the chart; an edge crossing the whole chart cannot donate a boundary row.
This preserves existing mildly bowed four-sided fillets. Trimmed transitions
instead use the existing exact-pcurve contour filling path. The same check
is used for face classification and structured boundary donation.

Exact-pcurve projection also compares the bounded 3D curve's endpoints with
its interior stationary solutions. A prepared endpoint can lie just beyond
the curve's parameter interval within CAD/float tolerance; an empty interior
projection is not a reason to abandon the exact contour.

Validated at densities 0.35, 0.5 and 0.65, with a repeated 0.5 build and both
SLX settings at 0.5. At density 0.5 the top face has 424 quads, the large
cylindrical fillet 49, and each small transition 6. These faces have no
out-of-contour cell centres. The welded body has no open or nonmanifold edges
(collapsed singular sides are vertices and are excluded from edge counts).

Related validation: 11 of 12 selected CTests pass. `BoxMinBoxFilledQuadro`
reports "Box used an emergency mesh fallback" both with this change and in
a control build restoring the previous classification/projection behavior;
that existing failure is outside this change.

Regression command: `ctest --test-dir build -C Release -R "^BoolAndFillQuadro$" --output-on-failure`.
