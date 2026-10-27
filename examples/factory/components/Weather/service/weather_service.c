#include "weather_service.h"
#include "weather_body.h"
#include "wifi_manager.h"
#include "sdkconfig.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BODY_LIMIT 16384
#define REFRESH_US (300LL*1000000)
#define RETRY_US (30LL*1000000)
static const char *TAG="weather";
static portMUX_TYPE data_lock=portMUX_INITIALIZER_UNLOCKED;
static weather_snapshot_t snapshot;
static TaskHandle_t worker;

typedef struct { esp_http_client_handle_t client; int64_t deadline; } http_reader_t;
static int read_http(void *context,char *buffer,size_t length) { return esp_http_client_read(((http_reader_t *)context)->client,buffer,(int)length); }
static bool complete_http(void *context) { return esp_http_client_is_complete_data_received(((http_reader_t *)context)->client); }
static bool expired_http(void *context) { return esp_timer_get_time()>=((http_reader_t *)context)->deadline; }

void weather_service_get_snapshot(weather_snapshot_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&data_lock); *out=snapshot; portEXIT_CRITICAL(&data_lock);
}

static void set_state(weather_state_t state, const char *message, const weather_data_t *data)
{
    portENTER_CRITICAL(&data_lock);
    snapshot.state=state;
    snprintf(snapshot.message,sizeof(snapshot.message),"%s",message);
    if (data) { snapshot.data=*data; snapshot.cached=true; ++snapshot.generation; }
    portEXIT_CRITICAL(&data_lock);
}

static bool fetch_weather(const char *url)
{
    char *body=calloc(1,BODY_LIMIT+1);
    if (!body) { set_state(WEATHER_ERROR,"Not enough memory. Will retry.",NULL); return false; }
    size_t length=0;
    int64_t started=esp_timer_get_time();
    int64_t deadline=started+20000000;
    const esp_http_client_config_t config={
        .url=url,.timeout_ms=10000,.buffer_size=1024,.buffer_size_tx=1024,
        .disable_auto_redirect=true,
        .user_agent="Waveshare-S3-Weather/1.0"
    };
    esp_http_client_handle_t client=esp_http_client_init(&config);
    bool success=false;
    if (!client) { set_state(WEATHER_ERROR,"Weather client unavailable.",NULL); free(body); return false; }
    ESP_LOGI(TAG,"Fetching weather (connect/read timeout: 10 s)");
    const char *stage="connect";
    esp_err_t result=esp_http_client_open(client,0);
    if (result==ESP_OK) {
        stage="headers";
        if (esp_http_client_fetch_headers(client)<0) result=ESP_FAIL;
    }
    int status=esp_http_client_get_status_code(client);
    http_reader_t context={.client=client,.deadline=deadline};
    const weather_body_reader_t reader={.context=&context,.read=read_http,.complete=complete_http,.expired=expired_http};
    if (result==ESP_OK && status==200) {
        stage="body";
        if (weather_body_read(&reader,body,BODY_LIMIT+1,&length)!=WEATHER_BODY_OK) result=ESP_FAIL;
    }
    if (result==ESP_OK && status==200 && esp_http_client_is_complete_data_received(client)) {
        weather_data_t data;
        weather_parse_result_t parsed=weather_parse_json(body,length,CONFIG_S35_WEATHER_CITY_LABEL,&data);
        if (parsed==WEATHER_PARSE_OK) {
            set_state(WEATHER_READY,"",&data); success=true;
            ESP_LOGI(TAG,"Weather updated: %s, %s C",data.weather,data.temp);
        } else {
            set_state(WEATHER_ERROR,parsed==WEATHER_PARSE_API_ERROR?"Weather API error. Will retry.":"Invalid weather data. Will retry.",NULL);
        }
    } else {
        char message[64];
        if (status>0 && status!=200)
            snprintf(message,sizeof(message),"Weather server HTTP %d. Will retry.",status);
        else
            snprintf(message,sizeof(message),"Weather %s failed. Will retry.",stage);
        set_state(WEATHER_ERROR,message,NULL);
        int socket_error=esp_http_client_get_errno(client);
        esp_err_t transport_error=esp_http_client_get_and_clear_last_tls_error(client,NULL,NULL);
        ESP_LOGW(TAG,"Request failed: stage=%s, result=%s, transport=%s, errno=%d, HTTP=%d, bytes=%u, elapsed=%lld ms",
                 stage,esp_err_to_name(result),esp_err_to_name(transport_error),socket_error,status,
                 (unsigned)length,(long long)((esp_timer_get_time()-started)/1000));
    }
    esp_http_client_cleanup(client);
    free(body);
    return success;
}

static void weather_worker(void *arg)
{
    (void)arg;
    char url[1024];
    if (!weather_build_url(url,sizeof(url),CONFIG_S35_WEATHER_API_ID,CONFIG_S35_WEATHER_API_KEY,
                           CONFIG_S35_WEATHER_PROVINCE,CONFIG_S35_WEATHER_CITY)) {
        set_state(WEATHER_ERROR,"Set weather API details in menuconfig.",NULL);
        /* Keep the task alive; the UI can safely notify it until reboot. */
        for (;;) ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
    }
    bool was_connected=false;
    int64_t next_fetch=0;
    for (;;) {
        wifi_manager_snapshot_t network;
        wifi_manager_get_snapshot(&network);
        int64_t now=esp_timer_get_time();
        if (!network.connected) {
            set_state(WEATHER_WAIT_NETWORK,"Connect in Settings > Wi-Fi setup.",NULL);
            was_connected=false;
        } else {
            if (!was_connected) next_fetch=0;
            was_connected=true;
            if (now>=next_fetch) {
                set_state(WEATHER_LOADING,"Updating weather...",NULL);
                bool success=fetch_weather(url);
                next_fetch=esp_timer_get_time()+(success?REFRESH_US:RETRY_US);
            }
        }
        if (ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(500))) next_fetch=0;
    }
}

bool weather_service_start(void)
{
    if (worker) return true;
    portENTER_CRITICAL(&data_lock);
    snprintf(snapshot.data.city,sizeof(snapshot.data.city),"%s",CONFIG_S35_WEATHER_CITY_LABEL);
    portEXIT_CRITICAL(&data_lock);
    set_state(WEATHER_WAIT_NETWORK,"Waiting for Wi-Fi...",NULL);
    return xTaskCreate(weather_worker,"s35_weather",6144,NULL,3,&worker)==pdPASS;
}

void weather_service_request_refresh(void)
{
    if (worker) xTaskNotifyGive(worker);
}
