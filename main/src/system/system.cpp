#include "system.h"
#include "logger.h"
#include "memory.h"
#include "definition.h"
#include <nvs_flash.h>
#include <esp_matter_core.cpp>  // include definition to config nvs
#include <esp_matter_bridge.h>
#include <esp_matter_feature.h>
#include <esp_netif.h>
#include "device_onoff_light.h"

CSystem* CSystem::_instance = nullptr;
bool CSystem::m_default_btn_pressed_long = false;
bool CSystem::m_commisioning_session_working = false;

CSystem::CSystem() 
{
    m_root_node = nullptr;
    m_handle_default_btn = nullptr;
    m_device_list.clear();
}

CSystem::~CSystem()
{
    if (_instance) {
        delete _instance;
        _instance = nullptr;
    }
}

CSystem* CSystem::Instance()
{
    if (!_instance) {
        _instance = new CSystem();
    }

    return _instance;
}

bool CSystem::initialize()
{
    GetLogger(eLogType::Info)->Log("Start Initializing System");
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to initialize nsv flash (%d)", ret);
        return false;
    }

    if (!init_default_button()) {
        GetLogger(eLogType::Warning)->Log("Failed to init default on-board button");
    }

    // create matter root node
    esp_matter::node::config_t node_config;
    snprintf(node_config.root_node.basic_information.node_label, sizeof(node_config.root_node.basic_information.node_label), PRODUCT_NAME);
    m_root_node = esp_matter::node::create(&node_config, matter_attribute_update_callback, matter_identification_callback);
    if (!m_root_node) {
        GetLogger(eLogType::Error)->Log("Failed to create root node");
        return false;
    }
    GetLogger(eLogType::Info)->Log("Root node (endpoint 0) added");

    // prevent endpoint id increment when board reset
    matter_set_min_endpoint_id(1);
    
    // start matter
    ret = esp_matter::start(matter_event_callback);
    if (ret != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to start matter (ret: %d)", ret);
        return false;
    }
    GetLogger(eLogType::Info)->Log("Matter started");

    // enable chip shell
    esp_matter::console::diagnostics_register_commands();
    esp_matter::console::init();

    // set matter endpoints
    CDeviceOnOffLight *dev = new CDeviceOnOffLight();
    if (dev && dev->matter_add_endpoint()) {
        m_device_list.push_back(dev);
    }

    GetLogger(eLogType::Info)->Log("System Initialized");
    print_system_info();

    return true;
}

void CSystem::release()
{
    deinit_default_button();
}

void CSystem::callback_default_button(void *arg, void *data)
{
    button_event_t event = iot_button_get_event(arg);

//  GetLogger(eLogType::Info)->Log("button callback event: %d", event);
    switch (event) {
    case BUTTON_PRESS_DOWN: // 0
        break;
    case BUTTON_PRESS_UP:   // 1
        if (m_default_btn_pressed_long) {
            _instance->factory_reset();
        }
        m_default_btn_pressed_long = false;
        break;
    case BUTTON_SINGLE_CLICK:   // 4
        _instance->print_system_info();
        break;
    case BUTTON_DOUBLE_CLICK:   // 5
        _instance->print_matter_endpoints_info();
        break;
    case BUTTON_LONG_PRESS_START:   // 6
        m_default_btn_pressed_long = true;
        GetLogger(eLogType::Info)->Log("ready to factory reset");
        break;
    case BUTTON_LONG_PRESS_HOLD:    // 7
        m_default_btn_pressed_long = true;
        break;
    default:
        break;
    }
}

bool CSystem::init_default_button()
{
    button_config_t cfg = button_config_t();
    cfg.type = BUTTON_TYPE_GPIO;
    cfg.long_press_time = 5000;
    cfg.short_press_time = 180;
    cfg.gpio_button_config.gpio_num = GPIO_PIN_DEFAULT_BTN;
    cfg.gpio_button_config.active_level = 0; // active low (zero level when pressed)

    m_handle_default_btn = iot_button_create(&cfg);
    if (!m_handle_default_btn) {
        GetLogger(eLogType::Error)->Log("Failed to create iot button");
        return false;
    }

    iot_button_register_cb(m_handle_default_btn, BUTTON_PRESS_DOWN, callback_default_button, nullptr);
    iot_button_register_cb(m_handle_default_btn, BUTTON_PRESS_UP, callback_default_button, nullptr);
    iot_button_register_cb(m_handle_default_btn, BUTTON_SINGLE_CLICK, callback_default_button, nullptr);
    iot_button_register_cb(m_handle_default_btn, BUTTON_DOUBLE_CLICK, callback_default_button, nullptr);
    iot_button_register_cb(m_handle_default_btn, BUTTON_LONG_PRESS_START, callback_default_button, nullptr);
    iot_button_register_cb(m_handle_default_btn, BUTTON_LONG_PRESS_HOLD, callback_default_button, nullptr);
    
    return true;
}

bool CSystem::deinit_default_button()
{
    if (m_handle_default_btn) {
        iot_button_delete(m_handle_default_btn);
        return true;
    }

    return false;
}

