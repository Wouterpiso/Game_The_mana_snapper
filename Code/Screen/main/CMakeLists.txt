idf_component_register(SRCS "main.cpp" "MenuScreen.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES driver drivers entities st7789_display json)