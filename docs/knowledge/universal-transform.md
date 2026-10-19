# Universal Transform

Transform activates its own `transform` command key, separate from the dedicated Move, Rotate and Scale commands. It displays translation arrows, rotation arcs, a screen-plane move ring and a central white uniform-scale cube simultaneously. The central cube takes priority for scaling; the surrounding ring area remains available for screen-plane movement.

Picking a handle chooses the drag operation without changing the active command. Existing preview and commit paths apply to objects, groups and selected curve points. Numeric dialogs and dedicated commands retain their original single-operation gizmos. The current axes are world axes; the normal-based frame of Smart Hybrid Extrude is separate.

Regression: `CatalogImportTests --test-scene-persistence --universal-transform` locates handles through the real hit test, drags all three operations, checks movement, rotation preview, changed volume after scale, persistent Transform command state, and switching back to dedicated Move. `--output <path>` saves the viewport.

## Numeric input and keyboard transforms

During any Move, Rotate or Scale gizmo drag, Space opens a compact field beside the cursor. It contains the total distance, angle in degrees or scale factor since the drag began. Editing replaces that total. Move uses the configured display length unit; both decimal dot and comma are accepted. Mouse movement and release are paused while the field is open. Enter commits; Esc in the field cancels the preview. Outside the numeric editor, ordinary gizmo drags retain their existing Esc-to-finish behavior. Ctrl snaps rotation to the nearest 45 degrees, including when the modifier changes without further mouse movement.

Dedicated commands default to M (Move), R (Rotate), and S (Scale). The first shortcut shows the gizmo. Repeating the command's current shortcut, including a user-remapped shortcut, starts a mouse-driven transform without handles: Move in the screen plane, Rotate around the screen normal through the gizmo center, Scale uniformly. X/Y/Z starts or switches to a world-axis constraint. Changing the constraint restores the original geometry before starting the new preview. Left click or Enter confirms; right click or Esc cancels. Toolbar/menu activation selects the regular gizmo. A second Esc leaves the tool. Scale factors are limited to 0.001–1000 per operation so the preview remains invertible.

Regression: `CatalogImportTests --test-scene-persistence --transform-interaction --output <path>` checks real handle picking, total-value editing, numeric focus and release, distance units, decimal input, committed geometry, Undo/Redo, all free/axis modes, remapped shortcuts, screen rotation, modifier snapping, invalid input and cancellation. The output includes the numeric editor.

`GizmoRepeatShortcuts` (`--test-scene-persistence --gizmo-repeat-shortcuts`) sends keys through Qt's window input pipeline and shortcut map. For Move, Rotate and Scale, both gizmo and free mode, it checks that Space opens the numeric editor without triggering Repeat or resetting the preview. Space inside a text field stays in that field. After confirming or canceling numeric input, and after exiting Transform, Space resumes Repeat. Copy → Move is included to catch both accidental duplication during editing and loss of the remembered outer Copy command. This native-window test runs serially because it requires keyboard focus.

`RepeatCommand` locates Copy and array actions by permanent `objectName` values, not localized hotkey IDs or captions. It checks actual object counts and the current non-modal array dialogs, including preservation of parameters across cancellation, confirmation and Repeat.
