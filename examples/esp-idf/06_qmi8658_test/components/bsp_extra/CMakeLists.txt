set(SRCS "")
list(APPEND SRCS
    "src/bsp_board_extra.c"
)

set(INCLUDE_DIRS "")
list(APPEND INCLUDE_DIRS "include")

foreach(SRC_DIR IN LISTS SRC_DIRS)
    file(GLOB_RECURSE SRC ${SRC_DIR}/*.c)
    list(APPEND SRCS ${SRC})
    list(APPEND INCLUDE_DIRS ${SRC_DIR}/include)
endforeach()

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    REQUIRES waveshare__esp32_s3_touch_lcd_1_85B 
                espressif__gmf_core
                espressif__gmf_audio
                espressif__gmf_io 
                espressif__esp_audio_simple_player
    PRIV_REQUIRES esp_timer fatfs esp_psram esp_mm
)

set_source_files_properties(
    ${PROJ_SRCS_CPP}
    PROPERTIES
        COMPILE_FLAGS "-Wno-missing-field-initializers"
)