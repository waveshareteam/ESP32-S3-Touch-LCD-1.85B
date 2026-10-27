
#include "esp_netif_sntp.h"
#include "nvs_flash.h"
#include "bsp/esp-bsp.h"
#include "lv_demos.h"
#define TAG   "main"



void app_main(void)
{
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    lv_display_t *disp = bsp_display_start();
    lv_indev_t *tp = bsp_display_get_input_dev();
    bsp_display_backlight_on();

    bsp_display_lock(-1);
    lv_demo_widgets();
    bsp_display_unlock();
}


