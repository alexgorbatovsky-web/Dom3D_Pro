# Panel from Contour

`SolidContourPanel` is exposed as **Panel** on the Solid tab. The dialog uses
the selected body/curve where available, honors a selected face, and otherwise
finds a face that can be divided by the contour. A surface can also be chosen
from the dialog's list.

The current camera direction is captured when the tool opens. Extruding the
exact curve wire in this direction and splitting the chosen face performs the
projection and boundary trimming together. The first version requires exactly
two resulting regions. The user chooses the smaller or larger region.

If an open contour leaves the face uncut at the normal 1e-6 tolerance, the
split retries with 1e-4 model units. This accommodates tiny endpoint gaps after
snapping/fitting/projection without extending the curve across real gaps.
Successful cuts and closed contours retain the original tolerance.

Gap is an inset in the view's projection plane, applied to all panel boundaries,
including holes. Intersecting the inset prism with the original patch preserves
the curved surface. It is not a constant geodesic distance on steep surfaces.
Positive recess depth offsets the resulting panel opposite its oriented normal.
Negative recess depth moves it outward along that normal. Thickness remains
one-sided inward from the displaced panel in both cases.

New panels have a positive **Thickness T** (default 1 model unit) and are closed
solid objects. Thickness extends inward from the recessed panel face using the
same one-sided offset construction as Shell. The surrounding output remains a
surface set; the other faces of the source belong to that output.
The original body is hidden, retained as the parametric source, and restored by
Undo. Both outputs retain references to the source body and contour, the face
index, projection direction, gap, depth, thickness and region choice. Replaying the contour
dependency rebuilds them. Face indexing assumes unchanged source topology.

Saved legacy panels without thickness retain the surface behavior (T=0).
Invalid offsets fail before adding outputs. Panel Bulge accepts the solid panel
and rebuilds its selected side plus its one-sided thickness, preserving a closed
body. Selecting a whole body initially chooses the largest face; selecting a
face explicitly takes precedence.

Tests: `PanelContour` covers open/closed contours, gap, holes, reversed normals,
a cylindrical panel, rejected inputs and saved parametric replay.
`PanelContourUI` covers dialog acceptance/cancellation and Undo/Redo.
`PanelContourSavedBoundary` uses the saved Panel scene and its camera direction
to check both region choices, area conservation and valid solids at T=1.
