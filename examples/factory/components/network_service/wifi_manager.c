/* One worker owns connection, read-only scans and the C5-style SoftAP portal.
 * Prefer a verified saved network at boot; otherwise use the RLCD default. */
#include "wifi_manager.h"
#include "bsp/esp-bsp.h"
#include "wifi_credentials.h"
#include "portal_dns.h"
#include "cJSON.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONNECTED_BIT BIT0
#define SCAN_DONE_BIT BIT1
#define SCAN_FAILED_BIT BIT2
#define CONNECT_TIMEOUT_US (45LL * 1000000)
#define RETRY_US (10LL * 1000000)

static const char *TAG="s35_wifi";
extern const char root_html_start[] asm("_binary_root_html_start");
extern const char root_html_end[] asm("_binary_root_html_end");
extern const char home_html_start[] asm("_binary_home_html_start");
extern const char home_html_end[] asm("_binary_home_html_end");
typedef struct { bool scan, portal_scan; uint32_t portal_session; wifi_credentials_t credentials; } command_t;
static QueueHandle_t commands;
static EventGroupHandle_t events;
static portMUX_TYPE snapshot_lock=portMUX_INITIALIZER_UNLOCKED;
static wifi_manager_snapshot_t snapshot;
static bool config_pending;
static bool portal_requested;
static wifi_local_portal_register_t local_portal_routes;
static wifi_local_portal_stop_t local_portal_stop;
enum { PORTAL_SETTINGS=1, PORTAL_BADGE=2, PORTAL_BROWSER=4 };
static unsigned portal_owners;
static bool web_transfer;
static bool portal_closing;
static int64_t web_activity;
#define WEB_IDLE_US (10LL * 60 * 1000000)
static uint32_t portal_session, published_session;
/* Written only by the worker; copied alongside each published snapshot. */
static uint32_t worker_session;
static wifi_scan_snapshot_t scan_snapshot;
static wifi_scan_record_t *scan_records;
static httpd_handle_t server;
static int dns_socket=-1;
static esp_netif_t *ap_netif;
/* Shared with the SNTP callback. Retain validity while the system runs offline. */
static bool time_synchronized;
static bool time_restored_from_rtc;
static uint32_t rtc_sync_generation;

/* snapshot_lock protects ownership and session generation. Device screens
 * and the browser release only their own lease, preserving active transfers. */
static void portal_acquire_locked(unsigned owner)
{
    if (!portal_requested) ++portal_session;
    portal_owners|=owner;
    portal_requested=true;
}
static void portal_release_locked(unsigned owner)
{
    portal_owners&=~owner;
    portal_requested=portal_owners!=0;
}
static bool portal_expire_locked(int64_t now)
{
    if (!(portal_owners&PORTAL_BROWSER) || now-web_activity<WEB_IDLE_US) return false;
    portal_release_locked(PORTAL_BROWSER);
    web_transfer=false;
    return true;
}

static void publish(const wifi_manager_snapshot_t *view)
{
    portENTER_CRITICAL(&snapshot_lock);
    snapshot=*view;
    published_session=worker_session;
    portEXIT_CRITICAL(&snapshot_lock);
}

void wifi_manager_get_snapshot(wifi_manager_snapshot_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&snapshot_lock);
    *out=snapshot;
    if (published_session!=portal_session) {
        out->provisioning=false;
        out->provisioning_success=false;
    }
    if (config_pending) {
        out->connecting=true; out->connected=false;
        out->credentials_saved=false; out->provisioning_success=false;
    }
    portEXIT_CRITICAL(&snapshot_lock);
}

bool wifi_manager_is_connected(void)
{
    wifi_manager_snapshot_t view;
    wifi_manager_get_snapshot(&view);
    // Disconnect events clear the bit immediately, before the worker publishes
    // its next snapshot. Reading cached state never queries an unassociated STA.
    return events && (xEventGroupGetBits(events)&CONNECTED_BIT) && view.connected;
}

bool wifi_manager_get_clock(wifi_clock_snapshot_t *out)
{
    if (!out) return false;
    memset(out,0,sizeof(*out));
    portENTER_CRITICAL(&snapshot_lock);
    out->source=time_synchronized?WIFI_CLOCK_NETWORK:
                time_restored_from_rtc?WIFI_CLOCK_RTC:WIFI_CLOCK_UNSET;
    portEXIT_CRITICAL(&snapshot_lock);
    return out->source!=WIFI_CLOCK_UNSET && gettimeofday(&out->instant,NULL)==0 &&
           localtime_r(&out->instant.tv_sec,&out->local)!=NULL;
}

bool wifi_manager_get_local_time(struct tm *out)
{
    if (!out) return false;
    wifi_clock_snapshot_t clock;
    if (!wifi_manager_get_clock(&clock)) return false;
    *out=clock.local;
    return true;
}

/* RTC stores a Beijing calendar; mktime converts it to the UTC system epoch.
 * Seed only at worker startup, before SNTP can run, never on app entry. */
