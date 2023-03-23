#ifndef _WS2812_H_
#define _WS2812_H_
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdint.h>
#include <vector>

typedef struct st_rgb
{
    uint8_t r, g, b;
    st_rgb(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0) {
        r = red;
        g = green;
        b = blue;
    }
} RGB;

#ifdef __cplusplus
extern "C" {
#endif

class CWS2812Ctrl
{
public:
    CWS2812Ctrl();
    virtual ~CWS2812Ctrl();
    static CWS2812Ctrl* Instance();

public:
    bool initialize(uint8_t gpio_pin_no, uint16_t pixel_cnt);
    bool set_pixel_rgb_value(int index, uint8_t red, uint8_t green, uint8_t blue, bool update = true);
    bool update_color();
    bool clear_color();

    bool set_brightness(uint8_t value, bool save_memory = true, bool verbose = true);
    uint8_t get_brightness();
    
    RGB get_common_color();
    bool set_common_color(uint8_t red, uint8_t green, uint8_t blue, bool save_memory = true);

    bool blink(uint32_t duration_ms = 1000, uint32_t count = 1);
    bool blink_demo();

private:
    static CWS2812Ctrl *_instance;

    uint8_t m_gpio_pin_no;
    uint8_t m_brightness;
    
    RGB m_common_color;
    std::vector<RGB> m_pixel_values;
    std::vector<uint32_t> m_pixel_conv_values;
    
    uint32_t m_blink_duration_ms;
    uint32_t m_blink_count;
    
    QueueHandle_t m_queue_command;
    TaskHandle_t m_task_handle;
    bool m_task_keepalive;
    
    bool set_pwm_duty(uint32_t duty, bool verbose = true);

    static void func_command(void *param);
};

inline CWS2812Ctrl* GetWS2812Ctrl() {
    return CWS2812Ctrl::Instance();
}

#ifdef __cplusplus
}
#endif
#endif