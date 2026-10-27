#include <Arduino.h>
#include "ESP_I2S.h"
#include <Wire.h>
#include "esp_check.h"
#include "es8311.h"
#include "music.h"

#define EXAMPLE_I2C_MASTER_SDA GPIO_NUM_11  // SDA pin
#define EXAMPLE_I2C_MASTER_SCL GPIO_NUM_10  // SCL pin

#define I2S_NUM I2S_NUM_0
#define I2S_MCK_PIN GPIO_NUM_2   
#define I2S_BCK_PIN GPIO_NUM_48   
#define I2S_LRCK_PIN GPIO_NUM_38
#define I2S_DOUT_PIN GPIO_NUM_47
#define I2S_DIN_PIN GPIO_NUM_39
 

#define EXAMPLE_SAMPLE_RATE (24000)
#define EXAMPLE_MCLK_MULTIPLE (256)  // If not using 24-bit data width, 256 should be enough
#define EXAMPLE_MCLK_FREQ_HZ (EXAMPLE_SAMPLE_RATE * EXAMPLE_MCLK_MULTIPLE)
#define EXAMPLE_VOICE_VOLUME (60)

I2SClass i2s;

void setupI2S() {
  i2s.setPins(I2S_BCK_PIN, I2S_LRCK_PIN, I2S_DOUT_PIN, I2S_DIN_PIN, I2S_MCK_PIN);
  // Initialize the I2S bus in standard mode
  if (!i2s.begin(I2S_MODE_STD, EXAMPLE_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    Serial.println("Failed to initialize I2S bus!");
    return;
  }
}

static esp_err_t es8311_codec_init(void) {
  es8311_handle_t es_handle = es8311_create(I2C_NUM_0, ES8311_ADDRRES_0);
  ESP_RETURN_ON_FALSE(es_handle, ESP_FAIL, TAG, "es8311 create failed");
  const es8311_clock_config_t es_clk = {
    .mclk_inverted = false,
    .sclk_inverted = false,
    .mclk_from_mclk_pin = true,
    .mclk_frequency = EXAMPLE_MCLK_FREQ_HZ,
    .sample_frequency = EXAMPLE_SAMPLE_RATE
  };

  ESP_ERROR_CHECK(es8311_init(es_handle, &es_clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16));
  ESP_RETURN_ON_ERROR(es8311_voice_volume_set(es_handle, EXAMPLE_VOICE_VOLUME, NULL), TAG, "set es8311 volume failed");
  ESP_RETURN_ON_ERROR(es8311_microphone_config(es_handle, false), TAG, "set es8311 microphone failed");

  return ESP_OK;
}

void setup() {

  Serial.begin(115200);
  Wire.begin( EXAMPLE_I2C_MASTER_SDA, EXAMPLE_I2C_MASTER_SCL,100000); 



  es8311_codec_init();

  setupI2S();
  Serial.println("I2S Initialized");

  pinMode(9, OUTPUT);     
  digitalWrite(9, HIGH); 
}

void loop() {
  i2s.write((uint8_t *)audio_data, AUDIO_SAMPLES * 2);
}
