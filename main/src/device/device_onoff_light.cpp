#include "device_onoff_light.h"
#include "system.h"
#include "logger.h"
#include "ws2812.h"

CDeviceOnOffLight::CDeviceOnOffLight()
{
    m_matter_update_by_client_clus_onoff_attr_onoff = false;
    GetWS2812Ctrl()->set_common_color(255, 255, 255);
    m_state_onoff = GetWS2812Ctrl()->get_brightness() ? true : false;
}

bool CDeviceOnOffLight::matter_add_endpoint()
{
    esp_matter::node_t *root = GetSystem()->get_root_node();
    esp_matter::endpoint::on_off_light::config_t config_endpoint;
    config_endpoint.on_off.on_off = false;
    config_endpoint.on_off.lighting.start_up_on_off = nullptr;
    uint8_t flags = esp_matter::ENDPOINT_FLAG_DESTROYABLE;
    m_endpoint = esp_matter::endpoint::on_off_light::create(root, &config_endpoint, flags, nullptr);
    if (!m_endpoint) {
        GetLogger(eLogType::Error)->Log("Failed to create endpoint");
        return false;
    }

    return CDevice::matter_add_endpoint();;
}

bool CDeviceOnOffLight::matter_init_endpoint()
{
    matter_update_all_attribute_values();
    
    return true;
}

void CDeviceOnOffLight::matter_on_change_attribute_value(esp_matter::attribute::callback_type_t type, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *value)
{
    if (cluster_id == chip::app::Clusters::OnOff::Id) {
        if (attribute_id == chip::app::Clusters::OnOff::Attributes::OnOff::Id) {
            if (type == esp_matter::attribute::callback_type_t::PRE_UPDATE) {
                GetLogger(eLogType::Info)->Log("MATTER::PRE_UPDATE >> cluster: OnOff(0x%04X), attribute: OnOff(0x%04X), value: %d", cluster_id, attribute_id, value->val.b);
                if (!m_matter_update_by_client_clus_onoff_attr_onoff) {
                    if (value->val.b) {
                        GetWS2812Ctrl()->set_brightness(100);
                    } else {
                        GetWS2812Ctrl()->set_brightness(0);
                    }
                } else {
                    GetLogger(eLogType::Info)->Log("Attribute is updated by this device");
                    m_matter_update_by_client_clus_onoff_attr_onoff = false;
                }
            } else if (type == esp_matter::attribute::callback_type_t::POST_UPDATE) {
                GetLogger(eLogType::Info)->Log("MATTER::POST_UPDATE >> cluster: OnOff(0x%04X), attribute: OnOff(0x%04X), value: %d", cluster_id, attribute_id, value->val.b);
            }
        }
    }
}

void CDeviceOnOffLight::matter_update_all_attribute_values()
{
    matter_update_clus_onoff_attr_onoff();
}

void CDeviceOnOffLight::matter_update_clus_onoff_attr_onoff()
{
    esp_err_t ret;
    uint32_t cluster_id, attribute_id;
    esp_matter_attr_val_t val;

    m_matter_update_by_client_clus_onoff_attr_onoff = true;
    cluster_id = chip::app::Clusters::OnOff::Id;
    attribute_id = chip::app::Clusters::OnOff::Attributes::OnOff::Id;
    val = esp_matter_bool((bool)m_state_onoff);
    ret = esp_matter::attribute::update(m_endpoint_id, cluster_id, attribute_id, &val);
    if (ret != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to update attribute (%d)", ret);
    }
}

void CDeviceOnOffLight::toggle_state_action()
{
    if (m_state_onoff) {
        GetWS2812Ctrl()->set_brightness(0);
        m_state_onoff = false;
    } else {
        GetWS2812Ctrl()->set_brightness(100);
        m_state_onoff = true;
    }
    matter_update_clus_onoff_attr_onoff();
}