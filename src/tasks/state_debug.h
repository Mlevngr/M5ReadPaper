// Lightweight debug print helper for state machine (timestamped)
#pragma once
#include <M5Unified.h>
inline void sm_dbg_printf(const char *fmt, ...)
{
    char buf[256];
    int n = snprintf(buf, sizeof(buf), "[STATE_MACHINE %lu] ", millis());
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);
    va_end(ap);
    Serial.printf("%s", buf);
}