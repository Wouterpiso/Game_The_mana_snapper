# components/st7789_display/CMakeLists.txt
idf_component_register(
    SRCS "St7789Display.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer esp_lcd
)