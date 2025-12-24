/**
 * @file state_main_menu.h
 * @brief 主菜单状态处理相关的声明和定义
 * 
 * 这个头文件包含了处理主菜单状态时所需的函数声明和常量定义。
 * 主菜单状态是系统的主要导航界面，提供各种功能的入口。
 */

#ifndef STATE_MAIN_MENU_H
#define STATE_MAIN_MENU_H

#include "state_machine_task.h"

/**
 * @brief 处理主菜单状态的系统消息
 * 
 * 该函数负责处理在主菜单状态下接收到的各种系统消息，包括：
 * - 超时消息：控制自动锁屏机制
 * - 触摸消息：处理用户交互
 * - 用户活动消息：更新活动时间戳
 * - 电池/电源相关消息：处理电源状态变化
 * 
 * @param msg 指向系统消息结构体的指针
 */
// 注意：实际的函数定义在 StateMachineTask 类中，这里只是文档说明

// 主菜单相关的常量定义（如有需要可在此处添加）

#endif // STATE_MAIN_MENU_H