#pragma once
#include "VmExitCommon.h"

// EPT 相关处理
void vmexit_ept_violation_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
void vmexit_ept_misconfiguration_handler(__vcpu* vcpu); // 前置声明 VM-exit 分支处理函数，便于后续分发调用
