#include "Driver.h"
#include "hypervisor_gateway.h"
#include "AsmCallset.h"
#include "vmcall_reason.h"
#include "poolmanager.h"
#include "Globals.h"

namespace hvgt
{
    namespace
    {
        bool IsVmoffRequired(unsigned int processor_index)
        {
            return g_vmm_context.vcpu != nullptr &&
                processor_index < g_vmm_context.processor_count &&
                g_vmm_context.vcpu[processor_index].vcpu_status.vmm_launched != 0;
        }

        void MarkVmoffCompleted(unsigned int processor_index)
        {
            if (g_vmm_context.vcpu == nullptr || processor_index >= g_vmm_context.processor_count)
            {
                return;
            }

            auto& vcpu = g_vmm_context.vcpu[processor_index];
            vcpu.vcpu_status.vmm_launched = 0;
            vcpu.vcpu_status.vmx_on = 0;
            vcpu.vmx_off_state.vmx_off_executed = 0;
            vcpu.vmx_off_state.vmx_off_completed = 0;
        }

        bool IsVcpuQuiescent(__vcpu const& vcpu)
        {
            return hv::vmm_release_policy::IsVcpuQuiescent(
                vcpu.vcpu_status.vmx_on,
                vcpu.vcpu_status.vmm_launched,
                vcpu.vmx_off_state.vmx_off_executed);
        }
    }

    bool vmoff(unsigned __int32 processor_count)
    {
        NT_ASSERT(KeGetCurrentIrql() <= APC_LEVEL);
        if (KeGetCurrentIrql() > APC_LEVEL)
        {
            outDebug("VMXOFF 失败：当前 IRQL 过高，无法安全切换 CPU 亲和性。\n");
            return false;
        }
        if (processor_count != 0 && g_vmm_context.vcpu == nullptr)
        {
            return false;
        }

        unsigned int safe_processor_count = processor_count;
        bool all_completed = processor_count <= g_vmm_context.processor_count;
        if (g_vmm_context.processor_count < safe_processor_count)
        {
            safe_processor_count = g_vmm_context.processor_count;
        }

        for (unsigned int iter = 0; iter < safe_processor_count; ++iter)
        {
            auto& vcpu = g_vmm_context.vcpu[iter];
            if (!IsVmoffRequired(iter))
            {
                if (vcpu.vcpu_status.vmm_launched == 0 &&
                    vcpu.vmx_off_state.vmx_off_executed != 0 &&
                    vcpu.vmx_off_state.vmx_off_completed != 0)
                {
                    MarkVmoffCompleted(iter);
                    continue;
                }

                if (!IsVcpuQuiescent(vcpu))
                {
                    all_completed = false;
                    outDebug("VMXOFF 无法通过VMCALL处理当前VCPU状态：CPU=%u VmxOn=%llu Launched=%llu Requested=%llu Completed=%llu\n",
                        iter,
                        vcpu.vcpu_status.vmx_on,
                        vcpu.vcpu_status.vmm_launched,
                        vcpu.vmx_off_state.vmx_off_executed,
                        vcpu.vmx_off_state.vmx_off_completed);
                }
                continue;
            }

            vcpu.vmx_off_state.vmx_off_completed = 0;
            GROUP_AFFINITY original_affinity = {};
            KeSetSystemGroupAffinityThread(&vcpu.processor_affinity, &original_affinity);
            bool vmoff_completed = false;
            __try
            {
                vmoff_completed = __vm_call(VMCALL_VMXOFF, 0, 0, 0);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                outDebug("VMCALL_VMXOFF 异常：CPU=%u Code=0x%X\n", iter, GetExceptionCode());
                all_completed = false;
            }
            KeRevertToUserGroupAffinityThread(&original_affinity);

            if (vmoff_completed &&
                vcpu.vcpu_status.vmm_launched == 0 &&
                vcpu.vmx_off_state.vmx_off_executed != 0 &&
                vcpu.vmx_off_state.vmx_off_completed != 0)
            {
                MarkVmoffCompleted(iter);
            }
            else
            {
                all_completed = false;
                outDebug("VMCALL_VMXOFF 未确认完成：CPU=%u Return=%u Launched=%llu Requested=%llu Completed=%llu\n",
                    iter,
                    vmoff_completed ? 1u : 0u,
                    vcpu.vcpu_status.vmm_launched,
                    vcpu.vmx_off_state.vmx_off_executed,
                    vcpu.vmx_off_state.vmx_off_completed);
            }
        }

        for (unsigned int iter = 0; iter < safe_processor_count; ++iter)
        {
            if (!IsVcpuQuiescent(g_vmm_context.vcpu[iter]))
            {
                all_completed = false;
            }
        }

        return all_completed;
    }
}