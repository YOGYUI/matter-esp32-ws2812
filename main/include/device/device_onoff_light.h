#pragma once
#ifndef _DEVICE_ONOFF_LIGHT_H_
#define _DEVICE_ONOFF_LIGHT_H_

#include "device.h"

class CDeviceOnOffLight : public CDevice
{
public:
    CDeviceOnOffLight();

    bool matter_add_endpoint() override;
    bool matter_init_endpoint() override;
    void matter_on_change_attribute_value(
        esp_matter::attribute::callback_type_t type,
        uint32_t cluster_id,
        uint32_t attribute_id,
        esp_matter_attr_val_t *value
    ) override;
    void matter_update_all_attribute_values() override;

private:
    bool m_matter_update_by_client_clus_onoff_attr_onoff;

    void matter_update_clus_onoff_attr_onoff();
};

#endif