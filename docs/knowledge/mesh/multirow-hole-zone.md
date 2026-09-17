# Several rows of rectangular openings

Update 2026-09-12: the single multirow zone and rectangular half-plane cutter
described below have been replaced by [separate row frames and native SLX
cutting](grouped-hole-frames.md). The earlier topology tests alone did not
establish the intended mesh structure.

`Box_Min_14_Box.dom3d` is copied unchanged from
`C:/Users/Alex/Documents/CAD2Quads/Box_Min_14_Box.dom3d`.
Its Boolean Cut has 76 faces; face 4 contains fourteen rectangular openings
in two rows of different lengths.

The shared hole zone previously required all hole centres to belong to one
row. Allow several rows when the existing Cartesian boundary-node checks
permit it. Subdivide the empty space between rows according to the background
step, without introducing unmatched nodes along hole edges.

SLX still cuts a filled background around the shared zone. For this rectangular
cut, partition each cell against the four half-planes, retain the outside
pieces and rejoin pieces belonging to the same original cell. This avoids
the generic closed-contour cut losing coarse frame cells. Shared cut nodes
are inserted before boundary tracing; artificial collinear nodes on the outer
CAD edge are removed so adjacent solid faces retain matching samples.
Nonconvex transition quads are triangulated. Local transition triangles remain.

`MultirowHoleZone` checks both modes at densities .15, .20, .35, .50 and
repeated .15: rebuild success without triangle fallback, actual SLX execution,
closed manifold welded mesh, face area within 0.1%, a majority of quads, and
no folded quads. Together with RoundedOpeningCollar, SixHoleSharedZone,
SmallHoleSlxCollar, HoleLowDensityCollar and FourHoleCollars, all six checks
passed on 2026-09-11. Release Dom3D_Pro was rebuilt and the .15 SLX result
visually inspected.

The separate mixed Low Poly fallback for the hairdryer only fills missing
faces. The user reports that its overall mesh quality remains unacceptable;
that case is not resolved by this panel fix.
