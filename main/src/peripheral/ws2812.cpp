#include "ws2812.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/rmt.h"
#include "definition.h"
#include "logger.h"
#include "memory.h"

CWS2812Ctrl* CWS2812Ctrl::_instance = nullptr;

enum CMD_TYPE {
    SETRGB = 0,
    BLINK = 1,
    BLINK_DEMO = 2,
};

CWS2812Ctrl::CWS2812Ctrl()
{
    m_gpio_pin_no = 0;
    m_task_keepalive = true;
    m_brightness = 0;
    m_common_color = RGB();
    m_blink_duration_ms = 0;
    m_blink_count = 0;
}

CWS2812Ctrl::~CWS2812Ctrl()
{
    m_task_keepalive = false;
}

CWS2812Ctrl* CWS2812Ctrl::Instance()
{
    if (!_instance) {
        _instance = new CWS2812Ctrl();
    }

    return _instance;
}

bool CWS2812Ctrl::initialize(uint8_t gpio_pin_no, uint16_t pixel_cnt)
{
    esp_err_t ret;

    m_gpio_pin_no = gpio_pin_no;
    m_pixel_values.resize(pixel_cnt);
    m_pixel_conv_values.resize(pixel_cnt);

    m_queue_command = xQueueCreate(10, sizeof(int));
    xTaskCreate(func_command, "TASK_WS2812_CTRL", TASK_STACK_DEPTH, this, TASK_PRIORITY_WS2812, &m_task_handle);

    gpio_config_t gpio_cfg;
    gpio_cfg.pin_bit_mask   = 1ULL << m_gpio_pin_no;
    gpio_cfg.mode           = GPIO_MODE_OUTPUT;
    gpio_cfg.pull_up_en     = GPIO_PULLUP_DISABLE;
    gpio_cfg.pull_down_en   = GPIO_PULLDOWN_DISABLE;
    gpio_cfg.intr_type      = GPIO_INTR_DISABLE;
    ret = gpio_config(&gpio_cfg);
    if (ret != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to configure GPIO (ret %d)", ret);
        return false;
    }

    ledc_timer_config_t ledc_timer_cfg;
    ledc_timer_cfg.speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_timer_cfg.duty_resolution = LEDC_TIMER_10_BIT;
    ledc_timer_cfg.timer_num = LEDC_TIMER_0;
    ledc_timer_cfg.freq_hz = LED_PWM_FREQUENCY;
    ledc_timer_cfg.clk_cfg = LEDC_AUTO_CLK;
    ledc_timer_config(&ledc_timer_cfg);

    ledc_channel_config_t ledc_ch_cfg;
    ledc_ch_cfg.gpio_num = GPIO_PIN_WS2812_PWM;
    ledc_ch_cfg.speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_ch_cfg.channel = LEDC_CHANNEL_0;
    ledc_ch_cfg.intr_type = LEDC_INTR_DISABLE;
    ledc_ch_cfg.timer_sel = LEDC_TIMER_0;
    ledc_ch_cfg.duty = 0;
    ledc_ch_cfg.hpoint = 0;
    ledc_ch_cfg.flags.output_invert = 1;
    
    ledc_channel_config(&ledc_ch_cfg);

    return true;
}

bool CWS2812Ctrl::set_pwm_duty(uint32_t duty, bool verbose/*=true*/)
{
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

static void IRAM_ATTR set_databit_low(uint8_t pin_no)
{
    // T0H: 220ns ~ 380ns
    GPIO_REG_WRITE(GPIO_OUT_W1TS_REG, 1UL << pin_no);
    __asm__ __volatile__(
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;");

    // T0L: 580ns ~ 1us
    GPIO_REG_WRITE(GPIO_OUT_W1TC_REG, 1UL << pin_no);
    __asm__ __volatile__(
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;");
}

static void IRAM_ATTR set_databit_high(uint8_t pin_no)
{
    // T1H: 580ns ~ 1us
    GPIO_REG_WRITE(GPIO_OUT_W1TS_REG, 1UL << pin_no);
    __asm__ __volatile__(
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;");

    // T1L: 220ns ~ 420ns
    GPIO_REG_WRITE(GPIO_OUT_W1TC_REG, 1UL << pin_no);
    __asm__ __volatile__(
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;"
    "nop; nop; nop; nop; nop; nop; nop; nop;");
}

static uint32_t convert_rgb_to_u32(RGB rgb) 
{
    return ((uint32_t)rgb.g) << 16 | ((uint32_t)rgb.r) << 8 | (uint32_t)rgb.b;
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

    uint32_t duty = (uint32_t)((double)value / 100. * LED_PWM_DUTY_MAX);
    return set_pwm_duty(duty, verbose);
}

uint8_t CWS2812Ctrl::get_brightness()
{
    return m_brightness;
}

RGB CWS2812Ctrl::get_common_color()
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

bool CWS2812Ctrl::blink(uint32_t duration_ms/*=1000*/, uint32_t count/*=1*/)
{
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

void CWS2812Ctrl::func_command(void *param)
{
    CWS2812Ctrl *obj = static_cast<CWS2812Ctrl *>(param);
    int *cmd_type = nullptr;
    uint8_t brightness;
    uint32_t delay;
    bool blink_demo = false;

    GetLogger(eLogType::Info)->Log("Realtime Task for WS2812 Module Started");
    while (obj->m_task_keepalive) {
        if (xQueueReceive(obj->m_queue_command, (void *)&cmd_type, pdMS_TO_TICKS(WS2812_REFRESH_TIME_MS)) == pdTRUE) {
            if (*cmd_type == SETRGB) {
                for (size_t i = 0; i < obj->m_pixel_values.size(); i++) {
                    RGB rgb = obj->m_pixel_values[i];
                    obj->m_pixel_conv_values[i] = convert_rgb_to_u32(rgb);
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

        for (auto & value : obj->m_pixel_conv_values) {
            for (uint8_t i = 0; i < 24; i++) {
                if (value & (1UL << (23 - i)))
                    set_databit_high(obj->m_gpio_pin_no);
                else
                    set_databit_low(obj->m_gpio_pin_no);
            }
        }
        
        if (blink_demo) {
            if (obj->m_blink_count > 0) {
                RGB rgb;
                uint32_t rm = obj->m_blink_count % 8;
                if (rm == 0) {
                    rgb = RGB(255, 0, 0);
                } else if (rm == 1) {
                    rgb = RGB(0, 255, 0);
                } else if (rm == 2) {
                    rgb = RGB(0, 0, 255);
                } else if (rm == 3) {
                    rgb = RGB(255, 255, 0);
                } else if (rm == 4) {
                    rgb = RGB(255, 0, 255);
                } else if (rm == 5) {
                    rgb = RGB(0, 255, 255);
                } else if (rm == 6) {
                    rgb = RGB(255, 70, 0);
                } else if (rm == 7) {
                    rgb = RGB(0, 128, 0);
                } else {
                    rgb = RGB(255, 255, 255);
                }

                for (size_t i = 0; i < obj->m_pixel_values.size(); i++) {
                    obj->m_pixel_conv_values[i] = convert_rgb_to_u32(rgb);
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
