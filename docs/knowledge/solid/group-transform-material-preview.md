# Group transforms after material previews

`RenderMaterialSphereGL` creates a temporary `CAlfaDoc`. Its constructor makes
that document current; its destructor clears the current-document pointer.
Previously a cache miss therefore detached the editing document. Selection
highlighting survived, but `CGroup::GetBounds` could no longer resolve member
IDs through `GetAlfaDoc`, so Move/Rotate/Scale had no gizmo center. Group
transforms also rely on that lookup.

The preview now saves the current document with a local lifetime guard declared
before the temporary document, restoring it after the temporary is destroyed.
Cached previews keep the existing early return.

`GroupTransformAfterMaterialPreview` loads the unchanged user fixture
`tests/data/ui-regression/Table_And_Chair.dom3d`, renders an actual OpenGL
material preview, checks both fresh and cached previews, and checks gizmo
centers for assemblies 11 and 31. It applies Move, Rotate, and uniform Scale
through the viewport and checks every solid's CAD center of mass against the
expected transform; members of the other assembly must remain unchanged.
The test requires the native OpenGL platform (do not use Qt offscreen).

Before the fix, the test reproduced a selected assembly with no gizmo center
after rendering the preview. After the fix, it and ScenePersistence,
PrimitiveEnter, PropertyPanelTests, and ProceduralMaterial pass in Release.

## Move completion and linked table legs

The table also exposed repeated work in `RebuildAssociativeClones(source_id)`.
Its propagation loop retained all changed IDs and rebuilt the same clones on
every pass, up to the document object count. Track successfully rebuilt clone
pointers per call so subsequent passes only discover downstream clones.

On this fixture, Move completion fell from 3473 ms to about 110 ms. The test
now exercises both LMB release and Escape, Undo/Redo, and a downstream clone
stored before its source. It verifies an unrelated clone is not rebuilt and
uses a generous 1500 ms completion limit to catch the original repeated work.
The chair completes in about 1–3 ms. Timings are local Release measurements.
