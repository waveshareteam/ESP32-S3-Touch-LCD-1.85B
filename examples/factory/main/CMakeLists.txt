idf_component_register(SRCS "main.cpp" "dark/scr_bg.c" INCLUDE_DIRS "."
    REQUIRES esp32_s3_touch_lcd_1_85B brookesia_core Settings AIChats DrawingBoard
             Photos Weather Music Sensor Games MicTest ButtonTest DisplayTest
             VideoPlayer APP_chipmunk2d network_service Battery esp_timer)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
