set(PROJ_SRC ${CMAKE_CURRENT_LIST_DIR})

file(GLOB_RECURSE PROJ_SRCS_C ${PROJ_SRC}/*.c)
file(GLOB_RECURSE PROJ_SRCS_CPP ${PROJ_SRC}/*.cpp)

idf_component_register(
    SRCS  ${PROJ_SRCS_C} ${PROJ_SRCS_CPP}
    INCLUDE_DIRS ${PROJ_SRC} chipmunk chipmunk_demo
    REQUIRES esp32_s3_touch_lcd_1_85B qmi8658 esp_lvgl_adapter
    WHOLE_ARCHIVE
)

set_source_files_properties(
    ${PROJ_SRCS_CPP}
    PROPERTIES
        COMPILE_FLAGS "-Wno-missing-field-initializers"
)
