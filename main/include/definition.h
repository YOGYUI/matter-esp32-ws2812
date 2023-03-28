#ifndef _DEFINITION_H_
#define _DEFINITION_H_
#pragma once

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif
#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#define PRODUCT_NAME            "YOGYUI-MATTER"

#define GPIO_PIN_DEFAULT_BTN    0
#define GPIO_PIN_WS2812_DATA    18
#define GPIO_PIN_WS2812_PWM     19

#define WS2812_ARRAY_COUNT      16
#define WS2812_REFRESH_TIME_MS  100
#define LED_PWM_FREQUENCY       50000
#define LED_PWM_DUTY_MAX        400
#define LED_PWM_DUTY_MIN        50
#define LED_SET_ALL             -1

#define TASK_STACK_DEPTH        4096
#define TASK_PRIORITY_WS2812    2

/**
 0 = on/off
 1 = level control
 */
#define LIGHT_TYPE  1

#endif