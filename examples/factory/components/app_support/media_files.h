#pragma once
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { char **paths; size_t count; } media_files_t;
typedef bool (*media_cancel_fn)(void *ctx);
bool media_files_scan(const char *folder,const char *extension,const char *extension2,media_files_t *out,media_cancel_fn cancel,void *ctx,char *error,size_t error_size);
void media_files_free(media_files_t *files);
#ifdef __cplusplus
}
#endif
