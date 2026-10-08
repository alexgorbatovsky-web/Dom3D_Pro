# Planar sketch geometry and changes

Sketch geometry is authoritative in `SketchPoint { double u, v; }`. Endpoints are
shared `SketchNode` objects owned by the sketch; adjacent connected links refer to
the same node. Arc grips are UV points. Bezier handles are UV offsets from their
anchors, so moving a shared anchor through either neighbouring link carries its
handle once. World coordinates are derived as `origin + u*x_axis + v*y_axis`.

`CPoint3d` getters return computed `(u,v,0)` values for existing CAD algorithms.
`P()` is read-only; its result is a compatibility cache, never editable geometry.
Detached links can be constructed normally. Attached links require a sketch edit
transaction for modification; untracked writes are rejected before changing data.

Public sketch operations open nested transactions automatically. For a compound
change, call `BeginEdit()`, perform changes, then `CommitEdit()` or `CancelEdit()`.
The outer transaction owns one snapshot. Constraints and curve validation run before
commit; a failed operation invalidates the transaction and restores its snapshot.
Snapshots have independent nodes. Rollback preserves line and node identities.

`GetRevisions()` separates local geometry, topology and plane placement. Commit
compares these aspects with the original state and calls `SetChangeCallback` once
with the changed aspects. A no-op produces no revision or callback. Placement-only
changes do not rerun the constraint solver. An internal generation invalidates
display geometry during previews as well as after rollback.

The viewport brackets a sketch drag with a transaction. Escape cancels it. Mouse
release records one Undo command and rebuilds the sketch's profile/trim dependents
and associative clones. Undo/Redo restore geometry on the same sketch object.

Project XML continues to read legacy local `x1,y1,x2,y2` values (they mean UV), and
now writes a `coordinates="uv"` marker and persistent link/node IDs. Existing files
without IDs receive them on loading. No independent world-point array is saved.
World-profile conversion accepts plane residuals up to 1e-6 model units; larger
deviations are rejected with `GetLastGeometryError()` rather than silently flattened.

Checks: `SketchGeometryTests`, `SketchProfileBuilderTests`,
`SketchFeatureShapeBuilderTests`, and `SketchTransactions`. They cover planar
segments/arcs/Bezier curves, shared anchors, no-op/commit/cancel, invalid edits,
stable IDs, repeated transforms, independent copies, mouse dragging, Escape,
single-command Undo/Redo and project round trips.

## Whole sketch figures

Circle, ellipse and regular-polygon drawing tools retain a `SketchPrimitive`
definition in UV: center, radius / two semi-axes, angle in radians and side count.
The existing SmartLine links are derived compatibility geometry; direct node,
handle, constraint, fillet and segment edits are rejected for whole figures.
In sketch edit mode, double-click the outline to edit its parameters. The dialog
uses the existing document change path for dependent rebuilding and Undo/Redo.

Primitive definitions survive contour insertion, copying, transactions and project
XML round trips. Older contours without this definition remain ordinary editable
contours; they are not guessed to be primitives. Circle/ellipse display samples are
evaluated analytically, and OCC profiles use one exact circular/elliptic edge.

`TrimCircle(start_angle, sweep)` retains a counterclockwise circular interval as
one exact `CSketchArcLine`, discarding the whole-figure definition atomically.
This is the geometry entry point for subsequent sketch cutting UI; the parameter
dialog itself does not cut figures. Ordinary double-click insertion must not
implicitly split a whole figure.

`MultiSketch` checks primitive creation, rejected individual-node edits, atomic
validation/cancel, exact conic area, serialization, circular trimming, and radius
dialog Undo/Redo.

### Live primitive editing and two-point conics

Whole figures expose a center grip and dimension grips in the viewport. The
polygon radius grip also controls rotation. Conics show their oriented frame,
axis endpoints and corner; the circle radius grip preserves a circle, while its
second-axis or frame-corner grip can create an ellipse. Holding Shift on the
corner makes the two radii equal. Derived segment nodes remain inaccessible.

Drags use the original figure definition and the existing sketch transaction:
preview while dragging, one Undo on release, full rollback on Escape. The moving
figure's own vertices are excluded from snapping. Picking includes center/frame
grips, so these work after leaving and reentering a multi-contour sketch.

The Sketch panel now has one Ellipse / Circle button. P1 sets the center and P2
sets the frame corner in the sketch's UV axes. Shift uses the larger absolute
UV extent for both radii. Pressing/releasing Shift updates a stationary preview;
the final mouse press determines whether to save a circle or ellipse.

### Nonmodal parameters, conic arcs and Pie

