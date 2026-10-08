# FBX exchange

Import and Export list **Autodesk FBX (*.fbx)** immediately after OBJ.

- Imports binary and ASCII FBX meshes, nested node transforms, source units,
  repeated mesh instances, names, material colours, opacity, common PBR values,
  embedded and external textures, normals and UV coordinates.
- Exports binary FBX 7.5. Mesh coordinates use centimetres in the file and each
  object keeps a separate placement node. Visible Dom3D meshes and tessellated
  Solid surfaces are exported with materials and embedded texture images.
  Original triangles, quads and n-gons are preserved without triangulating
  the subdivision control cage.
- Mesh Sharp edges export as native `LayerElementSmoothing` (`ByEdge`,
  `Direct`, 0 = hard, 1 = smooth) and `LayerElementEdgeCrease` (1 = sharp).
  Corner normals respect the sharp boundaries. Shared control points remain
  shared across normal and UV seams. Coincident positions within each exported
  mesh are welded consistently with Assimp, including CAD patch boundaries
  when Sharp layers are present.
  Assimp's binary output is completed with these layers because `aiMesh` has
  no per-edge attribute storage. OBJ export is independent and unchanged.
- Imported geometry is converted to Dom3D millimetres. Mirrored node transforms
  reverse winding through `CMesh3D::ApplyAffineTransform`.
- Import is transactional and rejects invalid indices, non-finite geometry,
  singular transforms, empty scenes and oversized input. Export replaces the
  destination atomically.

FBX animation, skeletons, cameras, lights, NURBS construction history and
designer application constraints are outside Dom3D's mesh exchange model.

The importer/exporter uses a pinned Assimp v6.0.5 source build with only its FBX
modules enabled. Its license is distributed in `third_party/assimp-LICENSE.txt`.

`ExchangeIOTests --write-fbx-test <directory>` writes `sharp-edges.fbx` and
checks native edge layers and shared control points. The Blender check also
verifies quad preservation and a closed subdivided cube assembled from separate
CAD surface patches. For an independent Blender
4.5 check, run `blender --background --factory-startup --python-exit-code 1
--python tests/ValidateFbxSharpBlender.py -- <directory>/sharp-edges.fbx`.
