/* SPDX-FileCopyrightText: 2026 Waveshare
 * SPDX-License-Identifier: Apache-2.0 */
#include "MicTest.hpp"
#include "bsp_board_extra.h"
#include "bsp/esp-bsp.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

namespace {
constexpr uint32_t RATE = 16000, SECONDS = 5, MAX_FRAMES = RATE * SECONDS;
constexpr size_t BLOCK_FRAMES = 320, MIC_SLOTS[] = {0, 1};
constexpr size_t RECORD_BYTES = MAX_FRAMES * 2 * sizeof(int16_t);
constexpr uint32_t IO_TIMEOUT_MS = 100;
static_assert(BSP_AUDIO_VOICE_CHANNELS == 2);

lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int width, const lv_font_t *font)
{
    auto *obj = lv_label_create(parent);
    lv_obj_set_pos(obj, x, y); lv_obj_set_width(obj, width);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_black(), 0);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
    lv_label_set_text(obj, text);
    return obj;
}
void text(lv_obj_t *obj, const char *value)
{ if (std::strcmp(lv_label_get_text(obj), value)) lv_label_set_text(obj, value); }
void enable(lv_obj_t *obj, bool enabled)
{
    if (enabled) lv_obj_remove_state(obj, LV_STATE_DISABLED);
    else lv_obj_add_state(obj, LV_STATE_DISABLED);
}
lv_obj_t *button(lv_obj_t *parent, const char *caption, int x, int y, int width,
                 lv_event_cb_t callback, void *context)
{
    auto *obj = lv_button_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y); lv_obj_set_size(obj, width, 34);
    lv_obj_remove_flag(obj, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE));
    lv_obj_set_style_bg_color(obj, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, lv_color_black(), 0);
    lv_obj_set_style_radius(obj, 8, 0);
    lv_obj_set_style_text_font(obj, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(obj, lv_color_black(), 0);
    lv_obj_set_style_bg_color(obj, lv_color_black(), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(obj, lv_color_white(), LV_STATE_PRESSED);
    lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_BOTTOM, LV_STATE_DISABLED);
    auto *caption_obj = lv_label_create(obj);
    lv_label_set_text(caption_obj, caption); lv_obj_center(caption_obj);
    lv_obj_add_event_cb(obj, callback, LV_EVENT_CLICKED, context);
    return obj;
}
}

