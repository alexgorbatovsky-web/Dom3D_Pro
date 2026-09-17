# Shared seams in mixed render meshes

Frame-2 with a 1 mm fillet had valid, closed CAD topology but an open render
mesh. BuildHybridRenderMesh retained regular CNet grids on some faces and
OCCT triangulations on trimmed neighbours. Their shared curved edges used
different samples and therefore different chords. Nearby-vertex snapping
could not make those two boundaries coincide.

When a connected component needs triangulation, the render path now uses
the whole-shape OCCT triangulation for every face in that component. A
topological edge map and adjacency traversal propagate this decision without
geometric proximity guesses. Disconnected components containing only regular
faces retain their grids. The explicit Mesh Quadro path is unchanged.

Frame2FilletRenderSeams builds the 1 mm fillet on the repaired fixture, calls
the default render rebuild, and checks both welded closure and exact float
coordinate edge pairing. Every nondegenerate edge must have two incident
polygons without tolerance-based welding. Before the change the render mesh
had open/nonmanifold boundaries; afterwards there are zero unmatched edges.

Frame-2-solid.dom3d is derived from the user's original Frame-2 by rebuilding
Smart Hybrid with consistent shared-curve fitting. The original legacy
fixture is retained separately for the kernel crash regression.
