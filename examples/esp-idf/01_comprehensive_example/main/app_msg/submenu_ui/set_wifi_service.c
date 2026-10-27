#include "set_wifi_service.h"
#include <stdio.h>  

#include "esp_log.h"
#include "esp_wifi.h"

#include "set_lv_style.h"

static lv_obj_t *return_screen = NULL;
static lv_obj_t *wifi_screen = NULL;

static lv_obj_t *list1 = NULL;
static lv_obj_t *wifi_cfg_con  = NULL; 

static lv_obj_t *back_btn = NULL;     
static lv_obj_t *scan_btn  = NULL; 
static lv_obj_t * spinner  = NULL; 


static const char *TAG = "wifi_scan";
static EventGroupHandle_t s_wifi_event_group;
static void wifi_scan_ui_task(void *arg);

#define WIFI_STARE_SCAN BIT0

static void back_btn_click_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {

        if(return_screen) {
            lv_screen_load(return_screen);
        }
    }
}


static void scan_wifi_btn_click_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        lv_obj_clean(list1);
        xEventGroupSetBits(s_wifi_event_group, WIFI_STARE_SCAN);
        lv_obj_remove_flag(spinner,LV_OBJ_FLAG_HIDDEN);
    }
}

static void event_handler(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi started");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected from AP");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}


void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();


    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    xTaskCreate(wifi_scan_ui_task, "wifi_scan_ui_task", 4096, NULL, 5, NULL);
}

static void do_wifi_scan(void)
{
    ESP_LOGI(TAG, "Starting Wi-Fi scan...");

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
    };

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true)); // true = 阻塞直到扫描完成

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    ESP_LOGI(TAG, "Found %d networks", ap_count);

    wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (ap_list) {
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_list));
        for (int i = 0; i < ap_count; i++) {
            ESP_LOGI(TAG, "%2d: SSID=%-32s  RSSI=%d  CH=%d",
                     i + 1,
                     (char *)ap_list[i].ssid,
                     ap_list[i].rssi,
                     ap_list[i].primary);
        }

        bsp_display_lock(-1);
        
        for (int i = 0; i < ap_count; i++) 
        {
            char label[64];
            snprintf(label, sizeof(label), "%s  (%d dBm)",
                        (char *)ap_list[i].ssid, ap_list[i].rssi);


            lv_obj_t *btn = lv_list_add_button(list1, LV_SYMBOL_WIFI, label);

        }
        lv_obj_add_flag(spinner,LV_OBJ_FLAG_HIDDEN);
        bsp_display_unlock();

        free(ap_list);
    }
}


static void wifi_scan_ui_task(void *pvParameters)
{
    while (1) 
    {

        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                               WIFI_STARE_SCAN,
                                               pdTRUE, pdFALSE,
                                               portMAX_DELAY);
        if (bits & WIFI_STARE_SCAN) 
        {
            do_wifi_scan();
        }
    
    }
}


void init_wifi_service_screen(lv_obj_t *parent, lv_obj_t *ret_scr)
{
    return_screen = ret_scr;
    set_screen_styles_t *styles = get_set_screen_styles();

    wifi_screen = lv_obj_create(parent);
    lv_obj_set_size(wifi_screen, 360, 280);
    lv_obj_set_style_bg_color(wifi_screen, lv_color_hex(0xFAFAFA), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(wifi_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(wifi_screen, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(wifi_screen, 16, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(wifi_screen, 4, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(wifi_screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(wifi_screen, LV_OPA_10, LV_PART_MAIN);
    lv_obj_set_style_shadow_offset_y(wifi_screen, 4, LV_PART_MAIN);
    lv_obj_center(wifi_screen);

    back_btn = lv_button_create(wifi_screen);
    lv_obj_add_style(back_btn, &styles->back_btn, LV_PART_MAIN);
    lv_obj_add_style(back_btn, &styles->back_btn_pressed, LV_STATE_PRESSED);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 45, 0);  
    lv_obj_add_event_cb(back_btn, back_btn_click_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_icon = lv_label_create(back_btn);
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_center(back_icon);

    lv_obj_t *title_label = lv_label_create(wifi_screen);
    lv_label_set_text(title_label, "WIFI");  
    lv_obj_set_style_text_color(title_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_22, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);

    scan_btn = lv_button_create(wifi_screen);
    lv_obj_add_style(scan_btn, &styles->back_btn, LV_PART_MAIN);
    lv_obj_add_style(scan_btn, &styles->back_btn_pressed, LV_STATE_PRESSED);
    lv_obj_align(scan_btn, LV_ALIGN_TOP_RIGHT, -45, 0);  
    lv_obj_add_event_cb(scan_btn, scan_wifi_btn_click_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *scan_icon = lv_label_create(scan_btn);
    lv_label_set_text(scan_icon, LV_SYMBOL_REFRESH);
    lv_obj_center(scan_icon);

    spinner = lv_spinner_create(parent);
    lv_obj_set_size(spinner, 100, 100);
    lv_obj_center(spinner);
    lv_spinner_set_anim_params(spinner, 1000, 200);
    lv_obj_add_flag(spinner,LV_OBJ_FLAG_HIDDEN);

    list1 = lv_list_create(wifi_screen);
    lv_obj_add_style(list1, &styles->list, LV_PART_MAIN);
    lv_obj_set_size(list1, 340, 240);  
    lv_obj_align(list1,LV_ALIGN_CENTER,0,30);
    lv_obj_set_style_pad_top(list1, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(list1, 0, LV_PART_MAIN);
}

