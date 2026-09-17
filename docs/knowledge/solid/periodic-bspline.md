# Closed B-splines use control points

New closed B-splines without explicit knots use a uniform periodic B-spline
basis of the selected degree, evaluated with de Boor's algorithm. The periodic
NURBS evaluator is shared, with unit weights for non-rational B-splines.
The control polygon remains unchanged when closing a curve. Cubic curves have
a smooth periodic seam; degree-one curves intentionally follow the polygon.

Existing closed curves without an explicit knot vector previously used
Catmull–Rom interpolation. Loading an unversioned closed B-spline preserves
that behavior via `legacy_closed_interpolation_`. XML stores
`closedBSplineMode`; the text stream stores an additional mode value after the
point count. Clones preserve the mode. Opening an old curve and closing it
again explicitly switches it to the new control-point behavior.

Explicit knot vectors, Bezier curves and rational NURBS keep their existing
evaluation paths. Open clamped B-splines continue to pass through their two
end poles but generally not their interior poles.

`PeriodicBSpline` checks degree 1/2/3, the cubic basis, seam continuity,
convex-hull containment, unit weights, cloning and XML/text roundtrips,
including the old closed curve in `Swept-2.dom3d`.
