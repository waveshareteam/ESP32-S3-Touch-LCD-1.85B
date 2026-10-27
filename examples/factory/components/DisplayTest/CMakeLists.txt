idf_component_register(SRCS "DisplayTest.cpp" "assets/img_app_display_test.c"
                       INCLUDE_DIRS "." REQUIRES brookesia_core lvgl app_support
                       PRIV_REQUIRES esp32_s3_touch_lcd_1_85B)
