idf_component_register(
    SRCS "uart_echo_example_main.c"
    REQUIRES esp_driver_uart esp_driver_gpio ksdiy_example_display
    INCLUDE_DIRS "."
)
