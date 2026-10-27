set(PROJ_SRC ${CMAKE_CURRENT_LIST_DIR})

file(GLOB_RECURSE PROJ_SRCS_C ${PROJ_SRC}/*.c)
file(GLOB_RECURSE PROJ_SRCS_CPP ${PROJ_SRC}/*.cpp)

idf_component_register(
    SRCS ${PROJ_SRCS_C} ${PROJ_SRCS_CPP} "third_party/xiaozhi_esp32/ogg_demuxer.cc"
    INCLUDE_DIRS ${PROJ_SRC}
    REQUIRES brookesia_core bsp_extra esp_audio_codec esp_audio_effects esp-sr
             xiaozhi-fonts esp_event esp_netif esp_xiaozhi nvs_flash esp_wifi
    PRIV_REQUIRES app_update esp_http_client cjson efuse esp_hw_support
                  esp_app_format esp_partition esp_timer mbedtls mcp-c-sdk spi_flash
)

set_source_files_properties(
    ${PROJ_SRCS_CPP}
    PROPERTIES
        COMPILE_FLAGS "-Wno-missing-field-initializers"
)
