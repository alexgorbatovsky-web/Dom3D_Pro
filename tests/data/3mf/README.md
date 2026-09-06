# Independent 3MF fixtures

Unmodified samples from **3MF Consortium lib3mf v2.5.0**, under the BSD-2-Clause
license reproduced in `../../../third_party/lib3mf-NOTICES.txt` (relative to the
repository root: `third_party/lib3mf-NOTICES.txt`).

- [WagonWithWheels.3mf](https://github.com/3MFConsortium/lib3mf/blob/v2.5.0/Tests/TestFiles/BuildItems/WagonWithWheels.3mf):
  repeated component instances, transforms and an extra build item; six meshes.
- [Texture.3mf](https://github.com/3MFConsortium/lib3mf/blob/v2.5.0/Tests/TestFiles/CPP_UnitTests/Texture.3mf):
  texture groups, per-corner UVs and an image packaged inside 3MF.

`ThreeMfIOTests.cpp` also generates focused fixtures for units, nested matrices,
reflection, material palettes, gradients, metadata and failure handling.

`SlicerMetadata.3mf` is a small synthetic package shaped like OrcaSlicer/BambuStudio
production-extension output. It verifies external object parts, slicer metadata,
extra attributes and transforms. The three `SlicerInvalid*`/`SlicerRequired*`
variants verify that tolerant metadata parsing does not relax geometry or required
extension validation. Regenerate them with `generate_slicer_fixtures.py`; they do
not contain geometry from a user file.
