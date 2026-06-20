#pragma once
#include "../Driver.h"

// I/O 指令相关的 VM-Exit 处理函数
// 注：当前项目中未启用 I/O bitmap，因此没有 I/O 指令的 VM-Exit
// 如果未来需要拦截 I/O 指令，可在此处声明处理函数
