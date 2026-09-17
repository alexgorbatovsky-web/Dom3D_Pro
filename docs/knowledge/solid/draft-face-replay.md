# Draft Face replay direction

Fixture: `tests/data/mesh-regression/Draft_Face.dom3d`.
Run `ctest --test-dir build -C Release -R "^DraftFaceReplay$" --output-on-failure`.

Live Draft Face used increasing CAD curve parameters to orient its selected
edge. History replay used the display spline endpoints instead. Display
splines can follow the opposite wire orientation, reversing the neutral
plane normal and the meaning of a positive angle. Opening Solid Editor
therefore changed the draft before the user edited anything.

`DraftFaceEdgeEndpoints` is shared by live axis selection and history replay.
It retains the live tool's CAD parameter direction, straight-edge check and
fallback for surfaces without a CAD edge. Existing saved angles are replayed
using the same convention that originally produced them.

The regression compares CAD vertex positions and volume against the saved
body, repeats replay, edits the angle to its negative and back, then saves,
reloads and replays again.

On the supplied file, the previous replay moved vertices by 11.2152 mm.
The corrected replay differs by at most 5.2e-11 mm. DraftFaceReplay,
ScenePersistence, BooleanTool and SolidCenterlines all pass.

## Editable history after failed rebuilds

A zero draft angle is a valid no-op and retains the operation. Previously it
made replay fail. Draft-bearing histories now replay on a candidate clone,
keeping the original body until all operations succeed. Failed edits cannot
fall through to rebuilding only a selected primitive, which can replace the
finished body with a partial history. Boolean-tool edits also retain a backup
from before their parameters were changed.

The regression covers zero-angle disable/re-enable, editing the base box,
and rejection of a non-finite angle without changing shape or saved history.
