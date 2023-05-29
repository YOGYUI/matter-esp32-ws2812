#include "memory.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "logger.h"
#include "cJSON.h"
#include "ws2812.h"

#define MEMORY_NAMESPACE "yogyui"

CMemory* CMemory::_instance;

CMemory::CMemory()
{
}

CMemory::~CMemory()
{
}

CMemory* CMemory::Instance()
{
    if (!_instance) {
        _instance = new CMemory();
    }

    return _instance;
}

void CMemory::Release()
{
    if (_instance) {
        delete _instance;
        _instance = nullptr;
    }
}

bool CMemory::read_nvs(const char *key, void *out, size_t data_size)
{
    nvs_handle handle;

    esp_err_t err = nvs_open(MEMORY_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to open nvs (ret=%d)", err);
        return false;
    }

    size_t temp = data_size;
    err = nvs_get_blob(handle, key, out, &temp);
    if (err != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to get blob (%s, ret=%d)", key, err);
        nvs_close(handle);
        return false;
    }

    nvs_close(handle);
    return true;
}

bool CMemory::write_nvs(const char *key, const void *data, const size_t data_size)
{
    nvs_handle handle;

    esp_err_t err = nvs_open(MEMORY_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to open nvs (ret=%d)", err);
        return false;
    }

    err = nvs_set_blob(handle, key, data, data_size);
    if (err != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to set nvs blob (%s, ret=%d)", key, err);
        nvs_close(handle);
        return false;
    }

    err = nvs_commit(handle);
    if (err != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to commit nvs (ret=%d)", err);
        nvs_close(handle);
        return false;
    }

    nvs_close(handle);
    return true;
}

bool CMemory::load_ws2812_brightness(uint8_t *brightness)
{
    uint8_t temp;
    if (read_nvs("ws2812_br", &temp, sizeof(uint8_t))) {
        GetLogger(eLogType::Info)->Log("load <ws2812 brightness> from memory: %d", temp);
        *brightness = temp;
    } else{
        return false;
    }

    return true;
}

bool CMemory::save_ws2812_brightness(const uint8_t brightness)
{
    if (write_nvs("ws2812_br", &brightness, sizeof(uint8_t))) {
        GetLogger(eLogType::Info)->Log("save <ws2812 brightness> to memory: %d", brightness);
    } else {
        return false;
    }

    return true;
}

bool CMemory::load_ws2812_color(uint8_t *red, uint8_t *green, uint8_t *blue)
{
    RGB_t rgb;
    if (read_nvs("ws2812_rgb", &rgb, sizeof(RGB_t))) {
        GetLogger(eLogType::Info)->Log("load <ws2812 rgb> from memory");
        *red = rgb.r;
        *green = rgb.g;
        *blue = rgb.b;
    } else{
        return false;
    }

    return true;
}

bool CMemory::save_ws2812_color(const uint8_t red, uint8_t green, uint8_t blue)
{
    RGB_t rgb = RGB_t(red, green, blue);
    if (write_nvs("ws2812_rgb", &rgb, sizeof(RGB_t))) {
        GetLogger(eLogType::Info)->Log("save <ws2812 rgb> to memory");
    } else {
        return false;
    }

    return true;
}