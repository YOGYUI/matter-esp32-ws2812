#include "device_colorcontrol_light.h"
#include "system.h"
#include "logger.h"
#include "ws2812.h"
#include <esp_matter_endpoint.h>
#include <esp_matter_attribute_utils.h>

CDeviceColorControlLight::CDeviceColorControlLight()
{
    m_matter_update_by_client_clus_onoff_attr_onoff = false;
    m_matter_update_by_client_clus_levelcontrol_attr_currentlevel = false;
    m_matter_update_by_client_clus_colorcontrol_attr_currenthue = false;
    m_matter_update_by_client_clus_colorcontrol_attr_currentsaturation = false;
    m_state_brightness = MAX(1, GetWS2812Ctrl()->get_brightness());
    m_state_onoff = m_state_brightness ? true : false;
}

bool CDeviceColorControlLight::matter_add_endpoint()
{
    esp_matter::node_t *root = GetSystem()->get_root_node();

    esp_matter::endpoint::extended_color_light::config_t config_endpoint;
    config_endpoint.on_off.on_off = false;
    config_endpoint.on_off.lighting.start_up_on_off = nullptr;
    config_endpoint.level_control.current_level = m_state_brightness;
    config_endpoint.level_control.lighting.min_level = 1;
    config_endpoint.level_control.lighting.max_level = 254;
    config_endpoint.level_control.lighting.start_up_current_level = m_state_brightness;
    /**
    * 3.2.7.9. Color Mode Attribute
    * The ColorMode attribute indicates which attributes are currently determining the color of the device.
    * The value of the ColorMode attribute cannot be written directly - 
    * it is set upon reception of any command in section Commands to the appropriate mode for that command.
    */
    /*
    uint8_t color_mode = 0;
    config_endpoint.color_control.color_mode = color_mode;
    config_endpoint.color_control.enhanced_color_mode = color_mode;
    config_endpoint.color_control.color_temperature.startup_color_temperature_mireds = nullptr;
    */

    uint8_t flags = esp_matter::ENDPOINT_FLAG_DESTROYABLE;
    m_endpoint = esp_matter::endpoint::extended_color_light::create(root, &config_endpoint, flags, nullptr);
    if (!m_endpoint) {
        GetLogger(eLogType::Error)->Log("Failed to create endpoint");
        return false;
    }

    return CDevice::matter_add_endpoint();
}

bool CDeviceColorControlLight::matter_init_endpoint()
{
    esp_err_t ret;
    /** 
    * device type ID를 추가해준다
    */
   /*
    esp_matter::endpoint::add_device_type(m_endpoint, ESP_MATTER_EXTENDED_COLOR_LIGHT_DEVICE_TYPE_ID, ESP_MATTER_EXTENDED_COLOR_LIGHT_DEVICE_TYPE_VERSION);
    */

    /** 
    * hue, saturation attribute를 추가해준다
    */
    esp_matter::cluster::color_control::feature::hue_saturation::config_t cfg;
    cfg.current_hue = 0;
    cfg.current_saturation = 0;
    esp_matter::cluster_t *cluster = esp_matter::cluster::get(m_endpoint, chip::app::Clusters::ColorControl::Id);
    ret = esp_matter::cluster::color_control::feature::hue_saturation::add(cluster, &cfg);
    if (ret != ESP_OK) {
        GetLogger(eLogType::Warning)->Log("Failed to add hue_saturation feature (ret: %d)", ret);
    }

    /** 
    * feature map & color capabilities 속성을 바꿔준다 (HS만 활성화)
    * 3.2.5. Features
    * | Bit | Code |     Feature       |
    * |  0  | HS   | Hue/Saturation    |
    * |  1  | EHUE | Enhanced Hue      |
    * |  2  | CL   | Color Loop        |
    * |  3  | XY   | XY                |
    * |  4  | CT   | Color Temperature |
    */
    esp_matter::attribute_t *attribute = esp_matter::attribute::get(cluster, chip::app::Clusters::Globals::Attributes::FeatureMap::Id);
    esp_matter_attr_val_t val = esp_matter_invalid(NULL);
    esp_matter::attribute::get_val(attribute, &val);
    val.val.u32 = 0x13;
    ret = esp_matter::attribute::set_val(attribute, &val);
    if (ret != ESP_OK) {
        GetLogger(eLogType::Warning)->Log("Failed to change feature map value (ret: %d)", ret);
    }
    attribute = esp_matter::attribute::get(cluster, chip::app::Clusters::ColorControl::Attributes::ColorCapabilities::Id);
    esp_matter::attribute::get_val(attribute, &val);
    val.val.u16 = 0x13;
    ret = esp_matter::attribute::set_val(attribute, &val);
    if (ret != ESP_OK) {
        GetLogger(eLogType::Warning)->Log("Failed to change color capabilities value (ret: %d)", ret);
    }

    // matter_update_all_attribute_values();
    
    return true;
}

