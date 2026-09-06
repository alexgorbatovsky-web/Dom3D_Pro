# FBX exchange

Import and Export list **Autodesk FBX (*.fbx)** immediately after OBJ.

- Imports binary and ASCII FBX meshes, nested node transforms, source units,
  repeated mesh instances, names, material colours, opacity, common PBR values,
  embedded and external textures, normals and UV coordinates.
- Exports binary FBX 7.5. Mesh coordinates use centimetres in the file and each
  object keeps a separate placement node. Visible Dom3D meshes and tessellated
  Solid surfaces are exported with materials and embedded texture images.
- Imported geometry is converted to Dom3D millimetres. Mirrored node transforms
  reverse winding through `CMesh3D::ApplyAffineTransform`.
- Import is transactional and rejects invalid indices, non-finite geometry,
  singular transforms, empty scenes and oversized input. Export replaces the
  destination atomically.

FBX animation, skeletons, cameras, lights, NURBS construction history and
designer application constraints are outside Dom3D's mesh exchange model.

The importer/exporter uses a pinned Assimp v6.0.5 source build with only its FBX
modules enabled. Its license is distributed in `third_party/assimp-LICENSE.txt`.
