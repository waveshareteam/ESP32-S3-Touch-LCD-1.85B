idf_component_register(SRCS "DrawingBoard.cpp" "assets/image_draw.c"
    INCLUDE_DIRS "." REQUIRES brookesia_core lvgl esp32_s3_touch_lcd_1_85B)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
