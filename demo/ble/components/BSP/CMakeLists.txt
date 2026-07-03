set(src_dirs
            WS2812
            )

set(include_dirs
            WS2812
            )

set(requires
            driver
            led_strip)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})

component_compile_options(-ffast-math -O3 -Wno-error=format=-Wno-format)
