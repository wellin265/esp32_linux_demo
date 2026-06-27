if(ESP_PLATFORM)
    file(GLOB_RECURSE SOURCES
        ./src/eez/*.cpp
        ./src/eez/*.c
    )

    idf_component_register(
        SRCS ${SOURCES}
        INCLUDE_DIRS ./src ./src/eez/libs/agg
        REQUIRES lvgl
    )

    target_compile_definitions(
        ${COMPONENT_LIB}
        PUBLIC EEZ_FOR_LVGL LV_LVGL_H_INCLUDE_SIMPLE)

    target_compile_options(
        ${COMPONENT_LIB}
        PRIVATE -Wno-error=dangling-pointer)
else()
    cmake_minimum_required(VERSION 3.12)

    include_directories(
        ./src
        ./src/eez/libs/agg
    )

    file(GLOB_RECURSE SOURCES
        ./src/eez/*.cpp
        ./src/eez/*.c
    )

    ADD_LIBRARY(eez-framework STATIC ${SOURCES})

    target_include_directories(eez-framework SYSTEM PUBLIC ./src ./src/eez/libs/agg)
endif()
