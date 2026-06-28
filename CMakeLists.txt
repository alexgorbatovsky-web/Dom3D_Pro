cmake_minimum_required(VERSION 3.16)

project(Dom3D_Pro LANGUAGES CXX)

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
    src/MaterialLibrary.cpp
    src/CAlfaObject.cpp
    src/CAlfaDoc.cpp
    src/CMesh3D.cpp
    src/Line2D.cpp
    src/Net.cpp
    src/SurfaceUVMapping.cpp
    src/CBSpline.cpp
    src/CPolyline.cpp
    src/CView2d.cpp
    src/CView3d.cpp
    src/Dom3DProjectSerializer.cpp
    src/CadCurve3D.cpp
    src/IgesIO.cpp
    src/ObjIO.cpp
    src/ProjectIO.cpp
    src/StepIO.cpp
    src/ThreeDSIO.cpp
    src/solid/SolidBoxTool.cpp
    src/solid/SolidCylinderTool.cpp
    src/solid/SolidPrismTool.cpp
    src/solid/SurfaceSet.cpp
    src/solid/SolidTool.cpp
    src/ui/MainWindow.cpp
    src/ui/MaterialEditorDialog.cpp
    src/ui/MaterialDrag.cpp
    src/ui/MaterialSphereBrowser.cpp
    src/ui/OpenGLViewport.cpp
    src/ui/PreferencesDialog.cpp
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
    src/Material.h
    src/MaterialLibrary.h
    src/CAlfaDoc.h
    src/Common.h
    src/CMesh3D.h
    src/Line2D.h
    src/Net.h
    src/CBSpline.h
    src/CPolyline.h
    src/CView2d.h
    src/CView3d.h
    src/Dom3DProjectSerializer.h
    src/CadCurve3D.h
    src/IgesIO.h
    src/ObjIO.h
    src/OpenGLCompat.h
    src/ProjectIO.h
    src/StepIO.h
    src/ThreeDSIO.h
    src/solid/SolidBoxTool.h
    src/solid/SolidCylinderTool.h
    src/solid/SolidPrismTool.h
    src/solid/SurfaceSet.h
    src/solid/SolidTool.h
    src/ui/MainWindow.h
    src/ui/MaterialEditorDialog.h
    src/ui/MaterialDrag.h
    src/ui/MaterialSphereBrowser.h
    src/ui/OpenGLViewport.h
    src/ui/PreferencesDialog.h
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

if(WIN32)
    list(APPEND DOM3D_SOURCES src/resources/Dom3D_Pro.rc)
endif()

add_executable(Dom3D_Pro
    ${DOM3D_SOURCES}
    ${DOM3D_HEADERS}
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

target_compile_features(Dom3D_Pro PRIVATE cxx_std_17)
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

if(MSVC)
    target_compile_options(Dom3D_Pro PRIVATE
        /W4
        /permissive-
        "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/legacy_compat.h"
    )
endif()
