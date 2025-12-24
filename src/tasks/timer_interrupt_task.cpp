#include "timer_interrupt_task.h"
#include "device_interrupt_task.h"
#include <driver/timer.h>
#include <esp_attr.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 单一的 hw_timer 实现：在 ISR 中用 vTaskNotifyGiveFromISR 唤醒 DeviceInterruptTask
static hw_timer_t* s_timer = nullptr;
static uint32_t s_interval_ms = 0;

// 双周期支持（方案1）：ISR 内轻量计数与 flag
static volatile uint32_t s_tick_count = 0;
static volatile bool s_two_min_flag = false;
static uint32_t s_min_threshold = 0; // 以 tick 为单位的阈值
static const uint32_t ONE_MIN_MS = 60000; // 1 minutes in ms

// 5 秒周期支持 - 文件作用域变量
static volatile uint32_t s_tick_count_5s = 0;
static volatile bool s_five_sec_flag = false;
static uint32_t s_5s_threshold = 0;

static void IRAM_ATTR onTimer() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    TaskHandle_t h = DeviceInterruptTask::getTaskHandle();
    if (h) {
        vTaskNotifyGiveFromISR(h, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }

    // 仅做轻量计数与比较，放到 ISR 中以免增加任务负担
    if (s_min_threshold) {
        s_tick_count++;
        if (s_tick_count >= s_min_threshold) {
            s_tick_count = 0;
            s_two_min_flag = true; // 任务端会读取并清理
        }
    }
    // 5s counter
    if (s_5s_threshold) {
        s_tick_count_5s++;
        if (s_tick_count_5s >= s_5s_threshold) {
            s_tick_count_5s = 0;
            s_five_sec_flag = true;
        }
    }
}

bool TimerInterruptTask::initialize(uint32_t ms) {
    if (s_timer) return true; // already initialized

    const int timer_no = 0;
    const uint32_t prescaler = 80; // 80 MHz / 80 = 1 MHz -> 1 tick = 1 us
    s_timer = timerBegin(timer_no, prescaler, true);
    if (!s_timer) return false;
    timerAttachInterrupt(s_timer, &onTimer, false);
    timerAlarmWrite(s_timer, (uint64_t)ms * 1000ULL, true);
    timerAlarmEnable(s_timer);
    s_interval_ms = ms;

    // 计算 2 分钟阈值（以 tick 计数），向上取整以避免丢失微小时间
    if (ms > 0) {
        s_min_threshold = (ONE_MIN_MS + ms - 1) / ms;
    } else {
        s_min_threshold = 0;
    }

    // 计算 5s 阈值
    // 更改为 1 秒周期支持（原来为 5 秒）
    const uint32_t FIVE_SEC_MS = 1000;
    if (ms > 0) {
        s_5s_threshold = (FIVE_SEC_MS + ms - 1) / ms;
    } else {
        s_5s_threshold = 0;
    }
    return true;
}

void TimerInterruptTask::destroy() {
    if (!s_timer) return;
    timerAlarmDisable(s_timer);
    timerDetachInterrupt(s_timer);
    timerEnd(s_timer);
    s_timer = nullptr;
    s_interval_ms = 0;

    // 清理双周期状态
    s_tick_count = 0;
    s_two_min_flag = false;
    s_min_threshold = 0;
}

// 兼容函数，供 main.cpp 等旧代码使用
bool initializeTimerInterrupt() {
    return TimerInterruptTask::initialize();
}

void destroyTimerInterrupt() {
    TimerInterruptTask::destroy();
}

// 方案1 的接口实现
bool TimerInterruptTask::isTwoMinuteExpired() {
    return s_two_min_flag;
}

void TimerInterruptTask::resetTwoMinuteFlag() {
    s_two_min_flag = false;
}

bool TimerInterruptTask::isFiveSecondExpired() {
    return s_five_sec_flag;
}

void TimerInterruptTask::resetFiveSecondFlag() {
    s_five_sec_flag = false;
}

// 当 2 分钟周期到期时，向状态机发送消息，参考 DeviceInterruptTask::checkBatteryStatus() 的消息构造方式
void TimerInterruptTask::timerInformStatus() {
    SystemMessage_t msg;
    msg.type = MSG_TIMER_MIN_TIMEOUT; // 复用现有类型，若需要可新增 MSG_TIMER_2MIN_TIMEOUT
    msg.timestamp = millis();

    // 目前不需要额外的数据字段，保留 generic
    msg.data.generic.reserved = 0;

    if (!sendStateMachineMessage(msg)) {
#if DBG_TIMER_INTERRUPT_TASK
        Serial.println("[TIMER_INTERRUPT] 发送2分钟消息给状态机失败");
#endif
    } else {
#if DBG_TIMER_INTERRUPT_TASK
        Serial.println("[TIMER_INTERRUPT] 已发送2分钟消息给状态机");
#endif
    }
}

void TimerInterruptTask::timerInformStatus5s() {
    SystemMessage_t msg;
    msg.type = MSG_TIMER_5S_TIMEOUT;
    msg.timestamp = millis();
    msg.data.generic.reserved = 0;

    if (!sendStateMachineMessage(msg)) {
#if DBG_TIMER_INTERRUPT_TASK
        Serial.println("[TIMER_INTERRUPT] 发送5s消息给状态机失败");
#endif
    } else {
#if DBG_TIMER_INTERRUPT_TASK
        Serial.println("[TIMER_INTERRUPT] 已发送5s消息给状态机");
#endif
    }
}
