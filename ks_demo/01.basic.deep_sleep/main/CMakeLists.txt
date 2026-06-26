set(srcs
    "deep_sleep_example_main.c"
    "gpio_wakeup.c"
    "ext_wakeup.c"
)
set(includes ".")

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${includes}
)
