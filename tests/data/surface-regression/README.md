# Smart Hybrid orientation fixtures

- `SmartHybridCarOpen.dom3d`: curve-only copy of the reported Car-2 frame,
  with 34 curves and 15 exterior patches. The four-curve cabin partition
  must not become a sixteenth patch: each of its edges would have three faces.
- `SmartHybridCarClosed.dom3d`: the same frame with two floor cross curves,
  giving 36 curves and 18 exterior patches.

`SmartHybridCarOpenNormals` and `SmartHybridCarClosedNormals` check outward
CAD normals, display polygon winding, shading normals, low-poly quad winding,
and preservation through project save/load and parametric replay.
