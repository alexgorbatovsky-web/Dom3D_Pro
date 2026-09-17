# Two-rail sweep section placement

Both `SolidSweepTwoRails` and `SurfaceSweepTwoRails` expose `dx`, `dy`, and
`angle`, with the same units, defaults, and ranges as `SolidSweptTool`.

- Delta X shifts the section along the local axis between the rails.
- Delta Y shifts along the perpendicular section axis.
- Angle rotates the section in its local plane, in degrees.

Placement is applied after the existing rail-dependent scaling. Translation
uses the unrotated local frame, so changing Angle does not rotate the offsets.
The rails remain construction references; a displaced or rotated section need
not touch them. Zero placement preserves the previous geometry.

The sampled surface, polygonal solid, and exact CAD-wire solid paths all apply
the same placement. Exact wires retain their original curve geometry during
the section transformation. Non-finite placement values are rejected.

Creation stores zero defaults in the operation. Rebuilding reads placement
through ToolRegistry. Existing documents without these values receive zero
from the normal saved-parameter/default merge. Profile and rail IDs remain
stored but are hidden from the parameter panel.

`TwoRailSweepSurfaceBuilderTests` checks translation, quarter-turn rotation,
combined placement, reset, validity, and non-finite rejection in all three
geometry paths, alongside the existing curved/reversed-rail cases.

## Document activation regression (Swept-2)

The surface command used to run through generic `Activate` before reaching
its selection-specific UI branch. Generic activation overwrote the references
captured during creation with zero defaults, and a failed invocation could
write that history onto a selected spline. The surface command now dispatches
before generic activation, alongside the solid command. Registry activation
and explicit creation preserve the new object's saved references and return
no active object when creation fails. Surface creation also ensures IDs for
every source before storing them.

`TwoRailSurfaceDocument` loads the original Swept-2 regression fixture, restores
its known source references in memory, and tests each placement parameter,
each spline dependency, save/reload, fresh activation, and incomplete selection.
Legacy files whose references are already zero require explicit relinking;
the application does not guess their source curves.