static void restore_rtc_time(void)
{
    bsp_sensor_datetime_t rtc;
    esp_err_t result=bsp_rtc_get_datetime(&rtc);
    if (result!=ESP_OK) {
        ESP_LOGW(TAG,"RTC time unavailable; waiting for network time: %s",esp_err_to_name(result));
        return;
    }
    struct tm local={.tm_year=rtc.year-1900,.tm_mon=rtc.month-1,.tm_mday=rtc.day,
                     .tm_hour=rtc.hour,.tm_min=rtc.minute,.tm_sec=rtc.second,.tm_isdst=-1};
    time_t epoch=mktime(&local);
    /* Reject normalized invalid dates rather than silently using a new date. */
    if (epoch==(time_t)-1 || local.tm_year!=rtc.year-1900 || local.tm_mon!=rtc.month-1 ||
        local.tm_mday!=rtc.day || local.tm_hour!=rtc.hour || local.tm_min!=rtc.minute ||
        local.tm_sec!=rtc.second) {
        ESP_LOGW(TAG,"Invalid RTC calendar; waiting for network time");
        return;
    }
    struct timeval now={.tv_sec=epoch,.tv_usec=0};
    if (settimeofday(&now,NULL)!=0) return;
    portENTER_CRITICAL(&snapshot_lock);
    time_restored_from_rtc=true;
    portEXIT_CRITICAL(&snapshot_lock);
    ESP_LOGI(TAG,"System time restored from RTC (UTC+8): %04u-%02u-%02u %02u:%02u:%02u",
             rtc.year,rtc.month,rtc.day,rtc.hour,rtc.minute,rtc.second);
}

/* I2C stays on this internal-stack worker, never the GUI or SNTP callback.
 * Generations retain a correction that arrives during an in-flight RTC write. */
static void poll_rtc_writeback(void)
{
    static uint32_t completed_generation;
    static int64_t retry_at;
    portENTER_CRITICAL(&snapshot_lock);
    uint32_t generation=rtc_sync_generation;
    portEXIT_CRITICAL(&snapshot_lock);
    if (generation==completed_generation || esp_timer_get_time()<retry_at) return;
    wifi_clock_snapshot_t clock;
    if (!wifi_manager_get_clock(&clock)) return;
    struct tm *local=&clock.local;
    if (local->tm_year<100 || local->tm_year>199) {
        completed_generation=generation;
        ESP_LOGW(TAG,"Network date outside RTC range (2000-2099); system time retained");
        return;
    }
    bsp_sensor_datetime_t rtc={.year=local->tm_year+1900,.month=local->tm_mon+1,
        .day=local->tm_mday,.hour=local->tm_hour,.minute=local->tm_min,
        .second=local->tm_sec,.weekday=local->tm_wday};
    esp_err_t result=bsp_rtc_set_datetime(&rtc);
    if (result==ESP_ERR_INVALID_STATE) {
        result=bsp_rtc_init();
        if (result==ESP_OK) {
            /* Initialization may wait for the oscillator. Use fresh time. */
            if (!wifi_manager_get_clock(&clock)) return;
            rtc=(bsp_sensor_datetime_t){.year=local->tm_year+1900,.month=local->tm_mon+1,
                .day=local->tm_mday,.hour=local->tm_hour,.minute=local->tm_min,
                .second=local->tm_sec,.weekday=local->tm_wday};
            result=bsp_rtc_set_datetime(&rtc);
        }
    }
    if (result==ESP_OK) {
        completed_generation=generation;
        retry_at=0;
        ESP_LOGI(TAG,"Beijing network time saved to RTC");
    } else {
        retry_at=esp_timer_get_time()+30000000;
        ESP_LOGW(TAG,"RTC time writeback failed; retry in 30 s: %s",esp_err_to_name(result));
    }
}

static void time_sync_complete(struct timeval *tv)
{
    struct tm local;
    if (!tv || !localtime_r(&tv->tv_sec,&local)) return;
    portENTER_CRITICAL(&snapshot_lock);
    time_synchronized=true;
    ++rtc_sync_generation;
    portEXIT_CRITICAL(&snapshot_lock);
    ESP_LOGI(TAG,"SNTP synced (UTC+8): %04d-%02d-%02d %02d:%02d:%02d",
             local.tm_year+1900,local.tm_mon+1,local.tm_mday,
             local.tm_hour,local.tm_min,local.tm_sec);
}

/* Run on the existing Wi-Fi worker after STA receives an IP. The thread-safe
 * netif API schedules SNTP on lwIP; no wait for a reply, no GUI or RTC access. */
static void poll_time_sync(bool connected)
{
    static bool initialized, started_for_connection;
    static int64_t retry_at, next_pending_log;
    if (!connected) {
        started_for_connection=false;
        retry_at=0;
        return;
    }
    int64_t now=esp_timer_get_time();
    if (started_for_connection) {
        portENTER_CRITICAL(&snapshot_lock);
        bool synced=time_synchronized;
        portEXIT_CRITICAL(&snapshot_lock);
        if (!synced && now>=next_pending_log) {
            ESP_LOGW(TAG,"SNTP has not received a reply yet; retrying ntp.aliyun.com / pool.ntp.org (UDP 123)");
            next_pending_log=now+60000000;
        }
        return;
    }
    if (now<retry_at) return;

    esp_err_t result;
    if (!initialized) {
        esp_sntp_config_t config=ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(
            2,ESP_SNTP_SERVER_LIST("ntp.aliyun.com","pool.ntp.org"));
        config.wait_for_sync=false;
        config.sync_cb=time_sync_complete;
        result=esp_netif_sntp_init(&config);
        initialized=result==ESP_OK;
    } else {
        result=esp_netif_sntp_start();
    }
    if (result==ESP_OK) {
        started_for_connection=true;
        next_pending_log=now+30000000;
        ESP_LOGI(TAG,"SNTP started: ntp.aliyun.com, pool.ntp.org (waiting for reply)");
    } else {
        retry_at=now+RETRY_US;
        ESP_LOGW(TAG,"SNTP start failed; retrying: %s",esp_err_to_name(result));
    }
}

static void set_message(wifi_manager_snapshot_t *view, const char *message)
{
    snprintf(view->message,sizeof(view->message),"%s",message);
    publish(view);
}

