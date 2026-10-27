#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

#define MUSIC_PATH_MAX 320
#define MUSIC_TITLE_MAX 128
typedef enum { MUSIC_OK, MUSIC_NO_FOLDER, MUSIC_EMPTY, MUSIC_IO_ERROR,
               MUSIC_NO_MEMORY, MUSIC_BAD_MP3, MUSIC_CANCELLED } music_result_t;
typedef struct { char **paths; size_t count; } music_files_t;
typedef struct {
    uint32_t duration_ms, sample_rate;
    unsigned channels;
    char title[MUSIC_TITLE_MAX], artist[96];
} music_info_t;
typedef bool (*music_cancel_fn)(void *context);

music_result_t music_scan(const char *folder, music_files_t *files, music_cancel_fn cancel, void *ctx);
void music_files_free(music_files_t *files);
/* Parse MPEG Layer III frames without decoding; Xing/Info and VBRI fast paths.
 * Files without a frame count are scanned, so VBR duration is not a CBR estimate. */
music_result_t music_probe(const char *path, music_info_t *info, music_cancel_fn cancel, void *ctx);
void music_title(const char *path, char *title, size_t capacity);
void music_format_time(uint32_t ms, char *out, size_t capacity);

int music_frame_bytes(const unsigned char *data);
bool music_skip_id3(FILE *file);
#ifdef __cplusplus
}
#endif
