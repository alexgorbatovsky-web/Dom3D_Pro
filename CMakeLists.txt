cmake_minimum_required(VERSION 3.16)

project(Dom3D_Pro LANGUAGES CXX)

include(CTest)

if(WIN32)
    find_program(DOM3D_POWERSHELL_EXECUTABLE NAMES pwsh powershell)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(DOM3D_POWERSHELL_EXECUTABLE)
        add_custom_target(CodeWiki
            COMMAND "${DOM3D_POWERSHELL_EXECUTABLE}"
                -NoProfile
                -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/Generate-CodeWiki.ps1"
                -ProjectRoot "${CMAKE_CURRENT_SOURCE_DIR}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Generating Dom3D Pro Code Wiki from project files"
            VERBATIM
        )
        if(Python3_Interpreter_FOUND)
            add_custom_target(HtmlWiki
                COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/Generate-HtmlWiki.py"
                    --project-root "${CMAKE_CURRENT_SOURCE_DIR}"
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                COMMENT "Generating local Dom3D Pro HTML Wiki"
                VERBATIM
            )
            add_dependencies(HtmlWiki CodeWiki)

            add_custom_target(OpenWiki
                COMMAND "${DOM3D_POWERSHELL_EXECUTABLE}"
                    -NoProfile
                    -ExecutionPolicy Bypass
                    -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/Open-CodeWiki.ps1"
                    -ProjectRoot "${CMAKE_CURRENT_SOURCE_DIR}"
                    -PythonExecutable "${Python3_EXECUTABLE}"
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                COMMENT "Opening local Dom3D Pro Wiki"
                VERBATIM
            )
            add_dependencies(OpenWiki HtmlWiki)
        endif()
    endif()
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(DOM3D_QT_MSVC2022_PATH "C:/QtE/6.11.1/msvc2022_64" CACHE PATH "Qt 6 MSVC 2022 64-bit installation path")
if(EXISTS "${DOM3D_QT_MSVC2022_PATH}/lib/cmake/Qt6/Qt6Config.cmake")
    list(PREPEND CMAKE_PREFIX_PATH "${DOM3D_QT_MSVC2022_PATH}")
endif()

find_package(Qt6 COMPONENTS Widgets OpenGLWidgets Xml PrintSupport QUIET)
if(NOT Qt6_FOUND)
    message(FATAL_ERROR
        "Qt 6 was not found. Expected Qt MSVC 2022 x64 at:\n"
        "  C:/QtE/6.11.1/msvc2022_64\n"
        "If Qt is installed elsewhere, configure this project with one of these options:\n"
        "  cmake -S . -B build -DDOM3D_QT_MSVC2022_PATH=C:/QtE/<version>/msvc2022_64\n"
        "  or set Qt6_DIR=C:/QtE/<version>/msvc2022_64/lib/cmake/Qt6\n"
        "In Visual Studio: Project > CMake Settings > CMake command arguments, add:\n"
        "  -DDOM3D_QT_MSVC2022_PATH=C:/QtE/<version>/msvc2022_64"
    )
endif()
find_package(OpenGL REQUIRED)
include(cmake/Lib3MF.cmake)
include(cmake/Assimp.cmake)

# Legacy 3DCoat communication/utility engine. The copied comms sources depend
# on the sibling comms-Math module, so keep the original 3DCoat checkout as a
# configurable dependency instead of baking its files into Dom3D Pro.
set(DOM3D_3DCOAT_SOURCE_DIR
    "C:/3DCoat/3dcoat/3D-CoatV4"
    CACHE PATH "3DCoat source directory containing comms-Math")
set(DOM3D_COMMS_MATH_DIR
    "${DOM3D_3DCOAT_SOURCE_DIR}/comms-Math"
    CACHE PATH "3DCoat comms-Math source directory")

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/comms/comms.h")
    message(FATAL_ERROR "Dom3D comms sources were not found in src/comms")
endif()
if(NOT EXISTS "${DOM3D_COMMS_MATH_DIR}/comms-Math.h")
    message(FATAL_ERROR
        "comms-Math was not found at:\n"
        "  ${DOM3D_COMMS_MATH_DIR}\n"
        "Configure DOM3D_3DCOAT_SOURCE_DIR or DOM3D_COMMS_MATH_DIR.")
endif()

