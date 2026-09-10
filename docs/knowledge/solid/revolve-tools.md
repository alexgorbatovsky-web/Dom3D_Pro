# Revolve tools and input prompts

The Solid and Surfaces panels previously shared `SurfaceOfRevolution`, which always built a solid profile face and rejected standalone splines.

- `SurfaceOfRevolution` remains the **Solid Revolve** operation ID for compatibility. It accepts an open or closed Sketch, or an XY-planar Polyline. Open profiles are closed to the axis before rotation. Standalone splines are not accepted by this solid builder.
- `SurfaceRevolve` is **Surface Revolve**. It rotates a path wire into `CSurfaceSet`, without adding end caps or converting it into a solid. It accepts open and closed 2D/3D Polylines, Sketches and splines, including Bezier and NURBS representations supported by CBSpline.
- The axis passes through the sketch origin; for a Polyline its origin is (0,0,first-point Z), preserving the earlier convention. For standalone splines the axis passes through the world origin. X/Y/Z refer to world directions.
- Preview and parametric replay share `BuildSurfaceRevolveProfile`. Open splines use the existing sweep path conversion; closed splines use periodic interpolation of 128 evaluated points, so this conversion is approximate.
- Polyhedron still accepts Sketch profiles only, open or closed. Open ends are capped.

Selection prompts now identify required object types, closure and the next action instead of “select the required geometry and continue”. Revolve and Polyhedron also expose their requirements in button tooltips. An unsuccessful initial Solid Revolve keeps its parameter panel and selected profile, allowing an axis correction.

Regression: CTest `RevolveTools` covers UI entry, open/closed spline rotation, partial angles, sketch/3D-polyline input, parametric replay after source edits, serialization and legacy solid behavior. Existing sketch/profile/centerline tests cover shared functionality.
