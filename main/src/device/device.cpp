#include "device.h"
#include "logger.h"
#include "system.h"

CDevice::CDevice()
{
    m_state_onoff = false;
    m_state_brightness = 0;
    m_endpoint = nullptr;
    m_endpoint_id = 0;
}

CDevice::~CDevice()
{

}

bool CDevice::matter_add_endpoint()
{
    esp_err_t ret;
    
    if (m_endpoint != nullptr) {
        matter_init_endpoint();

        // get endpoint id
        m_endpoint_id = esp_matter::endpoint::get_id(m_endpoint);

        ret = esp_matter::endpoint::enable(m_endpoint);  // should be called after esp_matter::start()
        if (ret != ESP_OK) {
            GetLogger(eLogType::Error)->Log("Failed to enable endpoint (%d, ret=%d)", m_endpoint_id, ret);
            matter_destroy_endpoint();
            return false;
        }
    } else {
        GetLogger(eLogType::Error)->Log("endpoint instance is null!");
        return false;
    }

    return true;
}

bool CDevice::matter_init_endpoint()
{
    return true;
}

bool CDevice::matter_destroy_endpoint()
{
    esp_err_t ret;

    esp_matter::node_t *root = GetSystem()->get_root_node();
    ret = esp_matter::endpoint::destroy(root, m_endpoint);
    if (ret != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to destroy endpoint (%d)", ret);
        return false;
    }

    return true;
}

esp_matter::endpoint_t* CDevice::matter_get_endpoint() 
{ 
    return m_endpoint; 
}

uint16_t CDevice::matter_get_endpoint_id()
{ 
    return m_endpoint_id; 
}

void CDevice::matter_on_change_attribute_value(esp_matter::attribute::callback_type_t type, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *value)
{
    
}

void CDevice::matter_update_all_attribute_values()
{

}

void CDevice::toggle_state_action()
{
    
}