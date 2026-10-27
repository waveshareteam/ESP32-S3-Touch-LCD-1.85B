#include "music_file.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/stat.h>

/* File scanning and data conversion shared by the worker and UI. */
void music_files_free(music_files_t *files)
{
    for (size_t i = 0; i < files->count; ++i) free(files->paths[i]);
    free(files->paths); *files = (music_files_t){0};
}
static int compare(const void *a, const void *b)
{
    const char *x = *(const char *const *)a, *y = *(const char *const *)b;
    int result = strcasecmp(x, y);
    return result ? result : strcmp(x, y);
}
music_result_t music_scan(const char *folder, music_files_t *files, music_cancel_fn cancel, void *ctx)
{
    *files = (music_files_t){0};
    DIR *dir = opendir(folder);
    if (!dir) return errno == ENOENT ? MUSIC_NO_FOLDER : MUSIC_IO_ERROR;
    music_result_t result = MUSIC_OK;
    size_t capacity = 0;
    for (;;) {
        if (cancel && cancel(ctx)) { result = MUSIC_CANCELLED; break; }
        errno = 0;
        struct dirent *entry = readdir(dir);
        if (!entry) { if (errno) result = MUSIC_IO_ERROR; break; }
        const char *ext = strrchr(entry->d_name, '.');
        if (entry->d_name[0] == '.' || !ext || strcasecmp(ext, ".mp3")) continue;
        char path[MUSIC_PATH_MAX];
        int length = snprintf(path, sizeof(path), "%s/%s", folder, entry->d_name);
        if (length < 0 || (size_t)length >= sizeof(path)) continue;
        struct stat st;
        if (stat(path, &st)) { result = MUSIC_IO_ERROR; break; }
        if (!S_ISREG(st.st_mode)) continue;
        if (files->count == capacity) {
            size_t next = capacity ? capacity * 2 : 16;
            char **grown = realloc(files->paths, next * sizeof(*grown));
            if (!grown) { result = MUSIC_NO_MEMORY; break; }
            files->paths = grown; capacity = next;
        }
        char *copy = malloc((size_t)length + 1);
        if (!copy) { result = MUSIC_NO_MEMORY; break; }
        memcpy(copy, path, (size_t)length + 1);
        files->paths[files->count++] = copy;
    }
    closedir(dir);
    if (result == MUSIC_OK && !files->count) result = MUSIC_EMPTY;
    if (result != MUSIC_OK) music_files_free(files);
    else qsort(files->paths, files->count, sizeof(*files->paths), compare);
    return result;
}
void music_title(const char *path, char *title, size_t capacity)
{
    const char *name = strrchr(path, '/'); name = name ? name + 1 : path;
    size_t length = strlen(name);
    if (length >= 4 && !strcasecmp(name + length - 4, ".mp3")) length -= 4;
    if (!capacity) return;
    if (length >= capacity) length = capacity - 1;
    /* Avoid leaving a truncated UTF-8 character at the end. Font fallback is UI-owned. */
    while (length && ((unsigned char)name[length] & 0xc0) == 0x80) --length;
    memcpy(title, name, length); title[length] = 0;
}
void music_format_time(uint32_t ms, char *out, size_t capacity)
{
    snprintf(out, capacity, "%02lu:%02lu", (unsigned long)(ms / 60000), (unsigned long)(ms / 1000 % 60));
}
static uint32_t be32(const unsigned char *p)
{ return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
typedef struct { unsigned size, rate, samples, version, channels, crc; } frame_t;
static uint32_t syncsafe(const unsigned char *p)
{ return ((uint32_t)(p[0]&127)<<21)|((uint32_t)(p[1]&127)<<14)|((uint32_t)(p[2]&127)<<7)|(p[3]&127); }
static void tag_text(const unsigned char *p,size_t n,char *out,size_t capacity)
{
    if(n<2 || !capacity) return;
    unsigned encoding=*p++; --n;
    if(encoding==0 || encoding==3) {
        size_t size=0;
        for(size_t i=0;i<n && p[i];++i) {
            unsigned c=p[i];
            if(c<32) c=' ';
            if(encoding==0 && c>=128) {
                if(size+2>=capacity) break;
                out[size++]=(char)(0xc0|(c>>6)); out[size++]=(char)(0x80|(c&63));
            } else { if(size+1>=capacity) break; out[size++]=(char)c; }
        }
        if(encoding==3 && size<n) while(size && (p[size]&0xc0)==0x80) --size;
        out[size]=0; return;
    }
    if(encoding!=1 && encoding!=2) return;
    bool little=false;
    if(n>=2 && ((p[0]==0xff && p[1]==0xfe)||(p[0]==0xfe && p[1]==0xff))) {
        little=p[0]==0xff; p+=2; n-=2;
    }
    size_t used=0;
    for(size_t i=0;i+1<n;i+=2) {
        uint32_t cp=little ? p[i]|((uint32_t)p[i+1]<<8) : ((uint32_t)p[i]<<8)|p[i+1];
        if(!cp) break;
        if(cp>=0xd800 && cp<=0xdbff && i+3<n) {
            uint32_t lo=little ? p[i+2]|((uint32_t)p[i+3]<<8) : ((uint32_t)p[i+2]<<8)|p[i+3];
            if(lo>=0xdc00 && lo<=0xdfff) { cp=0x10000+((cp-0xd800)<<10)+lo-0xdc00; i+=2; }
        }
        if(cp>=0xd800 && cp<=0xdfff) cp='?';
        if(cp<32) cp=' ';
        unsigned count=cp<128?1:cp<2048?2:cp<65536?3:4;
        if(used+count>=capacity) break;
        if(count==1) out[used++]=(char)cp;
        else {
            out[used++]=(char)((count==2?0xc0:count==3?0xe0:0xf0)|(cp>>(6*(count-1))));
            for(int shift=(int)(count-2)*6;shift>=0;shift-=6) out[used++]=(char)(0x80|((cp>>shift)&63));
        }
    }
    out[used]=0;
}
static void read_tags(FILE *file,const unsigned char *id3,long end,music_info_t *info)
{
    unsigned version=id3[3];
    /* Uncommon compressed/unsynchronised tags are optional metadata: skip them,
     * without rejecting the underlying MP3 or trying to read cover pictures. */
    if((version!=3 && version!=4) || (id3[5]&0xc0)) return;
    long pos=10;
    while(pos+10<=end) {
        unsigned char head[10],text[512];
        if(fseek(file,pos,SEEK_SET) || fread(head,1,10,file)!=10 || !head[0]) return;
        uint32_t size=version==4?syncsafe(head+4):be32(head+4);
        if(!size || size>(uint32_t)(end-pos-10)) return;
        bool title=!memcmp(head,"TIT2",4),artist=!memcmp(head,"TPE1",4);
        if((title||artist) && head[9]==0) {
            size_t read=size<sizeof(text)?size:sizeof(text);
            if(fread(text,1,read,file)!=read) return;
            tag_text(text,read,title?info->title:info->artist,title?sizeof(info->title):sizeof(info->artist));
        }
        pos+=10+(long)size;
        if(info->title[0] && info->artist[0]) return;
    }
}
static bool header(const unsigned char *p, frame_t *f)
{
    static const unsigned rates[] = {44100,48000,32000};
    static const unsigned m1[] = {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320};
    static const unsigned m2[] = {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160};
    unsigned ver = (p[1] >> 3) & 3, layer = (p[1] >> 1) & 3;
    unsigned bitrate = p[2] >> 4, rate = (p[2] >> 2) & 3;
    if (p[0] != 0xff || (p[1] & 0xe0) != 0xe0 || ver == 1 || layer != 1 ||
        !bitrate || bitrate == 15 || rate == 3) return false;
    f->rate = rates[rate] / (ver == 3 ? 1 : ver == 2 ? 2 : 4);
    f->samples = ver == 3 ? 1152 : 576;
    f->size = (ver == 3 ? 144000 : 72000) * (ver == 3 ? m1[bitrate] : m2[bitrate]) / f->rate + ((p[2] >> 1) & 1);
    f->version = ver; f->channels = (p[3] >> 6) == 3 ? 1 : 2; f->crc = (p[1] & 1) ? 0 : 2;
    return f->size >= 4 && f->size <= 2048;
}
music_result_t music_probe(const char *path, music_info_t *info, music_cancel_fn cancel, void *ctx)
{
    *info = (music_info_t){0};
    FILE *file = fopen(path, "rb");
    if (!file) return MUSIC_IO_ERROR;
    setvbuf(file, NULL, _IOFBF, 8192);
    unsigned char data[2048];
    music_result_t result = MUSIC_BAD_MP3;
    if (fread(data, 1, 10, file) != 10) goto done;
    long offset = 0;
    if (!memcmp(data, "ID3", 3)) {
        if ((data[6] | data[7] | data[8] | data[9]) & 0x80) goto done;
        offset = 10 + ((long)data[6]<<21) + ((long)data[7]<<14) + ((long)data[8]<<7) + data[9];
        read_tags(file,data,offset,info);
        if (data[3] == 4 && (data[5] & 0x10)) offset += 10;
    }
    if (fseek(file, offset, SEEK_SET)) { result = MUSIC_IO_ERROR; goto done; }
    uint64_t samples = 0;
    unsigned frames = 0, searched = 0, first_rate = 0, first_version = 0;
    if (fread(data, 1, 4, file) != 4) goto done;
    for (;;) {
        if (cancel && cancel(ctx)) { result = MUSIC_CANCELLED; goto done; }
        frame_t f;
        if (!header(data, &f)) {
            if (frames) {
                /* ID3v1/APE trailers are allowed; garbage inside audio is not. */
                if (!memcmp(data, "TAG", 3) || !memcmp(data, "APET", 4)) break;
                goto done;
            }
            if (++searched > 65536) goto done;
            memmove(data, data + 1, 3);
            int c = fgetc(file); if (c == EOF) goto done;
            data[3] = (unsigned char)c; continue;
        }
        if (frames && (f.rate != first_rate || f.version != first_version)) goto done;
        if (fread(data + 4, 1, f.size - 4, file) != f.size - 4) goto done;
        if (!frames) {
            first_rate = f.rate; first_version = f.version;
            info->sample_rate = f.rate; info->channels = f.channels;
            unsigned side = f.version == 3 ? (f.channels == 1 ? 17 : 32) : (f.channels == 1 ? 9 : 17);
            unsigned pos = 4 + f.crc + side;
            uint32_t count = 0;
            if (pos + 12 <= f.size && (!memcmp(data+pos,"Xing",4) || !memcmp(data+pos,"Info",4)) &&
                (be32(data+pos+4) & 1)) count = be32(data+pos+8);
            if (!count && f.size >= 54 && !memcmp(data+36,"VBRI",4)) count = be32(data+50);
            if (count) {
                uint64_t duration = (uint64_t)count * f.samples * 1000 / f.rate;
                if (duration && duration <= UINT32_MAX) {
                    info->duration_ms = (uint32_t)duration; result = MUSIC_OK; goto done;
                }
            }
        }
        ++frames; samples += f.samples;
        if((frames & 255)==0) vTaskDelay(1);
        size_t got = fread(data, 1, 4, file);
        if (!got) break;
        if (got != 4) goto done;
    }
    if (frames && samples * 1000 / first_rate <= UINT32_MAX) {
        info->duration_ms = (uint32_t)(samples * 1000 / first_rate); result = MUSIC_OK;
    }
done:
    if (ferror(file)) result = MUSIC_IO_ERROR;
    fclose(file);
    if (result != MUSIC_OK) *info = (music_info_t){0};
    return result;
}


int music_frame_bytes(const unsigned char *data) { frame_t f; return header(data,&f)?(int)f.size:0; }
bool music_skip_id3(FILE *file) {
    unsigned char data[10];
    if(fread(data,1,10,file)!=10) return false;
    long start=0;
    if(!memcmp(data,"ID3",3)) {
        if((data[6]|data[7]|data[8]|data[9])&128) return false;
        start=10+(long)syncsafe(data+6);
        if(data[3]==4 && (data[5]&16)) start+=10;
    }
    return fseek(file,start,SEEK_SET)==0;
}
