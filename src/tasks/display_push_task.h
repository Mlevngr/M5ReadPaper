#pragma once
#include <M5Unified.h>
#include <stdint.h>

// 初始化显示推送任务（创建队列并启动任务）
bool initializeDisplayPushTask(size_t queue_len = 8);

// 销毁显示推送任务
void destroyDisplayPushTask();

// 向显示推送队列中推入一个简单消息（目前仅消息类型，使用 uint8_t）
bool enqueueDisplayPush(uint8_t msgType);

// 重置pushSprite计数器
void resetDisplayPushCount();

// 获取当前pushSprite计数
uint32_t getDisplayPushCount();

// 常量：推送消息类型
static const uint8_t DISPLAY_PUSH_MSG_TYPE_FLUSH = 1;
static const uint8_t DISPLAY_PUSH_MSG_TYPE_FLUSH_TRANS = 2;
static const uint8_t DISPLAY_PUSH_MSG_TYPE_FLUSH_INVERT_TRANS = 3;
static const uint8_t DISPLAY_PUSH_MSG_TYPE_FLUSH_QUALITY = 4;
