#pragma once

#include "VmcsControls.h"

struct __vcpu;

void fill_vmcs(__vcpu* vcpu, void* guest_rsp); // 根据 vCPU 状态填充 VMCS（函数声明）
