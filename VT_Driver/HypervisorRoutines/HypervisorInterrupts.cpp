#include "HypervisorInternal.h"

namespace hv
{
    /// <summary>
    /// 向来宾系统注入中断或异常
    /// </summary>
    /// <param name="vector">中断/异常向量号</param>
    /// <param name="type">中断类型（外部/异常/软件等）</param>
    /// <param name="error_code">需要注入的错误码</param>
    /// <param name="deliver_error_code">是否需要携带错误码</param>
    //void inject_interruption(unsigned __int32 vector, unsigned __int32 type, unsigned __int32 error_code, bool deliver_error_code) // 组装并写入 VM-entry 中断信息，供下一次 VM-entry 注入
    //	{
    //		__vmentry_interrupt_info interrupt = { 0 }; // 定义并清零 VM-entry 中断信息结构体
    //
    //		interrupt.interruption_type = type;       // 设置中断类型（外部中断、异常、软件中断等）
    //		interrupt.interrupt_vector = vector;      // 设置中断向量号（例如 #PF、#GP 等）
    //		interrupt.deliver_error_code = deliver_error_code; // 标记是否需要错误码（某些异常要求）
    //		interrupt.valid = 1;                      // 标记该中断信息有效，VM-entry 时硬件会注入
    //		if (type == INTERRUPT_TYPE_SOFTWARE_EXCEPTION || type == INTERRUPT_TYPE_PRIVILEGED_SOFTWARE_INTERRUPT || type == INTERRUPT_TYPE_SOFTWARE_INTERRUPT) // 软件触发的异常/中断需要记录指令长度
    //			hv::vmwrite<unsigned __int64>(VM_ENTRY_INSTRUCTION_LENGTH, hv::vmread(VM_EXIT_INSTRUCTION_LENGTH)); // 对软件中断/异常，需写入指令长度，使 CPU 正确推进 RIP
    //		if (deliver_error_code == true) // 仅当异常需要错误码时才写入
    //			hv::vmwrite<unsigned __int64>(VM_ENTRY_EXCEPTION_ERROR_CODE, error_code); // 如果该异常需要错误码，则写入 VM-entry 错误码字段
    //		hv::vmwrite<unsigned __int64>(VM_ENTRY_INTERRUPTION_INFO_FIELD, interrupt.all); // 将构造好的中断信息写入 VMCS，等待 VM-entry 注入
    //	}
    void inject_interruption(unsigned __int32 vector, unsigned __int32 type, unsigned __int32 error_code, bool deliver_error_code) // Safe VM-entry event injection.
    {
        __vmentry_interrupt_info pending = { 0 };
        pending.all = static_cast<unsigned __int32>(hv::vmread(VM_ENTRY_INTERRUPTION_INFO_FIELD));
        if (pending.valid)
        {
            return;
        }

        __vmentry_interrupt_info interrupt = { 0 };

        interrupt.interruption_type = type;
        interrupt.interrupt_vector = vector;
        interrupt.deliver_error_code = deliver_error_code ? 1 : 0;
        interrupt.valid = 1;

        if (type == INTERRUPT_TYPE_SOFTWARE_EXCEPTION || type == INTERRUPT_TYPE_PRIVILEGED_SOFTWARE_INTERRUPT || type == INTERRUPT_TYPE_SOFTWARE_INTERRUPT)
        {
            hv::vmwrite<unsigned __int64>(VM_ENTRY_INSTRUCTION_LENGTH, hv::vmread(VM_EXIT_INSTRUCTION_LENGTH));
        }

        if (deliver_error_code == true)
        {
            hv::vmwrite<unsigned __int64>(VM_ENTRY_EXCEPTION_ERROR_CODE, error_code);
        }

        hv::vmwrite<unsigned __int64>(VM_ENTRY_INTERRUPTION_INFO_FIELD, interrupt.all);
    }

    /// <summary>
    /// 通过写复位 I/O 端口触发硬复位
    /// </summary>
    void hard_reset() // 通过 I/O 端口触发硬复位
    {
        __reset_control_register reset_register; // 定义复位控制寄存器结构体
        reset_register.all = __inbyte(RESET_IO_PORT); // 从固定 I/O 端口读取当前复位控制寄存器的值

        reset_register.reset_cpu = 1; // 请求 CPU 复位
        reset_register.system_reset = 1; // 请求系统级复位（硬复位）

        __outbyte(RESET_IO_PORT, reset_register.all); // 将修改后的复位控制值写回 I/O 端口，触发复位
    }
}
