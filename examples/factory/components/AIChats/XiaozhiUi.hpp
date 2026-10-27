/* SPDX-FileCopyrightText: 2026 Waveshare
 * SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "lvgl.h"

namespace esp_brookesia::apps {
class XiaozhiUi {
public:
    using ActionCallback = void (*)(void *);
    ~XiaozhiUi() { destroy(); }
    bool preload() { return true; }
    bool create(lv_obj_t *parent, ActionCallback action, void *context, bool multiline = true);
    void destroy();
    void setBackCallback(ActionCallback callback) { _back_callback = callback; }
    void setNetworkReady(bool ready);
    void setStatus(const char *status);
    void setAction(const char *label, bool enabled);
    void setChatMessage(const char *role, const char *text);
    void clearChatMessages();
    void setEmotion(const char *emotion);
    void setActivation(const char *code, const char *message, bool visible);
    lv_obj_t *root() const { return _root; }
private:
    lv_obj_t *_root = nullptr, *_network = nullptr, *_status = nullptr;
    lv_obj_t *_face = nullptr, *_eyes[2] = {}, *_mouth = nullptr, *_smile = nullptr;
    lv_obj_t *_action = nullptr, *_action_label = nullptr;
    lv_obj_t *_chat_panel = nullptr, *_chat_scroll = nullptr, *_role = nullptr, *_chat = nullptr;
    lv_obj_t *_activation = nullptr, *_code = nullptr, *_instruction = nullptr;
    ActionCallback _action_callback = nullptr, _back_callback = nullptr;
    void *_context = nullptr;
    char _emotion[32] = {};
    static void onAction(lv_event_t *event);
    static void onBack(lv_event_t *event);
};
}
