#pragma once
#include <stdint.h>

// 简单的定时器中断管理，用硬件定时器唤醒 DeviceInterruptTask
// 提供 initialize(ms) 和 destroy()
namespace TimerInterruptTask {
    // 初始化硬件定时器，ms 为周期（毫秒），返回是否成功
    bool initialize(uint32_t ms = 20);

    // 销毁/禁用定时器
    void destroy();

    // 方案1 额外接口：用于两个周期（高频唤醒 + 低频2分钟事件）
    // 返回是否 2 分钟周期到期（由 ISR 内的轻量计数器设置 flag）
    bool isTwoMinuteExpired();

    // 重置 2 分钟到期 flag（在任务上下文清理）
    void resetTwoMinuteFlag();

    // Five-second cycle support
    bool isFiveSecondExpired();
    void resetFiveSecondFlag();
    void timerInformStatus5s();

    // 当 2 分钟周期到期时，向状态机发送相应消息（封装为全局函数）
    void timerInformStatus();
}

// 兼容旧的全局函数接口（main.cpp 使用）
bool initializeTimerInterrupt();
void destroyTimerInterrupt();