static bool save_credentials(const wifi_credentials_t *credentials)
{
    nvs_handle_t nvs;
    if (nvs_open("s35_wifi",NVS_READWRITE,&nvs)!=ESP_OK) return false;
    esp_err_t result=nvs_set_blob(nvs,"credentials",credentials,sizeof(*credentials));
    if (result==ESP_OK) result=nvs_commit(nvs);
    nvs_close(nvs);
    return result==ESP_OK;
}

static bool load_credentials(wifi_credentials_t *out)
{
    nvs_handle_t nvs;
    if (nvs_open("s35_wifi",NVS_READONLY,&nvs)!=ESP_OK) return false;
    wifi_credentials_t candidate={0};
    size_t size=sizeof(candidate);
    esp_err_t result=nvs_get_blob(nvs,"credentials",&candidate,&size);
    nvs_close(nvs);
    if (result!=ESP_OK || size!=sizeof(candidate) || !wifi_credentials_valid(&candidate)) return false;
    *out=candidate;
    return true;
}

static void network_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base==IP_EVENT && id==IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *ip=data;
        if (ip) {
            ESP_LOGI(TAG,"STA IP=" IPSTR ", gateway=" IPSTR,
                     IP2STR(&ip->ip_info.ip),IP2STR(&ip->ip_info.gw));
            esp_netif_dns_info_t dns;
            if (esp_netif_get_dns_info(ip->esp_netif,ESP_NETIF_DNS_MAIN,&dns)==ESP_OK &&
                dns.ip.type==ESP_IPADDR_TYPE_V4)
                ESP_LOGI(TAG,"STA DNS=" IPSTR,IP2STR(&dns.ip.u_addr.ip4));
        }
        xEventGroupSetBits(events,CONNECTED_BIT);
    }
    else if (base==WIFI_EVENT && id==WIFI_EVENT_SCAN_DONE) {
        const wifi_event_sta_scan_done_t *scan=data;
        xEventGroupSetBits(events,SCAN_DONE_BIT | (scan && scan->status ? SCAN_FAILED_BIT : 0));
    }
    else if ((base==WIFI_EVENT && (id==WIFI_EVENT_STA_DISCONNECTED || id==WIFI_EVENT_STA_STOP)) ||
             (base==IP_EVENT && id==IP_EVENT_STA_LOST_IP)) xEventGroupClearBits(events,CONNECTED_BIT);
}

static esp_err_t root_get(httpd_req_t *request)
{
    httpd_resp_set_type(request,"text/html; charset=utf-8");
    httpd_resp_set_hdr(request,"Cache-Control","no-store");
    return httpd_resp_send(request,home_html_start,home_html_end-home_html_start);
}

static esp_err_t unavailable(httpd_req_t *request,const char *message)
{
    httpd_resp_set_status(request,"503 Service Unavailable");
    httpd_resp_set_type(request,"text/plain; charset=utf-8");
    return httpd_resp_sendstr(request,message);
}

static esp_err_t wifi_get(httpd_req_t *request)
{
    portENTER_CRITICAL(&snapshot_lock);
    bool available=portal_requested && snapshot.provisioning && !portal_closing;
    if (available) {
        portal_acquire_locked(PORTAL_BROWSER);
        web_activity=esp_timer_get_time();
        web_transfer=false;
    }
    wifi_local_portal_stop_t stop=local_portal_stop;
    portEXIT_CRITICAL(&snapshot_lock);
    if (!available) return unavailable(request,"Hotspot is closing. Reopen Phone connection on the device.");
    /* HTTP handlers are serialized: an upload finishes before this switch. */
    if (stop) stop();
    httpd_resp_set_type(request,"text/html; charset=utf-8");
    httpd_resp_set_hdr(request,"Cache-Control","no-store");
    return httpd_resp_send(request,root_html_start,root_html_end-root_html_start);
}

static esp_err_t status_get(httpd_req_t *request)
{
    wifi_manager_touch_web_session();
    wifi_manager_snapshot_t view;
    wifi_manager_get_snapshot(&view);
    cJSON *root=cJSON_CreateObject();
    if (!root) return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"No memory");
    cJSON_AddBoolToObject(root,"connected",view.connected);
    cJSON_AddBoolToObject(root,"connecting",view.connecting);
    cJSON_AddBoolToObject(root,"credentials_saved",view.credentials_saved);
    cJSON_AddStringToObject(root,"message",view.message);
    cJSON_AddStringToObject(root,"ssid",view.ssid);
    char *json=cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"No memory");
    httpd_resp_set_type(request,"application/json");
    httpd_resp_set_hdr(request,"Cache-Control","no-store");
    esp_err_t result=httpd_resp_sendstr(request,json);
    cJSON_free(json);
    return result;
}

static esp_err_t config_post(httpd_req_t *request)
{
    char body[1025];
    if (request->content_len==0 || request->content_len>sizeof(body)-1)
        return httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"Invalid request size");
    size_t used=0;
    int64_t deadline=esp_timer_get_time()+8000000;
    while (used<request->content_len) {
        int count=httpd_req_recv(request,body+used,request->content_len-used);
        if (count==HTTPD_SOCK_ERR_TIMEOUT && esp_timer_get_time()<deadline) continue;
        if (count<=0) return httpd_resp_send_err(request,HTTPD_408_REQ_TIMEOUT,"Request timed out");
        used+=(size_t)count;
    }
    body[used]=0;
    command_t command={0};
    if (!wifi_credentials_parse(body,used,&command.credentials))
        return httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"Invalid SSID or password");
    portENTER_CRITICAL(&snapshot_lock);
    bool busy=config_pending || snapshot.connecting || scan_snapshot.running || !portal_requested || web_transfer;
    if (!busy) {
        portal_acquire_locked(PORTAL_BROWSER);
        web_activity=esp_timer_get_time();
        config_pending=true; command.portal_session=portal_session;
    }
    portEXIT_CRITICAL(&snapshot_lock);
    if (busy) {
        httpd_resp_set_status(request,"409 Conflict");
        return httpd_resp_sendstr(request,"Finish the image editor or wait for the Wi-Fi operation, then retry.");
    }
    if (xQueueSend(commands,&command,0)!=pdTRUE) {
        portENTER_CRITICAL(&snapshot_lock);
        config_pending=false;
        portEXIT_CRITICAL(&snapshot_lock);
        httpd_resp_set_status(request,"503 Service Unavailable");
        return httpd_resp_sendstr(request,"Try again");
    }
    httpd_resp_set_type(request,"application/json");
    httpd_resp_set_status(request,"202 Accepted");
    return httpd_resp_sendstr(request,"{\"status\":\"ok\"}");
}

