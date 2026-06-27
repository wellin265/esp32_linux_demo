# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components")

set(EXCLUDE_COMPONENTS
    esp-code-scanner
    fb_gfx
    mdns)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
add_compile_options(-Wno-error=format= -Wno-format)
add_compile_options(-fdiagnostics-color=always)
project(face_recognition)
