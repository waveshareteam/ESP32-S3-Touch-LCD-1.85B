#include "media_files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
static int compare(const void *a,const void *b) { return strcasecmp(*(const char *const *)a,*(const char *const *)b); }
void media_files_free(media_files_t *f) { for(size_t i=0;i<f->count;++i) free(f->paths[i]); free(f->paths); *f=(media_files_t){0}; }
bool media_files_scan(const char *folder,const char *ext,const char *ext2,media_files_t *out,media_cancel_fn cancel,void *ctx,char *error,size_t cap) {
    *out=(media_files_t){0};
    DIR *dir=opendir(folder);
    if(!dir) { snprintf(error,cap,errno==ENOENT?"Folder missing: %s":"Cannot read: %s",folder); return false; }
    bool ok=true; size_t allocated=0;
    for(;;) {
        if(cancel && cancel(ctx)) { ok=false; break; }
        errno=0; struct dirent *entry=readdir(dir);
        if(!entry) { if(errno) ok=false; break; }
        const char *suffix=strrchr(entry->d_name,'.');
        if(entry->d_name[0]=='.' || !suffix || (strcasecmp(suffix,ext) && (!ext2 || strcasecmp(suffix,ext2)))) continue;
        char path[384]; int n=snprintf(path,sizeof(path),"%s/%s",folder,entry->d_name);
        if(n<0 || n>=sizeof(path)) continue;
        struct stat st; if(stat(path,&st)) { ok=false; break; } if(!S_ISREG(st.st_mode)) continue;
        if(out->count==allocated) {
            size_t next=allocated?allocated*2:16; char **grown=realloc(out->paths,next*sizeof(*grown));
            if(!grown) { ok=false; break; } out->paths=grown; allocated=next;
        }
        char *copy=strdup(path); if(!copy) { ok=false; break; }
        out->paths[out->count++]=copy;
    }
    closedir(dir);
    if(!ok) { media_files_free(out); snprintf(error,cap,"File scan failed / cancelled"); return false; }
    if(!out->count) { snprintf(error,cap,"No %s files in %s",ext,folder); return true; }
    qsort(out->paths,out->count,sizeof(*out->paths),compare); error[0]=0; return true;
}