static esp_err_t portal_close_post(httpd_req_t *request)
{
    portENTER_CRITICAL(&snapshot_lock);
    portal_owners=0;
    portal_requested=false;
    web_transfer=false;
    portEXIT_CRITICAL(&snapshot_lock);
    httpd_resp_set_type(request,"application/json");
    return httpd_resp_sendstr(request,"{\"status\":\"closed\"}");
}

static esp_err_t scan_post(httpd_req_t *request)
{
    portENTER_CRITICAL(&snapshot_lock);
    bool accepted=commands && snapshot.ready && snapshot.provisioning && portal_requested &&
                  !scan_snapshot.running && !config_pending && !snapshot.connecting && !web_transfer && !portal_closing;
    command_t command={.scan=true,.portal_scan=true,.portal_session=portal_session};
    if (accepted) {
        scan_snapshot.running=true; scan_snapshot.error=ESP_OK; scan_snapshot.count=0;
        portal_acquire_locked(PORTAL_BROWSER); web_activity=esp_timer_get_time();
    }
    portEXIT_CRITICAL(&snapshot_lock);
    if (!accepted) {
        httpd_resp_set_status(request,"409 Conflict");
        return httpd_resp_sendstr(request,"Finish the image editor or wait for the current scan, then retry.");
    }
    if (xQueueSend(commands,&command,0)!=pdTRUE) {
        portENTER_CRITICAL(&snapshot_lock);
        scan_snapshot.running=false; scan_snapshot.error=ESP_ERR_NO_MEM;
        portEXIT_CRITICAL(&snapshot_lock);
        return unavailable(request,"Scan queue is busy. Retry.");
    }
    httpd_resp_set_type(request,"application/json");
    return httpd_resp_sendstr(request,"{\"status\":\"scanning\"}");
}

static esp_err_t scan_get(httpd_req_t *request)
{
    wifi_manager_touch_web_session();
    unsigned first=0;
    char query[32],value[12];
    if (httpd_req_get_url_query_len(request)) {
        if (httpd_req_get_url_query_str(request,query,sizeof(query))!=ESP_OK ||
            httpd_query_key_value(query,"first",value,sizeof(value))!=ESP_OK)
            return httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"Invalid scan page");
        char *end; long parsed=strtol(value,&end,10);
        if (!value[0] || *end || parsed<0 || parsed>UINT16_MAX)
            return httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"Invalid scan page");
        first=(unsigned)parsed;
    }
    wifi_scan_snapshot_t page; wifi_manager_get_scan_snapshot(&page,(uint16_t)first);
    cJSON *json=cJSON_CreateObject();
    if (!json) return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"No memory");
    cJSON_AddBoolToObject(json,"running",page.running);
    cJSON_AddNumberToObject(json,"error",page.error);
    cJSON_AddNumberToObject(json,"count",page.count);
    cJSON_AddNumberToObject(json,"revision",page.revision);
    cJSON *records=cJSON_AddArrayToObject(json,"records");
    if (!records) {
        cJSON_Delete(json);
        return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"No memory");
    }
    for (unsigned i=0;i<page.page_count;++i) {
        cJSON *record=cJSON_CreateObject();
        if (!record) {
            cJSON_Delete(json);
            return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"No memory");
        }
        cJSON_AddStringToObject(record,"ssid",page.records[i].ssid);
        cJSON_AddStringToObject(record,"security",page.records[i].security);
        cJSON_AddNumberToObject(record,"rssi",page.records[i].rssi);
        cJSON_AddItemToArray(records,record);
    }
    char *body=cJSON_PrintUnformatted(json); cJSON_Delete(json);
    if (!body) return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"No memory");
    httpd_resp_set_type(request,"application/json");
    httpd_resp_set_hdr(request,"Cache-Control","no-store");
    esp_err_t result=httpd_resp_sendstr(request,body); cJSON_free(body);
    return result;
}

static esp_err_t redirect_get(httpd_req_t *request, httpd_err_code_t error)
{
    (void)error;
    httpd_resp_set_status(request,"302 Found");
    httpd_resp_set_hdr(request,"Location",WIFI_PROVISION_URL "/");
    return httpd_resp_sendstr(request,"Open device setup");
}

static esp_err_t captive_probe_get(httpd_req_t *request)
{
    return redirect_get(request,HTTPD_404_NOT_FOUND);
}

static void stop_portal(void)
{
    if (server) { httpd_stop(server); server=NULL; }
    if (dns_socket>=0) { close(dns_socket); dns_socket=-1; }
    if (local_portal_stop) local_portal_stop();
}

