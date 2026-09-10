# Sheet bend: local thickness and moving side

`Detail-1.dom3d` contains an existing right flange and a line across the small
left flange. Measuring thickness from all vertices included the upright flange
and moved the neutral line outside the material. Thickness is now measured
through the material at samples along the bend line on its supporting plane.

The creation dialog requires an explicit moving side and a successful preview.
Changing parameters restores the source shape; Cancel restores it without adding
history. Direction is still rotation viewed along the directed line. Moving side
is left/right when viewed from outside the supporting face. The saved `side`
parameter defaults to zero for old operations.

The resulting bend must remain one valid solid and conserve volume. Oversized
bend allowances that add or remove material are rejected.

Verification:

```powershell
ctest --test-dir build -C Release -R '^SheetBend' --output-on-failure
```

`SheetBendDetail` checks both moving sides and rotation directions on a flat
sheet, rejects an oversized radius, and bends the small left flange of the real
fixture while retaining the main body and existing right flange. It also replays
the operation history and compares the resulting geometry.
`SheetBendPreview` checks required side selection, preview invalidation, and
Cancel restoration in the actual dialog.

## Third bend and surface picking

The updated `Detail-1-third-bend.dom3d` has two existing bends and a new line
at Z=1.37241548 above the 3 mm sheet (Z=-3..0). Requiring a line on the outer
face rejected it. Parallel construction lines now project onto the nearest
supporting sheet face and resolve to its neutral plane. The dialog reports the
projection offset; its preview and guide show the actual bend position.
Candidate support planes must be parallel to the line, and material samples
must confirm that the projected line crosses the sheet.

The dialog's Pick the part to bend action ray-intersects the selected solid,
including clicks inside a face without a nearby vertex. The signed distance
from the clicked point to the directed bend axis selects the moving side and
starts the preview. Escape returns to the dialog. Other document commands are
blocked during the pick so the source cannot disappear while the dialog is hidden.

A yellow arc with an arrowhead shows the moving side's rotation. It uses the
same resolved axis and normal as construction and updates with angle, direction,
and side. It is a direction guide, not a radius dimension. Cancel/accept removes it.

`SheetBendThird` verifies preservation of both existing flanges, solid validity,
volume, and history replay. `SheetBendPreview` also checks surface ray picking,
pick cancellation, automatic preview after a click, and guide cleanup.

The bend dialog waits for `QDialog::finished` in a separate event loop. Using
`QDialog::exec()` here ends the command as soon as the dialog is hidden for a
surface pick. The UI regression now lets the event loop run between pressing
Pick, pressing Escape, and clicking the flange with a mouse event. The previous
test emitted the pick signal in the same callback and missed this lifetime bug.

The dialog is modeless, so the viewport remains available for navigation while
editing bend parameters. Unrelated editing commands are guarded during preview,
and autosave timer signals are paused until the dialog closes. The UI regression
also sends a wheel event to verify zoom with the parameters still open.

Combo-box popups are separate windows, so the input guard follows QObject
ownership instead of QWidget ancestry to allow their mouse and keyboard input.
The UI regression opens the direction popup, waits for its native animation,
selects the opposite direction with the mouse, then switches back by keyboard.

The direction arc disables inherited OpenGL depth and stipple state while
painting, then restores that state. A 5 px yellow stroke with black/white
outlines and a 22 px arrowhead remains visible over the sheet. The optional
`--bend-guide-capture <png>` UI-test argument captures the actual native viewport.

## Fourth bend: trimmed supporting faces

After the third upward bend, a nearby plane belonging to another flange could
win the distance comparison even though the new line did not cross that face.
Thickness detection on this empty projection then failed. Supporting-face
selection now checks projected samples against each face's trimmed interior
and measures consistent local thickness before ranking eligible candidates.

`SheetBendFourth` uses the updated user file and line 94. It verifies the 3 mm
base and its neutral plane at Z=-1.5, bends the right portion together with its
existing flange, and checks that the fixed part is unchanged. Solid validity,
volume conservation, and full operation-history replay are also checked.
