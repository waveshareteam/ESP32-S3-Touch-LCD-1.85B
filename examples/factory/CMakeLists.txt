cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(01_factory)

# Xiaozhi uses the 16 px Chinese text font; other font sizes and emoji are unused.
idf_component_get_property(font_lib 78__xiaozhi-fonts COMPONENT_LIB)
idf_component_get_property(font_dir 78__xiaozhi-fonts COMPONENT_DIR)
set_property(TARGET ${font_lib} PROPERTY SOURCES "${font_dir}/src/font_puhui_16_4.c")
