#include "ws2812.h"
#include "logger.h"
#include "memory.h"
#include "driver/ledc.h"

CWS2812Ctrl* CWS2812Ctrl::_instance = nullptr;

#define RMT_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)

enum CMD_TYPE {
    SETRGB = 0,
    BLINK = 1,
    BLINK_DEMO = 2,
};

CWS2812Ctrl::CWS2812Ctrl()
{
    m_initialized = false;
    m_keep_task_alive = false;
    m_brightness = 0;
    m_common_color = rgb_t();
    m_hsv_value = hsv_t();
    m_blink_duration_ms = 0;
    m_blink_count = 0;

    m_rmt_ch_handle = nullptr;
    m_rmt_enc_base = nullptr;
    m_rmt_enc_bytes = nullptr;
    m_rmt_enc_copy = nullptr;
    m_rmt_state = 0;
}

CWS2812Ctrl::~CWS2812Ctrl()
{
    release();
}

CWS2812Ctrl* CWS2812Ctrl::Instance()
{
    if (!_instance) {
        _instance = new CWS2812Ctrl();
    }

    return _instance;
}

static size_t func_rmt_encode(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)
{
    CWS2812Ctrl *obj = GetWS2812Ctrl();
    rmt_encoder_handle_t enc_bytes = obj->get_rmt_encoder_bytes();
    rmt_encoder_handle_t enc_copy = obj->get_rmt_encoder_copy();
    rmt_symbol_word_t reset_code = obj->get_rmt_reset_code();

    rmt_encode_state_t session_state = (rmt_encode_state_t)0;
    int state = 0;
    size_t encoded_symbols = 0;

    switch (obj->get_rmt_state()) {
    case 0:
        encoded_symbols += enc_bytes->encode(enc_bytes, channel, primary_data, data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            obj->set_rmt_state(1);
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
        }
        break;
    case 1:
        encoded_symbols += enc_copy->encode(enc_copy, channel, &reset_code, sizeof(reset_code), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            obj->set_rmt_state(0);
            state |= RMT_ENCODING_COMPLETE;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
        }
        break;
    default:
        break;
    }

    *ret_state = (rmt_encode_state_t)state;
    return encoded_symbols;
}

static esp_err_t func_rmt_reset(rmt_encoder_t *encoder)
{
    CWS2812Ctrl *obj = GetWS2812Ctrl();
    rmt_encoder_handle_t enc_bytes = obj->get_rmt_encoder_bytes();
    rmt_encoder_handle_t enc_copy = obj->get_rmt_encoder_copy();
    rmt_encoder_reset(enc_bytes);
    rmt_encoder_reset(enc_copy);
    obj->set_rmt_state(0);
    return ESP_OK;
}

static esp_err_t func_rmt_delete(rmt_encoder_t *encoder)
{
    CWS2812Ctrl *obj = GetWS2812Ctrl();
    rmt_encoder_handle_t enc_base = obj->get_rmt_encoder_base();
    rmt_encoder_handle_t enc_bytes = obj->get_rmt_encoder_bytes();
    rmt_encoder_handle_t enc_copy = obj->get_rmt_encoder_copy();
    rmt_del_encoder(enc_bytes);
    rmt_del_encoder(enc_copy);
    if (enc_base) {
        delete enc_base;
        enc_base = nullptr;
    }

    return ESP_OK;
}

bool CWS2812Ctrl::initialize()
{
    m_initialized = false;

    m_pixel_values.resize(WS2812_ARRAY_COUNT);

    if (!init_ledc())
        return false;

    if (!init_rmt())
        return false;

    m_keep_task_alive = true;
    m_queue_command = xQueueCreate(10, sizeof(int));
    xTaskCreate(func_command, "TASK_WS2812_CTRL", TASK_STACK_DEPTH, this, TASK_PRIORITY_WS2812, &m_task_handle);

    m_initialized = true;

    uint8_t brightness = 0;
    GetMemory()->load_ws2812_brightness(&brightness);
    set_brightness(brightness);

    uint8_t red = 0, green = 0, blue = 0;
    GetMemory()->load_ws2812_color(&red, &green, &blue);
    set_common_color(red, green, blue);

    return true;
}

