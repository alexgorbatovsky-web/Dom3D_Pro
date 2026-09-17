# Explicit All Edges for Fillet and Chamfer

Both property forms expose an **All Edges** button. Starting `fillet_edge` with
only a selected body no longer implicitly chooses `fillet_all_edges` or attempts
the remembered radius on the entire body. Starting either tool with no selection
keeps the tool open. Selecting a body while waiting also leaves geometry alone;
the button explicitly selects its edges. Selected faces expand to their boundary
edges; explicitly selected edges remain restricted to those edges.

Switching into edge selection preserves the initial body/face/edge selection.
All Edges restores the live operation's base before gathering edges, invalidates
pending fillet work, and uses the existing selected-edge history representation.
The separate legacy `fillet_all_edges` command remains supported.

An invalid initial chamfer distance now retains the live session so it can be
reduced without selecting edges again. Acceptance validates the current distance
before committing. Fillet continues to use its existing preview-validity check.

`EdgeToolAllEdges` covers empty selection, selecting a body after launch, explicit
all-edge preview, invalid-size OK, reducing size, Undo/Redo, relaunch on a body,
selected face boundaries, one selected edge, and cancellation for both tools.

Validation also passes `MovedCylinderFilletHandle`, `LiveFilletValidation` and
`ScenePersistence`. The broader `PropertyPanelTests` fails its unrelated Hole
diameter drag-label assertion; a control build with the All Edges form block
removed reproduces the same failure. Source bytes were restored afterwards.
