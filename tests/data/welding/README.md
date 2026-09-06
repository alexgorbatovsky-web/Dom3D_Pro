# Bezier collar welding

`Bezier-4.dom3d` is a copy of the reported model, tested with SLX at Density 0.5.
Increasing a curved boundary from 3 to 5 points by subdividing its chords
left a 0.022836 mm gap against the neighbouring analytic surface. Welding
correctly rejected it at the minimum-edge / 5 tolerance of 0.0224838 mm.

The regression rebuilds the mesh, welds it at that unchanged tolerance, and
checks for zero open boundary loops, manifold edges, 3055 retained cells,
and no collapsed cell indices.

Removing the global 27-point boundary cap increased the expected count from
2839 to 3055; the welding tolerance and topology checks are unchanged.

Run: `ctest --test-dir build -C Release -R BezierCollarWelding --output-on-failure`.
