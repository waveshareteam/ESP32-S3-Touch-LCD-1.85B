#include "Display_ST77916.h"
#include "LVGL_Driver.h"
#include "I2C_Driver.h"
#include <Wire.h>
#include <BQ27220.h>

typedef struct {
    lv_obj_t *soc_label;
    lv_obj_t *vol_label;
    lv_obj_t *curr_label;
    lv_obj_t *temp_label;
} battery_ui_t;

SemaphoreHandle_t wire_mutex;
BQ27220 gauge;
static battery_ui_t bat_ui;
static lv_timer_t *bat_timer = NULL;


static void battery_timer_cb(lv_timer_t *timer)
{
    int soc;
    int mv;
    int ma;
    float tC;
    char buf[32];
    if (xSemaphoreTake(wire_mutex, pdMS_TO_TICKS(20))) {

        soc = gauge.readStateOfChargePercent();
        mv = gauge.readVoltageMillivolts();
        ma = gauge.readCurrentMilliamps();
        tC = gauge.readTemperatureCelsius();
        xSemaphoreGive(wire_mutex); 

        Serial.printf("SOC= %d%%  V= %d mV  I= %d mA  T= %.1f C\r\n", soc, mv, ma, tC);
    } else {
        Serial.println("I2C Bus Busy...");
        return; 
    }

    lv_label_set_text_fmt(bat_ui.soc_label,  "SOC: %d %%",    soc);
    lv_label_set_text_fmt(bat_ui.vol_label,  "Voltage: %d mV", mv);
    lv_label_set_text_fmt(bat_ui.curr_label, "Current: %d mA", ma);
    sprintf(buf, "Temp: %.1f C", tC);
    lv_label_set_text(bat_ui.temp_label, buf);
}
void battery_screen_create(void)
{
  lv_obj_t *scr = lv_scr_act(); 
  lv_obj_set_size(scr, 360, 360);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_set_style_clip_corner(scr, 0, 0);

  lv_obj_t *panel = lv_obj_create(scr);
  lv_obj_set_size(panel, 340, 320);
  lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(panel, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_radius(panel, 20, 0);
  lv_obj_set_style_border_width(panel, 0, 0);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(panel, 20, 0);
  lv_obj_set_style_pad_top(panel, 40, 0);

  bat_ui.soc_label  = lv_label_create(panel);
  bat_ui.vol_label  = lv_label_create(panel);
  bat_ui.curr_label = lv_label_create(panel);
  bat_ui.temp_label = lv_label_create(panel);

  lv_obj_set_style_text_font(bat_ui.soc_label,  &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_font(bat_ui.vol_label,  &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_font(bat_ui.curr_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_font(bat_ui.temp_label, &lv_font_montserrat_16, 0);

  lv_label_set_text(bat_ui.soc_label,  "SOC: -- %");
  lv_label_set_text(bat_ui.vol_label,  "Voltage: -- mV");
  lv_label_set_text(bat_ui.curr_label, "Current: -- mA");
  lv_label_set_text(bat_ui.temp_label, "Temp: -- C");

  if (bat_timer == NULL) {
      bat_timer = lv_timer_create(battery_timer_cb, 1000, NULL);
  }
}

void setup() {

  Serial.begin(115200);
  wire_mutex = xSemaphoreCreateMutex();
  
  if (!gauge.begin(Wire, 0x55, I2C_SDA_PIN, I2C_SCL_PIN, 400000)) {
    Serial.println("BQ27220 not found");
    while (1) delay(1000);
  }

  Serial.println("BQ27220 ready");

  I2C_Init();
  Backlight_Init();
  LCD_Init();
  Lvgl_Init();

  battery_screen_create();
  
}

void loop() {
  Lvgl_Loop();
  vTaskDelay(pdMS_TO_TICKS(10));
}