bool CWS2812Ctrl::init_ledc()
{
    esp_err_t ret;

    ledc_timer_config_t ledc_timer_cfg;
    ledc_timer_cfg.speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_timer_cfg.duty_resolution = LEDC_TIMER_10_BIT;
    ledc_timer_cfg.timer_num = LEDC_TIMER_0;
    ledc_timer_cfg.freq_hz = LED_PWM_FREQUENCY;
    ledc_timer_cfg.clk_cfg = LEDC_AUTO_CLK;
    ret = ledc_timer_config(&ledc_timer_cfg);
    if (ret != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to configure ledc timer (ret %d)", ret);
        return false;
    }

    ledc_channel_config_t ledc_ch_cfg;
    ledc_ch_cfg.gpio_num = GPIO_PIN_WS2812_PWM;
    ledc_ch_cfg.speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_ch_cfg.channel = LEDC_CHANNEL_0;
    ledc_ch_cfg.intr_type = LEDC_INTR_DISABLE;
    ledc_ch_cfg.timer_sel = LEDC_TIMER_0;
    ledc_ch_cfg.duty = 0;
    ledc_ch_cfg.hpoint = 0;
    ledc_ch_cfg.flags.output_invert = 0;
    ret = ledc_channel_config(&ledc_ch_cfg);
    if (ret != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to configure ledc channel (ret %d)", ret);
        return false;
    }

    return true;
}

bool CWS2812Ctrl::init_rmt()
{
    esp_err_t ret;

    rmt_tx_channel_config_t rmt_tx_ch_cfg;
    rmt_tx_ch_cfg.gpio_num = (gpio_num_t)GPIO_PIN_WS2812_DATA;
    rmt_tx_ch_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
    rmt_tx_ch_cfg.resolution_hz = RMT_RESOLUTION_HZ;
    rmt_tx_ch_cfg.mem_block_symbols = 64;
    rmt_tx_ch_cfg.trans_queue_depth = 4;
    rmt_tx_ch_cfg.flags.invert_out = 0;
    rmt_tx_ch_cfg.flags.io_od_mode = 0;
    rmt_tx_ch_cfg.flags.with_dma = 0;
    ret = rmt_new_tx_channel(&rmt_tx_ch_cfg, &m_rmt_ch_handle);
    if (ret != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to create RMT TX channel (ret %d)", ret);
        return false;
    }

    m_rmt_enc_base = new rmt_encoder_t();
    if (!m_rmt_enc_base) {
        GetLogger(eLogType::Error)->Log("Failed to create RMT base encoder (ret %d)", ret);
        return false;
    }
    m_rmt_enc_base->encode = func_rmt_encode;
    m_rmt_enc_base->reset = func_rmt_reset;
    m_rmt_enc_base->del = func_rmt_delete;

    rmt_bytes_encoder_config_t rmt_bytes_enc_cfg;
    rmt_bytes_enc_cfg.bit0.duration0 = 0.3 * RMT_RESOLUTION_HZ / 1000000;  // T0H=300ns
    rmt_bytes_enc_cfg.bit0.level0 = 1;
    rmt_bytes_enc_cfg.bit0.duration1 = 0.9 * RMT_RESOLUTION_HZ / 1000000;  // T0L=900ns
    rmt_bytes_enc_cfg.bit0.level1 = 0;
    rmt_bytes_enc_cfg.bit1.duration0 = 0.9 * RMT_RESOLUTION_HZ / 1000000;  // T1H=900ns
    rmt_bytes_enc_cfg.bit1.level0 = 1;
    rmt_bytes_enc_cfg.bit1.duration1 = 0.3 * RMT_RESOLUTION_HZ / 1000000;  // T1L=300ns
    rmt_bytes_enc_cfg.bit1.level1 = 0;
    rmt_bytes_enc_cfg.flags.msb_first = 1;
    ret = rmt_new_bytes_encoder(&rmt_bytes_enc_cfg, &m_rmt_enc_bytes);
    if (ret != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to create RMT bytes encoder (ret %d)", ret);
        return false;
    }

    rmt_copy_encoder_config_t rmt_copy_enc_cfg;
    ret = rmt_new_copy_encoder(&rmt_copy_enc_cfg, &m_rmt_enc_copy);
    if (ret != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to create RMT copy encoder (ret %d)", ret);
        return false;
    }

    uint32_t reset_ticks = RMT_RESOLUTION_HZ / 1000000 * 300 / 2; // reset code = 300us
    m_rmt_reset_code.duration0 = reset_ticks;
    m_rmt_reset_code.level0 = 0;
    m_rmt_reset_code.duration1 = reset_ticks;
    m_rmt_reset_code.level1 = 0;

    // set enable rmt channel
    ret = rmt_enable(m_rmt_ch_handle);
    if (ret != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to enable RMT (ret %d)", ret);
        return false;
    }

    return true;
}

