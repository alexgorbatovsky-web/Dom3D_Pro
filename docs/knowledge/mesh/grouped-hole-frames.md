# Separate row frames and native SLX cutting

Date: 2026-09-12.

Fixtures: `Box_Min_6_Boxex`, `Box_Min_5_Box`, `Box_Min_12_Box`, and
`Box_Min_14_Box`. The five-hole fixture was copied unchanged from
`Documents/CAD2Quads`; the twelve-hole fixture from
`Desktop/Notes_2022/Dom-3D_Pro/Bags_Mesh`.

The old shared-zone path used a single bounding rectangle for all holes and
clipped SLX background cells against four half-planes. It bypassed vertex
snapping. The multirow panel could therefore be watertight while still having
the wrong grouping and transition structure.

Aligned rectangular holes are now grouped by row and proximity. Each group
gets its own frame; margins respect the exterior and leave space between rows.
The twelve- and fourteen-hole panels have two frames. Ordinary Quadro fills
the exterior with both frames as holes. SLX fills the background once and
cuts each frame through `CMesh3D::TrimByPline`, then attaches the local hole
strip to the actual cut boundary. The half-plane cutter is removed.

The boundary-preserving SLX option retains the existing exterior and earlier
frame vertices. Its snapping assigns each contour node to at most one vertex,
resolves equal-distance alternatives deterministically, and rejects moves
that reverse adjacent face areas. Fixed boundary cells receive shared edge
intersections. Existing Var-11 and line split operations process the contour
chains, including repeated passes through a cell.

Two common cutting details were corrected:

- Point-on-segment tests compare distance with distance rather than using the
  same epsilon for a cross product regardless of segment length.
- A concave exterior corner is classified by its non-boundary vertices. Its
  vertex average can lie inside the hole and cannot alone decide deletion.

Near-collinear transition quads are split before conversion from UV, avoiding
folds caused by float rounding. Local transition triangles remain; this does
not guarantee an all-quad mesh or apply a new density limit.

`FrameSlxBoundary` verifies actual interior vertex movement, unchanged exterior
vertices, two boundary loops, connectivity and retained area. The four
`Grouped*Holes` tests verify frame counts, hole topology, connectivity, area,
quad majority, unfolded quads, welded closure and mode selection at densities
0.15, 0.20, 0.35, 0.50, 0.70, 1.0 and repeated 0.35, in both modes.

Diagnostic meshes and top-view renders at SLX 0.35 are in `output/row-frames`.
Both two-row models were visually inspected. Related regression coverage
includes the previous collar, Boolean, prism and point-split cases.

Final optimized Release run: all 18 selected tests passed in 55.84 seconds.
The test log is `output/row-frames/release-tests.log`.
`build/Release/Dom3D_Pro.exe` was rebuilt with the updated cutter.
