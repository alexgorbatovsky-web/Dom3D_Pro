cmake_minimum_required(VERSION 3.16)

project(Dom3D_Pro LANGUAGES CXX)

include(CTest)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(DOM3D_QT_MSVC2022_PATH "C:/QtE/6.11.1/msvc2022_64" CACHE PATH "Qt 6 MSVC 2022 64-bit installation path")
if(EXISTS "${DOM3D_QT_MSVC2022_PATH}/lib/cmake/Qt6/Qt6Config.cmake")
    list(PREPEND CMAKE_PREFIX_PATH "${DOM3D_QT_MSVC2022_PATH}")
endif()

find_package(Qt6 COMPONENTS Widgets OpenGLWidgets Xml QUIET)
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

set(DOM3D_OPENCASCADE_INCLUDE_DIR "C:/OpenCascade/opencascade_build/inc" CACHE PATH "OpenCascade include directory")
set(DOM3D_OPENCASCADE_LIBRARY_DIR "C:/OpenCascade/opencascade_build/win64/vc14/libd" CACHE PATH "OpenCascade library directory")
set(DOM3D_OPENCASCADE_BINARY_DIR "C:/OpenCascade/opencascade_build/win64/vc14/bind" CACHE PATH "OpenCascade runtime DLL directory")

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
    src/main.cpp
    src/Material.cpp
    src/render/RenderScene.cpp
    src/render/BlenderCyclesRenderer.cpp
    src/MaterialLibrary.cpp
    src/FurnitureMaterialFactory.cpp
    src/CAlfaObject.cpp
    src/DrawingText.cpp
    src/CGroup.cpp
    src/CPart.cpp
    src/CAssembled.cpp
    src/CFacadeFurniture.cpp
    src/solid/FacadeFrameShapeBuilder.cpp
    src/CFurnitureAssemblies.cpp
    src/CFurnitureDrawer.cpp
    src/CKitchenCabinet.cpp
    src/CAlfaDoc.cpp
    src/UndoRedo.cpp
    src/ExtrudeShapeBuilder.cpp
    src/CMesh3D.cpp
    src/ReferenceImage.cpp
    src/FillContour.cpp
    src/Line2D.cpp
    src/Net.cpp
    src/SurfaceUVMapping.cpp
    src/CBSpline.cpp
    src/CPolyline.cpp
    src/CView2d.cpp
    src/CView3d.cpp
    src/Dom3DProjectSerializer.cpp
    src/CadCurve3D.cpp
    src/ExchangeIO.cpp
    src/IgesIO.cpp
    src/IgesShapeCollector.cpp
    src/ObjIO.cpp
    src/ProjectIO.cpp
    src/StepIO.cpp
    src/ThreeDSIO.cpp
    src/solid/BeamShapeBuilder.cpp
    src/solid/PlaneShapeBuilder.cpp
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
    src/solid/AssociativeClone.cpp
    src/solid/TwoSketchSolidBuilder.cpp
    src/solid/SolidTool.cpp
    src/solid/TrimShapeBuilder.cpp
    src/ui/MainWindow.cpp
    src/ui/MeasurementUnits.cpp
    src/ui/MaterialEditorDialog.cpp
    src/ui/MaterialDrag.cpp
    src/ui/MaterialSphereBrowser.cpp
    src/ui/OpenGLViewport.cpp
    src/ui/PreferencesDialog.cpp
    src/ui/LightingDialog.cpp
    src/ui/BlenderCyclesDialog.cpp
    src/ui/LanguageManager.cpp
    src/ui/HotkeyManagerDialog.cpp
    src/ui/CommandSearchPopup.cpp
    src/ui/PropertyPanel.cpp
    src/ui/QtSceneRenderer.cpp
    src/ui/ToolRegistry.cpp
    src/ui/BooleanDialog.cpp
    src/ui/ExtrudeFaceDialog.cpp
    src/resources/icons.qrc
    src/solid/Solid.cpp
    src/solid/SurfaceFace.cpp
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
    src/CFurnitureAssemblies.h
    src/CFurnitureDrawer.h
    src/CKitchenCabinet.h
    src/Material.h
    src/render/RenderScene.h
    src/render/IExternalRenderer.h
    src/render/BlenderCyclesRenderer.h
    src/MaterialLibrary.h
    src/FurnitureMaterialFactory.h
    src/CAlfaDoc.h
    src/UndoRedo.h
    src/ExtrudeShapeBuilder.h
    src/Common.h
    src/CMesh3D.h
    src/FillContour.h
    src/Line2D.h
    src/Net.h
    src/CBSpline.h
    src/CPolyline.h
    src/CView2d.h
    src/CView3d.h
    src/Dom3DProjectSerializer.h
    src/CadCurve3D.h
    src/ExchangeIO.h
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
    src/ui/MeasurementUnits.h
    src/ui/MaterialEditorDialog.h
    src/ui/MaterialDrag.h
    src/ui/MaterialSphereBrowser.h
    src/ui/OpenGLViewport.h
    src/ui/PreferencesDialog.h
    src/ui/LightingDialog.h
    src/ui/BlenderCyclesDialog.h
    src/ui/LanguageManager.h
    src/ui/HotkeyManagerDialog.h
    src/ui/CommandSearchPopup.h
    src/ui/PropertyPanel.h
    src/ui/QtSceneRenderer.h
    src/ui/ToolRegistry.h
    src/ui/BooleanDialog.h
    src/ui/ExtrudeFaceDialog.h
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
    src/LinkLineHor.cpp
    src/LinkLineHor.h
    src/LinkLineVert.cpp
    src/LinkLineVert.h
    src/SmartLine.cpp
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

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Dom3D_Pro)
set_target_properties(Dom3D_Pro PROPERTIES
    OUTPUT_NAME "Dom3D_Pro"
    WIN32_EXECUTABLE TRUE
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Dom3D_Pro>"
    VS_DEBUGGER_ENVIRONMENT "PATH=${DOM3D_QT_MSVC2022_PATH}/bin;${DOM3D_OPENCASCADE_BINARY_DIR};%PATH%"
)