static esp_err_t start_portal(wifi_manager_snapshot_t *view)
{
    if (view->provisioning) return ESP_OK;
    wifi_config_t ap={.ap={.channel=1,.max_connection=4,.authmode=WIFI_AUTH_WPA2_PSK}};
    memcpy(ap.ap.ssid,view->ap_ssid,strlen(view->ap_ssid));
    ap.ap.ssid_len=strlen(view->ap_ssid);
    memcpy(ap.ap.password,WIFI_PROVISION_PASSWORD,strlen(WIFI_PROVISION_PASSWORD));
    esp_err_t result=esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (result==ESP_OK) result=esp_wifi_set_config(WIFI_IF_AP,&ap);
    if (result!=ESP_OK) return result;

    esp_netif_ip_info_t ip;
    result=esp_netif_get_ip_info(ap_netif,&ip);
    if (result!=ESP_OK) return result;
    esp_netif_dns_info_t dns={.ip={.type=ESP_IPADDR_TYPE_V4}};
    dns.ip.u_addr.ip4=ip.ip;
    esp_netif_dhcps_stop(ap_netif);
    esp_netif_set_dns_info(ap_netif,ESP_NETIF_DNS_MAIN,&dns);
    uint8_t offer_dns=1;
    esp_netif_dhcps_option(ap_netif,ESP_NETIF_OP_SET,ESP_NETIF_DOMAIN_NAME_SERVER,&offer_dns,sizeof(offer_dns));
    esp_netif_dhcps_option(ap_netif,ESP_NETIF_OP_SET,ESP_NETIF_CAPTIVEPORTAL_URI,
                          (void *)WIFI_PROVISION_URL,strlen(WIFI_PROVISION_URL));
    esp_netif_dhcps_start(ap_netif);

    httpd_config_t config=HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets=4;
    config.lru_purge_enable=true;
    config.recv_wait_timeout=5;
    config.stack_size=6144;
    config.max_uri_handlers=24;
    config.uri_match_fn=httpd_uri_match_wildcard;
    result=httpd_start(&server,&config);
    if (result!=ESP_OK) return result;
    const httpd_uri_t routes[]={
        {.uri="/",.method=HTTP_GET,.handler=root_get},
        {.uri="/wifi",.method=HTTP_GET,.handler=wifi_get},
        {.uri="/wifi/",.method=HTTP_GET,.handler=wifi_get},
        {.uri="/wifi_config",.method=HTTP_POST,.handler=config_post},
        {.uri="/status",.method=HTTP_GET,.handler=status_get},
        {.uri="/wifi/scan",.method=HTTP_POST,.handler=scan_post},
        {.uri="/wifi/scan",.method=HTTP_GET,.handler=scan_get},
        {.uri="/portal/close",.method=HTTP_POST,.handler=portal_close_post},
    };
    for (unsigned i=0;i<sizeof(routes)/sizeof(routes[0]) && result==ESP_OK;++i)
        result=httpd_register_uri_handler(server,&routes[i]);
    portENTER_CRITICAL(&snapshot_lock);
    wifi_local_portal_register_t local_routes=local_portal_routes;
    portEXIT_CRITICAL(&snapshot_lock);
    if (result==ESP_OK && local_routes) result=local_routes(server);
    // Register last so application routes take priority. Windows /connecttest.txt,
    // Android /generate_204 and Apple probes reach the portal without 404 spam.
    const httpd_uri_t captive={.uri="/*",.method=HTTP_GET,.handler=captive_probe_get};
    if (result==ESP_OK) result=httpd_register_uri_handler(server,&captive);
    if (result==ESP_OK) result=httpd_register_err_handler(server,HTTPD_404_NOT_FOUND,redirect_get);
    if (result!=ESP_OK) { stop_portal(); return result; }
    dns_socket=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if (dns_socket>=0) {
        const struct sockaddr_in address={.sin_family=AF_INET,.sin_port=htons(53),.sin_addr.s_addr=ip.ip.addr};
        if (bind(dns_socket,(const struct sockaddr *)&address,sizeof(address))<0 ||
            fcntl(dns_socket,F_SETFL,O_NONBLOCK)<0) { close(dns_socket); dns_socket=-1; }
    }
    /* Explicit 192.168.4.1 remains usable if a phone does not use captive DNS. */
    view->provisioning=true;
    view->provisioning_success=false;
    set_message(view,"Connect your phone to the device hotspot.");
    ESP_LOGI(TAG,"Provisioning ready: %s",view->ap_ssid);
    return ESP_OK;
}

static void poll_dns(void)
{
    if (dns_socket<0) return;
    esp_netif_ip_info_t ip;
    if (esp_netif_get_ip_info(ap_netif,&ip)!=ESP_OK) return;
    uint8_t ipv4[4]; memcpy(ipv4,&ip.ip.addr,4);
    for (int i=0;i<4;++i) {
        uint8_t query[512],reply[512];
        struct sockaddr_in client;
        socklen_t size=sizeof(client);
        int count=recvfrom(dns_socket,query,sizeof(query),0,(struct sockaddr *)&client,&size);
        if (count<=0) break;
        size_t length=portal_dns_reply(query,(size_t)count,reply,sizeof(reply),ipv4);
        if (length) sendto(dns_socket,reply,length,0,(struct sockaddr *)&client,size);
    }
}

static esp_err_t connect_sta(const wifi_credentials_t *credentials)
{
    esp_wifi_disconnect();
    xEventGroupClearBits(events,CONNECTED_BIT);
    wifi_config_t config={0};
    memcpy(config.sta.ssid,credentials->ssid,strlen(credentials->ssid));
    memcpy(config.sta.password,credentials->password,strlen(credentials->password));
    config.sta.threshold.authmode=credentials->password[0]?WIFI_AUTH_WPA2_PSK:WIFI_AUTH_OPEN;
    config.sta.sae_pwe_h2e=WPA3_SAE_PWE_BOTH;
    esp_err_t result=esp_wifi_set_config(WIFI_IF_STA,&config);
    return result==ESP_OK?esp_wifi_connect():result;
}

