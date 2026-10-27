#include "set_bat_msg.h"
#include "esp_log.h"
#include "set_lv_style.h"

#include "bq27220.h"   
#include "i2c_bus.h"

static const char *TAG = "set_bat_msg";


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

static battery_ui_t bat_ui;
static lv_obj_t *return_screen = NULL;
static lv_obj_t *bat_screen = NULL;
static lv_obj_t *back_btn = NULL;
static lv_timer_t *bat_msg_timer = NULL;


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

static void update_bat_ui(void)
{
    if (!bq27220) {
        return;
    }

    uint16_t soc = bq27220_get_state_of_charge(bq27220);
    uint16_t vol = bq27220_get_voltage(bq27220);
    int16_t current = bq27220_get_current(bq27220);
    uint16_t rc = bq27220_get_remaining_capacity(bq27220);
    uint16_t temp = bq27220_get_temperature(bq27220) / 10 - 273;
    uint16_t tte = bq27220_get_time_to_empty(bq27220);


    battery_status_t status = {};
    bq27220_get_battery_status(bq27220, &status);

    lv_label_set_text_fmt(bat_ui.soc_label,  "SOC: %d %%", soc);
    lv_label_set_text_fmt(bat_ui.vol_label,  "Voltage: %d mV", vol);
    lv_label_set_text_fmt(bat_ui.curr_label, "Current: %d mA", current);
    lv_label_set_text_fmt(bat_ui.cap_label,  "Capacity: %d mAh", rc);
    lv_label_set_text_fmt(bat_ui.temp_label, "Temp: %d C", temp);

    if (current < 0) {
        lv_label_set_text_fmt(bat_ui.time_label, "Time to Empty: %d min", tte);
    } else {
        uint16_t ttf = bq27220_get_time_to_full(bq27220);
        lv_label_set_text_fmt(bat_ui.time_label, "Time to Full: %d min", ttf);
    }

    lv_label_set_text_fmt(bat_ui.status1_label,
                          "DSG:%d SYSDWN:%d TDA:%d BATTPRES:%d",
                          status.DSG, status.SYSDWN, status.TDA, status.BATTPRES);

    lv_label_set_text_fmt(bat_ui.status2_label,
                          "AUTH:%d OCVGD:%d TCA:%d CHGINH:%d",
                          status.AUTH_GD, status.OCVGD, status.TCA, status.CHGINH);

}


static void update_timer_cb(lv_timer_t *t)
{
    (void)t;
    update_bat_ui();
}

static void back_btn_click_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        if (bat_msg_timer) {
            lv_timer_del(bat_msg_timer);
            bat_msg_timer = NULL;
        }

        if (return_screen) {
            lv_scr_load(return_screen);
        }
    }
}


static void bat_msg_screen_load_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SCREEN_LOADED) {
        if (bat_msg_timer == NULL) {
            bat_msg_timer = lv_timer_create(update_timer_cb, 1000, NULL);
        }
    }
}

static void bat_msg_screen_unload_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SCREEN_UNLOADED) {
        if (bat_msg_timer) {
            lv_timer_del(bat_msg_timer);
            bat_msg_timer = NULL;
        }
    }
}

void init_bat_settings_screen(lv_obj_t *parent, lv_obj_t *ret_scr)
{
    return_screen = ret_scr;

    lv_obj_add_event_cb(parent, bat_msg_screen_load_event_cb,
                        LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(parent, bat_msg_screen_unload_event_cb,
                        LV_EVENT_SCREEN_UNLOADED, NULL);

    lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);

    set_screen_styles_t *styles = get_set_screen_styles();

    bat_screen = lv_obj_create(parent);
    lv_obj_set_size(bat_screen, 360, 280);
    lv_obj_set_style_bg_color(bat_screen, lv_color_hex(0xFAFAFA), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bat_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(bat_screen, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bat_screen, 16, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(bat_screen, 4, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(bat_screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(bat_screen, LV_OPA_10, LV_PART_MAIN);
    lv_obj_set_style_shadow_offset_y(bat_screen, 4, LV_PART_MAIN);
    lv_obj_center(bat_screen);
    lv_obj_set_scrollbar_mode(bat_screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(bat_screen, LV_OBJ_FLAG_SCROLLABLE);

    back_btn = lv_button_create(bat_screen);
    lv_obj_add_style(back_btn, &styles->back_btn, LV_PART_MAIN);
    lv_obj_add_style(back_btn, &styles->back_btn_pressed, LV_STATE_PRESSED);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 45, 0);
    lv_obj_add_event_cb(back_btn, back_btn_click_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_icon = lv_label_create(back_btn);
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_center(back_icon);

    lv_obj_t *con = lv_obj_create(bat_screen);
    lv_obj_set_size(con, 360, 200);
    lv_obj_align(con, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(con, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(con,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    bat_ui.soc_label    = lv_label_create(con);
    bat_ui.vol_label    = lv_label_create(con);
    bat_ui.curr_label   = lv_label_create(con);
    bat_ui.cap_label    = lv_label_create(con);
    bat_ui.temp_label   = lv_label_create(con);
    bat_ui.time_label   = lv_label_create(con);
    bat_ui.status1_label = lv_label_create(con);
    bat_ui.status2_label = lv_label_create(con);

    lv_obj_set_style_text_font(bat_ui.soc_label, &lv_font_montserrat_22, 0);

    lv_label_set_text(bat_ui.soc_label,    "SOC: --%");
    lv_label_set_text(bat_ui.vol_label,    "Voltage: -- mV");
    lv_label_set_text(bat_ui.curr_label,   "Current: -- mA");
    lv_label_set_text(bat_ui.cap_label,    "Capacity: -- mAh");
    lv_label_set_text(bat_ui.temp_label,   "Temp: -- C");
    lv_label_set_text(bat_ui.time_label,   "Time: --");
    lv_label_set_text(bat_ui.status1_label,"Status1: --");
    lv_label_set_text(bat_ui.status2_label,"Status2: --");


}