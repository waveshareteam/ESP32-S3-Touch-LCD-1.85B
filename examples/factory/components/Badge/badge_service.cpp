#include "badge_service.hpp"
#include "wifi_manager.h"
#include "esp_spiffs.h"
#include "esp_jpeg_dec.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

namespace badge {
namespace {
// Static service state outlives asynchronous HTTP shutdown and APP objects.
SemaphoreHandle_t mutex;
State state;
std::atomic<bool> editing{false}, finished{false};
extern const char html_start[] asm("_binary_badge_html_start");
extern const char html_end[] asm("_binary_badge_html_end");
struct Lock {
    Lock() { xSemaphoreTake(mutex,portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(mutex); }
    Lock(const Lock &)=delete;
    Lock &operator=(const Lock &)=delete;
};
void path_for(int slot, char *path, unsigned cap) { snprintf(path,cap,"/badge/slot_%d.jpg",slot); }
bool exists(const char *path)
{
    // SPIFFS implements stat(), but its ESP-IDF VFS has no access() callback.
    struct stat info={};
    return stat(path,&info)==0 && S_ISREG(info.st_mode);
}
void storage_error(const char *operation,const char *path)
{
    const int code=errno;
    ESP_LOGE("badge","Storage %s failed for %s: errno=%d (%s)",operation,path,code,strerror(code));
}

bool replace_file(const char *path, const void *bytes, size_t size)
{
    char temp[80], backup[80];
    snprintf(temp,sizeof(temp),"%s.tmp",path);
    snprintf(backup,sizeof(backup),"%s.bak",path);
    FILE *f=fopen(temp,"wb");
    if (!f) { storage_error("open temporary file",temp); return false; }
    bool ok=fwrite(bytes,1,size,f)==size;
    if (!ok) storage_error("write temporary file",temp);
    if (fclose(f)!=0) { storage_error("close temporary file",temp); ok=false; }
    if (!ok) { unlink(temp); return false; }
    unlink(backup);
    bool previous=exists(path);
    if (previous && rename(path,backup)!=0) { storage_error("back up file",path); unlink(temp); return false; }
    if (rename(temp,path)!=0) {
        storage_error("commit temporary file",path);
        if (previous && rename(backup,path)!=0) storage_error("restore backup",backup);
        unlink(temp);
        return false;
    }
    unlink(backup);
    return true;
}
void recover(const char *path)
{
    char backup[80], temp[80];
    snprintf(backup,sizeof(backup),"%s.bak",path);
    snprintf(temp,sizeof(temp),"%s.tmp",path);
    if (!exists(path) && exists(backup)) rename(backup,path);
    unlink(temp);
    if (exists(path)) unlink(backup);
}
bool select_locked(int slot)
{
    if (slot<0 || slot>=SLOTS || !(state.available&(1<<slot))) return false;
    if (state.active==slot) return true;
    const char value='0'+slot;
    if (!replace_file("/badge/active",&value,1)) return false;
    state.active=slot;
    ++state.revision;
    return true;
}

// The caller holds the storage lock. Both JPEG decoding and HTTP transmission
// consume the returned buffer after unlocking, so neither stalls other readers.
uint8_t *read_image_file(const char *path,unsigned *size)
{
    *size=0;
    FILE *file=fopen(path,"rb");
    if (!file) return nullptr;
    uint8_t *bytes=nullptr;
    if (fseek(file,0,SEEK_END)==0) {
        const long length=ftell(file);
        if (length>0 && length<=static_cast<long>(MAX_JPEG) && fseek(file,0,SEEK_SET)==0) {
            bytes=static_cast<uint8_t *>(heap_caps_aligned_alloc(
                16,(static_cast<size_t>(length)+15)&~size_t(15),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
            if (bytes && fread(bytes,1,length,file)==static_cast<size_t>(length))
                *size=static_cast<unsigned>(length);
        }
    }
    fclose(file);
    if (!*size) { heap_caps_free(bytes); return nullptr; }
    return bytes;
}

bool decode(const uint8_t *input, unsigned size, uint16_t *pixels)
{
    jpeg_dec_config_t cfg=DEFAULT_JPEG_DEC_CONFIG();
    cfg.output_type=JPEG_PIXEL_FORMAT_RGB565_LE;
    jpeg_dec_handle_t decoder=nullptr;
    if (jpeg_dec_open(&cfg,&decoder)!=JPEG_ERR_OK) return false;
    jpeg_dec_io_t io={}; io.inbuf=const_cast<uint8_t *>(input); io.inbuf_len=size;
    jpeg_dec_header_info_t info={}; int length=0;
    bool ok=jpeg_dec_parse_header(decoder,&io,&info)==JPEG_ERR_OK &&
            info.width==SIZE && info.height==SIZE &&
            jpeg_dec_get_outbuf_len(decoder,&length)==JPEG_ERR_OK && length==(int)PIXEL_BYTES;
    if (ok) { io.outbuf=reinterpret_cast<uint8_t *>(pixels); ok=jpeg_dec_process(decoder,&io)==JPEG_ERR_OK; }
    jpeg_dec_close(decoder);
    return ok;
}

bool request_slot(httpd_req_t *r,int &slot)
{
    char query[40], value[8];
    if (httpd_req_get_url_query_str(r,query,sizeof(query))!=ESP_OK ||
        httpd_query_key_value(query,"slot",value,sizeof(value))!=ESP_OK ||
        value[0]<'0' || value[0]>='0'+SLOTS || value[1]) return false;
    slot=value[0]-'0';
    return true;
}
esp_err_t error(httpd_req_t *r, httpd_err_code_t code, const char *message)
{ return httpd_resp_send_err(r,code,message); }
bool allowed(httpd_req_t *r)
{
    wifi_manager_touch_web_session();
    if (editing.load() && wifi_manager_web_transfer_active()) return true;
    error(r,HTTPD_403_FORBIDDEN,"Badge editor is closed");
    return false;
}
esp_err_t ok(httpd_req_t *r)
{
    httpd_resp_set_type(r,"application/json");
    return httpd_resp_sendstr(r,"{\"status\":\"ok\"}");
}
esp_err_t index_get(httpd_req_t *r)
{
    if (!wifi_manager_begin_web_transfer()) {
        httpd_resp_set_status(r,"409 Conflict");
        httpd_resp_set_type(r,"text/html; charset=utf-8");
        return httpd_resp_sendstr(r,"<meta name='viewport' content='width=device-width,initial-scale=1'><p>Wait for Wi-Fi setup or scanning to finish, then retry.</p><a href='/'>Device home</a>");
    }
    if (!initialize()) {
        wifi_manager_end_web_transfer();
        return error(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Badge storage unavailable. Return home and retry.");
    }
    finished=false; editing=true;
    httpd_resp_set_type(r,"text/html; charset=utf-8");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    return httpd_resp_send(r,html_start,html_end-html_start);
}
esp_err_t state_get(httpd_req_t *r)
{
    if (!allowed(r)) return ESP_OK;
    State s;
    { Lock lock; s=state; }
    cJSON *root=cJSON_CreateObject();
    if (!root) return error(r,HTTPD_500_INTERNAL_SERVER_ERROR,"No memory");
    auto *slots=cJSON_AddArrayToObject(root,"slots");
    bool complete=slots && cJSON_AddNumberToObject(root,"slotCount",SLOTS) &&
                  cJSON_AddNumberToObject(root,"activeSlot",s.active);
    for (int i=0;i<SLOTS;++i) {
        if (!complete) break;
        auto *entry=cJSON_CreateObject();
        complete=entry && cJSON_AddNumberToObject(entry,"slot",i) &&
                 cJSON_AddBoolToObject(entry,"hasImage",(s.available&(1<<i))!=0) &&
                 cJSON_AddBoolToObject(entry,"isActive",s.active==i && (s.available&(1<<i))) &&
                 cJSON_AddItemToArray(slots,entry);
        if (!complete) cJSON_Delete(entry);
    }
    char *json=complete?cJSON_PrintUnformatted(root):nullptr;
    cJSON_Delete(root);
    if (!json) return error(r,HTTPD_500_INTERNAL_SERVER_ERROR,"No memory");
    httpd_resp_set_type(r,"application/json");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    esp_err_t result=httpd_resp_sendstr(r,json);
    cJSON_free(json);
    return result;
}
esp_err_t image_get(httpd_req_t *r)
{
    if (!allowed(r)) return ESP_OK;
    int slot; if (!request_slot(r,slot)) return error(r,HTTPD_400_BAD_REQUEST,"Invalid slot");
    // A single HTTP task serializes uploads and downloads. Copy out before sending
    // so a slow phone never holds the storage lock needed by the display worker.
    uint8_t *bytes=nullptr; unsigned size=0;
    {
        Lock lock; char path[64]; path_for(slot,path,sizeof(path));
        bytes=read_image_file(path,&size);
    }
    if (!bytes) return error(r,HTTPD_404_NOT_FOUND,"Image unavailable");
    httpd_resp_set_type(r,"image/jpeg");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    esp_err_t result=httpd_resp_send(r,reinterpret_cast<char *>(bytes),size);
    heap_caps_free(bytes);
    return result;
}
esp_err_t upload_post(httpd_req_t *r)
{
    if (!allowed(r)) return ESP_OK;
    int slot; if (!request_slot(r,slot)) return error(r,HTTPD_400_BAD_REQUEST,"Invalid slot");
    if (!r->content_len || r->content_len>MAX_JPEG)
        return error(r,HTTPD_400_BAD_REQUEST,"JPEG must be at most 256 KB");
    auto *bytes=static_cast<uint8_t *>(heap_caps_aligned_alloc(16,(r->content_len+15)&~15,
                                                           MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if (!bytes) return error(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Not enough memory");
    size_t used=0;
    const int64_t deadline=esp_timer_get_time()+20000000;
    while (used<r->content_len && editing && esp_timer_get_time()<deadline) {
        int n=httpd_req_recv(r,reinterpret_cast<char *>(bytes+used),r->content_len-used);
        if (n==HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (n<=0) break;
        used+=n;
    }
    // Reject incomplete transfers before reserving the decoded frame.
    if (used!=r->content_len) {
        heap_caps_free(bytes);
        return error(r,HTTPD_400_BAD_REQUEST,"Use a valid baseline 360 x 360 JPEG");
    }
    auto *pixels=static_cast<uint16_t *>(heap_caps_aligned_alloc(16,PIXEL_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if (!pixels) {
        heap_caps_free(bytes);
        return error(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Not enough memory");
    }
    bool valid=decode(bytes,used,pixels);
    heap_caps_free(pixels);
    bool saved=false, selected=false;
    if (valid && editing) {
        Lock lock; char path[64]; path_for(slot,path,sizeof(path));
        saved=replace_file(path,bytes,used);
        if (saved) {
            state.available|=1<<slot;
            ++state.revision;
            // An upload can succeed even if remembering the selected slot fails.
            selected=select_locked(slot);
        }
    }
    heap_caps_free(bytes);
    if (!valid) return error(r,HTTPD_400_BAD_REQUEST,"Use a valid baseline 360 x 360 JPEG");
    if (!saved) return error(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Could not save image. Please retry.");
    if (!selected) return error(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Image saved, but selection failed. Refresh and select it again.");
    return ok(r);
}
esp_err_t active_post(httpd_req_t *r)
{
    if (!allowed(r)) return ESP_OK;
    int slot;
    if (!request_slot(r,slot) || !select(slot)) return error(r,HTTPD_400_BAD_REQUEST,"Cannot select this slot");
    return ok(r);
}
esp_err_t image_delete(httpd_req_t *r)
{
    if (!allowed(r)) return ESP_OK;
    int slot; if (!request_slot(r,slot)) return error(r,HTTPD_400_BAD_REQUEST,"Invalid slot");
    Lock lock; char path[64]; path_for(slot,path,sizeof(path));
    if (unlink(path)) return error(r,HTTPD_404_NOT_FOUND,"Image not found");
    state.available&=~(1<<slot); ++state.revision;
    if (state.active==slot) {
        unlink("/badge/active");
        for (int i=0;i<SLOTS;++i) if (state.available&(1<<i)) { select_locked(i); break; }
    }
    return ok(r);
}
esp_err_t close_post(httpd_req_t *r)
{
    if (!allowed(r)) return ESP_OK;
    editing=false; finished=true;
    wifi_manager_end_web_transfer();
    return ok(r);
}
void stop_web_editor() { editing=false; finished=true; }
esp_err_t register_routes(httpd_handle_t server)
{
    const httpd_uri_t routes[]={
        {.uri="/badge",.method=HTTP_GET,.handler=index_get},
        {.uri="/badge/",.method=HTTP_GET,.handler=index_get},
        {.uri="/badge/state",.method=HTTP_GET,.handler=state_get},
        {.uri="/badge/image",.method=HTTP_GET,.handler=image_get},
        {.uri="/badge/image",.method=HTTP_DELETE,.handler=image_delete},
        {.uri="/badge/active",.method=HTTP_POST,.handler=active_post},
        {.uri="/badge/upload",.method=HTTP_POST,.handler=upload_post},
        {.uri="/badge/close",.method=HTTP_POST,.handler=close_post},
    };
    for (const auto &route:routes) {
        esp_err_t result=httpd_register_uri_handler(server,&route);
        if (result!=ESP_OK) return result;
    }
    return ESP_OK;
}
}

bool prepare()
{
    if (!mutex) mutex=xSemaphoreCreateMutex();
    return mutex && wifi_manager_register_local_portal(register_routes,stop_web_editor);
}
bool initialize()
{
    // prepare() creates synchronization before starting the APP worker.
    if (!mutex) return false;
    Lock lock;
    if (state.ready) return true;
    esp_vfs_spiffs_conf_t cfg={.base_path="/badge",.partition_label="assets",.max_files=4,.format_if_mount_failed=true};
    esp_err_t result=esp_vfs_spiffs_register(&cfg);
    if (result!=ESP_OK) {
        snprintf(state.error,sizeof(state.error),"Badge storage: %s",esp_err_to_name(result));
        return false;
    }
    state.error[0]=0;
    recover("/badge/active");
    for (int i=0;i<SLOTS;++i) {
        char path[64]; path_for(i,path,sizeof(path)); recover(path);
        if (exists(path)) state.available|=1<<i;
    }
    FILE *f=fopen("/badge/active","rb");
    if (f) { int value=fgetc(f); if (value>='0' && value<'0'+SLOTS) state.active=value-'0'; fclose(f); }
    if (!(state.available&(1<<state.active)))
        for (int i=0;i<SLOTS;++i) if (state.available&(1<<i)) { state.active=i; break; }
    state.ready=true; ++state.revision;
    ESP_LOGI("badge","Six image slots ready in the assets partition");
    return true;
}
bool snapshot(State &out)
{
    if (!mutex || xSemaphoreTake(mutex,0)!=pdTRUE) return false;
    out=state; xSemaphoreGive(mutex); return true;
}
bool load(int slot,uint16_t *pixels,char *message,unsigned cap)
{
    if (!mutex || slot<0 || slot>=SLOTS || !pixels) return false;
    uint8_t *bytes=nullptr; unsigned size=0;
    {
        Lock lock; char path[64]; path_for(slot,path,sizeof(path));
        bytes=read_image_file(path,&size);
    }
    bool ok=size && decode(bytes,size,pixels);
    heap_caps_free(bytes);
    if (!ok) snprintf(message,cap,"Cannot decode badge image.\nTap to open controls.");
    return ok;
}
bool select(int slot) { if (!mutex) return false; Lock lock; return select_locked(slot); }
bool next(int direction)
{
    if (!mutex) return false;
    Lock lock;
    for (int step=1;step<=SLOTS;++step) {
        int candidate=(state.active+(direction>0?step:-step)+SLOTS)%SLOTS;
        if (state.available&(1<<candidate)) return select_locked(candidate);
    }
    return false;
}
bool start_editor()
{
    if (!wifi_manager_start_local_portal(register_routes)) return false;
    finished=false; editing=true;
    return true;
}
void stop_editor()
{
    // A phone owns its transfer independently of the device's Badge overlay.
    if (!wifi_manager_web_transfer_active()) editing=false;
    wifi_manager_stop_local_portal();
}
bool editor_finished() { return finished.exchange(false); }
}
