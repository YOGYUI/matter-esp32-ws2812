#include "device_levelcontrol_light.h"
#include "system.h"
#include "logger.h"
#include "ws2812.h"

CDeviceLevelControlLight::CDeviceLevelControlLight()
{
    m_matter_update_by_client_clus_onoff_attr_onoff = false;
    m_matter_update_by_client_clus_levelcontrol_attr_currentlevel = false;
    GetWS2812Ctrl()->set_common_color(255, 255, 255);
    m_state_brightness = MAX(1, GetWS2812Ctrl()->get_brightness());
    m_state_onoff = m_state_brightness ? true : false;
}

bool CDeviceLevelControlLight::matter_add_endpoint()
{
    esp_matter::node_t *root = GetSystem()->get_root_node();
    esp_matter::endpoint::dimmable_light::config_t config_endpoint;
    config_endpoint.on_off.on_off = false;
    config_endpoint.on_off.lighting.start_up_on_off = nullptr;
    config_endpoint.level_control.current_level = m_state_brightness;
    //config_endpoint.level_control.on_level = nullptr;
    //config_endpoint.level_control.options = 0;
    config_endpoint.level_control.lighting.min_level = 1;
    config_endpoint.level_control.lighting.max_level = 254;
    config_endpoint.level_control.lighting.start_up_current_level = m_state_brightness;
    uint8_t flags = esp_matter::ENDPOINT_FLAG_DESTROYABLE;
    m_endpoint = esp_matter::endpoint::dimmable_light::create(root, &config_endpoint, flags, nullptr);
    if (!m_endpoint) {
        GetLogger(eLogType::Error)->Log("Failed to create endpoint");
        return false;
    }

    return CDevice::matter_add_endpoint();;
}

bool CDeviceLevelControlLight::matter_init_endpoint()
{
    matter_update_all_attribute_values();
    
    return true;
}

void CDeviceLevelControlLight::matter_on_change_attribute_value(esp_matter::attribute::callback_type_t type, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *value)
{
    if (type == esp_matter::attribute::callback_type_t::PRE_UPDATE) {
        if (cluster_id == chip::app::Clusters::OnOff::Id) {
            if (attribute_id == chip::app::Clusters::OnOff::Attributes::OnOff::Id) {
                GetLogger(eLogType::Info)->Log("MATTER::PRE_UPDATE >> cluster: OnOff(0x%04X), attribute: OnOff(0x%04X), value: %d", cluster_id, attribute_id, value->val.b);
                if (!m_matter_update_by_client_clus_onoff_attr_onoff) {
                    m_state_onoff = value->val.b;
                    if (m_state_onoff) {
                        GetWS2812Ctrl()->set_brightness(m_state_brightness);
                    } else {
                        GetWS2812Ctrl()->set_brightness(0);
                    }
                } else {
                    // GetLogger(eLogType::Info)->Log("Attribute is updated by this device");
                    m_matter_update_by_client_clus_onoff_attr_onoff = false;
                }
            }
        } else if (cluster_id == chip::app::Clusters::LevelControl::Id) {
            if (attribute_id == chip::app::Clusters::LevelControl::Attributes::CurrentLevel::Id) {
                GetLogger(eLogType::Info)->Log("MATTER::PRE_UPDATE >> cluster: LevelControl(0x%04X), attribute: CurrentLevel(0x%04X), value: %d", cluster_id, attribute_id, value->val.u8);
                if (!m_matter_update_by_client_clus_levelcontrol_attr_currentlevel) {
                    m_state_brightness = value->val.u8;
                    GetWS2812Ctrl()->set_brightness(value->val.u8);
                } else {
                    // GetLogger(eLogType::Info)->Log("Attribute is updated by this device");
                    m_matter_update_by_client_clus_levelcontrol_attr_currentlevel = false;
                }
            }
        }
    }
}

void CDeviceLevelControlLight::matter_update_all_attribute_values()
{
    matter_update_clus_onoff_attr_onoff();
    matter_update_clus_levelcontrol_attr_currentlevel();
}

void CDeviceLevelControlLight::matter_update_clus_onoff_attr_onoff()
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

void CDeviceLevelControlLight::matter_update_clus_levelcontrol_attr_currentlevel()
{
    esp_err_t ret;
    uint32_t cluster_id, attribute_id;
    esp_matter_attr_val_t val;

    m_matter_update_by_client_clus_levelcontrol_attr_currentlevel = true;
    cluster_id = chip::app::Clusters::LevelControl::Id;
    attribute_id = chip::app::Clusters::LevelControl::Attributes::CurrentLevel::Id;
    val = esp_matter_uint8(m_state_brightness);
    ret = esp_matter::attribute::update(m_endpoint_id, cluster_id, attribute_id, &val);
    if (ret != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to update attribute (%d)", ret);
    }
}

void CDeviceLevelControlLight::toggle_state_action()
{
    if (m_state_onoff) {
        GetWS2812Ctrl()->set_brightness(0);
        m_state_onoff = false;
    } else {
        GetWS2812Ctrl()->set_brightness(m_state_brightness);
        m_state_onoff = true;
    }
    matter_update_all_attribute_values();
}
