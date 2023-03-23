#include "device.h"
#include "logger.h"
#include "system.h"

CDevice::CDevice()
{
    m_state_onoff = false;
}

CDevice::~CDevice()
{

}

bool CDevice::matter_add_endpoint()
{
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
