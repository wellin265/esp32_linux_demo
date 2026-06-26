idf_component_register(
    SRCS
        "app_main.cpp"
        "who_camera.c"
        "who_cat_face_detection.cpp"
        "who_color_detection.cpp"
        "who_human_face_detection.cpp"
        "who_human_face_recognition.cpp"
        "who_motion_detection.cpp"
        "camera_lvgl_view.c"
    INCLUDE_DIRS "."
    REQUIRES fb_gfx spiffs esp_adc ksdiy_example_display
)
