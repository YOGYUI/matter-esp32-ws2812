#pragma once
#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <stdint.h>
#include "definition.h"
#include <esp_matter.h>
#include <esp_matter_core.h>

class CDevice
{
public:
    CDevice();
    virtual ~CDevice();

protected:
    esp_matter::endpoint_t *m_endpoint;
    uint16_t m_endpoint_id;
    bool m_state_onoff;

public:
    virtual bool matter_add_endpoint();
    virtual bool matter_init_endpoint();
    bool matter_destroy_endpoint();
    esp_matter::endpoint_t* matter_get_endpoint();
    uint16_t matter_get_endpoint_id();
    virtual void matter_on_change_attribute_value(
        esp_matter::attribute::callback_type_t type,
        uint32_t cluster_id,
        uint32_t attribute_id,
        esp_matter_attr_val_t *value
    );
    virtual void matter_update_all_attribute_values();
};

#endif