void CDeviceColorControlLight::matter_on_change_attribute_value(esp_matter::attribute::callback_type_t type, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *value)
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
                    m_matter_update_by_client_clus_levelcontrol_attr_currentlevel = false;
                }
            }
        } else if (cluster_id == chip::app::Clusters::ColorControl::Id) {
            if (attribute_id == chip::app::Clusters::ColorControl::Attributes::CurrentHue::Id) {
                GetLogger(eLogType::Info)->Log("MATTER::PRE_UPDATE >> cluster: ColorControl(0x%04X), attribute: CurrentHue(0x%04X), value: %d", cluster_id, attribute_id, value->val.u8);
                if (!m_matter_update_by_client_clus_colorcontrol_attr_currenthue) {
                    m_state_hue = value->val.u8;
                    int temp = REMAP_TO_RANGE(value->val.u8, 254, 360);
                    GetWS2812Ctrl()->set_hue(temp);
                } else {
                    m_matter_update_by_client_clus_colorcontrol_attr_currenthue = false;
                }
            } else if (attribute_id == chip::app::Clusters::ColorControl::Attributes::CurrentSaturation::Id) {
                GetLogger(eLogType::Info)->Log("MATTER::PRE_UPDATE >> cluster: ColorControl(0x%04X), attribute: CurrentSaturation(0x%04X), value: %d", cluster_id, attribute_id, value->val.u8);
                if (!m_matter_update_by_client_clus_colorcontrol_attr_currentsaturation) {
                    m_state_saturation = value->val.u8;
                    int temp = REMAP_TO_RANGE(value->val.u8, 254, 100);
                    GetWS2812Ctrl()->set_saturation(temp);
                } else {
                    m_matter_update_by_client_clus_colorcontrol_attr_currentsaturation = false;
                }
            }
            /* 
            else if (attribute_id == chip::app::Clusters::ColorControl::Attributes::ColorTemperatureMireds::Id) {
                GetLogger(eLogType::Info)->Log("MATTER::PRE_UPDATE >> cluster: ColorControl(0x%04X), attribute: ColorTemperatureMireds(0x%04X), value: %d", cluster_id, attribute_id, value->val.u8);
                uint32_t temp = REMAP_TO_RANGE_INVERSE(value->val.u16, 1000000);
                GetWS2812Ctrl()->set_temperature(temp);
            }
            */
        }
    }
}

void CDeviceColorControlLight::matter_update_all_attribute_values()
{
    matter_update_clus_onoff_attr_onoff();
    matter_update_clus_levelcontrol_attr_currentlevel();
    matter_update_clus_colorcontrol_attr_currenthue();
    matter_update_clus_colorcontrol_attr_currentsaturation();
}

void CDeviceColorControlLight::matter_update_clus_onoff_attr_onoff()
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

void CDeviceColorControlLight::matter_update_clus_levelcontrol_attr_currentlevel()
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

void CDeviceColorControlLight::matter_update_clus_colorcontrol_attr_currenthue()
{
    esp_err_t ret;
    uint32_t cluster_id, attribute_id;
    esp_matter_attr_val_t val;

    m_matter_update_by_client_clus_colorcontrol_attr_currenthue = true;
    cluster_id = chip::app::Clusters::ColorControl::Id;
    attribute_id = chip::app::Clusters::ColorControl::Attributes::CurrentHue::Id;
    val = esp_matter_uint8(m_state_hue);
    ret = esp_matter::attribute::update(m_endpoint_id, cluster_id, attribute_id, &val);
    if (ret != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to update attribute (%d)", ret);
    }
}

void CDeviceColorControlLight::matter_update_clus_colorcontrol_attr_currentsaturation()
{
    esp_err_t ret;
    uint32_t cluster_id, attribute_id;
    esp_matter_attr_val_t val;

    m_matter_update_by_client_clus_colorcontrol_attr_currentsaturation = true;
    cluster_id = chip::app::Clusters::ColorControl::Id;
    attribute_id = chip::app::Clusters::ColorControl::Attributes::CurrentSaturation::Id;
    val = esp_matter_uint8(m_state_saturation);
    ret = esp_matter::attribute::update(m_endpoint_id, cluster_id, attribute_id, &val);
    if (ret != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to update attribute (%d)", ret);
    }
}

void CDeviceColorControlLight::toggle_state_action()
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
