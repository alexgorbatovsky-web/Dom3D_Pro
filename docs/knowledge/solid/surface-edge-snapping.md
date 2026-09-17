# Surface edge snapping

`OpenGLViewport::SnapCreationPoint` includes visible CAD edges in the **Line**
target. This covers solids and `CSurfaceSet`, including trimmed boundaries.
Edges are deduplicated by topology. Screen-space local minima are located by
sampling and refined on `BRepAdaptor_Curve`; the returned point lies on the CAD
curve rather than a display-mesh chord. Endpoint candidates remain available.

Near a boundary, Line takes priority over Surface. Otherwise Surface retains
its existing nearest-face behavior. Modeling clicks and freehand previews use
the same snapping path. Work-plane constraints and explicit body restrictions
are respected. A ray from the candidate toward the camera rejects edges hidden
behind visible CAD faces. Hidden objects are excluded.

`SurfaceSnapping` checks planar and circular edges, both camera projections,
Line with and without Surface, hidden objects, occlusion, disabling Line and
the freehand picking path. `PanelContour` additionally verifies an open contour
whose endpoints are exactly on the face boundary: no extension is required.
This does not establish that every imported panel fixture will split correctly.
