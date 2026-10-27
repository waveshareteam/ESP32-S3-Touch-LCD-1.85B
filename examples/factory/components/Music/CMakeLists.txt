idf_component_register(SRCS "Music.cpp" "service/music_file.c" "assets/img_app_musicplayer.c" "assets/img_app_music_cover_1.c"
 INCLUDE_DIRS "." "service" REQUIRES bsp_extra app_support PRIV_REQUIRES esp_audio_codec)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
