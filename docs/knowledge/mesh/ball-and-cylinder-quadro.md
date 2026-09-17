# Ball_And_Cylinder: shared sphere/cylinder boundary

Regression fixture: `tests/data/mesh-regression/Ball_And_Cylinder.dom3d`.
Run `ctest --test-dir build -C Release -R "^BallAndCylinderQuadro$" --output-on-failure`.

The Boolean Union contains a clipped sphere, cylinder and planar cap. Its
sphere/cylinder intersection comprises two CAD edges and varies in cylinder V.
The old independent cylindrical grid left 126 open edges at density 0.45.
At 0.65 the cylinder retained cells but their total area was zero.

`sync_sphere_strip_rings` extends the existing sphere/fillet synchronization
to cylinders. It follows the actual sphere mesh boundary, retaining each
column's angular parameter and starting height, then interpolates to the
opposite constant-height rim. The cap receives that exact outer ring.
Split intersection edges are not replaced individually with a complete loop.
Topology, projection, opposite-rim height and angular monotonicity checks
restrict this reconstruction to a periodic strip with one spherical donor.

The test rebuilds at 0.45, 0.65, 0.25, 0.35, 0.5, 0.7, 1.0, 0.45 and 0.65.
It checks nonempty surfaces, mesh/CAD area agreement, unfolded cylinder quads
and exactly two incident faces at every welded mesh edge. Sphere clipping
and cap filling may retain triangles. The corrected fixture has no open or
nonmanifold edges at any tested density.