static esp_err_t initialize_radio(void)
{
    esp_err_t result=bsp_get_init_status()->wifi;
    if (result!=ESP_OK) return result;
    ap_netif=esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!ap_netif) return ESP_ERR_NO_MEM;
    result=esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (result==ESP_OK) result=esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,network_event,NULL);
    if (result==ESP_OK) result=esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,network_event,NULL);
    if (result==ESP_OK) result=esp_event_handler_register(IP_EVENT,IP_EVENT_STA_LOST_IP,network_event,NULL);
    if (result==ESP_OK) result=esp_wifi_set_mode(WIFI_MODE_STA);
    if (result==ESP_OK) result=bsp_wifi_start();
    return result;
}

void wifi_manager_get_scan_snapshot(wifi_scan_snapshot_t *out, uint16_t first)
{
    if (!out) return;
    portENTER_CRITICAL(&snapshot_lock);
    *out=scan_snapshot;
    out->first=first<out->count?first:0;
    if (out->count && scan_records) {
        uint16_t remaining=out->count-out->first;
        out->page_count=remaining<WIFI_SCAN_PAGE_SIZE?remaining:WIFI_SCAN_PAGE_SIZE;
        memcpy(out->records,scan_records+out->first,out->page_count*sizeof(out->records[0]));
    }
    portEXIT_CRITICAL(&snapshot_lock);
}

static const char *scan_security(wifi_auth_mode_t mode)
{
    switch (mode) {
    case WIFI_AUTH_OPEN: return "OPEN";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA3_PSK: return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
    default: return "SEC";
    }
}

static void finish_scan(esp_err_t error)
{
    wifi_scan_snapshot_t result={.error=error};
    wifi_scan_record_t *records=NULL;
    if (error==ESP_OK) {
        uint16_t count=0;
        error=esp_wifi_scan_get_ap_num(&count);
        if (error==ESP_OK && count) {
            records=calloc(count,sizeof(*records));
            if (!records) error=ESP_ERR_NO_MEM;
        }
        if (error==ESP_OK) {
            result.count=count;
            for (uint16_t i=0;i<count;++i) {
                /* Pop one driver record at a time: no large raw array on the
                 * worker stack, and no second full copy of the driver list. */
                wifi_ap_record_t ap;
                error=esp_wifi_scan_get_ap_record(&ap);
                if (error!=ESP_OK) break;
                wifi_scan_record_t *out=&records[i];
                memcpy(out->ssid,ap.ssid,32);
                out->ssid[32]=0;
                snprintf(out->security,sizeof(out->security),"%s",scan_security(ap.authmode));
                out->rssi=ap.rssi;
                out->channel=ap.primary;
            }
        }
    }
    if (error!=ESP_OK) {
        free(records); records=NULL;
        result.count=0; result.error=error;
        esp_wifi_clear_ap_list();
        ESP_LOGW(TAG,"Wi-Fi scan failed: %s",esp_err_to_name(error));
    }
    portENTER_CRITICAL(&snapshot_lock);
    wifi_scan_record_t *previous=scan_records;
    scan_records=records;
    result.revision=scan_snapshot.revision+1;
    scan_snapshot=result;
    portEXIT_CRITICAL(&snapshot_lock);
    free(previous);
}

