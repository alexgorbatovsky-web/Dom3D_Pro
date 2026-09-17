# Procedural perforation and honeycomb

The Procedural library category offers Perforation and Honeycomb presets. Material Editor → Advanced → Perforation / Honeycomb opens a local preview with OK/Cancel. The material retains its normal color, roughness and metallic controls.

Hole size is the round diameter or hexagon distance across flats. Bridge width is the minimum spacing between holes. A triangular lattice supports both patterns. Edge relief shades a rounded rim without modifying CAD geometry. Surface UV mapping uses the UV tile size; disabling it uses dominant-axis object-space projection in millimeters. UV distortion/seams follow the source surface; object-space projection can change at dominant-axis boundaries.

OpenGL discards hole fragments, including their depth writes. The same shader powers material thumbnails. Native projects, .d3mat files and material drag payload v9 store the parameters. Absent data leaves old materials unchanged. GLB exports bake an alpha mask, rim normals and metallic/roughness maps and use MASK alpha mode (BLEND when the material itself is translucent). CAD export and picking still use the original unperforated shape. External offline renderers have not been extended by this change.

Validation: `CatalogImportTests --test-procedural-material --perforation -platform windows` checks persistence, drag/drop, both GPU patterns, actual background openings in a plane, GLB baked alpha and editor Cancel/OK. Images are written to output/perforation.
