# Spatial curve drawing plane

Free 3D Polyline, B-Spline, Bezier and NURBS creation locks a view-parallel
plane through the first accepted point. A snapped point may leave that plane;
subsequent free points return to the original plane. Camera navigation does
not redefine it. Explicit XY/work-plane modes retain their own constraints.

The modern command uses `PickModelingPoint` for clicks and rubber-band motion.
Its plane is captured by `SetSpatialCurvePreviewPoints` when the first point
arrives, and cleared when the command ends or its points are emptied.
Hover feedback uses the same modern projection instead of the legacy active
curve path. Snap lookup uses the modern curve ID and excludes its latest node,
without invoking legacy getters that can create an active curve.

Node editing already freezes the view plane through the grabbed node at mouse
press. If navigation makes that plane unprojectable, movement now pauses rather
than falling back to a different plane.

Reported scene: `C:/Users/Alex/Documents/Dom3D Pro/Suface/Loft.dom3d`.
Existing saved curve coordinates are not flattened: off-plane nodes may be
intentional snaps.

Regression command:

```
CatalogImportTests --test-scene-persistence --curve-plane-only -platform offscreen
```

Checks four spatial curve types in orthographic and perspective views, first
point plane retention after an off-plane point and camera translation,
preview/click agreement, command cleanup, off-plane node snapping and the
node-edit plane anchor.

## Hidden-grid regression

Grid capture used the world floor even when that grid was hidden. With Line
snapping enabled, the pencil also entered the shared snap search and captured
these invisible floor points. `SnapSketchGridPoint` now requires the grid to be
visible. Synthetic alignment guides project their anchor onto the locked curve
plane, so a preceding off-plane geometry snap cannot change their depth.

`HiddenGridCurveSnapping` checks each applicable snap type and their combination
on all four spatial curve types and pencil strokes, in both projections. It
also verifies hidden-grid rejection and visible-grid capture. Use
`--camera-fixture <project.dom3d>` with `--hidden-grid-only` to repeat with a saved
camera (validated with the reported Loft file). Explicit WorkPlane mode remains
a separate constraint. The tests use temporary preferences.
