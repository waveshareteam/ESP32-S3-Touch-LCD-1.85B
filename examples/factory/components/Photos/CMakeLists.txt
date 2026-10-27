idf_component_register(SRCS "Photos.cpp" "assets/img_app_gallery.c" INCLUDE_DIRS "." REQUIRES app_support PRIV_REQUIRES esp_new_jpeg esp_timer)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
