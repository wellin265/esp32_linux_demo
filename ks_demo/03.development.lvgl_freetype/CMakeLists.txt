cmake_minimum_required(VERSION 3.16)
add_compile_options(-Wno-error=format= -Wno-format)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(spi_lcd_touch)

idf_component_get_property(freetype_lib espressif__freetype COMPONENT_LIB)
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_link_libraries(${lvgl_lib} PUBLIC ${freetype_lib})
