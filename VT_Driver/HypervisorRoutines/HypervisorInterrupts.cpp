#include "HypervisorInternal.h"

namespace hv
{
    void inject_interruption(unsigned __int32 vector, unsigned __int32 type, unsigned __int32 error_code, bool deliver_error_code)
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

    void hard_reset()
    {
        __reset_control_register reset_register;
        reset_register.all = __inbyte(RESET_IO_PORT);

        reset_register.reset_cpu = 1;
        reset_register.system_reset = 1;

        __outbyte(RESET_IO_PORT, reset_register.all);
    }
}