bool CWS2812Ctrl::release()
{
    m_initialized = false;
    m_keep_task_alive = false;
    
    if (m_rmt_enc_bytes) {
        rmt_del_encoder(m_rmt_enc_bytes);
    }
    if (m_rmt_enc_copy) {
        rmt_del_encoder(m_rmt_enc_copy);
    }
    if (m_rmt_enc_base) {
        delete m_rmt_enc_base;
    }

    return true;
}

bool CWS2812Ctrl::set_pwm_duty(uint32_t duty, bool verbose/*=true*/)
{
    if (!m_initialized) {
        GetLogger(eLogType::Error)->Log("Not initialized!");
        return false;
    }

    esp_err_t ret;
    ret = ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, duty);
    if (ret != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to set ledc duty (ret: %d)", ret);
        return false;
    }

    ret = ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
    if (ret != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to set update duty (ret: %d)", ret);
        return false;
    }
    
    if (verbose) {
        GetLogger(eLogType::Info)->Log("set pwm duty: %d", duty);
    }

    return true;
}

bool CWS2812Ctrl::set_pixel_rgb_value(int index, uint8_t red, uint8_t green, uint8_t blue, bool update/*=true*/)
{
    bool result = true;
    if (index >= 0 && index < m_pixel_values.size()) {
        m_pixel_values[index].r = red;
        m_pixel_values[index].g = green;
        m_pixel_values[index].b = blue;
    } else if (index < 0) {
        for (auto & rgb : m_pixel_values) {
            rgb.r = red;
            rgb.g = green;
            rgb.b = blue;
        }
    } else {
        result = false;
    }

    if (result && update) {
        result = update_color();
    }

    return result;
}

bool CWS2812Ctrl::clear_color()
{
    return set_pixel_rgb_value(-1, 0, 0, 0);
}

bool CWS2812Ctrl::update_color()
{
    if (!m_initialized) {
        GetLogger(eLogType::Error)->Log("Not initialized!");
        return false;
    }

    int *CMD = new int[1];
    CMD[0] = SETRGB;
    if (xQueueSend(m_queue_command, (void *)&CMD, pdMS_TO_TICKS(10)) != pdTRUE) {
        GetLogger(eLogType::Error)->Log("Failed to add command queue");
        delete CMD;
        return false;
    }

    return true;
}

bool CWS2812Ctrl::set_brightness(uint8_t value, bool save_memory/*=true*/,  bool verbose/*=true*/)
{
    m_brightness = value;
    if (save_memory) {
        GetMemory()->save_ws2812_brightness(value);
    }

    uint32_t duty;
    if (value) {
        duty = (uint32_t)((double)value / 255. * (LED_PWM_DUTY_MAX - LED_PWM_DUTY_MIN) + LED_PWM_DUTY_MIN);
    } else {
        duty = 0;
    }
    return set_pwm_duty(duty, verbose);
}

uint8_t CWS2812Ctrl::get_brightness()
{
    return m_brightness;
}

rgb_t CWS2812Ctrl::get_common_color()
{
    return m_common_color;
}

bool CWS2812Ctrl::set_common_color(uint8_t red, uint8_t green, uint8_t blue, bool save_memory/*=true*/)
{
    m_common_color.r = red;
    m_common_color.g = green;
    m_common_color.b = blue;
    if (save_memory) {
        GetMemory()->save_ws2812_color(red, green, blue);
    }

    GetLogger(eLogType::Info)->Log("set common color(%d,%d,%d)", red, green, blue);
    return set_pixel_rgb_value(LED_SET_ALL, red, green, blue, true);
}

