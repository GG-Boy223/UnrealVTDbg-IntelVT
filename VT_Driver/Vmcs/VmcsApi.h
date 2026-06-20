#pragma once

#include "VmcsControls.h"

struct __vcpu;

void fill_vmcs(__vcpu* vcpu, void* guest_rsp);
