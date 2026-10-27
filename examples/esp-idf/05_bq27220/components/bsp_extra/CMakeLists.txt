idf_component_register(SRCS "src/bsp_board_extra.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp32_s3_touch_lcd_1_85B esp_codec_dev
    PRIV_REQUIRES esp_timer)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
