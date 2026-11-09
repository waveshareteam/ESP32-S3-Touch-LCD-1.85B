idf_component_register(
    SRCS "LuckyWheel.cpp" "assets/img_app_lucky_wheel.c"
    INCLUDE_DIRS "."
    REQUIRES app_support
    PRIV_REQUIRES esp_hw_support esp_timer)
