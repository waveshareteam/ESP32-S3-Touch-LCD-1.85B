
#include "ESP_I2S.h"
#include "ESP_SR.h"
#include "es7210.h"
#include "esp_log.h"
#include "esp_check.h"
#include "Wire.h"

#define I2S_PIN_MCK GPIO_NUM_2
#define I2S_PIN_BCK GPIO_NUM_48
#define I2S_PIN_WS GPIO_NUM_38
#define I2S_PIN_DIN GPIO_NUM_39
#define I2S_PIN_DOUT GPIO_NUM_47
#define I2C_PIN_SDA GPIO_NUM_11
#define I2C_PIN_SCL GPIO_NUM_10

#define ES7210_I2C_ADDR (0x40)
#define ES7210_SAMPLE_RATE (16000)
#define ES7210_I2S_FORMAT ES7210_I2S_FMT_I2S
#define ES7210_MCLK_MULTIPLE (256)
#define ES7210_BIT_WIDTH ES7210_I2S_BITS_16B
#define ES7210_MIC_BIAS ES7210_MIC_BIAS_2V87
#define ES7210_MIC_GAIN ES7210_MIC_GAIN_30DB
#define ES7210_ADC_VOLUME (40)
static es7210_dev_handle_t es7210_handle = NULL;

I2SClass i2s;

// Generated using the following command:
// python3 tools/gen_sr_commands.py "Turn on the light,Switch on the light;Turn off the light,Switch off the light,Go dark;Start fan;Stop fan"
enum {
  SR_CMD_TURN_ON_THE_LIGHT,
  SR_CMD_TURN_OFF_THE_LIGHT,
  SR_CMD_START_FAN,
  SR_CMD_STOP_FAN,
};
static const sr_cmd_t sr_commands[] = {
  { 0, "Turn on the light", "TkN nN jc LiT" },
  { 0, "Switch on the light", "SWgp nN jc LiT" },
  { 1, "Turn off the light", "TkN eF jc LiT" },
  { 1, "Switch off the light", "SWgp eF jc LiT" },
  { 1, "Go dark", "Gb DnRK" },
  { 2, "Start fan", "STnRT FaN" },
  { 3, "Stop fan", "STnP FaN" },
};

void onSrEvent(sr_event_t event, int command_id, int phrase_id) {
  switch (event) {
    case SR_EVENT_WAKEWORD: Serial.println("WakeWord Detected!"); break;
    case SR_EVENT_WAKEWORD_CHANNEL:
      Serial.printf("WakeWord Channel %d Verified!\n", command_id);
      ESP_SR.setMode(SR_MODE_COMMAND);  // Switch to Command detection
      break;
    case SR_EVENT_TIMEOUT:
      Serial.println("Timeout Detected!");
      ESP_SR.setMode(SR_MODE_WAKEWORD);  // Switch back to WakeWord detection
      break;
    case SR_EVENT_COMMAND:
      Serial.printf("Command %d Detected! %s\n", command_id, sr_commands[phrase_id].str);
      // switch (command_id) {
      //   case SR_CMD_TURN_ON_THE_LIGHT: digitalWrite(LIGHT_PIN, HIGH); break;
      //   case SR_CMD_TURN_OFF_THE_LIGHT: digitalWrite(LIGHT_PIN, LOW); break;
      //   case SR_CMD_START_FAN: digitalWrite(FAN_PIN, HIGH); break;
      //   case SR_CMD_STOP_FAN: digitalWrite(FAN_PIN, LOW); break;
      //   default: Serial.println("Unknown Command!"); break;
      // }
      ESP_SR.setMode(SR_MODE_COMMAND);  // Allow for more commands to be given, before timeout
      // ESP_SR.setMode(SR_MODE_WAKEWORD); // Switch back to WakeWord detection
      break;
    default: Serial.println("Unknown Event!"); break;
  }
}

void es7210_init(void) {

  /* Create ES7210 device handle */
  es7210_i2c_config_t es7210_i2c_conf = {
    // .i2c_port = I2C_MASTER_NUM,
    .i2c_addr = ES7210_I2C_ADDR
  };
  ESP_ERROR_CHECK(es7210_new_codec(&es7210_i2c_conf, &es7210_handle));

  es7210_codec_config_t codec_conf = {};
  codec_conf.i2s_format = ES7210_I2S_FORMAT;
  codec_conf.mclk_ratio = ES7210_MCLK_MULTIPLE;
  codec_conf.sample_rate_hz = ES7210_SAMPLE_RATE;
  codec_conf.bit_width = ES7210_BIT_WIDTH;
  codec_conf.mic_bias = ES7210_MIC_BIAS;
  codec_conf.mic_gain = ES7210_MIC_GAIN;
  codec_conf.flags.tdm_enable = false;
  ESP_ERROR_CHECK(es7210_config_codec(es7210_handle, &codec_conf));
  ESP_ERROR_CHECK(es7210_config_volume(es7210_handle, ES7210_ADC_VOLUME));
}


void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_PIN_SDA, I2C_PIN_SCL);
  es7210_init();
  i2s.setPins(I2S_PIN_BCK, I2S_PIN_WS, I2S_PIN_DOUT, I2S_PIN_DIN, I2S_PIN_MCK);
  i2s.setTimeout(1000);
  i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);

  ESP_SR.onEvent(onSrEvent);
  ESP_SR.begin(i2s, sr_commands, sizeof(sr_commands) / sizeof(sr_cmd_t), SR_CHANNELS_STEREO, SR_MODE_WAKEWORD);
}

void loop() {}
