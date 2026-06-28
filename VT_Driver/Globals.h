#pragma once // 保证该头文件在同一编译单元中只被包含一次，避免重复定义

#ifndef _GLOBALS_H // 头文件保护宏开始，防止多次包含导致重定义
#define _GLOBALS_H // 定义保护宏

// Globals.h - 全局定义桥接文件
// 原始文件已拆分为4个子模块，本文件仅作为统一入口

#include "Globals/GlobalsConstants.h"
#include "Globals/GlobalsMacros.h"
#include "Globals/GlobalsStructures.h"
#include "Globals/GlobalsState.h"

#endif // !_GLOBALS_H
