# Filleted square rim density

`Square_Filleted.dom3d`, body `Beam`, exposed a local sizing mismatch at
density 0.25: the planar rim's outer boundary retained its minimum eight
intervals while the inner straight edges fell from four intervals to two.
The resulting rim lost patches and the welded body had 88 boundary edges.

In `CSolid::BuildQuadroMesh`, after applying the rectangular outer-boundary
floor, parallel inner straight edges now use that boundary's local step as
an additional sizing constraint. The existing maximum step ratio of 1.6 and
even interval counts are retained. Counts propagate through the shared-edge
and opposite-edge synchronization before walls and fillets are meshed.

The reported densities 0.25 and 0.30 both retain eight outer and four inner
intervals, with no open welded seams. Odd interval counts (three or five)
are outside this change's scope.

`SquareFilletedQuadro` exercises 0.20, 0.24, 0.25, 0.26, 0.29, 0.30, 0.31,
0.50 and a return to 0.25 with SLX enabled and disabled. It checks both rim
loops, connectedness, welded closure and the reported edge counts.
