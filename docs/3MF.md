# 3MF exchange

Import and Export list **3MF Manufacturing (*.3mf)** immediately before STL.
Dragging a 3MF into the application uses the same importer.

- Imports mesh build items and nested components, including repeated instances.
  Unit conversion (micrometres, millimetres, centimetres, metres, inches, feet),
  translation, rotation, scale, shear and reflection are applied to mesh geometry.
  Reflection reverses triangle winding. Unused resources are not imported.
- Imports named base materials, display colour and opacity, triangle colours,
  colour gradients, texture images and independent corner UVs. Different materials
  become separate editable meshes. Vertex gradients are baked into texture atlases
  with 32-pixel tiles. Composite mixtures become their weighted display colour.
- Exports visible meshes and tessellated Solid surfaces, with surface material
  overrides. Each mesh uses local coordinates and a placement transform. Solid
  surfaces are components of one build object. Output units are millimetres.
- Named materials and textures use standard 3MF Core/Materials properties.
  Dom3D's additional material parameters (roughness, metallic, coating, emission,
  etc.), extra texture maps, wire colour and group labels use namespaced metadata
  and attachments for a Dom3D round trip. Other readers can use the standard
  display material without understanding this metadata. Texture transforms are
  baked into exported UV coordinates.

Imported texture images are stored by content hash under the application's local
data directory, `Imported3MFTextures`. Saving a Dom3D project embeds these images.
Exports contain the images and do not depend on files beside the 3MF package.
Import is transactional for the scene; export uses atomic file replacement.
Common OrcaSlicer/BambuStudio package metadata and optional slicer attributes are
ignored when they have no Dom3D meaning. Unsupported required extensions and
geometry warnings remain import errors.

Unsupported non-mesh objects, recursive components, singular transforms, invalid
indices and unsupported combinations of layered material properties produce an
error rather than silently importing altered geometry. This is mesh exchange;
editable CAD/BRep operation trees and slicer-specific print settings are not
represented as Dom3D operations.

## Build

`cmake/Lib3MF.cmake` obtains the official Windows x64 lib3mf 2.5.0 SDK from a
pinned release URL and verifies its SHA-256. An already unpacked SDK can be selected
with `DOM3D_LIB3MF_ROOT`. The DLL and notices are copied next to the executable.
On other platforms, install the lib3mf 2.5 development package first.

Run `ctest --test-dir build -C Release -R ^ExchangeIOTests$ --output-on-failure`
to verify units, nested transforms, reflection, separate materials, alpha, textures,
UVs, Unicode paths and failure atomicity.

References: [lib3mf](https://github.com/3MFConsortium/lib3mf),
[Core specification](https://github.com/3MFConsortium/spec_core),
[Materials extension](https://github.com/3MFConsortium/spec_materials).
