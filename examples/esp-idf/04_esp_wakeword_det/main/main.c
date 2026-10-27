
#include "esp_netif_sntp.h"
#include "nvs_flash.h"
#include "bsp/esp-bsp.h"

#include "wake_word_det/wake_word_drv.h"

#define TAG   "main"

void app_main(void)
{
    wake_word_drv_init();
}


