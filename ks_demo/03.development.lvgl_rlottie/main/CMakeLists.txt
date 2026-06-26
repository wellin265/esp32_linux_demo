file(GLOB_RECURSE SOURCES_C *.c)
file(GLOB_RECURSE SOURCES_CPP *.cpp)

idf_component_register(
    SRCS ${SOURCES_C} ${SOURCES_CPP}
    INCLUDE_DIRS "."
    REQUIRES
        espressif2022__lottie_player
        esp_rom
        esp_adc
        nvs_flash
        esp_timer
        spiffs
        fatfs
        sdmmc)

target_link_libraries(${COMPONENT_LIB} INTERFACE "-u __wrap_longjmp")

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)

spiffs_create_partition_assets(
        animation
        "../lottie_assets"
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".json"
)