file(GLOB DOM3D_COMMS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/comms/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/comms/*.h")
file(GLOB DOM3D_COMMS_MATH_SOURCES CONFIGURE_DEPENDS
    "${DOM3D_COMMS_MATH_DIR}/*.cpp"
    "${DOM3D_COMMS_MATH_DIR}/*.h")

set(DOM3D_COMMS_BUILD_SOURCES ${DOM3D_COMMS_SOURCES})
# cCodecPng.cpp includes the third-party png.h outside its COMMS_PNG guard.
# Keep it visible in Visual Studio but do not compile it in the base module.
set(DOM3D_COMMS_NON_BUILD_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/comms/cCodecPng.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/comms/cWinMain.cpp")
list(REMOVE_ITEM DOM3D_COMMS_BUILD_SOURCES ${DOM3D_COMMS_NON_BUILD_SOURCES})
set_source_files_properties(${DOM3D_COMMS_NON_BUILD_SOURCES}
    PROPERTIES HEADER_FILE_ONLY TRUE)

add_library(dom3d_comms STATIC
    ${DOM3D_COMMS_BUILD_SOURCES}
    ${DOM3D_COMMS_NON_BUILD_SOURCES}
    ${DOM3D_COMMS_MATH_SOURCES})
target_compile_features(dom3d_comms PUBLIC cxx_std_17)
target_include_directories(dom3d_comms PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/comms"
    "${DOM3D_COMMS_MATH_DIR}")
target_compile_definitions(dom3d_comms PUBLIC DOM3D_COMMS_STANDALONE)
target_compile_definitions(dom3d_comms PRIVATE
    $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
    $<$<PLATFORM_ID:Windows>:_WINSOCK_DEPRECATED_NO_WARNINGS>)
target_link_libraries(dom3d_comms PUBLIC OpenGL::GL)
if(WIN32)
    target_link_libraries(dom3d_comms PUBLIC
        ws2_32 wininet comdlg32 shell32 ole32 uuid)
endif()
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src/comms"
             PREFIX "comms" FILES ${DOM3D_COMMS_SOURCES})
source_group(TREE "${DOM3D_COMMS_MATH_DIR}"
             PREFIX "comms-Math" FILES ${DOM3D_COMMS_MATH_SOURCES})

add_library(dom3d_mesh_adapter STATIC
    src/excomms/cMeshContainerAdapter.cpp
    src/excomms/cMeshContainerAdapter.h)
target_compile_features(dom3d_mesh_adapter PUBLIC cxx_std_17)
target_include_directories(dom3d_mesh_adapter PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(dom3d_mesh_adapter PUBLIC dom3d_comms)
source_group("excomms" FILES
    src/excomms/cMeshContainerAdapter.cpp
    src/excomms/cMeshContainerAdapter.h)

set(DOM3D_RAYTRACE_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../raytrace-0.1"
    CACHE PATH "Original Dom3D raytrace source directory")
if(NOT EXISTS "${DOM3D_RAYTRACE_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "Original Dom3D raytrace sources were not found at:\n"
        "  ${DOM3D_RAYTRACE_SOURCE_DIR}")
endif()
add_subdirectory("${DOM3D_RAYTRACE_SOURCE_DIR}"
                 "${CMAKE_BINARY_DIR}/original-raytrace")

set(DOM3D_OPENCASCADE_INCLUDE_DIR "C:/OpenCascade/opencascade_build/inc" CACHE PATH "OpenCascade include directory")
set(DOM3D_OPENCASCADE_LIBRARY_DIR "C:/OpenCascade/opencascade_build/win64/vc14/libd" CACHE PATH "OpenCascade library directory")
set(_dom3d_occ_runtime_default "C:/OpenCascade/opencascade_build/win64/vc14/bin")
if(NOT EXISTS "${_dom3d_occ_runtime_default}/TKSTEP.dll")
    # The local OCCT tree may contain only Debug core DLLs.  FreeCAD 0.20
    # ships ABI-compatible OCCT 7.6 Release DLLs, which avoid the extremely
    # slow validation and trace output of the Debug STEP translator.
    set(_dom3d_occ_runtime_default "C:/Program Files/FreeCAD 0.20/bin")
endif()
if(NOT EXISTS "${_dom3d_occ_runtime_default}/TKSTEP.dll")
    set(_dom3d_occ_runtime_default "C:/OpenCascade/opencascade_build/win64/vc14/bind")
endif()
set(DOM3D_OPENCASCADE_BINARY_DIR "${_dom3d_occ_runtime_default}" CACHE PATH "OpenCascade runtime DLL directory")

if(NOT EXISTS "${DOM3D_OPENCASCADE_INCLUDE_DIR}/TopoDS_Shape.hxx")
    message(FATAL_ERROR
        "OpenCascade headers were not found. Expected TopoDS_Shape.hxx at:\n"
        "  ${DOM3D_OPENCASCADE_INCLUDE_DIR}\n"
        "Configure with -DDOM3D_OPENCASCADE_INCLUDE_DIR=C:/path/to/opencascade/inc"
    )
endif()

set(DOM3D_OPENCASCADE_LIBS
    TKIGES
    TKV3d
    TKBRep
    TKernel
    TKG3d
    TKPrim
    TKXSBase
    TKMath
    TKFillet
    TKTopAlgo
    TKBO
    TKOffset
    TKFeat
    TKMesh
    TKGeomBase
    TKG2d
    TKBool
    TKShHealing
    TKGeomAlgo
    TKService
    TKHLR
    TKSTEP
    TKSTEPAttr
    TKSTEPBase
    TKSTEP209
)

set(DOM3D_SOURCES
    src/SurfaceSketchTrimBuilder.cpp
    src/ui/SurfaceSketchTrimDialog.cpp
    src/SaddleSurfaceBuilder.cpp
    src/ShoeBuilder.cpp
    src/Body1Builder.cpp
    src/BottleSection.cpp
    src/SketchEdgeConversion.cpp
    src/SketchBoolean.cpp
    src/BodySectionSketchBuilder.cpp
    src/ui/Body1Dialog.cpp
    src/TwoViewSurfaceBuilder.cpp
    src/ui/TwoViewSurfaceDialog.cpp
    src/SurfaceBridgeBuilder.cpp
    src/SplineBezierSections.cpp
    src/ExtractedEdgeCurve.cpp
    src/SurfaceOffsetBuilder.cpp
    src/ui/SurfaceOffsetDialog.cpp
    src/SurfaceTopologyBuilder.cpp
    src/SurfaceGraphBoundaryBuilder.cpp
    src/SurfaceGraphOffsetBuilder.cpp
    src/HybridExtrude.cpp
    src/ui/HybridExtrudeDialog.cpp
    src/SurfacePointTransform.cpp
    src/ui/SurfacePointTransformDialog.cpp
    src/ui/SurfaceKnotsDialog.cpp
    src/ui/SurfaceGraphOffsetDialog.cpp
    src/ui/SurfaceTopologyDialog.cpp
    src/ui/SurfaceBoundaryGraphDialog.cpp
    src/SurfaceFilletBuilder.cpp
    src/ui/SurfaceFilletDialog.cpp
    src/ui/SurfaceBridgeDialog.cpp
    src/SurfaceEdgePatchBuilder.cpp
    src/NSidedSurfaceBuilder.cpp
    src/ui/SurfacePatchDialog.cpp
    src/ui/NSidedSurfaceDialog.cpp
    src/main.cpp
    src/Material.cpp
    src/render/RenderScene.cpp
    src/render/BlenderCyclesRenderer.cpp
    src/render/NativeRaytraceRenderer.cpp
    src/MaterialLibrary.cpp
    src/FurnitureMaterialFactory.cpp
    src/CAlfaObject.cpp
    src/DrawingText.cpp
    src/CGroup.cpp
    src/CPart.cpp
    src/CAssembled.cpp
    src/CFacadeFurniture.cpp
    src/solid/FacadeFrameShapeBuilder.cpp
    src/KitchenLayout.cpp
    src/ui/KitchenLayoutDialog.cpp
    src/CFurnitureAssemblies.cpp
    src/CFurnitureDrawer.cpp
    src/CKitchenCabinet.cpp
    src/CAlfaDoc.cpp
    src/CurveEndpointLinks.cpp
    src/UndoRedo.cpp
    src/ExtrudeShapeBuilder.cpp
    src/CMesh3D.cpp
    src/solid/LowPolySharpEdges.cpp
    src/CMeshSubdivision.cpp
    src/CatmullClarkSurfaceBuilder.cpp
    src/Tools_Mesh3D.cpp
    src/mesh/ImageRelief.cpp
    src/ui/ImageReliefDialog.cpp
    src/solid/HolePlacement.cpp
    src/ReferenceImage.cpp
    src/FillContour.cpp
    src/ContourQuadrangulator3DCoat.cpp
    src/Line2D.cpp
    src/Net.cpp
    src/SurfaceUVMapping.cpp
    src/SurfacePatchBuilder.cpp
    src/CBSpline.cpp
    src/CPolyline.cpp
    src/Conic.cpp
    src/CView2d.cpp
    src/CView3d.cpp
    src/Dom3DProjectSerializer.cpp
    src/CadCurve3D.cpp
    src/ExchangeIO.cpp
    src/ThreeMfIO.cpp
    src/FbxIO.cpp
    src/GlbIO.cpp
    src/materials/GlbMaterialBaker.cpp
    src/IgesIO.cpp
    src/IgesShapeCollector.cpp
    src/ObjIO.cpp
    src/ProjectIO.cpp
    src/StepIO.cpp
    src/ThreeDSIO.cpp
    src/solid/BeamShapeBuilder.cpp
    src/solid/PlaneShapeBuilder.cpp
    src/solid/SheetBendShapeBuilder.cpp
    src/solid/OffsetFaceShapeBuilder.cpp
    src/solid/PolyhedronShapeBuilder.cpp
    src/solid/SketchFeatureShapeBuilder.cpp
    src/solid/SolidBeamTool.cpp
    src/solid/SolidBoxTool.cpp
    src/solid/SolidCylinderTool.cpp
    src/solid/SolidPrismTool.cpp
    src/solid/SolidSphereTool.cpp
    src/solid/SolidTorusTool.cpp
    src/solid/SurfaceSet.cpp
    src/solid/SurfaceDisplay.cpp
    src/solid/AssociativeClone.cpp
    src/solid/TwoSketchSolidBuilder.cpp
    src/solid/SolidTool.cpp
    src/solid/TrimShapeBuilder.cpp
    src/solid/PanelContourShapeBuilder.cpp
    src/ui/MainWindow.cpp
    src/ui/DraftingWorkspace.cpp
    src/ui/DraftingDimensions.cpp
    src/ui/MeasurementUnits.cpp
    src/ui/MaterialEditorDialog.cpp
    src/ui/MaterialDrag.cpp
    src/ui/MaterialPreviewGL.cpp
    src/materials/PlasterBaker.cpp
    src/ui/MaterialSphereBrowser.cpp
    src/ui/OpenGLViewport.cpp
    src/ui/ThemeManager.cpp
    src/ui/ThemeManager.h
    src/ui/PreferencesDialog.cpp
    src/ui/LightingDialog.cpp
    src/ui/BlenderCyclesDialog.cpp
    src/ui/NativeRaytraceDialog.cpp
    src/ui/RenderLightEditorDialog.cpp
    src/ui/LanguageManager.cpp
    src/ui/HotkeyManagerDialog.cpp
    src/ui/CommandSearchPopup.cpp
    src/ui/PropertyPanel.cpp
    src/ui/QtSceneRenderer.cpp
    src/ui/ToolRegistry.cpp
    src/ui/BooleanDialog.cpp
    src/ui/ExtrudeFaceDialog.cpp
    src/ui/ProjectOpenDialog.cpp
    src/resources/icons.qrc
    src/solid/Solid.cpp
    src/solid/SolidCenterlines.cpp
    src/solid/SurfaceFace.cpp
    src/solid/QuadroBoundary.cpp
    src/solid/QuadroFaceBoundary.cpp
    src/solid/QuadroPatchMesh.cpp
    src/solid/QuadroChartMesher.cpp
    src/solid/QuadroBodyMesher.cpp
    src/solid/PeriodicBandMesher.cpp
    src/solid/PlanarTrimRecovery.cpp
    src/solid/BoltCircleMesher.cpp
    src/solid/SteppedCylinderMesher.cpp
    src/Point3d.cpp
    src/ageom.cpp
    src/Plane.cpp
    src/Vector.cpp
    src/SystemCoord.cpp
    src/Triangle.cpp
    src/iges/AstraNurbs.cpp
    src/iges/AstraNurbsSurface.cpp
    src/iges/AstraSurface.cpp
    src/iges/AstraSurface2.cpp
    src/iges/AstraSurface3.cpp
    src/iges/AstraSurface4.cpp
    src/iges/AstraVect.cpp
    src/iges/Astra_spl.cpp
    src/iges/defines.cpp
    src/iges/SplineCurve.cpp
)

set(DOM3D_HEADERS
    src/CAlfaObject.h
    src/DrawingText.h
    src/IgesShapeCollector.h
    src/CGroup.h
    src/CPart.h
    src/CAssembled.h
    src/CFacadeFurniture.h
    src/solid/FacadeFrameShapeBuilder.h
    src/solid/SheetBendShapeBuilder.h
    src/CFurnitureAssemblies.h
    src/CFurnitureDrawer.h
    src/CKitchenCabinet.h
    src/Material.h
    src/render/RenderScene.h
    src/render/IExternalRenderer.h
    src/render/BlenderCyclesRenderer.h
    src/render/NativeRaytraceRenderer.h
    src/MaterialLibrary.h
    src/FurnitureMaterialFactory.h
    src/CAlfaDoc.h
    src/UndoRedo.h
    src/ExtrudeShapeBuilder.h
    src/Common.h
    src/CMesh3D.h
    src/CatmullClarkSurfaceBuilder.h
    src/Tools_Mesh3D.h
    src/mesh/ImageRelief.h
    src/ui/ImageReliefDialog.h
    src/solid/HolePlacement.h
    src/FillContour.h
    src/ContourQuadrangulator3DCoat.h
    src/Line2D.h
    src/SurfacePatchBuilder.h
    src/Net.h
    src/CBSpline.h
    src/CPolyline.h
    src/Conic.h
    src/CView2d.h
    src/CView3d.h
    src/Dom3DProjectSerializer.h
    src/CadCurve3D.h
    src/ExchangeIO.h
    src/FbxIO.h
    src/GlbIO.h
    src/IgesIO.h
    src/ObjIO.h
    src/OpenGLCompat.h
    src/ProjectIO.h
    src/StepIO.h
    src/ThreeDSIO.h
    src/solid/BeamShapeBuilder.h
    src/solid/PlaneShapeBuilder.h
    src/solid/OffsetFaceShapeBuilder.h
    src/solid/PolyhedronShapeBuilder.h
    src/solid/SketchFeatureShapeBuilder.h
    src/solid/SolidBeamTool.h
    src/solid/SolidBoxTool.h
    src/solid/SolidCylinderTool.h
    src/solid/SolidPrismTool.h
    src/solid/SolidSphereTool.h
    src/solid/SolidTorusTool.h
    src/solid/SurfaceSet.h
    src/solid/AssociativeClone.h
    src/solid/TwoSketchSolidBuilder.h
    src/solid/SolidTool.h
    src/solid/TrimShapeBuilder.h
    src/ui/MainWindow.h
    src/ui/DraftingWorkspace.h
    src/ui/MeasurementUnits.h
    src/ui/MaterialEditorDialog.h
    src/ui/MaterialDrag.h
    src/ui/MaterialSphereBrowser.h
    src/ui/OpenGLViewport.h
    src/ui/PreferencesDialog.h
    src/ui/LightingDialog.h
    src/ui/BlenderCyclesDialog.h
    src/ui/NativeRaytraceDialog.h
    src/ui/RenderLightEditorDialog.h
    src/ui/LanguageManager.h
    src/ui/HotkeyManagerDialog.h
    src/ui/CommandSearchPopup.h
    src/ui/PropertyPanel.h
    src/ui/DefaultDialogAccept.h
    src/ui/QtSceneRenderer.h
    src/ui/ToolRegistry.h
    src/ui/BooleanDialog.h
    src/ui/ExtrudeFaceDialog.h
    src/ui/ProjectOpenDialog.h
    src/solid/Solid.h
    src/solid/SurfaceFace.h
    src/Point3d.h
    src/ageom.h
    src/Plane.h
    src/Vector.h
    src/legacy_compat.h
    src/legacy_geometry_defs.h
    src/SystemCoord.h
    src/Triangle.h
    src/iges/AstraVect.h
    src/iges/defines.h
    src/iges/SplineCurve.h
)

# Minimal MFC-free sketch core.
set(DOM3D_SKETCH_SOURCES
    src/BezierSpline.cpp
    src/BezierSpline.h
    src/Dimens.cpp
    src/Dimens.h
    src/Constraint.cpp
    src/Constraint.h
    src/ConstraintBezierTangent.cpp
    src/ConstraintBezierTangent.h
    src/ConstraintHorLine.cpp
    src/ConstraintHorLine.h
    src/ConstraintVertLine.cpp
    src/ConstraintVertLine.h
    src/Fillet.cpp
    src/Fillet.h
    src/LinkLine.cpp
    src/LinkLine.h
    src/SurfaceRevolveProfile.h
    src/ui/RevolveToolHelp.h
    src/ui/StartupSplash.h
    src/SketchGeometry.h
    src/LinkLineHor.cpp
    src/LinkLineHor.h
    src/LinkLineVert.cpp
    src/LinkLineVert.h
    src/SmartLine.cpp
    src/Sketch.cpp
    src/MultiSketchProfileBuilder.cpp
    src/MultiSketchProfileBuilder.h
    src/Sketch.h
    src/SketchArcLine.cpp
    src/SmartLine.h
    src/SketchArcLine.h
    src/SketchProfileBuilder.cpp
    src/SketchProfileBuilder.h
    src/SweptSolidBuilder.cpp
    src/SweptSolidBuilder.h
    src/TwoRailSweepSurfaceBuilder.cpp
    src/FourSplineSurfaceBuilder.cpp
    src/TwoRailSweepSurfaceBuilder.h
)

# Remaining legacy sketch sources stay visible in the IDE while they are
# migrated away from MFC and the old Dom-3D object hierarchy.
set(DOM3D_SKETCH_MIGRATION_FILES
    src/Arc.cpp
    src/Arc.h
    src/Circle2D.cpp
    src/Circle2D.h
    src/Circle3D.cpp
    src/Circle3D.h
    src/LineStyleBox.cpp
    src/LineStyleBox.h
    src/OutLine.cpp
    src/OutLine.h
    src/Point5d.cpp
    src/Point5d.h
    src/Spline.cpp
    src/Spline.h
)

if(MSVC)
    # OCCT's worker-thread SEH translator requires asynchronous unwinding.
    set_property(SOURCE src/CAlfaDoc.cpp APPEND PROPERTY COMPILE_OPTIONS /EHa)
endif()

set_source_files_properties(
    ${DOM3D_SKETCH_MIGRATION_FILES}
    PROPERTIES HEADER_FILE_ONLY TRUE
)

if(WIN32)
    list(APPEND DOM3D_SOURCES src/resources/Dom3D_Pro.rc)
endif()

add_executable(Dom3D_Pro
    ${DOM3D_SOURCES}
    ${DOM3D_HEADERS}
    ${DOM3D_SKETCH_SOURCES}
    ${DOM3D_SKETCH_MIGRATION_FILES}
)
dom3d_link_3mf(Dom3D_Pro)
dom3d_link_assimp(Dom3D_Pro)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Dom3D_Pro)
set_target_properties(Dom3D_Pro PROPERTIES
    OUTPUT_NAME "Dom3D_Pro"
    WIN32_EXECUTABLE TRUE
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Dom3D_Pro>"
    VS_DEBUGGER_ENVIRONMENT "PATH=${DOM3D_QT_MSVC2022_PATH}/bin;${DOM3D_OPENCASCADE_BINARY_DIR};%PATH%"
)

# Mirror the real source tree in Visual Studio Solution Explorer. Sources and
# headers intentionally stay together, matching their folders on disk.
set(DOM3D_IDE_FILES
    ${DOM3D_SOURCES}
    ${DOM3D_HEADERS}
    ${DOM3D_SKETCH_SOURCES}
    ${DOM3D_SKETCH_MIGRATION_FILES}
)
source_group(
    TREE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES ${DOM3D_IDE_FILES}
)

target_compile_features(Dom3D_Pro PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(Dom3D_Pro PRIVATE /utf-8)
endif()
target_include_directories(Dom3D_Pro PRIVATE "${DOM3D_OPENCASCADE_INCLUDE_DIR}")
target_link_directories(Dom3D_Pro PRIVATE "${DOM3D_OPENCASCADE_LIBRARY_DIR}")
target_link_libraries(Dom3D_Pro PRIVATE
    Qt6::Widgets
    Qt6::OpenGLWidgets
    Qt6::Xml
    Qt6::PrintSupport
    OpenGL::GL
    raytrace
    dom3d_comms
    dom3d_mesh_adapter
    ${DOM3D_OPENCASCADE_LIBS})
if(MSVC)
    # Keep an address map next to the executable so Windows crash dumps from
    # optimized builds can be resolved even when no debugger was attached.
    target_link_options(Dom3D_Pro PRIVATE
        "/MAP:$<TARGET_FILE_DIR:Dom3D_Pro>/Dom3D_Pro.map"
        "/MAPINFO:EXPORTS")
endif()

if(WIN32)
    file(GLOB DOM3D_OPENCASCADE_RUNTIME_DLLS
        "${DOM3D_OPENCASCADE_BINARY_DIR}/TK*.dll"
    )
    add_custom_command(TARGET Dom3D_Pro POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${DOM3D_OPENCASCADE_RUNTIME_DLLS}
            "$<TARGET_FILE_DIR:Dom3D_Pro>"
        COMMENT "Copying OpenCascade runtime DLLs"
        COMMAND_EXPAND_LISTS
    )
endif()

add_custom_command(TARGET Dom3D_Pro POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/materials"
        "$<TARGET_FILE_DIR:Dom3D_Pro>/materials"
    COMMENT "Copying material library"
)

add_custom_command(TARGET Dom3D_Pro POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Languages"
        "$<TARGET_FILE_DIR:Dom3D_Pro>/Languages"
    COMMENT "Copying language files"
)

set(DOM3D_USER_MANUAL
    "${CMAKE_CURRENT_SOURCE_DIR}/output/pdf/User Manual Dom3D Pro.pdf")
if(EXISTS "${DOM3D_USER_MANUAL}")
    add_custom_command(TARGET Dom3D_Pro POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DOM3D_USER_MANUAL}"
            "$<TARGET_FILE_DIR:Dom3D_Pro>/User Manual Dom3D Pro.pdf"
        COMMENT "Copying Dom3D Pro user manual"
    )
endif()

set(DOM3D_WINDEPLOYQT "${DOM3D_QT_MSVC2022_PATH}/bin/windeployqt.exe")
if(WIN32 AND EXISTS "${DOM3D_WINDEPLOYQT}")
    add_custom_command(TARGET Dom3D_Pro POST_BUILD
        COMMAND "${DOM3D_WINDEPLOYQT}"
            --no-translations
            --no-compiler-runtime
            --dir "$<TARGET_FILE_DIR:Dom3D_Pro>"
            "$<TARGET_FILE:Dom3D_Pro>"
        COMMENT "Deploying Qt runtime DLLs"
    )
endif()

if(WIN32)
    target_link_libraries(Dom3D_Pro PRIVATE comdlg32)
endif()

if(WIN32)
    add_library(Dom3DThumbnailProvider SHARED
        src/shell/Dom3DThumbnailProvider.cpp
        src/shell/Dom3DThumbnailProvider.def
    )
    target_compile_features(Dom3DThumbnailProvider PRIVATE cxx_std_17)
    target_compile_definitions(Dom3DThumbnailProvider PRIVATE UNICODE _UNICODE)
    target_link_libraries(Dom3DThumbnailProvider PRIVATE
        advapi32
        crypt32
        ole32
        shell32
        windowscodecs
    )
    set_target_properties(Dom3DThumbnailProvider PROPERTIES
        OUTPUT_NAME "Dom3DThumbnailProvider"
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Dom3DThumbnailProvider>"
    )
endif()

if(MSVC)
    target_compile_options(Dom3D_Pro PRIVATE
        /W4
        /permissive-
        "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_compat.h"
    )
endif()

if(BUILD_TESTING)
    add_executable(Line2DTests
        tests/Line2DTests.cpp
        src/Line2D.cpp
        src/Line2D.h)
    target_compile_features(Line2DTests PRIVATE cxx_std_17)
    target_include_directories(Line2DTests PRIVATE src)
    if(MSVC)
        target_compile_options(Line2DTests PRIVATE /W4 /permissive-)
    endif()
    add_test(NAME Line2DTests COMMAND Line2DTests)

    add_executable(SurfacePatchBuilderTests
        tests/SurfacePatchBuilderTests.cpp
        src/SurfacePatchBuilder.cpp
        src/SurfacePatchBuilder.h)
    target_compile_features(SurfacePatchBuilderTests PRIVATE cxx_std_17)
    target_include_directories(SurfacePatchBuilderTests PRIVATE src)
    if(MSVC)
        target_compile_options(SurfacePatchBuilderTests PRIVATE /W4 /permissive-)
    endif()
    add_test(NAME SurfacePatchBuilderTests COMMAND SurfacePatchBuilderTests)

    add_executable(CommsSmokeTests tests/CommsSmokeTests.cpp)
    target_compile_features(CommsSmokeTests PRIVATE cxx_std_17)
    target_link_libraries(CommsSmokeTests PRIVATE dom3d_comms)
    add_test(NAME CommsSmokeTests COMMAND CommsSmokeTests)

    add_executable(MeshContainerAdapterTests tests/MeshContainerAdapterTests.cpp)
    target_compile_features(MeshContainerAdapterTests PRIVATE cxx_std_17)
    target_link_libraries(MeshContainerAdapterTests PRIVATE dom3d_mesh_adapter)
    add_test(NAME MeshContainerAdapterTests COMMAND MeshContainerAdapterTests)

    add_executable(ThemeTests tests/ThemeTests.cpp src/ui/ThemeManager.cpp
        src/ui/LanguageManager.cpp src/ui/LanguageManager.h)
    target_compile_features(ThemeTests PRIVATE cxx_std_17)
    target_include_directories(ThemeTests PRIVATE src)
    target_link_libraries(ThemeTests PRIVATE Qt6::Widgets Qt6::Xml)
    add_test(NAME Themes COMMAND ThemeTests)
    set_tests_properties(Themes PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(PropertyPanelTests
        tests/PropertyPanelTests.cpp
        src/ui/PropertyPanel.cpp
        src/ui/PropertyPanel.h
        src/ui/LanguageManager.cpp
        src/ui/LanguageManager.h
        src/ui/MeasurementUnits.cpp
        src/ui/MeasurementUnits.h
    )
    add_executable(ToolHintTests tests/ToolHintTests.cpp
        src/ui/LanguageManager.cpp src/ui/LanguageManager.h src/resources/icons.qrc)
    target_compile_features(ToolHintTests PRIVATE cxx_std_17)
    target_include_directories(ToolHintTests PRIVATE src)
    target_link_libraries(ToolHintTests PRIVATE Qt6::Widgets Qt6::Xml)
    if(MSVC)
        target_compile_options(ToolHintTests PRIVATE /utf-8)
    endif()
    add_custom_command(TARGET ToolHintTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Languages" "$<TARGET_FILE_DIR:ToolHintTests>/Languages")
    add_test(NAME ToolHints COMMAND ToolHintTests)
    set_tests_properties(ToolHints PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    target_compile_features(PropertyPanelTests PRIVATE cxx_std_17)
    target_include_directories(PropertyPanelTests PRIVATE
        src
        "${DOM3D_OPENCASCADE_INCLUDE_DIR}"
    )
    target_link_libraries(PropertyPanelTests PRIVATE Qt6::Widgets Qt6::Xml)
    if(MSVC)
        target_compile_options(PropertyPanelTests PRIVATE
            /W4
            /permissive-
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_compat.h"
        )
    endif()
    add_test(NAME PropertyPanelTests COMMAND PropertyPanelTests)
    # Popup focus must be deterministic while the user works in other windows.
    set_tests_properties(PropertyPanelTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(PbrMaterialLibraryTests
        tests/PbrMaterialLibraryTests.cpp
        src/Material.cpp
        src/MaterialLibrary.cpp
    )
    target_compile_features(PbrMaterialLibraryTests PRIVATE cxx_std_17)
    target_include_directories(PbrMaterialLibraryTests PRIVATE src)
    target_link_libraries(PbrMaterialLibraryTests PRIVATE Qt6::Core Qt6::Xml)
    if(MSVC)
        target_compile_options(PbrMaterialLibraryTests PRIVATE /W4 /permissive-)
    endif()
    add_test(NAME PbrMaterialLibraryTests COMMAND PbrMaterialLibraryTests)

    add_executable(QuadroBoundaryTests tests/QuadroBoundaryTests.cpp src/solid/QuadroBoundary.cpp src/solid/QuadroFaceBoundary.cpp src/solid/QuadroPatchMesh.cpp)
    target_compile_features(QuadroBoundaryTests PRIVATE cxx_std_17)
    target_include_directories(QuadroBoundaryTests PRIVATE src "${DOM3D_OPENCASCADE_INCLUDE_DIR}")
    target_link_directories(QuadroBoundaryTests PRIVATE "${DOM3D_OPENCASCADE_LIBRARY_DIR}")
    target_link_libraries(QuadroBoundaryTests PRIVATE ${DOM3D_OPENCASCADE_LIBS})
    add_test(NAME QuadroBoundarySnapshot COMMAND QuadroBoundaryTests)

    set(CATALOG_IMPORT_TEST_SOURCES ${DOM3D_SOURCES})
    list(REMOVE_ITEM CATALOG_IMPORT_TEST_SOURCES src/main.cpp)
    add_executable(CatalogImportTests
        tests/CatalogImportTests.cpp
        tests/PrismHollowFilletTests.cpp
        tests/BezierPocketFilletTests.cpp
        tests/TurnedBandMeshTests.cpp
        tests/NistCtcMeshTests.cpp
        tests/SolidCenterlineTests.cpp
        tests/RepeatCommandTests.cpp
        tests/ScenePersistenceTests.cpp
        tests/PanelContourTests.cpp
        tests/DraftingOutlineTests.cpp
        tests/DraftingDimensionTests.cpp
        tests/SheetBendTests.cpp
        tests/ProjectOpenDialogTests.cpp
        tests/BooleanToolTests.cpp
        tests/SolidPrimitiveToolTests.cpp
        tests/TileToolTests.cpp
        tests/SurfaceDisplayNetTests.cpp
        tests/HighDpiViewportTests.cpp
        tests/KitchenLayoutTests.cpp
        tests/CurveEndpointLinkTests.cpp
        tests/TextToCurvesTests.cpp
        tests/MultiSketchTests.cpp
        tests/ProceduralMaterialTests.cpp
        tests/ToolsMesh3DTests.cpp
        tests/ImageReliefTests.cpp
        tests/BallCylinderTests.cpp
        tests/SphereUnionTests.cpp
        tests/PlasticityImportTests.cpp
        tests/BoolAndFillTests.cpp
        tests/DraftFaceTests.cpp
        tests/HolePlacementTests.cpp
        tests/QuadroPillowTests.cpp
        tests/QuadroHairdryerTests.cpp
        tests/QuadroChartFillTests.cpp
        ${CATALOG_IMPORT_TEST_SOURCES}
        ${DOM3D_HEADERS}
        ${DOM3D_SKETCH_SOURCES}
    )
    target_compile_features(CatalogImportTests PRIVATE cxx_std_17)
    dom3d_link_3mf(CatalogImportTests)
    dom3d_link_assimp(CatalogImportTests)
    target_include_directories(CatalogImportTests PRIVATE
        src
        "${DOM3D_OPENCASCADE_INCLUDE_DIR}"
    )
    target_link_directories(CatalogImportTests PRIVATE
        "${DOM3D_OPENCASCADE_LIBRARY_DIR}"
    )
    # The combined scene/UI test harness keeps many fixtures in one function.
    # Match the usual 8 MiB test-thread stack instead of MSVC's 1 MiB default.
    if(MSVC)
        target_link_options(CatalogImportTests PRIVATE /STACK:8388608)
    endif()
    target_link_libraries(CatalogImportTests PRIVATE
        Qt6::Widgets Qt6::OpenGLWidgets Qt6::Xml Qt6::PrintSupport OpenGL::GL
        raytrace
        ${DOM3D_OPENCASCADE_LIBS}
    )
    if(WIN32)
        target_link_libraries(CatalogImportTests PRIVATE comdlg32)
    endif()
    if(MSVC)
        target_compile_options(CatalogImportTests PRIVATE
            /W4
            /permissive-
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_compat.h"
        )
    endif()
    target_sources(CatalogImportTests PRIVATE tests/NativeColorTests.cpp)
    add_test(NAME PlasticityImport COMMAND CatalogImportTests --test-plasticity-import
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Plasticity-F1.stp")
    set_tests_properties(PlasticityImport PROPERTIES TIMEOUT 120)
    add_test(NAME NativeColors COMMAND CatalogImportTests --test-native-colors)
    add_test(NAME ImageRelief COMMAND CatalogImportTests --test-image-relief)
    set_tests_properties(ImageRelief PROPERTIES TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME CatalogImportTests COMMAND CatalogImportTests)
    add_test(NAME SurfaceDisplayNet COMMAND CatalogImportTests --test-surface-display-net)
    add_test(NAME NistSteppedCylinderSpacing COMMAND CatalogImportTests --test-cylinder-step
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Nist_Ftc06_SteppedCylinders.stp"
        "${CMAKE_CURRENT_BINARY_DIR}/nist-stepped-cylinder-regression")
    set_tests_properties(NistSteppedCylinderSpacing PROPERTIES TIMEOUT 90 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME NistRegularDivideFacePanel COMMAND CatalogImportTests --test-cylinder-step
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Nist_Ftc06_SteppedCylinders.stp"
        "${CMAKE_CURRENT_BINARY_DIR}/nist-regular-panel-regression")
    set_tests_properties(NistRegularDivideFacePanel PROPERTIES TIMEOUT 90
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;DOM3D_TEST_REGULAR_PANEL=1")
    add_test(NAME NistRegularDivideFacePanelCoarse COMMAND CatalogImportTests --test-cylinder-step
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Nist_Ftc06_SteppedCylinders.stp"
        "${CMAKE_CURRENT_BINARY_DIR}/nist-regular-panel-coarse-regression")
    set_tests_properties(NistRegularDivideFacePanelCoarse PROPERTIES TIMEOUT 90
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;DOM3D_TEST_REGULAR_PANEL=1;DOM3D_TEST_PANEL_DENSITY=0.5")
    add_test(NAME NistTrimByPlineWithSlx COMMAND CatalogImportTests --test-cylinder-step
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Nist_Ftc06_SteppedCylinders.stp"
        "${CMAKE_CURRENT_BINARY_DIR}/nist-trimpline-slx-regression")
    set_tests_properties(NistTrimByPlineWithSlx PROPERTIES TIMEOUT 90
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;DOM3D_TEST_TRIM_OUTER_SLX=1")
    add_test(NAME NistDivideFaceOuterTrimOff COMMAND CatalogImportTests --test-cylinder-step
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Nist_Ftc06_SteppedCylinders.stp"
        "${CMAKE_CURRENT_BINARY_DIR}/nist-divide-outer-off-regression")
    set_tests_properties(NistDivideFaceOuterTrimOff PROPERTIES TIMEOUT 90
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;DOM3D_TEST_DIVIDE_ONLY=1")
    foreach(scale IN ITEMS 1 1.25 1.5 2)
        add_test(NAME HighDpiViewport_${scale} COMMAND CatalogImportTests --test-high-dpi-viewport)
        set_tests_properties(HighDpiViewport_${scale} PROPERTIES TIMEOUT 60
            ENVIRONMENT "QT_QPA_PLATFORM=windows;QT_SCALE_FACTOR=${scale}")
    endforeach()
    add_test(NAME CylinderPrismQuadroNormals COMMAND CatalogImportTests --test-cylinder-quadro-normals
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Cylinder_Min_Prizm_BAD_Quadro.dom3d")
    set_tests_properties(CylinderPrismQuadroNormals PROPERTIES TIMEOUT 60)
    add_test(NAME TwoRailSurfaceDocument COMMAND CatalogImportTests --test-two-rail-surface-document
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Swept-2.dom3d")
    set_tests_properties(TwoRailSurfaceDocument PROPERTIES TIMEOUT 120)
    add_test(NAME PeriodicBSpline COMMAND CatalogImportTests --test-periodic-bspline
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Swept-2.dom3d")
    set_tests_properties(PeriodicBSpline PROPERTIES TIMEOUT 120)
    add_test(NAME KitchenLayout COMMAND CatalogImportTests --test-kitchen-layout -platform offscreen)
    add_test(NAME CurveEndpointLinks COMMAND CatalogImportTests --test-curve-endpoint-links -platform offscreen)
    # The Windows offscreen plugin uses box glyphs; native font outlines require
    # the Windows font database. This test never shows its MainWindow.
    if(WIN32)
        set(DOM3D_TEXT_TEST_PLATFORM windows)
    else()
        set(DOM3D_TEXT_TEST_PLATFORM offscreen)
    endif()
    add_test(NAME TextToCurves COMMAND CatalogImportTests --test-text-to-curves -platform ${DOM3D_TEXT_TEST_PLATFORM})
    set_tests_properties(TextToCurves PROPERTIES TIMEOUT 60)
    add_test(NAME CylinderCenterDrag COMMAND CatalogImportTests --test-multi-sketch --cylinder-center-drag -platform ${DOM3D_TEXT_TEST_PLATFORM})
    set_tests_properties(CylinderCenterDrag PROPERTIES TIMEOUT 90)
    add_test(NAME DraftMeasureUndo COMMAND CatalogImportTests --test-multi-sketch --draft-measure-undo -platform ${DOM3D_TEXT_TEST_PLATFORM})
    set_tests_properties(DraftMeasureUndo PROPERTIES TIMEOUT 60)
    add_test(NAME MultiSketch COMMAND CatalogImportTests --test-multi-sketch -platform ${DOM3D_TEXT_TEST_PLATFORM})
    add_test(NAME BoxCutFilletRebuild COMMAND CatalogImportTests --test-multi-sketch --box-cut-fillet -platform ${DOM3D_TEXT_TEST_PLATFORM})
    set_tests_properties(BoxCutFilletRebuild PROPERTIES TIMEOUT 180)
    add_test(NAME SolidSketchTools COMMAND CatalogImportTests --test-multi-sketch --solid-tools-only -platform ${DOM3D_TEXT_TEST_PLATFORM})
    set_tests_properties(SolidSketchTools PROPERTIES TIMEOUT 180)
    add_test(NAME SweptEllipseEditing COMMAND CatalogImportTests --test-multi-sketch --swept-profile-perf "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/ui-regression/Swept_Ellipse_Edit.dom3d" -platform ${DOM3D_TEXT_TEST_PLATFORM})
    set_tests_properties(SweptEllipseEditing PROPERTIES TIMEOUT 120)
    add_test(NAME RevolveFilletUndo COMMAND CatalogImportTests --test-multi-sketch --revolve-undo-only -platform ${DOM3D_TEXT_TEST_PLATFORM})
    set_tests_properties(RevolveFilletUndo PROPERTIES TIMEOUT 120)
    add_test(NAME SweepFeatureCreationUndo COMMAND CatalogImportTests --test-multi-sketch --sweep-feature-undo-only -platform ${DOM3D_TEXT_TEST_PLATFORM})
    set_tests_properties(SweepFeatureCreationUndo PROPERTIES TIMEOUT 120)
    set_tests_properties(MultiSketch PROPERTIES TIMEOUT 60)
    add_test(NAME QuadroBodyBoundaryCache COMMAND CatalogImportTests --test-quadro-body-cache)
    add_test(NAME BridgeShellQuadro COMMAND CatalogImportTests --test-bridge-shell-quadro
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Bridge_Shell.dom3d")
    set_tests_properties(BridgeShellQuadro PROPERTIES TIMEOUT 90)
    add_test(NAME PillowCadQuadro COMMAND CatalogImportTests --test-pillow-cad-quadro
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Podushka-2.dom3d")
    add_test(NAME FilletMeshNormals COMMAND CatalogImportTests --test-fillet-mesh-normals
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Prism_Extrude_AllFillet.dom3d")
    add_test(NAME PeriodicFilletBands COMMAND CatalogImportTests --test-periodic-bands
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Box_And_Cylinder_Plast.dom3d")
    set_tests_properties(PeriodicFilletBands PROPERTIES TIMEOUT 90)
    add_test(NAME CylinderTurnedBands COMMAND CatalogImportTests --test-turned-bands
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Cylinder_Plast.dom3d" 0.40 plain)
    add_test(NAME CompoundTubeBands COMMAND CatalogImportTests --test-turned-bands
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/compound_498d6863_0365.dom3d" 0.35 slx)
    add_test(NAME CompoundSideBands COMMAND CatalogImportTests --test-turned-bands
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/compound_498d6927_0561.dom3d" 0.30 slx)
    set_tests_properties(CylinderTurnedBands CompoundTubeBands CompoundSideBands PROPERTIES TIMEOUT 90)
    add_test(NAME PocketRectangularBoundary COMMAND CatalogImportTests --test-demos-mesh
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Box_Min_Box_2.dom3d")
    set_tests_properties(PocketRectangularBoundary PROPERTIES TIMEOUT 90)
    add_test(NAME BallHemisphereQuadro COMMAND CatalogImportTests --test-demos-mesh
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Ball_Hemisphere.dom3d")
    set_tests_properties(BallHemisphereQuadro PROPERTIES TIMEOUT 90)
    add_test(NAME DemosGearMesh COMMAND CatalogImportTests --test-demos-mesh
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Gear-2.dom3d")
    add_test(NAME DemosNistCones COMMAND CatalogImportTests --test-demos-mesh
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Nist_Demos.dom3d")
    set_tests_properties(DemosGearMesh DemosNistCones PROPERTIES TIMEOUT 240)
    add_test(NAME NistCtc05Mesh COMMAND CatalogImportTests --test-nist-ctc
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Nist_Ctc05.dom3d")
    set_tests_properties(NistCtc05Mesh PROPERTIES TIMEOUT 120 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME NistCtc05DivideFace COMMAND CatalogImportTests --test-nist-ctc
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Nist_Ctc05.dom3d")
    set_tests_properties(NistCtc05DivideFace PROPERTIES TIMEOUT 120
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;DOM3D_TEST_CTC_DIVIDE=1")
    add_test(NAME FlangedCircularHoles COMMAND CatalogImportTests --test-turned-bands
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/flanged_498d68f0_0506.dom3d" 0.50 slx)
    set_tests_properties(FlangedCircularHoles PROPERTIES TIMEOUT 90)
    add_test(NAME FlangedIntersectingHoles COMMAND CatalogImportTests --test-turned-bands
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/flanged_498d6a4b_0853.dom3d" 0.50 slx)
    set_tests_properties(FlangedIntersectingHoles PROPERTIES TIMEOUT 90)
    add_test(NAME FlangedLargeIntersectingHoles COMMAND CatalogImportTests --test-turned-bands
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Flanged.dom3d" 0.55 slx)
    set_tests_properties(FlangedLargeIntersectingHoles PROPERTIES TIMEOUT 90)
    add_test(NAME RibQuadro COMMAND CatalogImportTests --test-rib-quadro
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Rib.dom3d")
    set_tests_properties(RibQuadro PROPERTIES TIMEOUT 90)
    add_test(NAME FrameCadQuadro COMMAND CatalogImportTests --test-frame-cad-quadro
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Frame.dom3d")
    set_tests_properties(PillowCadQuadro FrameCadQuadro PROPERTIES TIMEOUT 90)
    add_test(NAME HairdryerCadBoundary COMMAND CatalogImportTests --test-hairdryer-cad-boundary
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Hairdryer.dom3d")
    set_tests_properties(HairdryerCadBoundary PROPERTIES TIMEOUT 90)
    add_test(NAME MixedLowPolyCompletion COMMAND CatalogImportTests --test-mixed-low-poly)
    add_test(NAME MultirowHoleZone COMMAND CatalogImportTests --test-multirow-hole-zone
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Box_Min_14_Box.dom3d")
    set_tests_properties(MultirowHoleZone PROPERTIES TIMEOUT 90 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME FrameSlxBoundary COMMAND CatalogImportTests --test-frame-slx-boundary)
    add_test(NAME BoxAllFill4Normals COMMAND CatalogImportTests --test-fillet-mesh-normals
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Box_All_Fill_4.dom3d")
    set_tests_properties(BoxAllFill4Normals PROPERTIES TIMEOUT 90 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME ImportedFrameEndCaps COMMAND CatalogImportTests --test-imported-endcaps-window
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Frame_Imported_EndCaps.dom3d" frame)
    add_test(NAME FilletedTwoCylinders COMMAND CatalogImportTests --test-imported-endcaps-window
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Two_Cylinders_Filleted.dom3d" cylinders)
    set_tests_properties(ImportedFrameEndCaps FilletedTwoCylinders PROPERTIES TIMEOUT 180
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME SquareFilletedQuadro COMMAND CatalogImportTests --test-square-filleted-quadro
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Square_Filleted.dom3d)
    add_test(NAME Frame2HybridFillet COMMAND CatalogImportTests --test-frame2-fillet
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Frame-2.dom3d)
    set_tests_properties(Frame2HybridFillet PROPERTIES TIMEOUT 180)
    add_test(NAME Frame2FilletRenderSeams COMMAND CatalogImportTests --test-frame2-render-seams
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Frame-2-solid.dom3d)
    add_test(NAME GroupedSixHoles COMMAND CatalogImportTests --test-grouped-hole-zones
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Box_Min_6_Boxex.dom3d" 1)
    add_test(NAME GroupedFiveHoles COMMAND CatalogImportTests --test-grouped-hole-zones
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Box_Min_5_Box.dom3d" 1)
    add_test(NAME GroupedTwelveHoles COMMAND CatalogImportTests --test-grouped-hole-zones
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Box_Min_12_Box.dom3d" 2)
    add_test(NAME GroupedFourteenHoles COMMAND CatalogImportTests --test-grouped-hole-zones
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Box_Min_14_Box.dom3d" 2)
    set_tests_properties(FrameSlxBoundary GroupedSixHoles GroupedFiveHoles
        GroupedTwelveHoles GroupedFourteenHoles PROPERTIES TIMEOUT 180
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    set_tests_properties(MixedLowPolyCompletion PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME HairdryerCadChartConsumer COMMAND CatalogImportTests --diagnose-hairdryer-chart-fill
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Hairdryer.dom3d" "${CMAKE_CURRENT_BINARY_DIR}/hairdryer-chart-consumer")
    set_tests_properties(HairdryerCadChartConsumer PROPERTIES TIMEOUT 240)
    add_test(NAME RepeatCommand COMMAND CatalogImportTests --test-repeat-command -platform offscreen)
    add_test(NAME PanelContour COMMAND CatalogImportTests --test-panel-contour)
    add_test(NAME PanelContourSavedBoundary COMMAND CatalogImportTests --test-panel-contour)
    set_tests_properties(PanelContourSavedBoundary PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;DOM3D_PANEL_CONTOUR_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/data/ui-regression/Panel-boundary.dom3d"
        TIMEOUT 30)
    add_test(NAME CarBulgedPanelQuadro COMMAND CatalogImportTests --test-panel-contour)
    set_tests_properties(CarBulgedPanelQuadro PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;DOM3D_PANEL_MESH_FIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Car-2-BulgedPanel.dom3d"
        TIMEOUT 60)
    add_test(NAME PanelBulgeUI COMMAND CatalogImportTests --test-scene-persistence --bulge-only -platform offscreen)
    set_tests_properties(PanelBulgeUI PROPERTIES TIMEOUT 30)
    add_test(NAME PanelContourUI COMMAND CatalogImportTests --test-scene-persistence --panel-contour-only -platform offscreen)
    add_test(NAME ExtractFaceUI COMMAND CatalogImportTests --test-scene-persistence --extract-face-only -platform offscreen)
    set_tests_properties(ExtractFaceUI PROPERTIES TIMEOUT 30)
    set_tests_properties(PanelContourUI PROPERTIES TIMEOUT 30)
    add_test(NAME ScenePersistence COMMAND CatalogImportTests --test-scene-persistence -platform offscreen)
    add_test(NAME ClonePlacement COMMAND CatalogImportTests --test-scene-persistence --clone-placement-only -platform offscreen)
    set_tests_properties(ClonePlacement PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 120)
    add_test(NAME SurfaceTopology COMMAND CatalogImportTests --test-scene-persistence --surface-topology
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/JoinSurace.dom3d" -platform offscreen)
    set_tests_properties(SurfaceTopology PROPERTIES TIMEOUT 120)
    add_test(NAME SurfaceBridgeFourSplines COMMAND CatalogImportTests --test-scene-persistence --surface-bridge-file
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/SurfaceFourSplines.dom3d" -platform offscreen)
    set_tests_properties(SurfaceBridgeFourSplines PROPERTIES TIMEOUT 60)
    add_test(NAME SurfaceBridgeSaved COMMAND CatalogImportTests --test-scene-persistence --surface-bridge-file
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Bridge_Shell.dom3d")
    set_tests_properties(SurfaceBridgeSaved PROPERTIES TIMEOUT 90 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME SurfaceBridgeClosed COMMAND CatalogImportTests --test-scene-persistence --surface-bridge-file
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Bridge-2.dom3d" --closed-bridge -platform offscreen)
    add_test(NAME SurfaceBridgeClosedAlignment COMMAND CatalogImportTests --test-scene-persistence --surface-bridge-closed-only -platform offscreen)
    set_tests_properties(SurfaceBridgeClosed SurfaceBridgeClosedAlignment PROPERTIES TIMEOUT 90 WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME SurfacePatchFailureUI COMMAND CatalogImportTests --test-scene-persistence --surface-patch-file
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Patch_IncompatibleCorners.dom3d" -platform offscreen)
    set_tests_properties(SurfacePatchFailureUI PROPERTIES TIMEOUT 60)
    add_test(NAME SurfaceEdgePatchTool COMMAND CatalogImportTests --test-scene-persistence --surface-edge-patch -platform offscreen)
    set_tests_properties(SurfaceEdgePatchTool PROPERTIES TIMEOUT 180)
    add_test(NAME SurfaceBridgeComplex COMMAND CatalogImportTests --test-scene-persistence --surface-bridge-complex -platform offscreen)
    set_tests_properties(SurfaceBridgeComplex PROPERTIES TIMEOUT 120)
    add_test(NAME SurfaceBridgeTool COMMAND CatalogImportTests --test-scene-persistence --surface-bridge-only -platform offscreen)
    add_test(NAME SurfaceOffsetTool COMMAND CatalogImportTests --test-scene-persistence --surface-offset-only -platform offscreen)
    set_tests_properties(SurfaceOffsetTool PROPERTIES TIMEOUT 90)
    add_test(NAME OffsetBodyFillet COMMAND CatalogImportTests --test-scene-persistence
        --offset-body-fillet "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Offset_Surface_Body.dom3d" -platform offscreen)
    set_tests_properties(OffsetBodyFillet PROPERTIES TIMEOUT 90)
    add_test(NAME SewnBodyHistory COMMAND CatalogImportTests --test-scene-persistence
        --sewn-body-history "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Offset_Surface_Body.dom3d" -platform offscreen)
    set_tests_properties(SewnBodyHistory PROPERTIES TIMEOUT 120)
    add_test(NAME ExtractSurfaceEdge COMMAND CatalogImportTests --test-scene-persistence
        --extract-surface-edge "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Offset_Surface_Body.dom3d" -platform offscreen)
    set_tests_properties(ExtractSurfaceEdge PROPERTIES TIMEOUT 120)
    add_test(NAME LoftBezier COMMAND CatalogImportTests --test-scene-persistence
        --loft-bezier "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Offset_Surface_Body.dom3d" -platform offscreen)
    set_tests_properties(LoftBezier PROPERTIES TIMEOUT 120)
    add_test(NAME FilletResponsive COMMAND CatalogImportTests --test-scene-persistence
        --fillet-responsive "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Offset_Surface_Corners_R12.dom3d" -platform offscreen)
    set_tests_properties(FilletResponsive PROPERTIES TIMEOUT 180)
    add_test(NAME SurfaceDisplayOptions COMMAND CatalogImportTests --test-scene-persistence --surface-display-options -platform offscreen)
    set_tests_properties(SurfaceDisplayOptions PROPERTIES TIMEOUT 90)
    add_test(NAME SurfaceReverseNormalsTool COMMAND CatalogImportTests --test-scene-persistence --surface-reverse-normals -platform offscreen)
    set_tests_properties(SurfaceReverseNormalsTool PROPERTIES TIMEOUT 90)
    add_test(NAME SurfaceSketchTrim COMMAND CatalogImportTests --test-scene-persistence --surface-sketch-trim --fuselage
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Plane_Fuselag.dom3d")
    set_tests_properties(SurfaceSketchTrim PROPERTIES TIMEOUT 180)
    add_test(NAME SurfaceTwoViewFuselage COMMAND CatalogImportTests --test-scene-persistence --two-view-file
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Plane_Fuselag.dom3d")
    set_tests_properties(SurfaceTwoViewFuselage PROPERTIES TIMEOUT 120)
    add_test(NAME SurfaceTwoGuides COMMAND CatalogImportTests --test-scene-persistence --two-guides
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Helicopter-Trim.dom3d")
    set_tests_properties(SurfaceTwoGuides PROPERTIES TIMEOUT 120)
    add_test(NAME SurfaceTrimHelicopter COMMAND CatalogImportTests --test-scene-persistence --helicopter-trim
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Helicopter-Trim.dom3d")
    set_tests_properties(SurfaceTrimHelicopter PROPERTIES TIMEOUT 120)
    add_test(NAME SurfaceTwoViewHelicopter COMMAND CatalogImportTests --test-scene-persistence --two-view-file
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Helicopter.dom3d" --helicopter)
    set_tests_properties(SurfaceTwoViewHelicopter PROPERTIES TIMEOUT 120)
    add_test(NAME SurfaceSaddle COMMAND CatalogImportTests --test-scene-persistence --saddle)
    set_tests_properties(SurfaceSaddle PROPERTIES TIMEOUT 120)
    add_test(NAME Body2 COMMAND CatalogImportTests --test-scene-persistence --body2 --mouse-file "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Mouse.dom3d")
    set_tests_properties(Body2 PROPERTIES TIMEOUT 180)
    add_test(NAME BodyPrimitives COMMAND CatalogImportTests --test-scene-persistence --body-primitives)
    add_test(NAME SubdivideExtrude COMMAND CatalogImportTests --test-scene-persistence --subdivide-extrude)
    set_tests_properties(SubdivideExtrude PROPERTIES TIMEOUT 180)
    add_test(NAME ExtrudeTaperPreview COMMAND CatalogImportTests --test-scene-persistence --extrude-taper-preview)
    set_tests_properties(ExtrudeTaperPreview PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME FilletExtrudeDistance COMMAND CatalogImportTests --test-scene-persistence
        --fillet-extrude-distance "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/solid-regression/Extrude_Fillet_Distance.dom3d")
    set_tests_properties(FilletExtrudeDistance PROPERTIES TIMEOUT 90 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME FilletEdgeIdentity COMMAND CatalogImportTests --test-scene-persistence
        --fillet-edge-identity "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/solid-regression/Extrude_and_Fill.dom3d")
    set_tests_properties(FilletEdgeIdentity PROPERTIES TIMEOUT 90 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME ChamferEdgeIdentity COMMAND CatalogImportTests --test-scene-persistence
        --chamfer-edge-identity "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/solid-regression/Extrude_and_Fill.dom3d")
    set_tests_properties(ChamferEdgeIdentity PROPERTIES TIMEOUT 90 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME SolidExtrudeGizmo COMMAND CatalogImportTests --test-scene-persistence --solid-extrude-gizmo)
    set_tests_properties(SolidExtrudeGizmo PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME SectionGraphs COMMAND CatalogImportTests --test-scene-persistence --section-graphs -platform offscreen)
    add_test(NAME Bottle3Sections COMMAND CatalogImportTests --test-scene-persistence --bottle3 --bottle-file ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/solid-regression/Bottle-3_SL.dom3d -platform offscreen)
    set_tests_properties(Bottle3Sections PROPERTIES TIMEOUT 240)
    add_test(NAME Bottle3ModelControls COMMAND CatalogImportTests --test-scene-persistence --bottle3 --model-controls --bottle-file ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/solid-regression/Bottle-3_SL.dom3d -platform offscreen)
    set_tests_properties(Bottle3ModelControls PROPERTIES TIMEOUT 240)
    set_tests_properties(SectionGraphs PROPERTIES TIMEOUT 360)
    add_test(NAME BodySectionSketch COMMAND CatalogImportTests --test-scene-persistence --body-section-sketch -platform offscreen)
    set_tests_properties(BodySectionSketch PROPERTIES TIMEOUT 120)
    set_tests_properties(BodyPrimitives PROPERTIES TIMEOUT 240)
    add_test(NAME Body1 COMMAND CatalogImportTests --test-scene-persistence --body1)
    set_tests_properties(Body1 PROPERTIES TIMEOUT 180)
    add_test(NAME TangentCapExtractedRevolution COMMAND CatalogImportTests --test-scene-persistence --cap-file
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Tangent Cap.dom3d")
    set_tests_properties(TangentCapExtractedRevolution PROPERTIES TIMEOUT 60)
    add_test(NAME TangentCapSweptProfile COMMAND CatalogImportTests --test-scene-persistence --swept-cap-file
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Swept-1_3_Cap.dom3d")
    set_tests_properties(TangentCapSweptProfile PROPERTIES TIMEOUT 60)
    add_test(NAME SurfaceCap COMMAND CatalogImportTests --test-scene-persistence --surface-cap)
    add_test(NAME IgesExportVisibility COMMAND CatalogImportTests --iges-export-visibility)
    set_tests_properties(IgesExportVisibility PROPERTIES TIMEOUT 60)
    add_test(NAME NSidedSurface COMMAND CatalogImportTests --test-scene-persistence --n-sided-surface
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/X_NURBS_6.dom3d")
    set_tests_properties(NSidedSurface PROPERTIES TIMEOUT 90)
    add_test(NAME NSidedContinuity COMMAND CatalogImportTests --test-scene-persistence --n-sided-continuity)
    set_tests_properties(NSidedContinuity PROPERTIES TIMEOUT 120)
    set_tests_properties(SurfaceCap PROPERTIES TIMEOUT 90)
    add_test(NAME SurfaceCapComplex COMMAND CatalogImportTests --test-scene-persistence --surface-cap --input
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Cap.dom3d")
    set_tests_properties(SurfaceCapComplex PROPERTIES TIMEOUT 60
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME Body1TangentCap COMMAND CatalogImportTests --test-scene-persistence --body1-cap-file
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/solid-regression/Body-1_Filleted.dom3d")
    set_tests_properties(Body1TangentCap PROPERTIES TIMEOUT 180)
    add_test(NAME Body1NativeRadii COMMAND CatalogImportTests --test-scene-persistence --body1-cap-file
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/solid-regression/Body-1_Filleted.dom3d" --native-rounding)
    set_tests_properties(Body1NativeRadii PROPERTIES TIMEOUT 60)
    add_test(NAME ZebraDialog COMMAND CatalogImportTests --test-scene-persistence --zebra-dialog)
    set_tests_properties(ZebraDialog PROPERTIES TIMEOUT 90)
    add_test(NAME SurfaceTwoView COMMAND CatalogImportTests --test-scene-persistence --two-view)
    add_test(NAME JoinSurfaceEdges COMMAND CatalogImportTests --test-scene-persistence
        --join-surface-edges-file ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/ui-regression/Join_Surface_Edges.dom3d)
    set_tests_properties(SurfaceTwoView PROPERTIES TIMEOUT 120)
    add_test(NAME SurfaceFilletTool COMMAND CatalogImportTests --test-scene-persistence --surface-fillet-file
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Fillet.dom3d -platform offscreen)
    set_tests_properties(SurfaceFilletTool PROPERTIES TIMEOUT 120)
    add_test(NAME SurfaceFilletLoft3 COMMAND CatalogImportTests --test-scene-persistence --fillet-loft-file
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Loft-3.dom3d -platform offscreen)
    set_tests_properties(SurfaceFilletLoft3 PROPERTIES TIMEOUT 120)
    add_test(NAME FourCurveAssociativity COMMAND CatalogImportTests --test-scene-persistence --four-curves-associative-only -platform offscreen)
    set_tests_properties(SurfaceBridgeTool FourCurveAssociativity PROPERTIES TIMEOUT 90)
    add_test(NAME QuickMenuCancellation COMMAND CatalogImportTests --test-scene-persistence --quick-menu-cancel-only -platform offscreen)
    add_test(NAME CurveQuickMenu COMMAND CatalogImportTests --test-scene-persistence --curve-quick-menu-only -platform offscreen)
    set_tests_properties(CurveQuickMenu PROPERTIES TIMEOUT 90 WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    set_tests_properties(QuickMenuCancellation PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME LowPolyDialogMeshState COMMAND CatalogImportTests --test-scene-persistence --low-poly-state-only -platform offscreen)
    set_tests_properties(LowPolyDialogMeshState PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME RgbOpacity COMMAND CatalogImportTests --test-scene-persistence --rgb-opacity-only
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/ui-regression/F-1-RGB.dom3d")
    set_tests_properties(RgbOpacity PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=windows")
    add_test(NAME BallAndBoxQuadro COMMAND CatalogImportTests --test-ball-box-quadro
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Ball_And_Box.dom3d")
    add_test(NAME BallAndCylinderQuadro COMMAND CatalogImportTests --test-ball-cylinder
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Ball_And_Cylinder.dom3d")
    add_test(NAME SphereUnionQuadro COMMAND CatalogImportTests --test-sphere-union
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Ball_Plus_4_Ball.dom3d")
    set_tests_properties(SphereUnionQuadro PROPERTIES TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME TwoSpheresQuadro COMMAND CatalogImportTests --test-sphere-union
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Bal_Plus_Ball.dom3d")
    set_tests_properties(TwoSpheresQuadro PROPERTIES TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME FilletedSphereUnionQuadro COMMAND CatalogImportTests --test-sphere-union
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Ball_Plus_4_Ball_Filleted.dom3d")
    set_tests_properties(FilletedSphereUnionQuadro PROPERTIES TIMEOUT 300 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    set_tests_properties(BallAndCylinderQuadro PROPERTIES TIMEOUT 120 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME BoolAndFillQuadro COMMAND CatalogImportTests --test-bool-and-fill
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Bool_And_Fill.dom3d")
    set_tests_properties(BoolAndFillQuadro PROPERTIES TIMEOUT 120 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME BoxDraftsQuadro COMMAND CatalogImportTests --test-box-drafts-mesh
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Box_Min_Box_And_Drafts.dom3d")
    set_tests_properties(BoxDraftsQuadro PROPERTIES TIMEOUT 120 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME DraftFaceReplay COMMAND CatalogImportTests --test-draft-face
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Draft_Face.dom3d")
    set_tests_properties(DraftFaceReplay PROPERTIES TIMEOUT 120 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME RoundedOpeningCollar COMMAND CatalogImportTests --test-rounded-opening-collar
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Box_Min_Filleted_Box.dom3d")
    add_test(NAME SurfaceSnapping COMMAND CatalogImportTests --test-scene-persistence --surface-snapping-only -platform offscreen)
    add_test(NAME SpatialCurvePlane COMMAND CatalogImportTests --test-scene-persistence --curve-plane-only -platform offscreen)
    add_test(NAME BridgeEndpointDefaults COMMAND CatalogImportTests --test-scene-persistence --bridge-defaults-only -platform offscreen)
    add_test(NAME HiddenGridCurveSnapping COMMAND CatalogImportTests --test-scene-persistence --hidden-grid-only -platform offscreen)
    add_test(NAME SweptCurveWorkPlaneInput COMMAND CatalogImportTests --test-scene-persistence --swept-curve-input
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/ui-regression/Swept-1.dom3d -platform offscreen)
    add_test(NAME EdgeToolAllEdges COMMAND CatalogImportTests --test-scene-persistence --edge-tools-only -platform offscreen)
    add_test(NAME ExtrudeContourFillet COMMAND CatalogImportTests --test-scene-persistence --extrude-contour-fillet
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/ui-regression/SL_Extrude.dom3d)
    set_tests_properties(ExtrudeContourFillet PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120)
    add_test(NAME SketchJointTools COMMAND CatalogImportTests --test-scene-persistence --sketch-joints-only
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/ui-regression/SL_Extrude.dom3d)
    set_tests_properties(SketchJointTools PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120)
    add_test(NAME CurveReleaseDependencies COMMAND CatalogImportTests --test-scene-persistence --curve-release-only
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/ui-regression/Car-2-CurveRelease.dom3d -platform offscreen)
    add_test(NAME CutCurveGeometry COMMAND CatalogImportTests --test-scene-persistence --cut-curve-only
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/ui-regression/Cut_Curve.dom3d -platform offscreen)
    set_tests_properties(CutCurveGeometry PROPERTIES TIMEOUT 120)
    add_test(NAME TrimCurveByPlaneGeometry COMMAND CatalogImportTests --test-scene-persistence --trim-curve-plane-only
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/ui-regression/Cut_CurveBy_Plane.dom3d -platform offscreen)
    set_tests_properties(TrimCurveByPlaneGeometry PROPERTIES TIMEOUT 120)
    add_test(NAME BodyCopyPlacement COMMAND CatalogImportTests --test-scene-persistence --body-copy-placement -platform offscreen)
    add_test(NAME FrozenObjects COMMAND CatalogImportTests --test-scene-persistence --freeze-object-only -platform offscreen)
    add_test(NAME ArrayClickSelection COMMAND CatalogImportTests --test-scene-persistence --array-pick-only -platform offscreen)
    add_test(NAME BasicArrayPreview COMMAND CatalogImportTests --test-scene-persistence --basic-array-preview-only -platform offscreen)
    set_tests_properties(BasicArrayPreview PROPERTIES TIMEOUT 60)
    add_test(NAME CurveArray COMMAND CatalogImportTests --test-scene-persistence --curve-array-only -platform offscreen)
    set_tests_properties(CurveArray PROPERTIES TIMEOUT 60)
    add_test(NAME ValidateObjectsBeforeSave COMMAND CatalogImportTests --test-scene-persistence --validate-save-only
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/ui-regression/Work_Plane.dom3d -platform offscreen)
    set_tests_properties(ValidateObjectsBeforeSave PROPERTIES TIMEOUT 60)
    add_test(NAME ReportedWorkPlaneTrimSelection COMMAND CatalogImportTests --test-scene-persistence --work-plane-trim-scene
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/ui-regression/Work_Plane.dom3d -platform offscreen)
    set_tests_properties(ReportedWorkPlaneTrimSelection PROPERTIES TIMEOUT 60)
    add_test(NAME FaceSelectionDisplay COMMAND CatalogImportTests --test-scene-persistence
        --face-selection-display-only -platform windows)
    set_tests_properties(FaceSelectionDisplay PROPERTIES TIMEOUT 120)
    add_test(NAME MaterialBrushFace COMMAND CatalogImportTests --test-scene-persistence --material-brush-only -platform offscreen)
    add_test(NAME GroupTransformAfterMaterialPreview COMMAND CatalogImportTests --test-scene-persistence
        --group-transform-only "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/ui-regression/Table_And_Chair.dom3d")
    set_tests_properties(GroupTransformAfterMaterialPreview PROPERTIES TIMEOUT 60)
    add_test(NAME UniversalTransformGizmo COMMAND CatalogImportTests --test-scene-persistence --universal-transform)
    set_tests_properties(UniversalTransformGizmo PROPERTIES TIMEOUT 60)
    if(WIN32)
        # QOpenGLWidget needs the Windows platform plugin for framebuffer tests.
        # The test window is placed outside the desktop.
        add_test(NAME AxisClipping COMMAND CatalogImportTests --test-scene-persistence --axes-clipping-only -platform windows)
        set_tests_properties(AxisClipping PROPERTIES TIMEOUT 30)
    endif()
    add_test(NAME RevolveTools COMMAND CatalogImportTests --test-scene-persistence --revolve-tools-only -platform offscreen)
    set_tests_properties(RevolveTools PROPERTIES TIMEOUT 90)
    add_test(NAME SketchTransactions COMMAND CatalogImportTests --test-scene-persistence --sketch-transaction-only -platform offscreen)
    set_tests_properties(SketchTransactions PROPERTIES TIMEOUT 60)
    add_test(NAME MovedCylinderFilletHandle COMMAND CatalogImportTests
        --test-scene-persistence --fillet-handle-only -platform offscreen)
    add_test(NAME DraftingDimensions COMMAND CatalogImportTests --test-drafting-dimensions)
    add_test(NAME DraftingDimensionHoleRecovery COMMAND CatalogImportTests --test-drafting-dimensions --recover-detail
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Detail-1-dimensions-hole.dom3d")
    add_test(NAME DraftingOutlines COMMAND CatalogImportTests --test-drafting-outlines
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Wire_Drawing.dom3d")
    add_test(NAME DraftingFirstShow COMMAND CatalogImportTests --test-drafting-outlines
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Wire_Drawing.dom3d" --first-show-only
        -platform offscreen)
    add_test(NAME PolylineShift COMMAND CatalogImportTests --test-scene-persistence --polyline-shift-only -platform offscreen)
    add_test(NAME SheetBendDetail COMMAND CatalogImportTests --test-sheet-bend-detail
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Detail-1.dom3d")
    add_test(NAME SheetBendThird COMMAND CatalogImportTests --test-sheet-bend-third
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Detail-1-third-bend.dom3d")
    add_test(NAME SheetBendFourth COMMAND CatalogImportTests --test-sheet-bend-fourth
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Detail-1-fourth-bend.dom3d")
    add_test(NAME SheetBendPreview COMMAND CatalogImportTests --test-scene-persistence
        --sheet-bend-ui-only "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Detail-1.dom3d"
        -platform offscreen)
    set_tests_properties(ScenePersistence PROPERTIES TIMEOUT 60)
    add_test(NAME ProjectOpenDialog COMMAND CatalogImportTests --test-project-open-dialog -platform offscreen)
    set_tests_properties(ProjectOpenDialog PROPERTIES TIMEOUT 30)
    add_test(NAME BooleanTool COMMAND CatalogImportTests --test-boolean-tool -platform offscreen)
    add_test(NAME SolidPrimitiveTool COMMAND CatalogImportTests --test-solid-primitive-tool -platform offscreen)
    add_test(NAME WorkPlane COMMAND CatalogImportTests --test-solid-primitive-tool --work-plane-only -platform offscreen)
    add_test(NAME ReferencePlanes COMMAND CatalogImportTests --test-solid-primitive-tool --reference-planes-only -platform offscreen)
    set_tests_properties(ReferencePlanes PROPERTIES TIMEOUT 60)
    add_test(NAME SketchCameraAnimation COMMAND CatalogImportTests --test-solid-primitive-tool --camera-animation-only -platform offscreen)
    set_tests_properties(SketchCameraAnimation PROPERTIES TIMEOUT 30)
    add_test(NAME PrimitiveEnter COMMAND CatalogImportTests --test-solid-primitive-tool --primitive-enter-only -platform offscreen)
    set_tests_properties(PrimitiveEnter PROPERTIES TIMEOUT 30)
    add_test(NAME CoordinateAxisSelection COMMAND CatalogImportTests --test-solid-primitive-tool --coordinate-axis-only -platform offscreen)
    set_tests_properties(CoordinateAxisSelection PROPERTIES TIMEOUT 60)
    set_tests_properties(SolidPrimitiveTool PROPERTIES TIMEOUT 30)
    add_test(NAME TileTool COMMAND CatalogImportTests --test-tile -platform offscreen)
    add_test(NAME ProceduralMaterial COMMAND CatalogImportTests --test-procedural-material -platform offscreen)
    add_test(NAME LightRotation COMMAND CatalogImportTests --test-procedural-material --light-rotation -platform offscreen)
    add_test(NAME BlinnPhongLighting COMMAND CatalogImportTests --test-procedural-material --blinn-phong)
    add_test(NAME MaterialPreviewHiddenEdges COMMAND CatalogImportTests
        --test-procedural-material --hidden-edges-preview)
    set_tests_properties(MaterialPreviewHiddenEdges PROPERTIES TIMEOUT 60)
    add_test(NAME ProceduralPerforation COMMAND CatalogImportTests --test-procedural-material --perforation)
    set_tests_properties(ProceduralPerforation PROPERTIES TIMEOUT 120)
    if(WIN32)
        # Qt's offscreen platform cannot create a WGL preview context.
        set_tests_properties(MaterialPreviewHiddenEdges ProceduralPerforation PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=windows")
    endif()
    set_tests_properties(TileTool PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM_PLUGIN_PATH=${DOM3D_QT_MSVC2022_PATH}/plugins/platforms")
    add_test(NAME OrbitDimensionGrips COMMAND CatalogImportTests --test-face-primitive-cut --orbit-dimensions)
    set_tests_properties(OrbitDimensionGrips PROPERTIES TIMEOUT 30)
    add_test(NAME FacePrimitiveCut COMMAND CatalogImportTests --test-face-primitive-cut -platform offscreen)
    set_tests_properties(FacePrimitiveCut PROPERTIES TIMEOUT 30)
    add_test(NAME FacePrimitiveDragCut COMMAND CatalogImportTests --test-solid-primitive-tool --face-drag-cut-only -platform windows)
    set_tests_properties(FacePrimitiveDragCut PROPERTIES TIMEOUT 120)
    set_tests_properties(BooleanTool PROPERTIES TIMEOUT 30)
    set_tests_properties(RepeatCommand PROPERTIES TIMEOUT 30)
    add_test(NAME ToolsMesh3DFillContour
        COMMAND CatalogImportTests --test-tools-mesh3d
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Extrude_SL_Boundary_UV_D050.txt")
    set_tests_properties(ToolsMesh3DFillContour PROPERTIES TIMEOUT 30)
    add_test(NAME ObjSharpEdges COMMAND CatalogImportTests --test-obj-sharp-edges)
    add_test(NAME LowPolySharpEdges COMMAND CatalogImportTests --test-low-poly-sharp-edges)
    add_test(NAME MeshSubdivision COMMAND CatalogImportTests --test-mesh-subdivision)
    add_test(NAME ExtrudeCornerQuads COMMAND CatalogImportTests --test-extrude-corner-quads
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Extrude_AndThick.dom3d")
    set_tests_properties(ExtrudeCornerQuads PROPERTIES TIMEOUT 120)
    add_test(NAME SlxHoleScope COMMAND CatalogImportTests --test-slx-hole-scope
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Extrude_AndThick.dom3d")
    set_tests_properties(SlxHoleScope PROPERTIES TIMEOUT 120)
    add_test(NAME MeshDivideFace COMMAND CatalogImportTests --test-divide-face)
    set_tests_properties(MeshDivideFace PROPERTIES TIMEOUT 60)
    add_test(NAME MeshVar7Regression
        COMMAND CatalogImportTests --test-mesh-var7)
    add_test(NAME TrimClassificationDiagnostics
        COMMAND CatalogImportTests --test-trim-diagnostics)
    add_test(NAME CreateSketchSurface
        COMMAND CatalogImportTests --test-create-sketch-surface)
    add_test(NAME CurveToPolylineByLength
        COMMAND CatalogImportTests --test-curve-to-polyline-by-length)
    add_test(NAME ParametricCurveOffset
        COMMAND CatalogImportTests --test-curve-offset)
    add_test(NAME ParametricCurveMirrorCopy
        COMMAND CatalogImportTests --test-curve-mirror-copy)
    add_test(NAME ParametricCurveLink
        COMMAND CatalogImportTests --test-curve-link)
    add_test(NAME ParametricSmartHybrid
        COMMAND CatalogImportTests --test-smart-hybrid)
    add_test(NAME SmartHybridCarOpenNormals
        COMMAND CatalogImportTests --test-smart-hybrid-file
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/SmartHybridCarOpen.dom3d" 15)
    add_test(NAME SmartHybridCarClosedNormals
        COMMAND CatalogImportTests --test-smart-hybrid-file
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/SmartHybridCarClosed.dom3d" 18)
    add_test(NAME LiveFilletValidation
        COMMAND CatalogImportTests --test-live-fillet-validation)
    add_test(NAME PrismHollowFillet COMMAND CatalogImportTests --test-prism-hollow-fillet
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Prism_And_Hollow.dom3d")
    set_tests_properties(PrismHollowFillet PROPERTIES TIMEOUT 120 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME BezierPocketFillet COMMAND CatalogImportTests --test-bezier-pocket-fillet
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/solid-regression/Bezier-4.dom3d")
    set_tests_properties(BezierPocketFillet PROPERTIES TIMEOUT 120 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME PrismHollowFilletUI COMMAND CatalogImportTests --test-scene-persistence
        --prism-hollow-fillet-ui "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Prism_And_Hollow.dom3d")
    set_tests_properties(PrismHollowFilletUI PROPERTIES TIMEOUT 120 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME SolidCreationUndo COMMAND CatalogImportTests --test-solid-primitive-tool --creation-undo-only)
    set_tests_properties(SolidCreationUndo PROPERTIES TIMEOUT 120 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME WireThickness COMMAND CatalogImportTests --test-solid-primitive-tool --wire-thick-only)
    set_tests_properties(WireThickness PROPERTIES TIMEOUT 120 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME PipeSplineRadius COMMAND CatalogImportTests --test-solid-primitive-tool --pipe-spline-only
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Pipe.dom3d")
    set_tests_properties(PipeSplineRadius PROPERTIES TIMEOUT 120 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME TrimBySketchSavedShell COMMAND CatalogImportTests --test-solid-primitive-tool --trim-sketch-only
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/TrimBy_SL.dom3d")
    set_tests_properties(TrimBySketchSavedShell PROPERTIES TIMEOUT 120 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME OpenSketchTrimUI COMMAND CatalogImportTests --test-solid-primitive-tool --open-trim-only)
    set_tests_properties(OpenSketchTrimUI PROPERTIES TIMEOUT 120 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME BentOpenSketchTrim COMMAND CatalogImportTests --test-solid-primitive-tool --bent-trim-only
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/TrimBy_SL_Open.dom3d")
    set_tests_properties(BentOpenSketchTrim PROPERTIES TIMEOUT 120 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME SurfaceSweptClosedProfile COMMAND CatalogImportTests --test-solid-primitive-tool --surface-swept-file
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Swept-1_3.dom3d")
    set_tests_properties(SurfaceSweptClosedProfile PROPERTIES TIMEOUT 60)
    add_test(NAME SurfaceSweptOneRail COMMAND CatalogImportTests --test-solid-primitive-tool --surface-swept-only)
    set_tests_properties(SurfaceSweptOneRail PROPERTIES TIMEOUT 120 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME MeshWireColor
        COMMAND CatalogImportTests --test-mesh-wire-color)
    add_test(NAME LowPolyDensityCalibration
        COMMAND CatalogImportTests --test-low-poly-density-calibration)
    add_test(NAME SurfacePatchWelding
        COMMAND CatalogImportTests --test-surface-patch-welding)
    add_test(NAME BezierCollarWelding
        COMMAND CatalogImportTests --test-bezier-collar-welding
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/welding/Bezier-4.dom3d")
    add_test(NAME SmallHoleSlxCollar
        COMMAND CatalogImportTests --test-small-hole-slx-collar)
    add_test(NAME HoleLowDensityCollar
        COMMAND CatalogImportTests --test-hole-low-density-collar
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/welding/Hole.dom3d")
    add_test(NAME FusionCylinderFrontGuard
        COMMAND CatalogImportTests --fusion-cylinder-front-guard)
    add_test(NAME CylinderSeamTrimDirection
        COMMAND CatalogImportTests --test-cylinder-seam-trim)
    add_test(NAME CylinderMultipleWindows
        COMMAND CatalogImportTests --test-cylinder-multiple-windows
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Cylinder_Min_2_Cylinder.dom3d")
    set_tests_properties(CylinderMultipleWindows PROPERTIES TIMEOUT 60)
    add_test(NAME CylinderBoxWindows
        COMMAND CatalogImportTests --test-cylinder-box-windows
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Cylinder_Min_2_Box.dom3d")
    set_tests_properties(CylinderBoxWindows PROPERTIES TIMEOUT 60)
    add_test(NAME FourHoleCollars
        COMMAND CatalogImportTests --test-four-hole-collars
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Box_Min_4_Hole.dom3d")
    set_tests_properties(FourHoleCollars PROPERTIES TIMEOUT 60)
    add_test(NAME SixHoleSharedZone
        COMMAND CatalogImportTests --test-six-hole-shared-zone
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Box_Min_6_Boxex.dom3d")
    set_tests_properties(SixHoleSharedZone PROPERTIES TIMEOUT 60)
    add_test(NAME SpherePoleQuadro
        COMMAND CatalogImportTests --test-sphere-pole-quadro)
    add_test(NAME WireCircularCaps
        COMMAND CatalogImportTests --test-wire-circular-caps
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Wire.dom3d")
    add_test(NAME ExtrudeSlQuadro
        COMMAND CatalogImportTests --test-extrude-sl-quadro
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Extrude_SL.dom3d")
    add_test(NAME BoxMinBoxFilledQuadro
        COMMAND CatalogImportTests --test-box-min-box-filled
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Box_Min_Box_Filled.dom3d")
    add_test(NAME ChamferAndFilletsQuadro
        COMMAND CatalogImportTests --test-chamfer-and-fillets
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/ChamferAndFillets.dom3d")
    add_test(NAME RevolveAllFilletedQuadro
        COMMAND CatalogImportTests --test-revolve-all-filleted
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Revolve_All_Filleted.dom3d")
    set_tests_properties(RevolveAllFilletedQuadro PROPERTIES TIMEOUT 60)
    add_test(NAME ShellRimQuadro
        COMMAND CatalogImportTests --test-shell-rim-quadro
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Shell_Bag.dom3d")
    set_tests_properties(ShellRimQuadro PROPERTIES TIMEOUT 60)
    add_test(NAME TorusPlaneCutQuadro
        COMMAND CatalogImportTests --test-torus-plane-cut-quadro
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Torus_PlaneCut.dom3d")
    set_tests_properties(TorusPlaneCutQuadro PROPERTIES TIMEOUT 60)
    add_test(NAME TwoSketchCornerNormals
        COMMAND CatalogImportTests --test-two-sketch-corner-normals
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Two_SL.dom3d")
    set_tests_properties(TwoSketchCornerNormals PROPERTIES TIMEOUT 60)
    add_test(NAME PolyhedronProjectRebuild
        COMMAND CatalogImportTests --test-rebuild-polyhedron
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Polyhedron.dom3d"
            "${CMAKE_CURRENT_BINARY_DIR}/polyhedron-regression.dom3d")
    set_tests_properties(PolyhedronProjectRebuild PROPERTIES TIMEOUT 60)
    add_test(NAME SolidCenterlines COMMAND CatalogImportTests --test-solid-centerlines
        "${CMAKE_CURRENT_BINARY_DIR}/centerlines-regression")
    set_tests_properties(SolidCenterlines PROPERTIES TIMEOUT 120)
    add_test(NAME PrismTwoFilletsQuadro
        COMMAND CatalogImportTests --test-prism-two-fillets
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Prism_And_2_Fill_Var-2.dom3d")
    add_test(NAME MeshVar11Regression COMMAND CatalogImportTests --test-mesh-var11)
    add_test(NAME MeshVar5Reallocation COMMAND CatalogImportTests --test-mesh-var5-reallocation)
    add_test(NAME HoleFacePlacement COMMAND CatalogImportTests --test-hole-placement)
    set_tests_properties(HoleFacePlacement PROPERTIES TIMEOUT 60)
    add_test(NAME MeshPointSplitReallocation COMMAND CatalogImportTests --test-mesh-point-split-reallocation)
    add_test(NAME BooleanFourFillQuadro
        COMMAND CatalogImportTests --test-boolean-four-fill
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Boolean-4_All_fill.dom3d")
    add_test(NAME BooleanThreeSlx
        COMMAND CatalogImportTests --test-boolean-three-slx
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Boolean-3.dom3d")
    add_test(NAME PrismFilletedSlx
        COMMAND CatalogImportTests --test-prism-filleted-slx
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Box_Min_Prism-3_Filleted.dom3d")
    add_test(NAME BoxDoubleCutSlx
        COMMAND CatalogImportTests --test-box-double-cut-slx
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/mesh-regression/Box_Min_Box_And_Miin_Box.dom3d")
    add_test(NAME MeshRegressionCorpus
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/Test-MeshRegression.ps1"
            -BuildDirectory "${CMAKE_CURRENT_BINARY_DIR}"
            -Configuration "$<CONFIG>"
    )
    string(REPLACE ";" "\\;" DOM3D_CTEST_HOST_PATH "$ENV{PATH}")
    set_tests_properties(CylinderCenterDrag DraftMeasureUndo BoxCutFilletRebuild TextToCurves MultiSketch SolidSketchTools RevolveFilletUndo SweptEllipseEditing SweepFeatureCreationUndo PROPERTIES
        ENVIRONMENT "PATH=${DOM3D_QT_MSVC2022_PATH}/bin\;${DOM3D_OPENCASCADE_BINARY_DIR}\;${DOM3D_CTEST_HOST_PATH}")
    set_tests_properties(QuickMenuCancellation ObjSharpEdges LowPolySharpEdges MeshSubdivision ExtrudeCornerQuads SlxHoleScope HairdryerCadChartConsumer HairdryerCadBoundary PillowCadQuadro FrameCadQuadro FilletMeshNormals QuadroBodyBoundaryCache QuadroBoundarySnapshot CatalogImportTests RepeatCommand ScenePersistence SurfacePatchFailureUI SurfaceEdgePatchTool SurfaceBridgeFourSplines SurfaceBridgeSaved SurfaceBridgeComplex SurfaceBridgeTool SurfaceOffsetTool OffsetBodyFillet SewnBodyHistory ExtractSurfaceEdge LoftBezier FilletResponsive SurfaceDisplayOptions SurfaceReverseNormalsTool FourCurveAssociativity LowPolyDialogMeshState ProjectOpenDialog BooleanTool SolidPrimitiveTool FacePrimitiveCut OrbitDimensionGrips MeshVar7Regression
        TrimClassificationDiagnostics CreateSketchSurface
        CurveToPolylineByLength ParametricCurveOffset ParametricCurveMirrorCopy
        ParametricCurveLink ParametricSmartHybrid LiveFilletValidation MeshWireColor LowPolyDensityCalibration
        SurfacePatchWelding BezierCollarWelding SmallHoleSlxCollar HoleLowDensityCollar FusionCylinderFrontGuard
        CylinderSeamTrimDirection CylinderMultipleWindows CylinderBoxWindows FourHoleCollars SixHoleSharedZone SpherePoleQuadro ExtrudeSlQuadro BoxMinBoxFilledQuadro ChamferAndFilletsQuadro PrismTwoFilletsQuadro
        MeshVar11Regression MeshVar5Reallocation MeshPointSplitReallocation HoleFacePlacement PrismFilletedSlx BoxDoubleCutSlx BooleanFourFillQuadro BooleanThreeSlx MeshRegressionCorpus PROPERTIES
        ENVIRONMENT "PATH=${DOM3D_QT_MSVC2022_PATH}/bin\;${DOM3D_OPENCASCADE_BINARY_DIR}\;${DOM3D_CTEST_HOST_PATH}"
    )
    set_tests_properties(FusionCylinderFrontGuard PROPERTIES TIMEOUT 10)
    set_property(TEST RepeatCommand ScenePersistence SurfacePatchFailureUI SurfaceEdgePatchTool SurfaceBridgeFourSplines SurfaceBridgeSaved SurfaceBridgeComplex SurfaceBridgeTool SurfaceOffsetTool OffsetBodyFillet SewnBodyHistory ExtractSurfaceEdge LoftBezier FilletResponsive SurfaceDisplayOptions SurfaceReverseNormalsTool FourCurveAssociativity LowPolyDialogMeshState ProjectOpenDialog BooleanTool SolidPrimitiveTool FacePrimitiveCut APPEND PROPERTY ENVIRONMENT
        "QT_QPA_PLATFORM_PLUGIN_PATH=$<TARGET_FILE_DIR:Qt6::QOffscreenIntegrationPlugin>")
    set_tests_properties(CylinderSeamTrimDirection PROPERTIES TIMEOUT 20)
    set_tests_properties(ExtrudeSlQuadro PROPERTIES TIMEOUT 60)
    set_tests_properties(BoxMinBoxFilledQuadro PROPERTIES TIMEOUT 60)
    set_tests_properties(ChamferAndFilletsQuadro PROPERTIES TIMEOUT 60)
    set_tests_properties(PrismTwoFilletsQuadro PROPERTIES TIMEOUT 60)
    set_tests_properties(BoxDoubleCutSlx PROPERTIES TIMEOUT 120)
    set_tests_properties(PrismFilletedSlx PROPERTIES TIMEOUT 120)
    set_tests_properties(BooleanFourFillQuadro PROPERTIES TIMEOUT 120)
    set_tests_properties(BooleanThreeSlx PROPERTIES TIMEOUT 120)

    add_executable(ExchangeIOTests
        tests/ExchangeIOTests.cpp
        tests/ThreeMfIOTests.cpp
        tests/FbxIOTests.cpp
        tests/GlbIOTests.cpp
        ${CATALOG_IMPORT_TEST_SOURCES}
        ${DOM3D_HEADERS}
        ${DOM3D_SKETCH_SOURCES}
    )
    target_compile_features(ExchangeIOTests PRIVATE cxx_std_17)
    dom3d_link_3mf(ExchangeIOTests)
    dom3d_link_assimp(ExchangeIOTests)
    target_include_directories(ExchangeIOTests PRIVATE
        src
        "${DOM3D_OPENCASCADE_INCLUDE_DIR}"
    )
    target_link_directories(ExchangeIOTests PRIVATE
        "${DOM3D_OPENCASCADE_LIBRARY_DIR}"
    )
    target_link_libraries(ExchangeIOTests PRIVATE
        Qt6::Widgets Qt6::OpenGLWidgets Qt6::Xml Qt6::PrintSupport OpenGL::GL
        raytrace
        ${DOM3D_OPENCASCADE_LIBS}
    )
    if(WIN32)
        target_link_libraries(ExchangeIOTests PRIVATE comdlg32)
    endif()
    if(MSVC)
        target_compile_options(ExchangeIOTests PRIVATE
            /W4
            /permissive-
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_compat.h"
        )
    endif()
    add_test(NAME ExchangeIOTests COMMAND ExchangeIOTests)
    set_tests_properties(ExchangeIOTests PROPERTIES
        ENVIRONMENT "PATH=${DOM3D_QT_MSVC2022_PATH}/bin\;${DOM3D_OPENCASCADE_BINARY_DIR}\;${DOM3D_CTEST_HOST_PATH}"
    )

    add_executable(FourSplineSurfaceBuilderTests
        tests/FourSplineSurfaceBuilderTests.cpp
        src/FourSplineSurfaceBuilder.cpp
    )
    target_compile_features(FourSplineSurfaceBuilderTests PRIVATE cxx_std_17)
    target_include_directories(FourSplineSurfaceBuilderTests PRIVATE
        src
        "${DOM3D_OPENCASCADE_INCLUDE_DIR}"
    )
    target_link_directories(FourSplineSurfaceBuilderTests PRIVATE
        "${DOM3D_OPENCASCADE_LIBRARY_DIR}"
    )
    target_link_libraries(FourSplineSurfaceBuilderTests PRIVATE
        Qt6::Core
        ${DOM3D_OPENCASCADE_LIBS}
    )
    if(MSVC)
        target_compile_options(FourSplineSurfaceBuilderTests PRIVATE
            /W4
            /permissive-
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_compat.h"
        )
    endif()
    add_test(NAME FourSplineSurfaceBuilderTests
             COMMAND FourSplineSurfaceBuilderTests)
    set_tests_properties(FourSplineSurfaceBuilderTests PROPERTIES
        ENVIRONMENT "PATH=${DOM3D_OPENCASCADE_BINARY_DIR};$ENV{PATH}"
    )

    add_executable(FourSplineSurfaceComparison
        tests/FourSplineSurfaceComparison.cpp
        src/FourSplineSurfaceBuilder.cpp
    )
    target_compile_features(FourSplineSurfaceComparison PRIVATE cxx_std_17)
    target_include_directories(FourSplineSurfaceComparison PRIVATE
        src
        "${DOM3D_OPENCASCADE_INCLUDE_DIR}"
    )
    target_link_directories(FourSplineSurfaceComparison PRIVATE
        "${DOM3D_OPENCASCADE_LIBRARY_DIR}"
    )
    target_link_libraries(FourSplineSurfaceComparison PRIVATE
        Qt6::Core
        ${DOM3D_OPENCASCADE_LIBS}
    )
    if(MSVC)
        target_compile_options(FourSplineSurfaceComparison PRIVATE
            /W4
            /permissive-
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_compat.h"
        )
    endif()

    add_executable(IgesShapeCollectorTests
        tests/IgesShapeCollectorTests.cpp
        src/IgesShapeCollector.cpp
    )
    target_compile_features(IgesShapeCollectorTests PRIVATE cxx_std_17)
    target_include_directories(IgesShapeCollectorTests PRIVATE
        src
        "${DOM3D_OPENCASCADE_INCLUDE_DIR}"
    )
    target_link_directories(IgesShapeCollectorTests PRIVATE
        "${DOM3D_OPENCASCADE_LIBRARY_DIR}"
    )
    target_link_libraries(IgesShapeCollectorTests PRIVATE
        Qt6::Core
        ${DOM3D_OPENCASCADE_LIBS}
    )
    if(MSVC)
        target_compile_options(IgesShapeCollectorTests PRIVATE
            /W4
            /permissive-
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_compat.h"
        )
    endif()
    add_test(NAME IgesShapeCollectorTests COMMAND IgesShapeCollectorTests)
    set_tests_properties(IgesShapeCollectorTests PROPERTIES
        ENVIRONMENT "PATH=${DOM3D_OPENCASCADE_BINARY_DIR};$ENV{PATH}"
    )

    add_executable(SketchGeometryTests
        tests/SketchGeometryTests.cpp
        src/BezierSpline.cpp
        src/Dimens.cpp
        src/Fillet.cpp
        src/SketchArcLine.cpp
        src/LinkLine.cpp
        src/LinkLineHor.cpp
        src/LinkLineVert.cpp
    )
    target_compile_features(SketchGeometryTests PRIVATE cxx_std_17)
    target_include_directories(SketchGeometryTests PRIVATE src)
    target_link_libraries(SketchGeometryTests PRIVATE Qt6::Core)
    if(MSVC)
        target_compile_options(SketchGeometryTests PRIVATE
            /W4
            /permissive-
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_compat.h"
        )
    endif()
    add_test(NAME SketchGeometryTests COMMAND SketchGeometryTests)

    add_executable(SketchProfileBuilderTests
        tests/SketchProfileBuilderTests.cpp
        src/CAlfaObject.cpp
        src/BezierSpline.cpp
        src/Constraint.cpp
        src/ConstraintBezierTangent.cpp
        src/ConstraintHorLine.cpp
        src/ConstraintVertLine.cpp
        src/ExtrudeShapeBuilder.cpp
        src/Fillet.cpp
        src/LinkLine.cpp
        src/LinkLineHor.cpp
        src/LinkLineVert.cpp
        src/SketchArcLine.cpp
        src/SketchProfileBuilder.cpp
        src/SmartLine.cpp
    )
    target_compile_features(SketchProfileBuilderTests PRIVATE cxx_std_17)
    target_include_directories(SketchProfileBuilderTests PRIVATE
        src
        "${DOM3D_OPENCASCADE_INCLUDE_DIR}"
    )
    target_link_directories(SketchProfileBuilderTests PRIVATE
        "${DOM3D_OPENCASCADE_LIBRARY_DIR}"
    )
    target_link_libraries(SketchProfileBuilderTests PRIVATE
        Qt6::Core
        OpenGL::GL
        ${DOM3D_OPENCASCADE_LIBS}
    )
    if(MSVC)
        target_compile_options(SketchProfileBuilderTests PRIVATE
            /W4
            /permissive-
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_compat.h"
        )
    endif()
    add_test(NAME SketchProfileBuilderTests COMMAND SketchProfileBuilderTests)
    set_tests_properties(SketchProfileBuilderTests PROPERTIES
        ENVIRONMENT "PATH=${DOM3D_OPENCASCADE_BINARY_DIR};$ENV{PATH}"
    )

    add_executable(ExtrudeShapeBuilderTests
        tests/ExtrudeShapeBuilderTests.cpp
        src/ExtrudeShapeBuilder.cpp
    )
    target_compile_features(ExtrudeShapeBuilderTests PRIVATE cxx_std_17)
    target_include_directories(ExtrudeShapeBuilderTests PRIVATE
        src
        "${DOM3D_OPENCASCADE_INCLUDE_DIR}"
    )
    target_link_directories(ExtrudeShapeBuilderTests PRIVATE "${DOM3D_OPENCASCADE_LIBRARY_DIR}")
    target_link_libraries(ExtrudeShapeBuilderTests PRIVATE Qt6::Core ${DOM3D_OPENCASCADE_LIBS})
    if(MSVC)
        target_compile_options(ExtrudeShapeBuilderTests PRIVATE
            /W4
            /permissive-
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_compat.h"
        )
    endif()
    add_test(NAME ExtrudeShapeBuilderTests COMMAND ExtrudeShapeBuilderTests)
    set_tests_properties(ExtrudeShapeBuilderTests PROPERTIES
        ENVIRONMENT "PATH=${DOM3D_OPENCASCADE_BINARY_DIR};$ENV{PATH}"
    )

    add_executable(SketchFeatureShapeBuilderTests
        tests/SketchFeatureShapeBuilderTests.cpp
        src/ExtrudeShapeBuilder.cpp
        src/solid/SketchFeatureShapeBuilder.cpp
    )
    target_compile_features(SketchFeatureShapeBuilderTests PRIVATE cxx_std_17)
    target_include_directories(SketchFeatureShapeBuilderTests PRIVATE
        src
        "${DOM3D_OPENCASCADE_INCLUDE_DIR}"
    )
    target_link_directories(SketchFeatureShapeBuilderTests PRIVATE "${DOM3D_OPENCASCADE_LIBRARY_DIR}")
    target_link_libraries(SketchFeatureShapeBuilderTests PRIVATE Qt6::Core ${DOM3D_OPENCASCADE_LIBS})
    if(MSVC)
        target_compile_options(SketchFeatureShapeBuilderTests PRIVATE
            /W4
            /permissive-
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_compat.h"
        )
    endif()
    add_test(NAME SketchFeatureShapeBuilderTests COMMAND SketchFeatureShapeBuilderTests)
    set_tests_properties(SketchFeatureShapeBuilderTests PROPERTIES
        ENVIRONMENT "PATH=${DOM3D_OPENCASCADE_BINARY_DIR};$ENV{PATH}"
    )

    add_executable(OffsetFaceShapeBuilderTests
        tests/OffsetFaceShapeBuilderTests.cpp
        src/SurfaceOffsetBuilder.cpp
        src/solid/OffsetFaceShapeBuilder.cpp
        src/solid/TrimShapeBuilder.cpp
    )
    target_compile_features(OffsetFaceShapeBuilderTests PRIVATE cxx_std_17)
    target_include_directories(OffsetFaceShapeBuilderTests PRIVATE
        src
        "${DOM3D_OPENCASCADE_INCLUDE_DIR}"
    )
    target_link_directories(OffsetFaceShapeBuilderTests PRIVATE "${DOM3D_OPENCASCADE_LIBRARY_DIR}")
    target_link_libraries(OffsetFaceShapeBuilderTests PRIVATE Qt6::Core ${DOM3D_OPENCASCADE_LIBS})
    if(MSVC)
        target_compile_options(OffsetFaceShapeBuilderTests PRIVATE
            /W4
            /permissive-
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_compat.h"
        )
    endif()
    add_test(NAME OffsetFaceShapeBuilderTests COMMAND OffsetFaceShapeBuilderTests)
    set_tests_properties(OffsetFaceShapeBuilderTests PROPERTIES
        ENVIRONMENT "PATH=${DOM3D_OPENCASCADE_BINARY_DIR};$ENV{PATH}"
    )

    add_executable(SurfaceDisplayGeometryTests
        tests/SurfaceDisplayGeometryTests.cpp
        src/solid/SurfaceDisplay.cpp
    )
    target_compile_features(SurfaceDisplayGeometryTests PRIVATE cxx_std_17)
    target_include_directories(SurfaceDisplayGeometryTests PRIVATE
        src
        "${DOM3D_OPENCASCADE_INCLUDE_DIR}"
    )
    target_link_directories(SurfaceDisplayGeometryTests PRIVATE "${DOM3D_OPENCASCADE_LIBRARY_DIR}")
    target_link_libraries(SurfaceDisplayGeometryTests PRIVATE Qt6::Core ${DOM3D_OPENCASCADE_LIBS})
    if(MSVC)
        target_compile_options(SurfaceDisplayGeometryTests PRIVATE
            /W4
            /permissive-
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_compat.h"
        )
    endif()
    add_test(NAME SurfaceDisplayGeometryTests COMMAND SurfaceDisplayGeometryTests)
    set_tests_properties(SurfaceDisplayGeometryTests PROPERTIES
        ENVIRONMENT "PATH=${DOM3D_OPENCASCADE_BINARY_DIR};$ENV{PATH}"
    )

    add_executable(BeamShapeBuilderTests
        tests/BeamShapeBuilderTests.cpp
        src/solid/BeamShapeBuilder.cpp
    )
    target_compile_features(BeamShapeBuilderTests PRIVATE cxx_std_17)
    target_include_directories(BeamShapeBuilderTests PRIVATE
        src
        "${DOM3D_OPENCASCADE_INCLUDE_DIR}"
    )
    target_link_directories(BeamShapeBuilderTests PRIVATE "${DOM3D_OPENCASCADE_LIBRARY_DIR}")
    target_link_libraries(BeamShapeBuilderTests PRIVATE Qt6::Core ${DOM3D_OPENCASCADE_LIBS})
    if(MSVC)
        target_compile_options(BeamShapeBuilderTests PRIVATE
            /W4
            /permissive-
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_compat.h"
        )
    endif()
    add_test(NAME BeamShapeBuilderTests COMMAND BeamShapeBuilderTests)
    set_tests_properties(BeamShapeBuilderTests PROPERTIES
        ENVIRONMENT "PATH=${DOM3D_OPENCASCADE_BINARY_DIR};$ENV{PATH}"
    )

    add_executable(TrimShapeBuilderTests
        tests/TrimShapeBuilderTests.cpp
        src/solid/PlaneShapeBuilder.cpp
        src/solid/TrimShapeBuilder.cpp
    )
    target_compile_features(TrimShapeBuilderTests PRIVATE cxx_std_17)
    target_include_directories(TrimShapeBuilderTests PRIVATE
        src
        "${DOM3D_OPENCASCADE_INCLUDE_DIR}"
    )
    target_link_directories(TrimShapeBuilderTests PRIVATE "${DOM3D_OPENCASCADE_LIBRARY_DIR}")
    target_link_libraries(TrimShapeBuilderTests PRIVATE Qt6::Core ${DOM3D_OPENCASCADE_LIBS})
    if(MSVC)
        target_compile_options(TrimShapeBuilderTests PRIVATE
            /W4
            /permissive-
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_compat.h"
        )
    endif()
    add_test(NAME TrimShapeBuilderTests COMMAND TrimShapeBuilderTests)
    set_tests_properties(TrimShapeBuilderTests PROPERTIES
        ENVIRONMENT "PATH=${DOM3D_OPENCASCADE_BINARY_DIR};$ENV{PATH}"
    )

    add_executable(PolyhedronShapeBuilderTests
        tests/PolyhedronShapeBuilderTests.cpp
        src/solid/PolyhedronShapeBuilder.cpp
    )
    target_compile_features(PolyhedronShapeBuilderTests PRIVATE cxx_std_17)
    target_include_directories(PolyhedronShapeBuilderTests PRIVATE
        src
        "${DOM3D_OPENCASCADE_INCLUDE_DIR}"
    )
    target_link_directories(PolyhedronShapeBuilderTests PRIVATE "${DOM3D_OPENCASCADE_LIBRARY_DIR}")
    target_link_libraries(PolyhedronShapeBuilderTests PRIVATE Qt6::Core ${DOM3D_OPENCASCADE_LIBS})
    if(MSVC)
        target_compile_options(PolyhedronShapeBuilderTests PRIVATE
            /W4
            /permissive-
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_compat.h"
        )
    endif()
    add_test(NAME PolyhedronShapeBuilderTests COMMAND PolyhedronShapeBuilderTests)
    set_tests_properties(PolyhedronShapeBuilderTests PROPERTIES
        ENVIRONMENT "PATH=${DOM3D_OPENCASCADE_BINARY_DIR};$ENV{PATH}"
    )

    add_executable(FacadeFrameShapeBuilderTests
        tests/FacadeFrameShapeBuilderTests.cpp
        src/solid/FacadeFrameShapeBuilder.cpp
    )
    target_compile_features(FacadeFrameShapeBuilderTests PRIVATE cxx_std_17)
    target_include_directories(FacadeFrameShapeBuilderTests PRIVATE
        src
        "${DOM3D_OPENCASCADE_INCLUDE_DIR}"
    )
    target_link_directories(FacadeFrameShapeBuilderTests PRIVATE "${DOM3D_OPENCASCADE_LIBRARY_DIR}")
    target_link_libraries(FacadeFrameShapeBuilderTests PRIVATE Qt6::Core ${DOM3D_OPENCASCADE_LIBS})
    if(MSVC)
        target_compile_options(FacadeFrameShapeBuilderTests PRIVATE
            /W4
            /permissive-
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_compat.h"
        )
    endif()
    add_test(NAME FacadeFrameShapeBuilderTests COMMAND FacadeFrameShapeBuilderTests)
    set_tests_properties(FacadeFrameShapeBuilderTests PROPERTIES
        ENVIRONMENT "PATH=${DOM3D_OPENCASCADE_BINARY_DIR};$ENV{PATH}"
    )

    add_executable(TwoRailSweepSurfaceBuilderTests
        tests/TwoRailSweepSurfaceBuilderTests.cpp
        src/TwoRailSweepSurfaceBuilder.cpp
    )
    target_compile_features(TwoRailSweepSurfaceBuilderTests PRIVATE cxx_std_17)
    target_include_directories(TwoRailSweepSurfaceBuilderTests PRIVATE
        src
        "${DOM3D_OPENCASCADE_INCLUDE_DIR}"
    )
    target_link_directories(TwoRailSweepSurfaceBuilderTests PRIVATE "${DOM3D_OPENCASCADE_LIBRARY_DIR}")
    target_link_libraries(TwoRailSweepSurfaceBuilderTests PRIVATE Qt6::Core ${DOM3D_OPENCASCADE_LIBS})
    if(MSVC)
        target_compile_options(TwoRailSweepSurfaceBuilderTests PRIVATE
            /W4
            /permissive-
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_compat.h"
        )
    endif()
    add_test(NAME TwoRailSweepSurfaceBuilderTests COMMAND TwoRailSweepSurfaceBuilderTests)
    set_tests_properties(TwoRailSweepSurfaceBuilderTests PROPERTIES
        ENVIRONMENT "PATH=${DOM3D_OPENCASCADE_BINARY_DIR};$ENV{PATH}"
    )
    add_test(NAME SurfaceAlignmentUI COMMAND CatalogImportTests --test-scene-persistence
        --surface-alignment "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/JoinSurace.dom3d")
    set_tests_properties(SurfaceAlignmentUI PROPERTIES TIMEOUT 120
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PATH=${DOM3D_QT_MSVC2022_PATH}/bin\;${DOM3D_OPENCASCADE_BINARY_DIR}\;$ENV{PATH}")
    add_test(NAME SurfaceAlignmentFrame3 COMMAND CatalogImportTests --test-scene-persistence
        --surface-alignment "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Frame-3.dom3d")
    set_tests_properties(SurfaceAlignmentFrame3 PROPERTIES TIMEOUT 120
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PATH=${DOM3D_QT_MSVC2022_PATH}/bin\;${DOM3D_OPENCASCADE_BINARY_DIR}\;$ENV{PATH}")
    add_test(NAME SurfaceSeamHelicopter COMMAND CatalogImportTests --test-scene-persistence
        --surface-seam "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Surf_By_Two_Views_2.dom3d")
    set_tests_properties(SurfaceSeamHelicopter PROPERTIES TIMEOUT 180
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PATH=${DOM3D_QT_MSVC2022_PATH}/bin\;${DOM3D_OPENCASCADE_BINARY_DIR}\;$ENV{PATH}")
    add_test(NAME SurfaceBoundaryGraph COMMAND CatalogImportTests --test-scene-persistence
        --surface-boundary-graph "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Surf_By_Two_Views_2.dom3d")
    set_tests_properties(SurfaceBoundaryGraph PROPERTIES TIMEOUT 180
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PATH=${DOM3D_QT_MSVC2022_PATH}/bin\;${DOM3D_OPENCASCADE_BINARY_DIR}\;$ENV{PATH}")
    add_test(NAME SurfaceBoundaryMouse COMMAND CatalogImportTests --test-scene-persistence --surface-boundary-mouse)
    add_test(NAME SurfaceGraphOffsetUI COMMAND CatalogImportTests --test-scene-persistence --surface-graph-offset "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Surf_By_Two_Views_2.dom3d")
    set_tests_properties(SurfaceGraphOffsetUI PROPERTIES TIMEOUT 240
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PATH=${DOM3D_QT_MSVC2022_PATH}/bin\;${DOM3D_OPENCASCADE_BINARY_DIR}\;$ENV{PATH}")
    set_tests_properties(SurfaceBoundaryMouse PROPERTIES TIMEOUT 180
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PATH=${DOM3D_QT_MSVC2022_PATH}/bin\;${DOM3D_OPENCASCADE_BINARY_DIR}\;$ENV{PATH}")
    add_test(NAME SurfaceBoundarySymmetry COMMAND CatalogImportTests --test-scene-persistence --surface-boundary-symmetry)
    set_tests_properties(SurfaceBoundarySymmetry PROPERTIES TIMEOUT 180
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PATH=${DOM3D_QT_MSVC2022_PATH}/bin\;${DOM3D_OPENCASCADE_BINARY_DIR}\;$ENV{PATH}")
    add_executable(SurfaceAlignmentTests tests/SurfaceAlignmentTests.cpp src/SurfaceTopologyBuilder.cpp)
    target_compile_features(SurfaceAlignmentTests PRIVATE cxx_std_17)
    target_include_directories(SurfaceAlignmentTests PRIVATE src "${DOM3D_OPENCASCADE_INCLUDE_DIR}")
    target_link_directories(SurfaceAlignmentTests PRIVATE "${DOM3D_OPENCASCADE_LIBRARY_DIR}")
    target_link_libraries(SurfaceAlignmentTests PRIVATE ${DOM3D_OPENCASCADE_LIBS})
    add_test(NAME SurfaceAlignmentTests COMMAND SurfaceAlignmentTests)
    add_test(NAME SurfaceKnotsUI COMMAND CatalogImportTests --test-scene-persistence --surface-graph-offset "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Surf_By_Two_Views_2.dom3d" --surface-knots)
    set_tests_properties(SurfaceKnotsUI PROPERTIES TIMEOUT 240
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PATH=${DOM3D_QT_MSVC2022_PATH}/bin\;${DOM3D_OPENCASCADE_BINARY_DIR}\;$ENV{PATH}")
    add_test(NAME SurfaceGraphOffsetGrid COMMAND CatalogImportTests --test-scene-persistence --surface-graph-offset "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Surf_By_Two_Views_2.dom3d" --offset-grid -platform offscreen)
    set_tests_properties(SurfaceGraphOffsetGrid PROPERTIES TIMEOUT 240
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PATH=${DOM3D_QT_MSVC2022_PATH}/bin\;${DOM3D_OPENCASCADE_BINARY_DIR}\;$ENV{PATH}")
    add_test(NAME HybridImportedZebra COMMAND CatalogImportTests --test-scene-persistence --surface-graph-offset "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Helicopter_3DCoat.dom3d" --hybrid-diagnose)
    set_tests_properties(HybridImportedZebra PROPERTIES TIMEOUT 240)
    add_test(NAME SurfacePointTransformUI COMMAND CatalogImportTests --test-scene-persistence --surface-graph-offset "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Loft_Plast.dom3d" --point-transform)
    set_tests_properties(SurfacePointTransformUI PROPERTIES TIMEOUT 240
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PATH=${DOM3D_QT_MSVC2022_PATH}/bin\;${DOM3D_OPENCASCADE_BINARY_DIR}\;$ENV{PATH}")
    add_test(NAME HybridExtrudeUI COMMAND CatalogImportTests --test-scene-persistence --surface-graph-offset "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Loft_Plast.dom3d" --hybrid-extrude)
    get_test_property(SurfacePointTransformUI ENVIRONMENT EXTRUDE_TEST_ENV)
    set_tests_properties(HybridExtrudeUI PROPERTIES TIMEOUT 240 ENVIRONMENT "${EXTRUDE_TEST_ENV}")
    add_test(NAME HybridExtrudeAdjacentPatches COMMAND CatalogImportTests --test-scene-persistence --surface-graph-offset "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Surf_By_Two_Views_2_Extrude.dom3d" --hybrid-pair)
    set_tests_properties(HybridExtrudeAdjacentPatches PROPERTIES TIMEOUT 240 ENVIRONMENT "${EXTRUDE_TEST_ENV}")
    add_test(NAME LoftSmoothDisplacement COMMAND CatalogImportTests --test-scene-persistence --surface-graph-offset "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/surface-regression/Loft-3_Moved.dom3d" --loft-smooth)
    get_test_property(SurfacePointTransformUI ENVIRONMENT POINT_TRANSFORM_TEST_ENV)
    set_tests_properties(LoftSmoothDisplacement HybridImportedZebra PROPERTIES TIMEOUT 240 ENVIRONMENT "${POINT_TRANSFORM_TEST_ENV}")
    add_executable(SurfacePointTransformTests tests/SurfacePointTransformTests.cpp src/SurfacePointTransform.cpp src/HybridExtrude.cpp)
    target_compile_features(SurfacePointTransformTests PRIVATE cxx_std_17)
    target_include_directories(SurfacePointTransformTests PRIVATE src "${DOM3D_OPENCASCADE_INCLUDE_DIR}")
    target_link_directories(SurfacePointTransformTests PRIVATE "${DOM3D_OPENCASCADE_LIBRARY_DIR}")
    target_link_libraries(SurfacePointTransformTests PRIVATE ${DOM3D_OPENCASCADE_LIBS})
    add_test(NAME SurfacePointTransformTests COMMAND SurfacePointTransformTests)
    set_tests_properties(SurfacePointTransformTests PROPERTIES ENVIRONMENT "PATH=${DOM3D_OPENCASCADE_BINARY_DIR};$ENV{PATH}")
    add_executable(SurfaceGraphOffsetTests tests/SurfaceGraphOffsetTests.cpp src/SurfaceGraphOffsetBuilder.cpp src/SurfaceTopologyBuilder.cpp src/SurfaceGraphBoundaryBuilder.cpp)
    target_compile_features(SurfaceGraphOffsetTests PRIVATE cxx_std_17)
    target_include_directories(SurfaceGraphOffsetTests PRIVATE src "${DOM3D_OPENCASCADE_INCLUDE_DIR}")
    target_link_directories(SurfaceGraphOffsetTests PRIVATE "${DOM3D_OPENCASCADE_LIBRARY_DIR}")
    target_link_libraries(SurfaceGraphOffsetTests PRIVATE ${DOM3D_OPENCASCADE_LIBS})
    add_test(NAME SurfaceGraphOffsetTests COMMAND SurfaceGraphOffsetTests)
    set_tests_properties(SurfaceGraphOffsetTests PROPERTIES TIMEOUT 180 ENVIRONMENT "PATH=${DOM3D_OPENCASCADE_BINARY_DIR};$ENV{PATH}")
    set_tests_properties(SurfaceAlignmentTests PROPERTIES ENVIRONMENT "PATH=${DOM3D_OPENCASCADE_BINARY_DIR};$ENV{PATH}")
endif()
