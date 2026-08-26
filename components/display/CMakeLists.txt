idf_component_register(
    SRCS "display.cpp" "face_animation.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_lcd
)
