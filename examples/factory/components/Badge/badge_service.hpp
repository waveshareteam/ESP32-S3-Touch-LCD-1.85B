#pragma once
#include "esp_http_server.h"
#include <cstdint>

namespace badge {
constexpr int SIZE=360, SLOTS=6;
constexpr unsigned PIXEL_BYTES=SIZE*SIZE*2, MAX_JPEG=256*1024;
struct State {
    bool ready=false;
    uint8_t available=0;
    int active=0;
    uint32_t revision=0;
    char error[96]={};
};
// Register the shared web provider before Wi-Fi startup. Storage and decoder
// operations run on internal-stack workers or HTTP handlers, never on LVGL.
bool prepare();
bool initialize();
bool snapshot(State &out);
bool load(int slot, uint16_t *pixels, char *error, unsigned capacity);
bool select(int slot);
bool next(int direction);
bool start_editor();
void stop_editor();
bool editor_finished();
}
