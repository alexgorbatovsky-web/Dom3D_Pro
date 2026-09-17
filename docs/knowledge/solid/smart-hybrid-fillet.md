# Smart Hybrid shared boundaries and Fillet

Frame-2 was saved as a valid six-face shell although `make_solid=1` was
requested. Fillet at radius 1 mm crashed in TKFillet.dll with access violation
0xc0000005. Three user dumps have the same module-relative fault address,
0x85442; the failure also reproduced in an isolated diagnostic process.

FourSplineSurfaceBuilder fitted each input boundary with a tolerance based
on the containing patch's diagonal. The same short curve therefore received
different approximations on a long side and an end cap. Sewing could not
join all boundaries at the requested tolerance of 0.005.

The fitting tolerance now depends on the boundary curve's own bounding-box
diagonal, retaining the existing 0.02% factor and 0.001 floor. On Frame-2,
rebuilding produces a valid solid with 12 unique edges and edge tolerances
of 1e-7. All individual 1 mm edge fillets build valid results.

BuildLiveFilletShape also installs OCCT's thread-local exception translator.
On MSVC, CAlfaDoc.cpp uses /EHa to unwind the kernel's structured exceptions
into the existing Standard_Failure handler. A failed preview preserves the
source document. This does not repair old saved BReps automatically: rebuild
the parametric Hybrid, or use the separate repaired project produced during
the regression run. No separate geometric fillet algorithm was introduced.

Frame2HybridFillet loads the unchanged legacy fixture, exercises its formerly
crashing edge on a new worker thread, verifies the original shape survives,
rebuilds from its stored curve references, requires a valid 12-edge solid,
and checks all individual edge fillets at radius 1 mm.
