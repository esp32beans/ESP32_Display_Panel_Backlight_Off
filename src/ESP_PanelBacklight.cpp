/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ESP_PanelBacklight.h"

#include "private/CheckResult.h"

#define BACKLIGHT_CONFIG_DEFAULT(io_num, on_level)  \
    {                                               \
        .io_num = io_num,                           \
        .flags = {                                  \
            .use_pwm = false,                       \
            .light_on_level = on_level,             \
        },                                          \
    }

static const char *TAG = "ESP_PanelBacklight";

ESP_PanelBacklight::ESP_PanelBacklight(const ESP_PanelBacklightConfig_t *config):
    config(*config),
    is_initialized(false)
{
}

ESP_PanelBacklight::ESP_PanelBacklight(int io_num, int light_on_level):
    config((ESP_PanelBacklightConfig_t)BACKLIGHT_CONFIG_DEFAULT(io_num, light_on_level)),
    is_initialized(false)
{
}

ESP_PanelBacklight::~ESP_PanelBacklight()
{
    if (is_initialized) {
        del();
    }
}

void ESP_PanelBacklight::init(void)
{
    CHECK_FALSE_RETURN(config.io_num >= 0);

    if (config.flags.use_pwm) {
        ledc_timer_config_t timer_conf = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = config.pwm.resolution,
            .timer_num = config.pwm.timer,
            .freq_hz = config.pwm.freq_hz,
            .clk_cfg = LEDC_AUTO_CLK,
        };
        ledc_channel_config_t channel_conf = {
            .gpio_num = config.io_num,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = config.pwm.channel,
            .timer_sel = config.pwm.timer,
            .hpoint = 0,
        };
#if BACKLIGHT_DEFAULT_ON
        channel_conf.duty = config.flags.light_on_level ? BIT(config.pwm.resolution) - 1 : 0;
#else
        channel_conf.duty = 0;
#endif
        CHECK_ERROR_RETURN(ledc_timer_config(&timer_conf));
        CHECK_ERROR_RETURN(ledc_channel_config(&channel_conf));
    } else {
        gpio_config_t io_conf = {
            .pin_bit_mask = BIT64(config.io_num),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        CHECK_ERROR_RETURN(gpio_config(&io_conf));
#if BACKLIGHT_DEFAULT_ON
        CHECK_ERROR_RETURN(gpio_set_level((gpio_num_t)config.io_num, config.flags.light_on_level));
#else
        CHECK_ERROR_RETURN(gpio_set_level((gpio_num_t)config.io_num, 0));
#endif
    }
    is_initialized = true;
}

void ESP_PanelBacklight::setBrightness(uint8_t percent)
{
    CHECK_FALSE_RETURN(config.io_num >= 0);

    percent = percent > 100 ? 100 : percent;
    if (!config.flags.light_on_level) {
        percent = 100 - percent;
    }
    if (config.flags.use_pwm) {
        uint32_t duty_cycle = (BIT(config.pwm.resolution) * percent) / 100;
        ledc_channel_t chanel = config.pwm.channel;
        CHECK_ERROR_RETURN(ledc_set_duty(LEDC_LOW_SPEED_MODE, chanel, duty_cycle));
        CHECK_ERROR_RETURN(ledc_update_duty(LEDC_LOW_SPEED_MODE, chanel));
    } else {
        CHECK_ERROR_RETURN(gpio_set_level((gpio_num_t)config.io_num, percent));
    }
}

void ESP_PanelBacklight::on(void)
{
    setBrightness(100);
}

void ESP_PanelBacklight::off(void)
{
    setBrightness(0);
}

void ESP_PanelBacklight::del(void)
{
    if (config.flags.use_pwm) {
        CHECK_ERROR_RETURN(ledc_stop(LEDC_LOW_SPEED_MODE, config.pwm.channel, !config.flags.light_on_level));
    } else {
        gpio_reset_pin((gpio_num_t)config.io_num);
    }
    is_initialized = false;
}