bool CWS2812Ctrl::set_hue(uint16_t hue, bool update_color/*=true*/)
{
    bool result = true;
    m_hsv_value.hue = hue;
    if (update_color) {
        rgb_t rgb_conv = m_hsv_value.conv2rgb();
        result = set_common_color(rgb_conv.r, rgb_conv.g, rgb_conv.b);
    }
    return result;
}

bool CWS2812Ctrl::set_saturation(uint8_t saturation, bool update_color/*=true*/)
{
    bool result = true;
    m_hsv_value.saturation = saturation;
    if (update_color) {
        rgb_t rgb_conv = m_hsv_value.conv2rgb();
        result = set_common_color(rgb_conv.r, rgb_conv.g, rgb_conv.b);
    }
    return result;
}

bool CWS2812Ctrl::set_temperature(uint32_t temperature)
{
    /*
    m_hsv_value = convert_temperature_to_hue_saturation(temperature);
    rgb_t rgb_conv = m_hsv_value.conv2rgb();
    return set_common_color(rgb_conv.r, rgb_conv.g, rgb_conv.b);
    */
    return true;
}

bool CWS2812Ctrl::blink(uint32_t duration_ms/*=1000*/, uint32_t count/*=1*/)
{
    if (!m_initialized) {
        GetLogger(eLogType::Error)->Log("Not initialized!");
        return false;
    }

    m_blink_duration_ms = duration_ms;
    m_blink_count = count;

    int *CMD = new int[1];
    CMD[0] = BLINK;
    if (xQueueSend(m_queue_command, (void *)&CMD, pdMS_TO_TICKS(10)) != pdTRUE) {
        GetLogger(eLogType::Error)->Log("Failed to add command queue");
        delete CMD;
        return false;
    }

    GetLogger(eLogType::Info)->Log("set blink(%d,%d)", duration_ms, count);
    return true;
}

bool CWS2812Ctrl::blink_demo()
{
    if (!m_initialized) {
        GetLogger(eLogType::Error)->Log("Not initialized!");
        return false;
    }

    m_blink_count = 10;

    int *CMD = new int[1];
    CMD[0] = BLINK_DEMO;
    if (xQueueSend(m_queue_command, (void *)&CMD, pdMS_TO_TICKS(10)) != pdTRUE) {
        GetLogger(eLogType::Error)->Log("Failed to add command queue");
        delete CMD;
        return false;
    }

    GetLogger(eLogType::Info)->Log("set blink demo");
    return true;
}

rmt_channel_handle_t CWS2812Ctrl::get_rmt_channel()
{
    return m_rmt_ch_handle;
}

rmt_encoder_handle_t CWS2812Ctrl::get_rmt_encoder_base()
{
    return m_rmt_enc_base;
}

rmt_encoder_handle_t CWS2812Ctrl::get_rmt_encoder_bytes()
{
    return m_rmt_enc_bytes;
}

rmt_encoder_handle_t CWS2812Ctrl::get_rmt_encoder_copy()
{
    return m_rmt_enc_copy;
}

rmt_symbol_word_t CWS2812Ctrl::get_rmt_reset_code()
{
    return m_rmt_reset_code;
}

int CWS2812Ctrl::get_rmt_state()
{
    return m_rmt_state;
}

void CWS2812Ctrl::set_rmt_state(int value)
{
    m_rmt_state = value;
}

