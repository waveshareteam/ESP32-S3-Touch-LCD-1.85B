#include "Display_ST77916.h"
#include "LVGL_Driver.h"
#include "I2C_Driver.h"

void setup() {

  I2C_Init();
  Backlight_Init();
  LCD_Init();
  Lvgl_Init();

   lv_demo_widgets();
  // lv_demo_benchmark();
  // lv_demo_keypad_encoder();
  // lv_demo_music();
  // lv_demo_stress();
  

}

void loop() {
  Lvgl_Loop();
  vTaskDelay(pdMS_TO_TICKS(5));

}
