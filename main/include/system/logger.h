#pragma once
#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

#define MAXLEN_LOG_MSG  1024

typedef enum
{
    Info = 0,
	Warning,
	Error,
	Debug,
	Exception
} eLogType;

class CLogger
{
public:
    CLogger();
    virtual ~CLogger();

public:
    static CLogger* Instance(eLogType logtype, const char* funcname, const char* filename, const unsigned long fileline);
    static void Release();
    void Log(const char* msg, ...);

private:
    static CLogger* _instance;
    eLogType m_eLogType;
    std::string m_funcname;
    std::string m_filename;
    unsigned long m_fileline;

    void Process(std::string msg);
};

inline CLogger* _GetLogger(eLogType logtype, const char* funcname, const char* filename, const unsigned long fileline) {
    return CLogger::Instance(logtype, funcname, filename, fileline);
}

inline void ReleaseLogger() {
    CLogger::Release();
}

#define GetLoggerBase() _GetLogger(eLogType::Info, __PRETTY_FUNCTION__, __FILE__, __LINE__)
#define GetLogger(n) _GetLogger(n, __PRETTY_FUNCTION__, __FILE__, __LINE__)

#ifdef __cplusplus
};
#endif

#endif