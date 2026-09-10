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
