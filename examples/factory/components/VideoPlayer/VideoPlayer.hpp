#pragma once

#include "systems/phone/esp_brookesia_phone_app.hpp"
#include "bsp/esp-bsp.h"
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include "avi_player.h"
#include "esp_jpeg_dec.h"

namespace esp_brookesia::apps
{

    class VideoPlayer : public systems::phone::App
    {
    public:
        static VideoPlayer *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);
        ~VideoPlayer();

    protected:
        VideoPlayer(bool use_status_bar, bool use_navigation_bar);
        bool run(void) override;
        bool back(void) override;
        bool close(void) override;
        bool init(void) override;
        bool deinit(void) override;
        bool pause(void) override;
        bool resume(void) override;

    private:
        static VideoPlayer *_instance;
    };

} // namespace esp_brookesia::apps
