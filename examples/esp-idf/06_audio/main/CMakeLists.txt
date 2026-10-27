idf_component_register(SRCS "main.cpp" "MicTest.cpp" INCLUDE_DIRS "."
    REQUIRES esp32_s3_touch_lcd_1_85B bsp_extra PRIV_REQUIRES esp_timer)