/** 
* matter cd info (vendor id, product id)
* [reference]
* chip/examples/common/pigweed/rpc_services/Device.h
* chip/src/include/platform/DeviceInstanceInfoProvider.h
* chip/src/include/platform/CommissionableDataProvider.h
*/
uint16_t CSystem::matter_get_vendor_id()
{
    uint16_t vendor_id = 0;
    chip::DeviceLayer::DeviceInstanceInfoProvider *dev_info_provider = chip::DeviceLayer::GetDeviceInstanceInfoProvider();
    if (dev_info_provider) {
        chip::ChipError ret = dev_info_provider->GetVendorId(vendor_id);
        if (ret != CHIP_NO_ERROR) {
            GetLogger(eLogType::Error)->Log("Failed to get vendor ID");
        }
    }
    return vendor_id;
}

uint16_t CSystem::matter_get_product_id()
{
    uint16_t product_id = 0;
    chip::DeviceLayer::DeviceInstanceInfoProvider *dev_info_provider = chip::DeviceLayer::GetDeviceInstanceInfoProvider();
    if (dev_info_provider) {
        chip::ChipError ret = dev_info_provider->GetProductId(product_id);
        if (ret != CHIP_NO_ERROR) {
            GetLogger(eLogType::Error)->Log("Failed to get product ID");
        }
    }
    return product_id;
}

uint32_t CSystem::matter_get_setup_passcode()
{
    uint32_t passcode = 0;
    chip::DeviceLayer::CommissionableDataProvider *comm_data_provider = chip::DeviceLayer::GetCommissionableDataProvider();
    if (comm_data_provider) {
        chip::ChipError ret = comm_data_provider->GetSetupPasscode(passcode);
        if (ret != CHIP_NO_ERROR) {
            GetLogger(eLogType::Error)->Log("Failed to get setup passcode");
        }
    }
    return passcode;
}

uint16_t CSystem::matter_get_setup_discriminator()
{
    uint16_t discriminator;
    chip::DeviceLayer::CommissionableDataProvider *comm_data_provider = chip::DeviceLayer::GetCommissionableDataProvider();
    if (comm_data_provider) {
        chip::ChipError ret = comm_data_provider->GetSetupDiscriminator(discriminator);
        if (ret != CHIP_NO_ERROR) {
            GetLogger(eLogType::Error)->Log("Failed to get setup discriminator");
        }
    }
    return discriminator;
}

void CSystem::factory_reset()
{
    esp_err_t ret;
    
    ret = esp_matter::factory_reset();
    if (ret != ESP_OK) {
        GetLogger(eLogType::Error)->Log("Failed to matter factory reset (ret: %d)", ret);
    }
}

void CSystem::print_system_info()
{
    // network interface
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");   // "WIFI_AP_DEF"
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(netif, &ip_info);
    GetLogger(eLogType::Info)->Log("IPv4 Address: %d.%d.%d.%d", IP2STR(&ip_info.ip));
    GetLogger(eLogType::Info)->Log("Gateway: %d.%d.%d.%d", IP2STR(&ip_info.gw));
    // matter related information
    GetLogger(eLogType::Info)->Log("Vendor ID: 0x%04X", matter_get_vendor_id());
    GetLogger(eLogType::Info)->Log("Product ID: 0x%04X", matter_get_product_id());
    GetLogger(eLogType::Info)->Log("Setup Passcode: %d", matter_get_setup_passcode());
    GetLogger(eLogType::Info)->Log("Setup Discriminator: %d", matter_get_setup_discriminator());
}

void CSystem::print_matter_endpoints_info()
{
    if (!m_root_node)
        return;

    uint16_t endpoint_id;
    uint32_t cluster_id, attr_id;
    char attr_id_str_temp[7];
    esp_matter::endpoint_t *endpoint = esp_matter::endpoint::get_first(m_root_node);
    while (endpoint != nullptr) {
        endpoint_id = esp_matter::endpoint::get_id(endpoint);
        GetLogger(eLogType::Info)->Log(">> Endpoint 0x%02X", endpoint_id);
        
        uint8_t dev_type_count;
        uint32_t *dev_type_ids = esp_matter::endpoint::get_device_type_ids(endpoint, &dev_type_count);
        for (uint8_t cnt = 0; cnt < dev_type_count; cnt++) {
            GetLogger(eLogType::Info)->Log("Device Type: 0x%04X", dev_type_ids[cnt]);
        }

        esp_matter::cluster_t *cluster = esp_matter::cluster::get_first(endpoint);
        while (cluster != nullptr) {
            cluster_id = esp_matter::cluster::get_id(cluster);
            GetLogger(eLogType::Info)->Log(">>>> Cluster Id=0x%04X", cluster_id);

            uint32_t feature_map_value = 0;
            esp_matter::attribute_t *attr = esp_matter::attribute::get_first(cluster);
            std::string str_attrs = ">>>>>> Attribute Ids=";
            while (attr != nullptr) {
                attr_id = esp_matter::attribute::get_id(attr);
                if (attr_id == chip::app::Clusters::Globals::Attributes::FeatureMap::Id) {
                    esp_matter_attr_val_t val = esp_matter_invalid(NULL);
                    esp_matter::attribute::get_val(attr, &val);
                    feature_map_value = val.val.u32;
                }
                snprintf(attr_id_str_temp, 7, "0x%04X", attr_id);
                str_attrs += std::string(attr_id_str_temp);
                attr = esp_matter::attribute::get_next(attr);
                if (attr) {
                    str_attrs += ", ";
                }
            }
            GetLogger(eLogType::Info)->Log("%s (feature_map_val=0x%04X)", str_attrs.c_str(), feature_map_value);
            cluster = esp_matter::cluster::get_next(cluster);
        }
        endpoint = esp_matter::endpoint::get_next(endpoint);
    }
}

