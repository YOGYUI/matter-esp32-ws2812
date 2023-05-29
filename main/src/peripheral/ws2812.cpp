#include "ws2812.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/rmt_tx.h"
// #include "driver/rmt_rx.h"
#include "rom/gpio.h"
#include "definition.h"
#include "logger.h"
#include "memory.h"

CWS2812Ctrl* CWS2812Ctrl::_instance = nullptr;

enum CMD_TYPE {
    SETRGB = 0,
    BLINK = 1,
    BLINK_DEMO = 2,
};

static RGB_t convert_hue_saturation_to_rgb(HS_t hue_saturation) {
    uint16_t hue = hue_saturation.hue % 360;
    uint8_t saturation = hue_saturation.saturation;
    uint16_t hi = (hue / 60) % 6;
    uint16_t F = 100 * hue / 60 - 100 * hi;
    uint16_t P = 255 * (100 - saturation) / 100;
    uint16_t Q = 255 * (10000 - F * saturation) / 10000;
    uint16_t T = 255 * (10000 - saturation * (100 - F)) / 10000;
    RGB_t rgb;
    switch (hi) {
    case 0:
        rgb.r = 255;
        rgb.g = T;
        rgb.b = P;
        break;
    case 1:
        rgb.r = Q;
        rgb.g = 255;
        rgb.b = P;
        break;
    case 2:
        rgb.r = P;
        rgb.g = 255;
        rgb.b = T;
        break;
    case 3:
        rgb.r = P;
        rgb.g = Q;
        rgb.b = 255;
        break;
    case 4:
        rgb.r = T;
        rgb.g = P;
        rgb.b = 255;
        break;
    case 5:
        rgb.r = 255;
        rgb.g = P;
        rgb.b = Q;
        break;
    default:
        break;
    }

    rgb.r = rgb.r * 255 / 100;
    rgb.g = rgb.g * 255 / 100;
    rgb.b = rgb.b * 255 / 100;

    return rgb;
}

const HS_t temp_table[] = {
    {4, 100},  {8, 100},  {11, 100}, {14, 100}, {16, 100}, {18, 100}, {20, 100}, {22, 100}, {24, 100}, {25, 100},
    {27, 100}, {28, 100}, {30, 100}, {31, 100}, {31, 95},  {30, 89},  {30, 85},  {29, 80},  {29, 76},  {29, 73},
    {29, 69},  {28, 66},  {28, 63},  {28, 60},  {28, 57},  {28, 54},  {28, 52},  {27, 49},  {27, 47},  {27, 45},
    {27, 43},  {27, 41},  {27, 39},  {27, 37},  {27, 35},  {27, 33},  {27, 31},  {27, 30},  {27, 28},  {27, 26},
    {27, 25},  {27, 23},  {27, 22},  {27, 21},  {27, 19},  {27, 18},  {27, 17},  {27, 15},  {28, 14},  {28, 13},
    {28, 12},  {29, 10},  {29, 9},   {30, 8},   {31, 7},   {32, 6},   {34, 5},   {36, 4},   {41, 3},   {49, 2},
    {0, 0},    {294, 2},  {265, 3},  {251, 4},  {242, 5},  {237, 6},  {233, 7},  {231, 8},  {229, 9},  {228, 10},
    {227, 11}, {226, 11}, {226, 12}, {225, 13}, {225, 13}, {224, 14}, {224, 14}, {224, 15}, {224, 15}, {223, 16},
    {223, 16}, {223, 17}, {223, 17}, {223, 17}, {222, 18}, {222, 18}, {222, 19}, {222, 19}, {222, 19}, {222, 19},
    {222, 20}, {222, 20}, {222, 20}, {222, 21}, {222, 21}};

static HS_t convert_temperature_to_hue_saturation(uint32_t temperature) {
    HS_t hs;
    if (temperature < 600) {
        hs.hue = 0;
        hs.saturation = 100;
        return hs;
    } else if (temperature > 10000) {
        hs.hue = 222;
        hs.saturation = 21 + (temperature - 10000) * 41 / 990000;
        return hs;
    } else {
        hs.hue = temp_table[(temperature - 600) / 100].hue;
        hs.saturation = temp_table[(temperature - 600) / 100].saturation;
    }
    return hs;
}

CWS2812Ctrl::CWS2812Ctrl()
{
    m_gpio_pin_no = 0;
    m_task_keepalive = true;
    m_brightness = 0;
    m_common_color = RGB_t();
    m_hue_saturation = HS_t();
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
    ledc_ch_cfg.flags.output_invert = 0;
    
    ledc_channel_config(&ledc_ch_cfg);

    uint8_t brightness = 0;
    GetMemory()->load_ws2812_brightness(&brightness);
    set_brightness(brightness);

    uint8_t red = 0, green = 0, blue = 0;
    GetMemory()->load_ws2812_color(&red, &green, &blue);
    set_common_color(red, green, blue);

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

static uint32_t convert_rgb_to_u32(RGB_t rgb) 
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

RGB_t CWS2812Ctrl::get_common_color()
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

bool CWS2812Ctrl::set_hue(uint16_t hue)
{
    m_hue_saturation.hue = hue;
    RGB_t rgb_conv = convert_hue_saturation_to_rgb(m_hue_saturation);
    return set_common_color(rgb_conv.r, rgb_conv.g, rgb_conv.b);
}

bool CWS2812Ctrl::set_saturation(uint8_t saturation)
{
    m_hue_saturation.saturation = saturation;
    RGB_t rgb_conv = convert_hue_saturation_to_rgb(m_hue_saturation);
    return set_common_color(rgb_conv.r, rgb_conv.g, rgb_conv.b);
}

bool CWS2812Ctrl::set_temperature(uint32_t temperature)
{
    m_hue_saturation = convert_temperature_to_hue_saturation(temperature);
    RGB_t rgb_conv = convert_hue_saturation_to_rgb(m_hue_saturation);
    return set_common_color(rgb_conv.r, rgb_conv.g, rgb_conv.b);
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
                    RGB_t rgb = obj->m_pixel_values[i];
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
                RGB_t rgb;
                uint32_t rm = obj->m_blink_count % 8;
                if (rm == 0) {
                    rgb = RGB_t(255, 0, 0);
                } else if (rm == 1) {
                    rgb = RGB_t(0, 255, 0);
                } else if (rm == 2) {
                    rgb = RGB_t(0, 0, 255);
                } else if (rm == 3) {
                    rgb = RGB_t(255, 255, 0);
                } else if (rm == 4) {
                    rgb = RGB_t(255, 0, 255);
                } else if (rm == 5) {
                    rgb = RGB_t(0, 255, 255);
                } else if (rm == 6) {
                    rgb = RGB_t(255, 70, 0);
                } else if (rm == 7) {
                    rgb = RGB_t(0, 128, 0);
                } else {
                    rgb = RGB_t(255, 255, 255);
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
