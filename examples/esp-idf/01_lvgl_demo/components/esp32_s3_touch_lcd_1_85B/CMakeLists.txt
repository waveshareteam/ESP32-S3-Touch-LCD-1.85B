idf_component_register(
    SRCS "esp32_s3_touch_lcd_1_85B.cpp" "bsp_board.cpp"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES driver esp_lcd esp_lcd_st77916 esp_lcd_touch_cst816s
             esp_lvgl_adapter lvgl esp_codec_dev qmi8658
             fatfs sdmmc esp_wifi esp_netif
    PRIV_REQUIRES bsp_extra pcf85063a button nvs_flash esp_timer esp_event)

# Keep hardware initialization separate from the APP audio-session layer.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