source_group("Source Files" FILES ${DOM3D_SOURCES})
source_group("Header Files" FILES ${DOM3D_HEADERS})
source_group("Sketch" FILES ${DOM3D_SKETCH_SOURCES})
source_group("Sketch Migration (staged)" FILES ${DOM3D_SKETCH_MIGRATION_FILES})

target_compile_features(Dom3D_Pro PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(Dom3D_Pro PRIVATE /utf-8)
endif()
target_include_directories(Dom3D_Pro PRIVATE "${DOM3D_OPENCASCADE_INCLUDE_DIR}")
target_link_directories(Dom3D_Pro PRIVATE "${DOM3D_OPENCASCADE_LIBRARY_DIR}")
target_link_libraries(Dom3D_Pro PRIVATE Qt6::Widgets Qt6::OpenGLWidgets Qt6::Xml OpenGL::GL ${DOM3D_OPENCASCADE_LIBS})

if(WIN32)
    file(GLOB DOM3D_OPENCASCADE_RUNTIME_DLLS
        "${DOM3D_OPENCASCADE_BINARY_DIR}/*.dll"
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

    set(CATALOG_IMPORT_TEST_SOURCES ${DOM3D_SOURCES})
    list(REMOVE_ITEM CATALOG_IMPORT_TEST_SOURCES src/main.cpp)
    add_executable(CatalogImportTests
        tests/CatalogImportTests.cpp
        ${CATALOG_IMPORT_TEST_SOURCES}
        ${DOM3D_HEADERS}
        ${DOM3D_SKETCH_SOURCES}
    )
    target_compile_features(CatalogImportTests PRIVATE cxx_std_17)
    target_include_directories(CatalogImportTests PRIVATE
        src
        "${DOM3D_OPENCASCADE_INCLUDE_DIR}"
    )
    target_link_directories(CatalogImportTests PRIVATE
        "${DOM3D_OPENCASCADE_LIBRARY_DIR}"
    )
    target_link_libraries(CatalogImportTests PRIVATE
        Qt6::Widgets Qt6::OpenGLWidgets Qt6::Xml OpenGL::GL
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
    add_test(NAME CatalogImportTests COMMAND CatalogImportTests)
    string(REPLACE ";" "\\;" DOM3D_CTEST_HOST_PATH "$ENV{PATH}")
    set_tests_properties(CatalogImportTests PROPERTIES
        ENVIRONMENT "PATH=${DOM3D_QT_MSVC2022_PATH}/bin\;${DOM3D_OPENCASCADE_BINARY_DIR}\;${DOM3D_CTEST_HOST_PATH}"
    )

    add_executable(ExchangeIOTests
        tests/ExchangeIOTests.cpp
        ${CATALOG_IMPORT_TEST_SOURCES}
        ${DOM3D_HEADERS}
        ${DOM3D_SKETCH_SOURCES}
    )
    target_compile_features(ExchangeIOTests PRIVATE cxx_std_17)
    target_include_directories(ExchangeIOTests PRIVATE
        src
        "${DOM3D_OPENCASCADE_INCLUDE_DIR}"
    )
    target_link_directories(ExchangeIOTests PRIVATE
        "${DOM3D_OPENCASCADE_LIBRARY_DIR}"
    )
    target_link_libraries(ExchangeIOTests PRIVATE
        Qt6::Widgets Qt6::OpenGLWidgets Qt6::Xml OpenGL::GL
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
endif()
