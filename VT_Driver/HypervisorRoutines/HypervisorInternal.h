#pragma once

#include "../Driver.h"
#include "../poolmanager.h"
#include "../Globals.h"
#include "../interrupt.h"
#include "../ntapi.h"
#include "../cpuid.h"
#include "../crx.h"
#include "../msr.h"
#include "../mtrr.h"
#include "../EPT.h"
#include "../hypervisor_routines.h"
#include "../Vmcs/VmcsApi.h"
#include "../vmexit_handler.h"

#define NON_CANONICIAL_ADDRESS_END 0xFFFF800000000000
#define NON_CANONICIAL_ADDRESS_START 0x0000800000000000
