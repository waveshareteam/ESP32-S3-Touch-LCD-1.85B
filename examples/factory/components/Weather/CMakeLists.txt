idf_component_register(SRCS "Weather.cpp" "service/weather_service.c" "service/weather_body.c" "custom/weather_model.c" "assets/img_app_weather.c" "assets/weather_icons.c"
 INCLUDE_DIRS "." "service" "custom" REQUIRES app_support network_service PRIV_REQUIRES esp_http_client esp_timer json)
