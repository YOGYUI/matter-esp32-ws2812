#include "logger.h"
#include "esp_log.h"
#include <vector>
#include <cstdarg>

CLogger* CLogger::_instance;
static const char *TAG = "logger";

CLogger::CLogger()
{
    m_eLogType = eLogType::Info;
    m_funcname = "";
    m_filename = "";
    m_fileline = 0;
}

CLogger::~CLogger()
{
    
}

CLogger* CLogger::Instance(eLogType logtype, const char* funcname, const char* filename, const unsigned long fileline)
{
    if (!_instance) {
        _instance = new CLogger();
    }

    _instance->m_eLogType = logtype;
    _instance->m_funcname.assign(funcname, strlen(funcname));
    _instance->m_fileline = fileline;

    std::string temp;
    temp.assign(filename, strlen(filename));
    size_t pos = temp.rfind("/");
    if (pos != std::string::npos) {
        _instance->m_filename.assign(temp.substr(pos + 1, temp.length()));
    } else {
        _instance->m_filename.assign("?");
    }

    return _instance;
}

void CLogger::Release()
{
    if (_instance) {
        delete _instance;
        _instance = nullptr;
    }
}

void CLogger::Log(const char* msg, ...)
{
    va_list vaArgs;
    va_start(vaArgs, msg);

    va_list vaArgsCopy;
    va_copy(vaArgsCopy, vaArgs);
    const int len = vsnprintf(nullptr, 0, msg, vaArgsCopy);
    va_end(vaArgsCopy);

    std::vector<char> zc(len + 1);
    vsnprintf(zc.data(), zc.size(), msg, vaArgs);
    va_end(vaArgs);

    Process(std::string(zc.data(), len));
}

void CLogger::Process(std::string msg)
{
    std::string fullmsg;
    size_t colons, begin, end;
    colons = m_funcname.find("::");
    if (colons != std::string::npos) {
        begin = m_funcname.substr(0, colons).rfind(" ") + 1;
        end = m_funcname.rfind("(") - begin;
    } else {
        begin = m_funcname.substr(0, colons).find(" ") + 1;
        end = m_funcname.rfind("(") - begin;
    }
    std::string funcname = m_funcname.substr(begin, end);

    char szlog[256]{0,};
    snprintf(szlog, sizeof(szlog), "[%s] %s [%s:%lu]", funcname.c_str(), msg.c_str(), m_filename.c_str(), m_fileline);
    
    switch (m_eLogType) {
	case eLogType::Info:
		ESP_LOGI(TAG, "%s", szlog);
		break;
	case eLogType::Warning:
        ESP_LOGW(TAG, "%s", szlog);
		break;
	case eLogType::Error:
        ESP_LOGE(TAG, "%s", szlog);
		break;
	case eLogType::Debug:
        ESP_LOGD(TAG, "%s", szlog);
		break;
	case eLogType::Exception:
        ESP_LOGE(TAG, "%s", szlog);
		break;
    default:
        ESP_LOGI(TAG, "%s", szlog);
        break;
	}
}
