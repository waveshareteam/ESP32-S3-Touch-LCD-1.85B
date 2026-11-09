idf_component_register(
    SRCS "Badge.cpp" "badge_service.cpp" "assets/img_app_badge.c"
    INCLUDE_DIRS "."
    REQUIRES app_support network_service
    PRIV_REQUIRES esp_new_jpeg spiffs esp_http_server json esp_timer
    EMBED_FILES "badge.html")
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
