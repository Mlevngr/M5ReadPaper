#include "state_machine_task.h"
#include "state_debug.h"
#include "test/per_file_debug.h"
#include "device/powermgt.h"
#include "globals.h"

void StateMachineTask::handleShutdownState(const SystemMessage_t *msg)
{
#if DBG_STATE_MACHINE_TASK
    sm_dbg_printf("SHUTDOWN状态处理消息: %d (已在关机流程)\n", msg->type);
#endif
    show_shutdown_and_sleep();

    switch (msg->type)
    {
    default:
        // 其它消息在关机状态忽略
        break;
    }
}