namespace audio_demo {
MicTest::~MicTest() { close(); }
void MicTest::publish(const View &view)
{
    portENTER_CRITICAL(&lock_); view_ = view; portEXIT_CRITICAL(&lock_);
}
MicTest::View MicTest::snapshot()
{
    portENTER_CRITICAL(&lock_); View result = view_; portEXIT_CRITICAL(&lock_); return result;
}
void MicTest::error(View &view, const char *message, esp_err_t code)
{
    if (view.state == State::Recording) recorded_ = view.recorded = 0;
    view.state = State::Error;
    view.db[0] = view.db[1] = -60;
    std::snprintf(view.detail, sizeof(view.detail), "%s: %s", message, esp_err_to_name(code));
    ESP_LOGE("mic_test", "%s", view.detail);
    publish(view);
}
MicTest::Meter MicTest::createMeter(const char *name, int y)
{
    Meter meter;
    label(root_, name, 66, y, 60, &lv_font_montserrat_18);
    meter.result = label(root_, "LIVE", 132, y + 3, 76, &lv_font_montserrat_12);
    meter.value = label(root_, "-- dBFS", 210, y + 3, 84, &lv_font_montserrat_14);
    lv_obj_set_style_text_align(meter.value, LV_TEXT_ALIGN_RIGHT, 0);
    meter.bar = lv_bar_create(root_);
    lv_obj_remove_style_all(meter.bar);
    lv_obj_set_pos(meter.bar, 66, y + 28); lv_obj_set_size(meter.bar, 228, 10);
    lv_obj_set_style_radius(meter.bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_radius(meter.bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(meter.bar, lv_color_hex(0xdce4f1), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(meter.bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(meter.bar, lv_color_hex(0x2878d8), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(meter.bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_remove_flag(meter.bar, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_bar_set_range(meter.bar, -60, 0); lv_bar_set_value(meter.bar, -60, LV_ANIM_OFF);
    return meter;
}
bool MicTest::run()
{
    close();
    root_ = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, 360, 360);
    lv_obj_set_style_bg_color(root_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    auto *title = label(root_, "Mic Test", 60, 48, 240, &lv_font_montserrat_18);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    meters_[0] = createMeter("MIC1", 92);
    meters_[1] = createMeter("MIC2", 144);
    status_ = label(root_, "Starting audio...", 66, 200, 228, &lv_font_montserrat_14);
    time_ = label(root_, "0.0 / 5.0 s", 76, 226, 132, &lv_font_montserrat_14);
    volume_ = label(root_, "", 210, 228, 74, &lv_font_montserrat_12);
    lv_obj_set_style_text_align(volume_, LV_TEXT_ALIGN_RIGHT, 0);
    progress_ = lv_bar_create(root_);
    lv_obj_remove_style_all(progress_);
    lv_obj_set_pos(progress_, 76, 252); lv_obj_set_size(progress_, 208, 5);
    lv_obj_set_style_bg_color(progress_, lv_color_hex(0xdce4f1), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(progress_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(progress_, lv_color_hex(0x2878d8), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(progress_, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_remove_flag(progress_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_bar_set_range(progress_, 0, 50);
    play_[0] = button(root_, LV_SYMBOL_PLAY " MIC1", 76, 268, 98, playEvent, this);
    play_[1] = button(root_, LV_SYMBOL_PLAY " MIC2", 186, 268, 98, playEvent, this);
    record_ = button(root_, LV_SYMBOL_BULLET " Record", 112, 310, 136, recordEvent, this);
    lv_obj_set_style_bg_color(record_, lv_color_hex(0x2878d8), 0);
    lv_obj_set_style_border_color(record_, lv_color_hex(0x2878d8), 0);
    lv_obj_set_style_text_color(record_, lv_color_white(), 0);
    record_caption_ = lv_obj_get_child(record_, 0);
    timer_ = lv_timer_create(poll, 100, this);
    if (!timer_) { close(); return false; }
    startWorker(); refresh();
    return true;
}
bool MicTest::startWorker()
{
    stopWorker();
    command_ = Command::None; stopping_ = false;
    View view; view.recorded = recorded_; publish(view);
    if (!recording_) recording_ = static_cast<int16_t *>(heap_caps_malloc(RECORD_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!recording_) { error(view, "Recording memory", ESP_ERR_NO_MEM); return false; }
    done_ = xSemaphoreCreateBinary();
    if (!done_) { error(view, "Audio semaphore", ESP_ERR_NO_MEM); return false; }
    if (xTaskCreateWithCaps(taskEntry, "mic_test", 6144, this, 4, &task_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        task_ = nullptr; vSemaphoreDelete(done_); done_ = nullptr;
        error(view, "Audio task", ESP_ERR_NO_MEM); return false;
    }
    return true;
}
void MicTest::stopWorker()
{
    if (task_) {
        stopping_ = true;
        // I/O uses bounded timeouts. The worker closes the codecs before signalling.
        xSemaphoreTake(done_, portMAX_DELAY);
        vTaskDeleteWithCaps(task_); task_ = nullptr;
    }
    if (done_) { vSemaphoreDelete(done_); done_ = nullptr; }
}
void MicTest::taskEntry(void *context)
{
    auto *app = static_cast<MicTest *>(context);
    app->work();
    xSemaphoreGive(app->done_);
    // The owner joins and deletes this PSRAM-stack task with vTaskDeleteWithCaps.
    while (true) vTaskSuspend(nullptr);
}
bool MicTest::play(View &view, int channel, int16_t *buffer)
{
    view.state = State::Playing; view.channel = channel; view.position = 0;
    view.db[0] = view.db[1] = -60;
    std::snprintf(view.detail, sizeof(view.detail), "Playing MIC%d", channel + 1); publish(view);
    esp_err_t ret = bsp_audio_voice_close();
    if (ret == ESP_OK) ret = bsp_audio_voice_open(RATE, false);
    if (ret == ESP_OK) ret = bsp_audio_voice_mute(false);
    if (ret != ESP_OK) { error(view, "Playback setup", ret); return false; }
    ESP_LOGI("mic_test", "Play MIC%d, %lu frames, volume=%d", channel + 1,
             static_cast<unsigned long>(recorded_), bsp_extra_codec_volume_get());
    bool cancelled = false;
    while (view.position < recorded_ && !stopping_) {
        if (command_.exchange(Command::None) == Command::Stop) { cancelled = true; break; }
        const size_t frames = std::min<size_t>(BLOCK_FRAMES, recorded_ - view.position);
        for (size_t i = 0; i < frames; ++i) {
            const int16_t sample = recording_[(view.position + i) * 2 + channel];
            buffer[i * 2] = buffer[i * 2 + 1] = sample;
        }
        size_t offset = 0, bytes = frames * 2 * sizeof(int16_t);
        unsigned stalled = 0;
        while (offset < bytes && !stopping_) {
            if (command_.exchange(Command::None) == Command::Stop) { cancelled = true; break; }
            size_t written = 0;
            ret = bsp_audio_voice_write(reinterpret_cast<uint8_t *>(buffer) + offset, bytes - offset, &written, IO_TIMEOUT_MS);
            if ((ret != ESP_OK && ret != ESP_ERR_TIMEOUT) || written % 4 || written > bytes - offset || (!written && ++stalled >= 5)) {
                error(view, "Playback write", ret == ESP_OK ? ESP_FAIL : ret); return false;
            }
            if (written) stalled = 0;
            offset += written;
        }
        if (cancelled || stopping_) break;
        view.position += frames; publish(view);
    }
    // Six 240-frame DMA buffers plus one descriptor at 16 kHz: allow the tail to play.
    if (!cancelled && !stopping_) vTaskDelay(pdMS_TO_TICKS(110));
    ret = bsp_audio_voice_close();
    if (ret != ESP_OK) { error(view, "Playback close", ret); return false; }
    if (stopping_) return true;
    ret = bsp_audio_voice_open(RATE, true);
    if (ret != ESP_OK) { error(view, "Microphone reopen", ret); return false; }
    return true;
}
void MicTest::work()
{
    View view = snapshot();
    auto *buffer = static_cast<int16_t *>(heap_caps_malloc(BLOCK_FRAMES * BSP_AUDIO_VOICE_CHANNELS * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!buffer) { error(view, "Audio buffer", ESP_ERR_NO_MEM); return; }
    bool owned = false;
    // Retry may recover a previous codec-close error without granting the bus to another APP.
    if (bsp_extra_audio_session_get_owner() == BSP_EXTRA_AUDIO_OWNER_RECORDER && bsp_audio_voice_close() == ESP_OK)
        bsp_extra_audio_session_release(BSP_EXTRA_AUDIO_OWNER_RECORDER);
    for (unsigned attempt = 0; attempt < 40 && !stopping_; ++attempt) {
        if (bsp_extra_audio_session_acquire(BSP_EXTRA_AUDIO_OWNER_RECORDER) == ESP_OK) { owned = true; break; }
        vTaskDelay(pdMS_TO_TICKS(25));
    }
    if (!owned) {
        if (!stopping_) {
            char message[48]; std::snprintf(message, sizeof(message), "Audio busy (%s)", bsp_extra_audio_owner_name(bsp_extra_audio_session_get_owner()));
            error(view, message, ESP_ERR_INVALID_STATE);
        }
        heap_caps_free(buffer); return;
    }
    esp_err_t ret = bsp_audio_voice_open(RATE, true);
    if (ret != ESP_OK) error(view, "Microphone setup", ret);
    uint32_t warmup = RATE / 5;
    unsigned failures = 0;
    ESP_LOGI("mic_test", "MIC1=slot0, MIC2=slot1 in two-input mode; no reference; 16 kHz, 16-bit, 5 s, 320000 bytes PSRAM");
    while (ret == ESP_OK && !stopping_) {
        const Command command = command_.exchange(Command::None);
        if (command == Command::Record && !warmup) {
            recorded_ = 0; view.recorded = view.position = 0;
            view.clipped[0] = view.clipped[1] = false;
            view.state = State::Recording;
            std::snprintf(view.detail, sizeof(view.detail), "Recording both microphones...");
            ESP_LOGI("mic_test", "Record MIC1 and MIC2");
        } else if (command == Command::Stop && view.state == State::Recording) {
            view.state = State::Live;
        } else if ((command == Command::Play1 || command == Command::Play2) && recorded_ && view.state != State::Recording) {
            if (!play(view, command == Command::Play1 ? 0 : 1, buffer)) break;
            warmup = RATE / 5; view.state = State::Starting;
            std::snprintf(view.detail, sizeof(view.detail), "Resuming live input..."); publish(view);
        }
        size_t received = 0;
        ret = bsp_audio_voice_read(buffer, BLOCK_FRAMES * BSP_AUDIO_VOICE_CHANNELS * sizeof(int16_t), &received, IO_TIMEOUT_MS);
        if (stopping_) break;
        if ((ret != ESP_OK && ret != ESP_ERR_TIMEOUT) || received % (BSP_AUDIO_VOICE_CHANNELS * sizeof(int16_t))) {
            error(view, "Microphone read", ret == ESP_OK ? ESP_FAIL : ret); break;
        }
        ret = ESP_OK;
        if (!received) {
            if (++failures >= 30) { error(view, "No microphone samples", ESP_ERR_TIMEOUT); break; }
            continue;
        }
        failures = 0;
        size_t frames = received / (BSP_AUDIO_VOICE_CHANNELS * sizeof(int16_t));
        if (warmup) {
            warmup -= std::min<uint32_t>(warmup, frames);
            continue;
        }
        if (view.state == State::Starting) view.state = State::Live;
        for (size_t mic = 0; mic < 2; ++mic) {
            uint64_t energy = 0;
            for (size_t i = 0; i < frames; ++i) {
                const int32_t sample = buffer[i * BSP_AUDIO_VOICE_CHANNELS + MIC_SLOTS[mic]];
                energy += static_cast<int64_t>(sample) * sample;
                if (sample >= 32700 || sample <= -32700) view.clipped[mic] = true;
            }
            const double ratio = static_cast<double>(energy) / (frames * 32768.0 * 32768.0);
            const int db = ratio > 0 ? std::clamp(static_cast<int>(10 * std::log10(ratio)), -60, 0) : -60;
            // Immediate attack, bounded release keeps short sounds visible.
            view.db[mic] = std::max(db, view.db[mic] - 2);
        }
        if (view.state == State::Recording) {
            const size_t copy_frames = std::min<size_t>(frames, MAX_FRAMES - recorded_);
            for (size_t i = 0; i < copy_frames; ++i)
                for (size_t mic = 0; mic < 2; ++mic)
                    recording_[(recorded_ + i) * 2 + mic] = buffer[i * BSP_AUDIO_VOICE_CHANNELS + MIC_SLOTS[mic]];
            recorded_ += copy_frames; view.recorded = view.position = recorded_;
            if (recorded_ == MAX_FRAMES) {
                view.state = State::Live;
                ESP_LOGI("mic_test", "Recorded %lu frames per microphone", static_cast<unsigned long>(recorded_));
            }
        }
        if (view.state == State::Live)
            std::snprintf(view.detail, sizeof(view.detail), "%s", recorded_ ? "Recording ready" : "Live input");
        publish(view);
    }
    if (view.state == State::Recording) { recorded_ = 0; view.recorded = 0; }
    const esp_err_t close_ret = bsp_audio_voice_close();
    if (close_ret == ESP_OK) bsp_extra_audio_session_release(BSP_EXTRA_AUDIO_OWNER_RECORDER);
    else error(view, "Audio close", close_ret);
    heap_caps_free(buffer);
}
void MicTest::refresh()
{
    const View view = snapshot();
    char volume[20]; std::snprintf(volume, sizeof(volume), "Vol %d%%", bsp_extra_codec_volume_get());
    text(volume_, volume);
    const bool busy = view.state == State::Recording || view.state == State::Playing;
    for (size_t mic = 0; mic < 2; ++mic) {
        if (lv_bar_get_value(meters_[mic].bar) != view.db[mic]) lv_bar_set_value(meters_[mic].bar, view.db[mic], LV_ANIM_OFF);
        char db[20]; std::snprintf(db, sizeof(db), "%d dBFS", view.db[mic]); text(meters_[mic].value, db);
        text(meters_[mic].result, view.state == State::Playing ? "PLAYBACK" : view.state == State::Starting ? "WAITING" :
             view.state == State::Error ? "UNAVAILABLE" : view.clipped[mic] ? "CLIPPED" : "LIVE");
        enable(play_[mic], !busy && view.state == State::Live && view.recorded > 0);
    }
    text(status_, view.detail);
    text(record_caption_, busy ? LV_SYMBOL_STOP " Stop" : view.state == State::Error ? LV_SYMBOL_REFRESH " Retry" : LV_SYMBOL_BULLET " Record");
    lv_obj_center(record_caption_);
    enable(record_, view.state != State::Starting);
    const uint32_t position = view.state == State::Playing ? view.position : view.recorded;
    const uint32_t total = view.state == State::Playing ? view.recorded : MAX_FRAMES;
    char time[40];
    std::snprintf(time, sizeof(time), "%lu.%lu / %lu.%lu s", static_cast<unsigned long>(position / RATE),
                  static_cast<unsigned long>((position % RATE) * 10 / RATE), static_cast<unsigned long>(total / RATE),
                  static_cast<unsigned long>((total % RATE) * 10 / RATE));
    text(time_, time);
    const int progress = total ? position * 50 / total : 0;
    if (lv_bar_get_value(progress_) != progress) lv_bar_set_value(progress_, progress, LV_ANIM_OFF);
}
void MicTest::recordEvent(lv_event_t *event)
{
    auto *app = static_cast<MicTest *>(lv_event_get_user_data(event));
    const View view = app->snapshot();
    if (view.state == State::Error) app->startWorker();
    else if (view.state == State::Live) app->command_ = Command::Record;
    else if (view.state == State::Recording || view.state == State::Playing) app->command_ = Command::Stop;
    app->refresh();
}
void MicTest::playEvent(lv_event_t *event)
{
    auto *app = static_cast<MicTest *>(lv_event_get_user_data(event));
    const View view = app->snapshot();
    if (view.state == State::Live && view.recorded)
        app->command_ = lv_event_get_target(event) == app->play_[0] ? Command::Play1 : Command::Play2;
}
void MicTest::poll(lv_timer_t *timer)
{ static_cast<MicTest *>(lv_timer_get_user_data(timer))->refresh(); }
bool MicTest::pause()
{
    if (timer_) lv_timer_pause(timer_);
    stopWorker();
    return true;
}
bool MicTest::resume()
{
    startWorker();
    if (timer_) { refresh(); lv_timer_resume(timer_); }
    return true;
}
bool MicTest::close()
{
    if (timer_) { lv_timer_delete(timer_); timer_ = nullptr; }
    stopWorker();
    if (root_) { lv_obj_delete(root_); root_ = nullptr; }
    if (recording_) { heap_caps_free(recording_); recording_ = nullptr; }
    recorded_ = 0;
    return true;
}
}
