#pragma once
#include <string>
#include <stdint.h>

// mode: 0=no convert, 1=to simplified, 2=to traditional
void zh_conv_init();
std::string zh_conv_utf8(const std::string &in, uint8_t mode);

// SPIFFS 及运行时动态注册已移除；仅保留嵌入式二分查找表。