bool CSystem::matter_set_min_endpoint_id(uint16_t endpoint_id)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(ESP_MATTER_NVS_PART_NAME, ESP_MATTER_NVS_NODE_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_u16(handle, "min_uu_ep_id", endpoint_id);
        nvs_commit(handle);
        nvs_close(handle);
        GetLogger(eLogType::Info)->Log("Set minimum endpoint id as %d", endpoint_id);
    } else {
        GetLogger(eLogType::Warning)->Log("Failed to open node nvs: cannot reset min_uu_ep_id");
        return false;
    }
    return true;
}

bool CSystem::matter_align_endpoint_id()
{
    if (!m_root_node)
        return false;

    uint16_t endpoint_id;
    uint16_t max_endpoint_id = 0;
    esp_matter::endpoint_t *endpoint = esp_matter::endpoint::get_first(m_root_node);
    while (endpoint != nullptr) {
        endpoint_id = esp_matter::endpoint::get_id(endpoint);
        if (endpoint_id > max_endpoint_id) {
            max_endpoint_id = endpoint_id;
        }
        endpoint = esp_matter::endpoint::get_next(endpoint);
    }

    return matter_set_min_endpoint_id(max_endpoint_id + 1);
}

CDevice* CSystem::find_device_by_endpoint_id(uint16_t endpoint_id)
{
    for (auto & dev : m_device_list) {
        if (dev->matter_get_endpoint_id() == endpoint_id) {
            return dev;
        }
    }

    return nullptr;
}

void CSystem::matter_event_callback(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
        if (event->InterfaceIpAddressChanged.Type == chip::DeviceLayer::InterfaceIpChangeType::kIpV6_Assigned) {
            GetLogger(eLogType::Info)->Log("IP Address(v6) assigned");
        } else if (event->InterfaceIpAddressChanged.Type == chip::DeviceLayer::InterfaceIpChangeType::kIpV4_Assigned) {
            // IPv4 주소를 할당받으면 (commision된 AP에서 주소 할당) 웹서버를 (재)시작해준다 
            GetLogger(eLogType::Info)->Log("IP Address(v4) assigned");
        }
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        GetLogger(eLogType::Info)->Log("Commissioning complete");
        m_commisioning_session_working = false;
        break;
    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        GetLogger(eLogType::Error)->Log("Commissioning failed, fail safe timer expired");
        m_commisioning_session_working = false;
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
        GetLogger(eLogType::Info)->Log("Commissioning session started");
        m_commisioning_session_working = true;
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
        GetLogger(eLogType::Info)->Log("Commissioning session stopped");
        m_commisioning_session_working = false;
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
        GetLogger(eLogType::Info)->Log("Commissioning window opened");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
        GetLogger(eLogType::Info)->Log("Commissioning window closed");
        break;
    default:
        break;
    }
}

esp_err_t CSystem::matter_identification_callback(esp_matter::identification::callback_type_t type,  uint16_t endpoint_id, uint8_t effect_id, uint8_t effect_variant, void *priv_data)
{
    GetLogger(eLogType::Info)->Log("Identification callback > type: %d, endpoint_id: %d, effect_id: %d, effect_variant: %d", type, endpoint_id, effect_id, effect_variant);
    
    return ESP_OK;
}

esp_err_t CSystem::matter_attribute_update_callback(esp_matter::attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    /** 
     * esp_matter_attribute_utils.cpp > esp_matter::attribute::update(): Attribute update
     *
     * This API updates the attribute value.
     * After this API is called, the application gets the attribute update callback with `PRE_UPDATE`, then the
     * attribute is updated in the database, then the application get the callback with `POST_UPDATE`.
     */
    GetLogger(eLogType::Info)->Log("attribute update callback > type: %d, endpoint_id: %d, cluster_id: 0x%04X, attribute_id: 0x%04X", 
        type, endpoint_id, cluster_id, attribute_id);
    
    CDevice *device = GetSystem()->find_device_by_endpoint_id(endpoint_id);
    if (device){
        device->matter_on_change_attribute_value(type, cluster_id, attribute_id, val);
    }
    
    return ESP_OK;
}
