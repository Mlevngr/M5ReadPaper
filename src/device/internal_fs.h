#pragma once

#include <FS.h>

namespace InternalFS
{
    // Mount internal flash-backed filesystem (LittleFS preferred, fallback to SPIFFS).
    bool begin(bool formatOnFail = true);
    bool isMounted();
    fs::FS &fs();
    const char *label();
}
