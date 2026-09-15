cmake_minimum_required(VERSION 3.22.1)
project(codm_guest_imgui)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --------------------------------------------------
# Dobby prebuilt
# --------------------------------------------------
add_library(dobby STATIC IMPORTED)

set_target_properties(dobby PROPERTIES
    IMPORTED_LOCATION
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/${ANDROID_ABI}/libdobby.a"
)

# --------------------------------------------------
# ImGui
# --------------------------------------------------
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../deps/imgui")

add_library(codm_guest_imgui SHARED
    main.cpp

    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(codm_guest_imgui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_compile_definitions(codm_guest_imgui PRIVATE
    IMGUI_IMPL_OPENGL_ES3
)

target_compile_options(codm_guest_imgui PRIVATE
    -O2
    -fvisibility=hidden
    -fno-exceptions
    -fno-rtti
)

target_link_libraries(codm_guest_imgui
    dobby
    log
    android
    EGL
    GLESv3
    dl
)
