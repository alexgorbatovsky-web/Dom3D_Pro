# Curve node release: scoped dependency replay

The `MainWindow` handler for `TakeCurvePointDragChange` used to replay every
endpoint-linked curve in the document, then the edited curve. An independent
spline in `Frame/Car-2.dom3d` consequently rebuilt the Smart Hybrid body,
four panel outputs, bulged panel and shell. The measured handler time was
12,876 ms with four unrelated endpoint links, versus 1 ms after the fix.

The handler now always replays the edited curve and adds endpoint peers only
when that endpoint changed between the captured before/after point arrays.
Traversal follows individual endpoint pairs, not both ends of each encountered
curve. Curve IDs are deduplicated. The same dependency set is used for Undo and
Redo, which first restore points and synchronize linked ends. Existing actual
profile, trim and clone dependency replay remains in place.

`CurveReleaseDependencies` uses `tests/data/ui-regression/Car-2-CurveRelease.dom3d`
and verifies that editing its last, independent spline leaves every CAD shape
unchanged. No hardware-dependent timing assertion is used.
`PanelContourUI` also covers an unrelated linked panel, moving an interior node
of a linked peer, moving its connected endpoint, and endpoint Undo/Redo.

For timing another file, set `DOM3D_CURVE_RELEASE_INPUT` and run
`CatalogImportTests --test-scene-persistence --curve-release-profile -platform offscreen`.
It edits a copy in memory, prints the last spline's release-handler time and
changed CAD objects, and never saves the input file.
