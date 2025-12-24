#include "globals.h"

// Default orientation unknown
volatile int g_device_orientation = ORIENT_UNKNOWN;

// Existing globals
volatile bool g_disable_sd_access = false;
// Auto-read flag: default false
bool autoread = false;
// Auto-read speed default
uint8_t autospeed = 2;

// 字体加载位置: 0=缓存到内存, 1=按需从文件读取
int8_t fontLoadLoc = 1;
