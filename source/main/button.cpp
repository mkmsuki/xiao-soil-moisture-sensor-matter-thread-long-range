//
// Copyright 2026 AUTOMATOUS.IO
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// button.cpp

#include "button.h"
#include "status_led.h"
#include "app_priv.h"

#include <esp_log.h>
#include <esp_matter.h>
#include <esp_timer.h>
#include <button_gpio.h>
#include <iot_button.h>

#include <app/icd/server/ICDNotifier.h>

static const char *TAG = "button";

#define BUTTON_ACTIVE_LEVEL        0
#define FACTORY_RESET_PRESS_MS     10000
#define CALIBRATION_CLICK_COUNT    3

#define BUTTON_COOLDOWN_US     ((int64_t)CONFIG_SOIL_BUTTON_ICD_COOLDOWN_SECONDS * 1000000LL)
#define CALIBRATION_COOLDOWN_US ((int64_t)CONFIG_SOIL_BUTTON_CAL_COOLDOWN_SECONDS * 1000000LL)

// Returns true and records now when cooldown_us has passed since the last
// accepted event; false while still inside the cooldown. Rate-limits every
// button action so a wet or chattering CN4 button cannot keep the radio awake,
// resample with LED blinks, or start calibration over and over.
static bool cooldown_elapsed(int64_t *last_us, int64_t cooldown_us)
{
    const int64_t now_us = esp_timer_get_time();
    if (*last_us >= 0 && (now_us - *last_us) < cooldown_us) {
        return false;
    }
    *last_us = now_us;
    return true;
}

static void button_press_down_cb(void *arg, void *data)
{
    static int64_t s_last_icd_wake_us = -1;
    if (!cooldown_elapsed(&s_last_icd_wake_us, BUTTON_COOLDOWN_US)) {
        ESP_LOGD(TAG, "Press ignored: ICD wake cooldown");
        return;
    }

    // LIT ICD User Active Mode Trigger: a press makes the device reachable
    // for ActiveModeThreshold.
    CHIP_ERROR err = chip::DeviceLayer::PlatformMgr().ScheduleWork([](intptr_t) {
        chip::app::ICDNotifier::GetInstance().NotifyNetworkActivityNotification();
    });
    if (err != CHIP_NO_ERROR) {
        ESP_LOGW(TAG, "Failed to schedule ICD active mode trigger");
    }
}

static void button_single_click_cb(void *arg, void *data)
{
    static int64_t s_last_sample_us = -1;
    if (!cooldown_elapsed(&s_last_sample_us, BUTTON_COOLDOWN_US)) {
        ESP_LOGD(TAG, "Single press ignored: sample cooldown");
        return;
    }
    ESP_LOGI(TAG, "Single press: sampling now");
    app_sample_request(APP_SAMPLE_SHOW_LED);
}

static void button_triple_click_cb(void *arg, void *data)
{
    static int64_t s_last_cal_us = -1;
    if (!cooldown_elapsed(&s_last_cal_us, CALIBRATION_COOLDOWN_US)) {
        ESP_LOGW(TAG, "Triple press ignored: calibration cooldown (%d s)", CONFIG_SOIL_BUTTON_CAL_COOLDOWN_SECONDS);
        return;
    }
    ESP_LOGI(TAG, "Triple press: starting calibration");
    app_sample_request(APP_SAMPLE_CAL);
}

static void button_factory_reset_cb(void *arg, void *data)
{
    ESP_LOGW(TAG, "Long press: factory reset");
    status_led_blink(LED_RED, 10, 100);
    esp_matter::factory_reset();
}

esp_err_t button_init(void)
{
    button_handle_t handle = NULL;
    const button_config_t btn_cfg = {0};

    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = PIN_BUTTON,
        .active_level = BUTTON_ACTIVE_LEVEL,
        .enable_power_save = true,
    };

    if (iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create button device");
        return ESP_FAIL;
    }

    iot_button_register_cb(handle, BUTTON_PRESS_DOWN, NULL, button_press_down_cb, NULL);
    iot_button_register_cb(handle, BUTTON_SINGLE_CLICK, NULL, button_single_click_cb, NULL);

    button_event_args_t triple_args = {};
    triple_args.multiple_clicks.clicks = CALIBRATION_CLICK_COUNT;
    iot_button_register_cb(handle, BUTTON_MULTIPLE_CLICK, &triple_args, button_triple_click_cb, NULL);

    button_event_args_t long_args = {};
    long_args.long_press.press_time = FACTORY_RESET_PRESS_MS;
    iot_button_register_cb(handle, BUTTON_LONG_PRESS_START, &long_args, button_factory_reset_cb, NULL);

    ESP_LOGI(TAG, "Button initialized on GPIO%d", PIN_BUTTON);
    return ESP_OK;
}