static void network_worker(void *arg)
{
    (void)arg;
    restore_rtc_time();
    wifi_manager_snapshot_t view={0};
    uint8_t mac[6]={0};
    esp_read_mac(mac,ESP_MAC_WIFI_SOFTAP);
    snprintf(view.ap_ssid,sizeof(view.ap_ssid),"S3-LCD-1.85B-%02X%02X%02X",mac[3],mac[4],mac[5]);
    set_message(&view,"Starting Wi-Fi...");
    esp_err_t result=initialize_radio();
    if (result!=ESP_OK) {
        ESP_LOGE(TAG,"Wi-Fi initialization failed: %s",esp_err_to_name(result));
        set_message(&view,"Wi-Fi startup failed. Restart device.");
        vTaskDelete(NULL); return;
    }
    view.ready=true;
    wifi_credentials_t credentials={
        .ssid=WIFI_DEFAULT_STA_SSID,
        .password=WIFI_DEFAULT_STA_PASSWORD
    };
    view.credentials_saved=load_credentials(&credentials);
    bool need_save=false, scan_active=false;
    uint32_t credential_session=0;
    int64_t scan_deadline=0;
    memcpy(view.ssid,credentials.ssid,sizeof(view.ssid));
    view.connecting=true;
    result=connect_sta(&credentials);
    int64_t attempt_deadline=esp_timer_get_time()+CONNECT_TIMEOUT_US;
    int64_t next_retry=esp_timer_get_time()+RETRY_US;
    set_message(&view,result==ESP_OK?"Connecting to Wi-Fi...":"Wi-Fi retry pending...");

    for (;;) {
        command_t command;
        if (xQueueReceive(commands,&command,pdMS_TO_TICKS(100))==pdTRUE) {
            if (command.scan) {
                portENTER_CRITICAL(&snapshot_lock);
                bool want_portal=portal_requested;
                bool portal_scan_current=command.portal_scan && want_portal && command.portal_session==portal_session;
                portEXIT_CRITICAL(&snapshot_lock);
                if (((want_portal || view.provisioning) && !portal_scan_current) ||
                    (command.portal_scan && !portal_scan_current)) finish_scan(ESP_ERR_INVALID_STATE);
                else {
                    /* A disconnected STA may still be trying to associate. Suspend
                     * retries for the survey; an established connection stays up. */
                    if (!view.connected) {
                        esp_wifi_disconnect();
                        view.connecting=false;
                        publish(&view);
                        vTaskDelay(pdMS_TO_TICKS(100));
                    }
                    xEventGroupClearBits(events,SCAN_DONE_BIT | SCAN_FAILED_BIT);
                    wifi_scan_config_t config={
                        .show_hidden=true,.scan_type=WIFI_SCAN_TYPE_ACTIVE,
                        .scan_time.active={.min=30,.max=120}
                    };
                    result=esp_wifi_scan_start(&config,false);
                    if (result==ESP_ERR_WIFI_STATE) {
                        vTaskDelay(pdMS_TO_TICKS(100));
                        result=esp_wifi_scan_start(&config,false);
                    }
                    scan_active=result==ESP_OK;
                    scan_deadline=esp_timer_get_time()+15000000;
                    if (!scan_active) finish_scan(result);
                    next_retry=scan_active?scan_deadline:esp_timer_get_time();
                }
            } else {
                portENTER_CRITICAL(&snapshot_lock);
                bool current=portal_requested && command.portal_session==portal_session;
                if (!current) config_pending=false;
                portEXIT_CRITICAL(&snapshot_lock);
                if (!current) continue;
                if (scan_active) {
                    esp_wifi_scan_stop();
                    finish_scan(ESP_ERR_INVALID_STATE);
                    scan_active=false;
                }
                credentials=command.credentials;
                need_save=true;
                credential_session=command.portal_session;
                view.connected=false; view.connecting=true; view.credentials_saved=false;
                view.provisioning_success=false;
                memcpy(view.ssid,credentials.ssid,sizeof(view.ssid));
                result=connect_sta(&credentials);
                attempt_deadline=esp_timer_get_time()+CONNECT_TIMEOUT_US;
                next_retry=esp_timer_get_time()+RETRY_US;
                set_message(&view,result==ESP_OK?"Connecting to your Wi-Fi...":"Wi-Fi retry pending...");
                portENTER_CRITICAL(&snapshot_lock);
                config_pending=false;
                portEXIT_CRITICAL(&snapshot_lock);
            }
        }
        int64_t now=esp_timer_get_time();
        portENTER_CRITICAL(&snapshot_lock);
        portal_expire_locked(now);
        bool want_portal=portal_requested;
        uint32_t requested_session=portal_session;
        portEXIT_CRITICAL(&snapshot_lock);
        if (want_portal && requested_session!=worker_session) {
            worker_session=requested_session;
            view.provisioning_success=false;
            set_message(&view,"Starting hotspot...");
        }
        if (want_portal && !view.provisioning) {
            if (scan_active) {
                esp_wifi_scan_stop();
                finish_scan(ESP_ERR_INVALID_STATE);
                scan_active=false;
            }
            if (!view.connected && !need_save) {
                esp_wifi_disconnect();
                view.connecting=false;
            }
            result=start_portal(&view);
            if (result!=ESP_OK) {
                stop_portal();
                esp_wifi_set_mode(WIFI_MODE_STA);
                portENTER_CRITICAL(&snapshot_lock);
                if (portal_session==requested_session) {
                    portal_owners=0; portal_requested=false; web_transfer=false;
                }
                portEXIT_CRITICAL(&snapshot_lock);
                set_message(&view,"Could not start hotspot. Go back and retry.");
            }
        }
        if (scan_active && ((xEventGroupGetBits(events)&SCAN_DONE_BIT) || now>=scan_deadline)) {
            EventBits_t bits=xEventGroupClearBits(events,SCAN_DONE_BIT | SCAN_FAILED_BIT);
            bool done=(bits&SCAN_DONE_BIT)!=0;
            if (!done) esp_wifi_scan_stop();
            finish_scan(!done?ESP_ERR_TIMEOUT:(bits&SCAN_FAILED_BIT)?ESP_FAIL:ESP_OK);
            scan_active=false;
            next_retry=now;
        }
        bool connected=(xEventGroupGetBits(events)&CONNECTED_BIT)!=0;
        if (connected && !view.connected) {
            wifi_ap_record_t ap;
            connected=esp_wifi_sta_get_ap_info(&ap)==ESP_OK &&
                      strncmp((const char *)ap.ssid,credentials.ssid,sizeof(ap.ssid))==0;
            if (connected) {
                view.connected=true; view.connecting=false;
                bool save_failed=false;
                if (need_save) {
                    view.credentials_saved=save_credentials(&credentials);
                    save_failed=!view.credentials_saved;
                    need_save=false;
                    view.provisioning_success=credential_session==worker_session;
                }
                set_message(&view,save_failed?"Connected; could not save Wi-Fi.":"Connected");
                ESP_LOGI(TAG,"Wi-Fi connected: %s; saved=%d",view.ssid,view.credentials_saved);
            }
        }
        if (!connected && view.connected) {
            view.connected=false; view.connecting=!view.provisioning;
            attempt_deadline=now+CONNECT_TIMEOUT_US;
            next_retry=now+1000000;
            set_message(&view,view.provisioning?"Wi-Fi disconnected. Device hotspot remains available.":
                                              "Wi-Fi disconnected. Reconnecting...");
        }
        poll_time_sync(connected && view.connected);
        poll_rtc_writeback();
        if (credentials.ssid[0] && !view.connected && !scan_active && (!view.provisioning || view.connecting)) {
            if (now>=next_retry) {
                esp_wifi_connect(); next_retry=now+RETRY_US;
                if (!view.connecting) { view.connecting=true; attempt_deadline=now+CONNECT_TIMEOUT_US; publish(&view); }
            }
            if (view.connecting && now>=attempt_deadline) {
                view.connecting=false;
                set_message(&view,view.provisioning?"Connection failed. Check password / 2.4 GHz.":
                            "Hotspot unavailable. Check 2.4 GHz; retrying...");
            }
        }
        if (view.provisioning && !want_portal) {
            portENTER_CRITICAL(&snapshot_lock);
            bool closing=!portal_requested;
            if (closing) portal_closing=true;
            portEXIT_CRITICAL(&snapshot_lock);
            if (!closing) continue;
            stop_portal();
            esp_wifi_set_mode(WIFI_MODE_STA);
            view.provisioning=false;
            next_retry=now;
            portENTER_CRITICAL(&snapshot_lock);
            if (portal_session==requested_session) {
                portal_owners=0; portal_requested=false; web_transfer=false;
            }
            portal_closing=false;
            portEXIT_CRITICAL(&snapshot_lock);
            publish(&view);
        }
        poll_dns();
    }
}

