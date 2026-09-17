# Extract Face

`ExtractFaceTool` is available on the Solid tab and in the Solid Edit menu.
Starting the tool clears the previous selection and requests a face pick.
Clicking a face copies its CAD geometry into a separate single-face SurfaceSet;
the source body remains visible and unchanged. The copy is independent and
can be edited as a surface. Esc cancels the pick; Undo/Redo covers creation.

`ExtractFaceUI` checks deferred picking, cancellation, independent copying,
source visibility and Undo/Redo through the viewport selection signal.