void CWS2812Ctrl::func_command(void *param)
{
    CWS2812Ctrl *obj = static_cast<CWS2812Ctrl *>(param);
    int *cmd_type = nullptr;
    uint8_t brightness;
    uint32_t delay;
    bool blink_demo = false;

    rmt_transmit_config_t rmt_tx_cfg;
    rmt_tx_cfg.loop_count = 0;
    rmt_tx_cfg.flags.eot_level = 0;
    esp_err_t ret;
    uint8_t conv_array[WS2812_ARRAY_COUNT * 3] = { 0, };
    int timeout_ms = (int)(1.2 * WS2812_ARRAY_COUNT * 3 * 8 + 300);
    timeout_ms = MAX(timeout_ms, 1);

    GetLogger(eLogType::Info)->Log("Realtime Task for WS2812 Module Started");
    while (obj->m_keep_task_alive) {
        if (xQueueReceive(obj->m_queue_command, (void *)&cmd_type, pdMS_TO_TICKS(WS2812_REFRESH_TIME_MS)) == pdTRUE) {
            if (*cmd_type == SETRGB) {
                for (size_t i = 0; i < WS2812_ARRAY_COUNT; i++) {
                    rgb_t rgb = obj->m_pixel_values[i];
                    conv_array[i * 3 + 0] = rgb.g;
                    conv_array[i * 3 + 1] = rgb.r;
                    conv_array[i * 3 + 2] = rgb.b;
                }

                obj->set_rmt_state(0);
                ret = rmt_transmit(obj->m_rmt_ch_handle, obj->m_rmt_enc_base, conv_array, sizeof(conv_array), &rmt_tx_cfg);
                if (ret != ESP_OK) {
                    GetLogger(eLogType::Error)->Log("Failed to transmit rmt (return code: %u)", ret);
                }
                ret = rmt_tx_wait_all_done(obj->m_rmt_ch_handle, timeout_ms);
                if (ret != ESP_OK) {
                    GetLogger(eLogType::Error)->Log("Failed to rmt wait all done (timeout: %d, return code: %u)", timeout_ms, ret);
                }
            } else if (*cmd_type == BLINK) {
                delay = obj->m_blink_duration_ms / 42;
                brightness = obj->get_brightness();

                for (uint32_t i = 0; i < obj->m_blink_count; i++) {
                    for (int v = 0; v <= 100; v+=5) {
                        obj->set_brightness(v, false, false);
                        vTaskDelay(pdMS_TO_TICKS(delay));
                    }
                    for (int v = 100; v >= 0; v-=5) {
                        obj->set_brightness(v, false, false);
                        vTaskDelay(pdMS_TO_TICKS(delay));
                    }
                }

                obj->set_brightness(brightness, false, false);
            } else if (*cmd_type == BLINK_DEMO) {
                blink_demo = true;
            }
        }
        
        if (blink_demo) {
            if (obj->m_blink_count > 0) {
                rgb_t rgb;
                uint32_t rm = obj->m_blink_count % 8;
                if (rm == 0) {
                    rgb = rgb_t(255, 0, 0);
                } else if (rm == 1) {
                    rgb = rgb_t(0, 255, 0);
                } else if (rm == 2) {
                    rgb = rgb_t(0, 0, 255);
                } else if (rm == 3) {
                    rgb = rgb_t(255, 255, 0);
                } else if (rm == 4) {
                    rgb = rgb_t(255, 0, 255);
                } else if (rm == 5) {
                    rgb = rgb_t(0, 255, 255);
                } else if (rm == 6) {
                    rgb = rgb_t(255, 70, 0);
                } else if (rm == 7) {
                    rgb = rgb_t(0, 128, 0);
                } else {
                    rgb = rgb_t(255, 255, 255);
                }

                for (size_t i = 0; i < WS2812_ARRAY_COUNT; i++) {
                    rgb_t rgb = obj->m_pixel_values[i];
                    conv_array[i * 3 + 0] = rgb.g;
                    conv_array[i * 3 + 1] = rgb.r;
                    conv_array[i * 3 + 2] = rgb.b;
                }

                obj->set_rmt_state(0);
                ret = rmt_transmit(obj->m_rmt_ch_handle, obj->m_rmt_enc_base, conv_array, sizeof(conv_array), &rmt_tx_cfg);
                if (ret != ESP_OK) {
                    GetLogger(eLogType::Error)->Log("Failed to transmit rmt (return code: %u)", ret);
                }
                ret = rmt_tx_wait_all_done(obj->m_rmt_ch_handle, timeout_ms);
                if (ret != ESP_OK) {
                    GetLogger(eLogType::Error)->Log("Failed to rmt wait all done (timeout: %d, return code: %u)", timeout_ms, ret);
                }

                delay = 25;
                for (int v = 0; v <= 100; v+=5) {
                    obj->set_brightness(v, false, false);
                    vTaskDelay(pdMS_TO_TICKS(delay));
                }
                for (int v = 100; v >= 0; v-=5) {
                    obj->set_brightness(v, false, false);
                    vTaskDelay(pdMS_TO_TICKS(delay));
                }

                obj->m_blink_count--;
            } else {
                blink_demo = false;
            }
        }
    }

    GetLogger(eLogType::Info)->Log("Realtime Task for WS2812 Module Terminated");
    vTaskDelete(nullptr);
}
