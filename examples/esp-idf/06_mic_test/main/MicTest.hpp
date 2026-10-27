/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "bsp/esp-bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <atomic>

namespace audio_demo {
class MicTest final {
public:
    MicTest() = default;
    ~MicTest();
    bool init() { return true; }
    bool run();
    bool close();
    bool deinit() { return close(); }
    bool pause();
    bool resume();
private:
    enum class State { Starting, Live, Recording, Playing, Error };
    enum class Command { None, Record, Play1, Play2, Stop };
    struct View {
        State state = State::Starting;
        int db[2] = {-60, -60};
        bool clipped[2] = {};
        uint32_t recorded = 0, position = 0;
        int channel = 0;
        char detail[80] = "Starting audio...";
    };
    struct Meter { lv_obj_t *bar = nullptr, *value = nullptr, *result = nullptr; };
    static void taskEntry(void *context);
    static void poll(lv_timer_t *timer);
    static void recordEvent(lv_event_t *event);
    static void playEvent(lv_event_t *event);
    bool startWorker();
    void stopWorker();
    void work();
    bool play(View &view, int channel, int16_t *buffer);
    void publish(const View &view);
    View snapshot();
    void error(View &view, const char *message, esp_err_t code);
    void refresh();
    Meter createMeter(const char *name, int y);
    portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
    View view_;
    std::atomic<bool> stopping_{false};
    std::atomic<Command> command_{Command::None};
    TaskHandle_t task_ = nullptr;
    SemaphoreHandle_t done_ = nullptr;
    int16_t *recording_ = nullptr;
    uint32_t recorded_ = 0;
    lv_obj_t *root_ = nullptr, *status_ = nullptr, *record_ = nullptr, *record_caption_ = nullptr;
    lv_obj_t *play_[2] = {}, *time_ = nullptr, *progress_ = nullptr, *volume_ = nullptr;
    Meter meters_[2];
    lv_timer_t *timer_ = nullptr;
};
}
