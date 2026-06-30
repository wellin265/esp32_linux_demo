set(src_dirs
            SPI
            LCD
            )

set(include_dirs
            SPI
            LCD
            )

set(requires
            driver
            esp_lcd
            espressif__esp_lcd_ili9341)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})

component_compile_options(-ffast-math -O3 -Wno-error=format=-Wno-format)
