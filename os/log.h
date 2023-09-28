#ifndef LOG_H
#define LOG_H

#include "include.h"

typedef enum
{
    LOG_SEVERE,
    LOG_WARNING,
    LOG_INFO,
    LOG_SUCCESS
} LogSeverity;

void Log(char* Msg, LogSeverity Severity);
void LogChar(char Char, LogSeverity Severity);
void LogDisable();
void LogEnable();

#endif // LOG_H