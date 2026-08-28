# Generated project map

Generated from project files: 2026-08-27.  
Generator: [`tools/Generate-CodeWiki.ps1`](../../../tools/Generate-CodeWiki.ps1)

> This page is generated. Manual edits are replaced by the next `CodeWiki` run.

## Summary

- C/C++ source files: **372**
- Class and struct definitions found: **435**
- CMake targets: **24**
- `*Tests.cpp` files: **19**

## Subsystems

| Subsystem | Headers | Implementations | Total | Purpose |
|---|---:|---:|---:|---|
| `3DCoat` | 1 | 1 | 2 | Integrated 3DCoat contour-filling algorithms. |
| `comms` | 36 | 31 | 67 | Low-level 3DCoat graphics and utility components. |
| `core` | 74 | 72 | 146 | Document, scene objects, curves, meshes, materials, and common geometry. |
| `excomms` | 34 | 20 | 54 | Experimental mesh containers, codecs, and mesh operations. |
| `iges` | 3 | 10 | 13 | IGES geometry and spline support. |
| `render` | 4 | 3 | 7 | Scene preparation and external/native renderers. |
| `shell` | 0 | 1 | 1 | Windows shell integration. |
| `solid` | 20 | 20 | 40 | OpenCascade solids, surfaces, tools, and shape builders. |
| `ui` | 22 | 20 | 42 | Qt UI, viewport, property panels, and the tool registry. |

## CMake targets

| Target | Kind |
|---|---|
| `BeamShapeBuilderTests` | `executable` |
| `CatalogImportTests` | `executable` |
| `CommsSmokeTests` | `executable` |
| `dom3d_comms` | `library` |
| `dom3d_mesh_adapter` | `library` |
| `Dom3D_Pro` | `executable` |
| `Dom3DThumbnailProvider` | `library` |
| `ExchangeIOTests` | `executable` |
| `ExtrudeShapeBuilderTests` | `executable` |
| `FacadeFrameShapeBuilderTests` | `executable` |
| `FourSplineSurfaceBuilderTests` | `executable` |
| `FourSplineSurfaceComparison` | `executable` |
| `IgesShapeCollectorTests` | `executable` |
| `MeshContainerAdapterTests` | `executable` |
| `OffsetFaceShapeBuilderTests` | `executable` |
| `PbrMaterialLibraryTests` | `executable` |
| `PolyhedronShapeBuilderTests` | `executable` |
| `PropertyPanelTests` | `executable` |
| `SketchFeatureShapeBuilderTests` | `executable` |
| `SketchGeometryTests` | `executable` |
| `SketchProfileBuilderTests` | `executable` |
| `SurfacePatchBuilderTests` | `executable` |
| `TrimShapeBuilderTests` | `executable` |
| `TwoRailSweepSurfaceBuilderTests` | `executable` |

## Automated tests

- [BeamShapeBuilderTests](../../../tests/BeamShapeBuilderTests.cpp)
- [CatalogImportTests](../../../tests/CatalogImportTests.cpp)
- [CommsSmokeTests](../../../tests/CommsSmokeTests.cpp)
- [ExchangeIOTests](../../../tests/ExchangeIOTests.cpp)
- [ExtrudeShapeBuilderTests](../../../tests/ExtrudeShapeBuilderTests.cpp)
- [FacadeFrameShapeBuilderTests](../../../tests/FacadeFrameShapeBuilderTests.cpp)
- [FourSplineSurfaceBuilderTests](../../../tests/FourSplineSurfaceBuilderTests.cpp)
- [IgesShapeCollectorTests](../../../tests/IgesShapeCollectorTests.cpp)
- [MeshContainerAdapterTests](../../../tests/MeshContainerAdapterTests.cpp)
- [OffsetFaceShapeBuilderTests](../../../tests/OffsetFaceShapeBuilderTests.cpp)
- [PbrMaterialLibraryTests](../../../tests/PbrMaterialLibraryTests.cpp)
- [PolyhedronShapeBuilderTests](../../../tests/PolyhedronShapeBuilderTests.cpp)
- [PropertyPanelTests](../../../tests/PropertyPanelTests.cpp)
- [SketchFeatureShapeBuilderTests](../../../tests/SketchFeatureShapeBuilderTests.cpp)
- [SketchGeometryTests](../../../tests/SketchGeometryTests.cpp)
- [SketchProfileBuilderTests](../../../tests/SketchProfileBuilderTests.cpp)
- [SurfacePatchBuilderTests](../../../tests/SurfacePatchBuilderTests.cpp)
- [TrimShapeBuilderTests](../../../tests/TrimShapeBuilderTests.cpp)
- [TwoRailSweepSurfaceBuilderTests](../../../tests/TwoRailSweepSurfaceBuilderTests.cpp)

## Refresh

```powershell
cmake --build build --target CodeWiki
```
