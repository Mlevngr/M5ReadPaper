#include "internal_fs.h"

#if defined(USE_LITTLEFS) && __has_include(<LittleFS.h>)
#include <LittleFS.h>
#define INTERNAL_FS_INSTANCE LittleFS
#define INTERNAL_FS_NAME "LittleFS"
#else
#include <SPIFFS.h>
#define INTERNAL_FS_INSTANCE SPIFFS
#define INTERNAL_FS_NAME "SPIFFS"
#endif

namespace InternalFS
{
    namespace
    {
        bool mounted = false;
    }

    bool begin(bool formatOnFail)
    {
        if (mounted)
        {
            return true;
        }

        mounted = INTERNAL_FS_INSTANCE.begin(formatOnFail);
        return mounted;
    }

    bool isMounted()
    {
        return mounted;
    }

    fs::FS &fs()
    {
        return INTERNAL_FS_INSTANCE;
    }

    const char *label()
    {
        return INTERNAL_FS_NAME;
    }
}
