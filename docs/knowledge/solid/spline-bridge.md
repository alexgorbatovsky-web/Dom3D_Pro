# Editable spline bridges

Bridge Curves connection modes retain their saved numeric values: Straight=0,
Smooth=1; Spline=2 is the editable mode. New Spline bridges start with four
collinear Bezier poles. The two intermediate poles can be edited manually.
Handle length applies to Smooth only in the creation dialog.

On dependency replay, Spline updates only its first and last poles in place.
Intermediate poles remain in world coordinates; curve type, weights and knots
are preserved. Switching an existing bridge to Spline retains its current
shape. Straight and Smooth still regenerate their geometry from the sources.
No additional serialized state is necessary: existing spline geometry and the
mode parameter are saved in the project.

ParametricCurveLink covers existing Straight/Smooth behavior, switching to
Spline, edited poles surviving source edits, and save/load followed by editing
the other source endpoint.

## Default endpoint choice

Bridge Curves ranks endpoint pairs first by the number of occupied ends, then
by their 3D distance. An end is occupied when it has an explicit endpoint link
to an existing object or touches an endpoint of another open curve within the
modeling tolerance. This includes unselected and hidden bridges. Repeating
Bridge on the original pair therefore proposes the remaining free ends.
If all ends are occupied, the nearest pair remains the default. Manual endpoint
selection is always available.

`BridgeEndpointDefaults` exercises the actual dialog: first bridge, second
proposal, both sides occupied, Undo/Redo, save/load and manual override.
