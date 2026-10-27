#include <new>
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_brookesia.hpp"
#include "dark/stylesheet.hpp"
#include "Settings.hpp"
#include "XiaozhiApp.hpp"
#include "DrawingBoard.hpp"
#include "Photos.hpp"
#include "Weather.hpp"
#include "Music.hpp"
#include "Sensor.hpp"
#include "Battery.hpp"
#include "battery_service.hpp"
#include "esp_timer.h"
#include "Games.hpp"
#include "MicTest.hpp"
#include "ButtonTest.hpp"
#include "DisplayTest.hpp"
#include "VideoPlayer.hpp"
#include "chipmunk2d_game.hpp"
#include "wifi_manager.h"
#include "settings_service.h"

using namespace esp_brookesia;
using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems::phone;
using namespace esp_brookesia::apps;
static const char *TAG = "main";

static void refresh_desktop(Phone *phone)
{
    auto *status = phone->getDisplay().getStatusBar();
    struct tm now = {};
    if (status) {
        if (wifi_manager_get_local_time(&now)) status->setClock(now.tm_hour, now.tm_min);
        status->setWifiIconState(wifi_manager_is_connected() ? 2 : 0);
        const auto sample = battery_service_snapshot();
        const auto state = battery_state(sample, esp_timer_get_time());
        if (battery_has_values(state)) {
            status->setBatteryPercent(state == BatteryState::Charging, sample.data.percent);
            status->showBatteryPercent();
        } else {
            status->hideBatteryIcon();
            status->hideBatteryPercent();
        }
    }
    auto *recents = phone->getDisplay().getRecentsScreen();
    if (recents) recents->setMemoryLabel(
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024,
        heap_caps_get_total_size(MALLOC_CAP_INTERNAL) / 1024,
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024,
        heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / 1024);
}

static void update_desktop(lv_timer_t *timer)
{
    refresh_desktop(static_cast<Phone *>(lv_timer_get_user_data(timer)));
}

static bool app_start()
{
    LvLock::registerCallbacks([](int timeout_ms) {
        return bsp_display_lock(timeout_ms);
    }, []() { bsp_display_unlock(); return true; });
    LvLockGuard guard;
    auto *phone = new (std::nothrow) Phone(bsp_display_get());
    if (!phone) return false;
    auto *touch = bsp_display_get_input_dev();
    if (touch && !phone->setTouchDevice(touch)) return false;
    if (!phone->addStylesheet(STYLESHEET_360_360_DARK) ||
        !phone->activateStylesheet(STYLESHEET_360_360_DARK) || !phone->begin()) return false;
    // Hide the framework's default 100% until the first valid gauge sample.
    refresh_desktop(phone);

    struct Entry { const char *name; systems::phone::App *app; };
    const Entry apps[] = {
        {"Display Test", DisplayTest::requestInstance()},
        {"Settings", Settings::requestInstance()}, {"AIChats", XiaozhiApp::requestInstance()},
        {"Battery", Battery::requestInstance()},
        {"Touch Test", DrawingBoard::requestInstance()}, {"Photos", Photos::requestInstance()},
        {"Weather", Weather::requestInstance()}, {"Music", Music::requestInstance()},
        {"Sensor", Sensor::requestInstance()}, {"2048", Games::requestInstance()},
        {"Mic Test", MicTest::requestInstance()}, {"Button Test", ButtonTest::requestInstance()},
        {"Video", VideoPlayer::requestInstance()},
        {"Gravity", bsp_get_init_status()->imu == ESP_OK ? PhoneChipmunk2d::requestInstance() : nullptr},
    };
    unsigned installed = 0;
    for (const auto &entry : apps) {
        const int id = entry.app ? phone->installApp(entry.app) : -1;
        if (id >= 0) { ++installed; ESP_LOGI(TAG, "%s installed, ID=%d", entry.name, id); }
        else ESP_LOGW(TAG, "%s unavailable", entry.name);
    }
    if (!lv_timer_create(update_desktop, 1000, phone)) ESP_LOGW(TAG, "Desktop timer unavailable");
    ESP_LOGI(TAG, "1.85B round desktop ready: 360x360, 2x2 icons/page, %u/%u APPs", installed,
             unsigned(sizeof(apps) / sizeof(apps[0])));
    return installed > 0;
}

extern "C" void app_main(void)
{
    const esp_err_t ret = bsp_init();
    if (ret != ESP_OK) { ESP_LOGE(TAG, "BSP startup failed: %s", esp_err_to_name(ret)); return; }
    if (!settings_service_start()) ESP_LOGE(TAG, "Settings worker unavailable");
    if (!battery_service_start()) ESP_LOGE(TAG, "Battery worker unavailable");
    if (!wifi_manager_init()) ESP_LOGE(TAG, "Network worker unavailable");
    if (!app_start()) {
        ESP_LOGE(TAG, "Desktop startup failed");
        if (bsp_display_lock(1000)) {
            auto *label = lv_label_create(lv_screen_active());
            lv_label_set_text(label, "Desktop startup failed\nSee serial log");
            lv_obj_center(label);
            bsp_display_unlock();
        }
    }
    const esp_err_t shown = bsp_display_show();
    if (shown != ESP_OK) ESP_LOGE(TAG, "Initial display failed: %s", esp_err_to_name(shown));
    ESP_LOGI(TAG, "Startup main-task stack: %u bytes minimum free (configured %u)",
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
             static_cast<unsigned>(CONFIG_ESP_MAIN_TASK_STACK_SIZE));
}


