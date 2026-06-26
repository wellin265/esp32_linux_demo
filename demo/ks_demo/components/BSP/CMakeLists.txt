set(src_dirs
            MYIIC
            LCD
            MYSPI
            TCA9554
            CST816S
			)

set(include_dirs
            MYIIC
            LCD
            MYSPI
            TCA9554
            CST816S
			)

set(requires
            driver
            esp_lcd
            esp_io_expander
            espressif__esp_io_expander_tca9554
            espressif__esp_lcd_touch
            espressif__esp_lcd_touch_cst816s)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})

component_compile_options(-ffast-math -O3 -Wno-error=format=-Wno-format)
