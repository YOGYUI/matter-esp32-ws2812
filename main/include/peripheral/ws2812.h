#ifndef _WS2812_H_
#define _WS2812_H_
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/rmt_tx.h"
#include <stdint.h>
#include <vector>
#include "definition.h"

struct rgb_t
{
    uint8_t r, g, b;
    rgb_t(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0) {
        r = red;
        g = green;
        b = blue;
    }
};

struct hsv_t
{
    /**
     * @brief 
     * hue range: [0, 360] degree
     * saturation range: [0, 100]
     * value range: [0, 100]
     */
    uint32_t hue;           // 색상
    uint32_t saturation;    // 채도
    uint32_t value;         // 명도
    hsv_t(uint32_t h = 0, uint32_t s = 0, uint32_t v = 100) {
        hue = MIN(360, h);
        saturation = MIN(100, s);
        value = MIN(100, v);
    }

    rgb_t conv2rgb() {
        /**
         * @brief HSV to RGB conversion formula
         * @ref https://en.wikipedia.org/wiki/HSL_and_HSV
         */
        rgb_t rgb;

        /*
        double Sv = saturation / 100.;
        double V = value / 100.;
        double H = hue / 60.;
        
        double C = V * Sv;  // Chroma
        double X = C * (1 - abs(int(H) % 2 - 1));

        double R1, G1, B1;
        if (H < 1.) {
            R1 = C; G1 = X; B1 = 0.;
        } else if (H >= 1. && H < 2.) {
            R1 = X; G1 = C; B1 = 0.;
        } else if (H >= 2. && H < 3.) {
            R1 = 0.; G1 = C; B1 = X;
        } else if (H >= 3. && H < 4.) {
            R1 = 0.; G1 = X; B1 = C;
        } else if (H >= 4. && H < 5.) {
            R1 = X; G1 = 0.; B1 = C;
        } else {
            R1 = C; G1 = 0.; B1 = X;
        }

        double m = V - C;
        rgb.r = (uint8_t)((R1 + m) * 255.);
        rgb.g = (uint8_t)((G1 + m) * 255.);
        rgb.b = (uint8_t)((B1 + m) * 255.);
        */

        
        uint32_t h = hue % 360;
        uint32_t rgb_max = value * 2.55f;
        uint32_t rgb_min = rgb_max * (100 - saturation) / 100.0f;

        uint32_t i = h / 60;
        uint32_t diff = h % 60;

        // RGB adjustment amount by hue
        uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

        switch (i) {
        case 0:
            rgb.r = rgb_max;
            rgb.g = rgb_min + rgb_adj;
            rgb.b = rgb_min;
            break;
        case 1:
            rgb.r = rgb_max - rgb_adj;
            rgb.g = rgb_max;
            rgb.b = rgb_min;
            break;
        case 2:
            rgb.r = rgb_min;
            rgb.g = rgb_max;
            rgb.b = rgb_min + rgb_adj;
            break;
        case 3:
            rgb.r = rgb_min;
            rgb.g = rgb_max - rgb_adj;
            rgb.b = rgb_max;
            break;
        case 4:
            rgb.r = rgb_min + rgb_adj;
            rgb.g = rgb_min;
            rgb.b = rgb_max;
            break;
        default:
            rgb.r = rgb_max;
            rgb.g = rgb_min;
            rgb.b = rgb_max - rgb_adj;
            break;
        }

        return rgb;
    }

};

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
    bool initialize();
    bool release();
    bool set_pixel_rgb_value(int index, uint8_t red, uint8_t green, uint8_t blue, bool update = true);
    bool update_color();
    bool clear_color();

    bool set_brightness(uint8_t value, bool save_memory = true, bool verbose = true);
    uint8_t get_brightness();
    
    rgb_t get_common_color();
    bool set_common_color(uint8_t red, uint8_t green, uint8_t blue, bool save_memory = true);

    bool set_hue(uint16_t hue, bool update_color = true);
    bool set_saturation(uint8_t saturation, bool update_color = true);
    bool set_temperature(uint32_t temperature);

    bool blink(uint32_t duration_ms = 1000, uint32_t count = 1);
    bool blink_demo();

private:
    static CWS2812Ctrl *_instance;
    bool m_initialized;
    uint8_t m_brightness;
    rgb_t m_common_color;
    hsv_t m_hsv_value;
    std::vector<rgb_t> m_pixel_values;
    uint32_t m_blink_duration_ms;
    uint32_t m_blink_count;
    QueueHandle_t m_queue_command;
    TaskHandle_t m_task_handle;
    bool m_keep_task_alive;
    
    bool init_ledc();
    bool init_rmt();
    bool set_pwm_duty(uint32_t duty, bool verbose = true);

    static void func_command(void *param);

private:
    // RMT related variables
    rmt_channel_handle_t m_rmt_ch_handle;
    rmt_encoder_handle_t m_rmt_enc_base;
    rmt_encoder_handle_t m_rmt_enc_bytes;
    rmt_encoder_handle_t m_rmt_enc_copy;
    rmt_symbol_word_t m_rmt_reset_code;
    int m_rmt_state;

public:
    rmt_channel_handle_t get_rmt_channel();
    rmt_encoder_handle_t get_rmt_encoder_base();
    rmt_encoder_handle_t get_rmt_encoder_bytes();
    rmt_encoder_handle_t get_rmt_encoder_copy();
    rmt_symbol_word_t get_rmt_reset_code();
    int get_rmt_state();
    void set_rmt_state(int value);
};

inline CWS2812Ctrl* GetWS2812Ctrl() {
    return CWS2812Ctrl::Instance();
}

#ifdef __cplusplus
}
#endif
#endif