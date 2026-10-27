#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "bsp/esp-bsp.h"
#include "bq27220.h"   

static char *TAG = "app main";

static const parameter_cedv_t default_cedv = {
    .full_charge_cap = 500,
    .design_cap = 500,
    .reserve_cap = 0,
    .near_full = 200,
    .self_discharge_rate = 20,
    .EDV0 = 3490,
    .EDV1 = 3511,
    .EDV2 = 3535,
    .EMF = 3670,
    .C0 = 115,
    .R0 = 968,
    .T0 = 4547,
    .R1 = 4764,
    .TC = 11,
    .C1 = 0,
    .DOD0 = 4147,
    .DOD10 = 4002,
    .DOD20 = 3969,
    .DOD30 = 3938,
    .DOD40 = 3880,
    .DOD50 = 3824,
    .DOD60 = 3794,
    .DOD70 = 3753,
    .DOD80 = 3677,
    .DOD90 = 3574,
    .DOD100 = 3490,
};

static const gauging_config_t default_config = {
    .CCT = 1,
    .CSYNC = 0,
    .EDV_CMP = 0,
    .SC = 1,
    .FIXED_EDV0 = 0,
    .FCC_LIM = 1,
    .FC_FOR_VDQ = 1,
    .IGNORE_SD = 1,
    .SME0 = 0,
};

typedef struct {
    lv_obj_t *soc_label;
    lv_obj_t *vol_label;
    lv_obj_t *curr_label;
    lv_obj_t *cap_label;
    lv_obj_t *temp_label;
    lv_obj_t *time_label;
    lv_obj_t *status1_label;
    lv_obj_t *status2_label;
} battery_ui_t;

static bq27220_handle_t bq27220 = NULL;
static i2c_bus_handle_t i2c_bus = NULL;

bq27220_handle_t bq27220_drv_init(void)
{

    i2c_bus = bsp_i2c_bus_get_handle();

    bq27220_config_t bq27220_cfg = {
        .i2c_bus = i2c_bus,
        .cfg = &default_config,
        .cedv = &default_cedv,
    };
    bq27220 = bq27220_create(&bq27220_cfg); 

    if (!bq27220) {
        ESP_LOGE(TAG, "bq27220 create failed");
    }
    return bq27220;
}

static void test_bq27220_print_info(bq27220_handle_t bq27220Handle)
{
    battery_status_t status = {};
    bq27220_get_battery_status(bq27220Handle, &status);

    ESP_LOGI(TAG, "Battery Status1 - DSG:%d SYSDWN:%d TDA:%d BATTPRES:%d AUTH_GD:%d OCVGD:%d TCA:%d RSVD:%d",
             status.DSG, status.SYSDWN, status.TDA, status.BATTPRES,
             status.AUTH_GD, status.OCVGD, status.TCA, status.RSVD);

    ESP_LOGI(TAG, "Battery Status2 - CHGINH:%d FC:%d OTD:%d OTC:%d SLEEP:%d OCVFAIL:%d OCVCOMP:%d FD:%d",
             status.CHGINH, status.FC, status.OTD, status.OTC,
             status.SLEEP, status.OCVFAIL, status.OCVCOMP, status.FD);

    uint16_t vol = bq27220_get_voltage(bq27220Handle);
    int16_t current = bq27220_get_current(bq27220Handle);
    uint16_t rc = bq27220_get_remaining_capacity(bq27220Handle);
    uint16_t full_cap = bq27220_get_full_charge_capacity(bq27220Handle);
    uint16_t temp = bq27220_get_temperature(bq27220Handle) / 10 - 273;
    uint16_t cycle_cnt = bq27220_get_cycle_count(bq27220Handle);
    uint16_t soc = bq27220_get_state_of_charge(bq27220Handle);
    int16_t avg_power = bq27220_get_average_power(bq27220Handle);
    int16_t max_load = bq27220_get_maxload_current(bq27220Handle);
    uint16_t time_to_empty = bq27220_get_time_to_empty(bq27220Handle);
    uint16_t time_to_full = bq27220_get_time_to_full(bq27220Handle);

    ESP_LOGI(TAG,
             "Battery Info - Vol:%dmV Cur:%dmA Pwr:%dmW RC:%dmAh FCC:%dmAh "
             "Temp:%dC Cycle:%d SOC:%d%% MaxLoad:%dmA TTE:%dmin TTF:%dmin",
             vol, current, avg_power, rc, full_cap, temp,
             cycle_cnt, soc, max_load, time_to_empty, time_to_full);
}

void app_main()
{
    bsp_i2c_init();
    bq27220 = bq27220_drv_init();

    while (1)
    {
       test_bq27220_print_info(bq27220);
       vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
}
    