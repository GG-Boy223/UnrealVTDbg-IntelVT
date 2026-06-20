#pragma once
#include "VmExitCommon.h"

// EPT 相关处理
void vmexit_ept_violation_handler(__vcpu* vcpu);
void vmexit_ept_misconfiguration_handler(__vcpu* vcpu);