bool wifi_manager_init(void)
{
    if (commands) return true;
    /* POSIX CST-8 is Beijing (UTC+8), without DST; SNTP stores the UTC epoch. */
    if (setenv("TZ","CST-8",1)!=0) {
        ESP_LOGE(TAG,"Set Beijing timezone failed");
        return false;
    }
    tzset();
    commands=xQueueCreate(4,sizeof(command_t));
    events=xEventGroupCreate();
    if (commands && events && xTaskCreate(network_worker,"s35_wifi",6144,NULL,3,NULL)==pdPASS) return true;
    if (commands) vQueueDelete(commands);
    if (events) vEventGroupDelete(events);
    commands=NULL; events=NULL;
    return false;
}

bool wifi_manager_start_provisioning(void)
{
    wifi_manager_snapshot_t view;
    wifi_manager_get_snapshot(&view);
    if (!commands || !view.ready) return false;
    portENTER_CRITICAL(&snapshot_lock);
    if (portal_closing) { portEXIT_CRITICAL(&snapshot_lock); return false; }
    portal_acquire_locked(PORTAL_SETTINGS);
    portEXIT_CRITICAL(&snapshot_lock);
    return true;
}

void wifi_manager_stop_provisioning(void)
{
    portENTER_CRITICAL(&snapshot_lock);
    portal_release_locked(PORTAL_SETTINGS);
    portEXIT_CRITICAL(&snapshot_lock);
}

bool wifi_manager_register_local_portal(wifi_local_portal_register_t register_routes,
                                      wifi_local_portal_stop_t stop_editor)
{
    portENTER_CRITICAL(&snapshot_lock);
    bool accepted=register_routes && stop_editor &&
                  (!local_portal_routes || (local_portal_routes==register_routes && local_portal_stop==stop_editor));
    if (accepted) {
        local_portal_routes=register_routes;
        local_portal_stop=stop_editor;
    }
    portEXIT_CRITICAL(&snapshot_lock);
    return accepted;
}

bool wifi_manager_start_local_portal(wifi_local_portal_register_t register_routes)
{
    portENTER_CRITICAL(&snapshot_lock);
    bool accepted=register_routes && register_routes==local_portal_routes && commands &&
                  snapshot.ready && !config_pending && !scan_snapshot.running && !portal_closing;
    if (accepted) portal_acquire_locked(PORTAL_BADGE);
    portEXIT_CRITICAL(&snapshot_lock);
    return accepted;
}

void wifi_manager_stop_local_portal(void)
{
    portENTER_CRITICAL(&snapshot_lock);
    portal_release_locked(PORTAL_BADGE);
    portEXIT_CRITICAL(&snapshot_lock);
}

bool wifi_manager_begin_web_transfer(void)
{
    portENTER_CRITICAL(&snapshot_lock);
    bool accepted=commands && snapshot.ready && snapshot.provisioning && portal_requested &&
                  !config_pending && !snapshot.connecting && !scan_snapshot.running && !portal_closing;
    if (accepted) {
        portal_acquire_locked(PORTAL_BROWSER);
        web_transfer=true; web_activity=esp_timer_get_time();
    }
    portEXIT_CRITICAL(&snapshot_lock);
    return accepted;
}

void wifi_manager_end_web_transfer(void)
{
    portENTER_CRITICAL(&snapshot_lock);
    web_transfer=false;
    if (portal_owners&PORTAL_BROWSER) web_activity=esp_timer_get_time();
    portEXIT_CRITICAL(&snapshot_lock);
}

bool wifi_manager_web_transfer_active(void)
{
    portENTER_CRITICAL(&snapshot_lock);
    bool active=web_transfer;
    portEXIT_CRITICAL(&snapshot_lock);
    return active;
}

void wifi_manager_touch_web_session(void)
{
    portENTER_CRITICAL(&snapshot_lock);
    if (portal_owners&PORTAL_BROWSER) web_activity=esp_timer_get_time();
    portEXIT_CRITICAL(&snapshot_lock);
}

bool wifi_manager_request_scan(void)
{
    portENTER_CRITICAL(&snapshot_lock);
    bool accepted=commands && snapshot.ready && !scan_snapshot.running &&
                  !portal_requested && !snapshot.provisioning && !config_pending;
    if (accepted) {
        scan_snapshot.running=true;
        scan_snapshot.error=ESP_OK;
        scan_snapshot.count=0;
        ++scan_snapshot.revision;
    }
    portEXIT_CRITICAL(&snapshot_lock);
    if (!accepted) return false;
    command_t command={.scan=true};
    if (xQueueSend(commands,&command,0)==pdTRUE) return true;
    portENTER_CRITICAL(&snapshot_lock);
    scan_snapshot.running=false;
    scan_snapshot.error=ESP_ERR_NO_MEM;
    ++scan_snapshot.revision;
    portEXIT_CRITICAL(&snapshot_lock);
    return false;
}
