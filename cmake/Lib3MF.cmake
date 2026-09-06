# The official SDK exposes a C ABI, safe to use from both Debug and Release.
set(DOM3D_LIB3MF_ROOT "" CACHE PATH "Optional lib3mf 2.5 SDK directory")
if(WIN32)
    if(NOT DOM3D_LIB3MF_ROOT)
        include(FetchContent)
        FetchContent_Declare(dom3d_lib3mf_sdk
            URL https://github.com/3MFConsortium/lib3mf/releases/download/v2.5.0/lib3mf-2.5.0-Windows.zip
            URL_HASH SHA256=8e185bc19e5d544c3c1e489fd8f8fcecfb0530def36e5c8bcc81bebf2ac96aa1)
        FetchContent_MakeAvailable(dom3d_lib3mf_sdk)
        set(DOM3D_LIB3MF_ROOT "${dom3d_lib3mf_sdk_SOURCE_DIR}")
    endif()
    add_library(dom3d_lib3mf SHARED IMPORTED GLOBAL)
    set_target_properties(dom3d_lib3mf PROPERTIES
        IMPORTED_IMPLIB "${DOM3D_LIB3MF_ROOT}/lib/lib3mf.lib"
        IMPORTED_LOCATION "${DOM3D_LIB3MF_ROOT}/bin/lib3mf.dll"
        INTERFACE_INCLUDE_DIRECTORIES "${DOM3D_LIB3MF_ROOT}/include/Bindings/Cpp")
else()
    find_package(lib3mf 2.5 CONFIG REQUIRED HINTS "${DOM3D_LIB3MF_ROOT}")
    add_library(dom3d_lib3mf ALIAS lib3mf::lib3mf)
endif()

function(dom3d_link_3mf target)
    target_link_libraries(${target} PRIVATE dom3d_lib3mf)
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:dom3d_lib3mf>" "$<TARGET_FILE_DIR:${target}>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/third_party/lib3mf-NOTICES.txt"
                "$<TARGET_FILE_DIR:${target}>/lib3mf-NOTICES.txt"
            VERBATIM)
    endif()
endfunction()