Double-clicking a primitive outline opens a nonmodal tool window. Changes apply
immediately through the standard document/Undo path. The window follows grip
previews and Undo/Redo, resolves the current object/contour by stable identity
instead of retaining geometry pointers, and closes when that editing context ends.
Closing the window keeps changes; use Undo to revert them.

Conics additionally retain start and sweep angles (radians) and a Pie flag. The
editor exposes degrees and Full figure / Arc / Pie modes. A partial arc is open;
a partial Pie adds two straight radii to the center. A full sweep has no redundant
radial edges. Orange endpoint grips change the angular interval. Circles and
ellipses both support these modes. XML defaults preserve older full conics.

Rendering samples the analytic interval, while OCC uses one exact trimmed conic
edge, plus two edges for a partial Pie. Ellipses whose second semi-axis is larger
adjust both the OCC axis frame and the parameter interval. Primitive insertion
keeps open arcs separate instead of automatically joining them to other contours.


### Sketch contour Boolean operations

The Sketch panel provides **Union contours** and **Subtract contours**. Click two closed, non-construction boundaries in the same sketch; subtraction is first minus second. The first operand is highlighted cyan. Empty clicks and repeated selection of the first operand do not complete the operation. Esc cancels without changing geometry. The two operands are replaced in one document history step, leaving all other contours and the sketch placement/attachment intact. Results may contain holes, disconnected contours, or no contours (complete subtraction).

`CSketch::BooleanContours` uses planar OCC Fuse/Cut, simplifies shared boundaries, and stages all output before mutation. Uncut source boundaries keep primitive parameters/constraints. Cut circles become analytic editable arcs. Other cut curves use the shared `AppendSketchEdge` converter with a whole-interval fitting tolerance of 1e-5 sketch units for cubic segments. Section Sketch uses the same converter with its original .01 tolerance. The first operand ID is reused for the first result, further boundaries receive fresh IDs.

Regression coverage in MultiSketch includes both reference examples, holes, disjoint and coincident operands, reversed source winding, tilted sketch placement, elliptical cuts, invalid-input atomicity, real viewport clicks, cancellation, Undo/Redo and project round-trip.


### CSketch smart menu

The delayed sketch quick menu accepts both CSmartLine and CSketch. Closed-profile actions inspect all non-construction contours, so an open working contour disables Extrude/Cut while construction geometry does not. Empty sketches retain transform/copy actions. Sweep requires exactly one closed non-construction contour; its temporary world-space CSmartLine adapter preserves the CSketch as the referenced document owner. Creation and parameter/dependency rebuilds resolve that owner again, without replacing the source sketch or discarding other construction contours.


### Curves: numeric point editor and node types

In Curves point editing, selecting one Polyline/BSpline/Bezier/NURBS point opens the nonmodal XYZ editor beside the tools panel. Numeric changes share viewport point history and endpoint-link/dependency rebuilding. The editor follows drags and selection, and hides when editing ends. A stationary unmodified RMB click on the selected node cycles Corner → Control → Smooth → Corner; RMB navigation gestures elsewhere remain unchanged.

Mixed node curves store authored positions and `CurveNodeType` values on CBSpline. Corner nodes lie on the curve with independent one-sided tangents. Control nodes influence a smoothed anchor away from their position. Smooth nodes lie on the curve and share a tangent. Cubic spans drive both evaluation and OCC wire/export construction. Polyline conversion retains object ID and appearance; Bezier anchors become authored nodes. Bezier control handles are edited numerically and are not node-type targets. Explicit polyline fillets must be removed before type conversion. XML and legacy stream serialization preserve node types; untyped files keep their original evaluation.


### Shared point input

`BeginPick3DPoint`, `BeginPick3DPointOnPlane`, and `BeginPickXYPoint`
use `OpenGLViewport::PickRequestedPoint` for both hover feedback and acceptance.
The XY entry point reuses the plane-constrained initialization; Text keeps its
existing XY result signal. Explicit point input takes priority over plain Orbit
clicks and uses a crosshair, switching to the capture cursor at enabled snaps.
Plane-constrained input projects captured geometry onto the requested plane.
Use these entry points for new point prompts instead of separate mouse handlers.


### Chamfer edge persistence

ChamferSolid shares FilletEdgeIdentity with fillet_edge. Accepted operations
capture geometric edge references from the unmodified live-operation base,
with duplicate face references removed in the same order as builder edges.
Replay resolves those references on rebuilt geometry. Missing or ambiguous
edges fail transactionally without replacing the last valid body. Extrusion
height changes transport the references using the existing fillet mechanism.
Legacy project loading upgrades index-only ChamferSolid operations only when
replay preserves the saved body's area and volume. ChamferEdgeIdentity covers
UI capture, migration, contour topology changes, height edits, save/reload and
rollback when the referenced edge cannot be resolved.
