#pragma once

// Pure decision helpers shared by the kernel drivers and the user-mode
// simulation. Keep this file free of WDK-only types so the same logic can be
// compiled and exercised without a VMX-capable machine.

namespace hvm_debug_visibility
{
    enum class ProcessorMode : unsigned long
    {
        Kernel = 0,
        User = 1,
    };

    constexpr unsigned long kProcessDebugPort = 7;
    constexpr unsigned long kProcessDebugObjectHandle = 0x1E;
    constexpr unsigned long kProcessDebugFlags = 0x1F;

    constexpr unsigned long kThreadHideFromDebugger = 0x11;
    constexpr unsigned long kThreadWow64Context = 29;

    constexpr unsigned long kSystemKernelDebuggerInformation = 0x23;
    constexpr unsigned long kSystemKernelDebuggerInformationEx = 0x95;
    constexpr unsigned long kSystemHypervisorInformation = 91;
    constexpr unsigned long kSystemHandleInformation = 16;
    constexpr unsigned long kSystemExtendedHandleInformation = 64;
    constexpr unsigned long kSystemProcessorFeaturesInformation = 154;
    constexpr unsigned long kProcessorFeatureVirtualFirmwareEnabled = 21;

    constexpr unsigned long kThreadCreateFlagsHideFromDebugger = 0x00000004;
    constexpr unsigned long kWow64ContextDebugRegisterEndOffset = 0x1C;

    constexpr unsigned long long kCurrentProcessPseudoHandle = 0xFFFFFFFFFFFFFFFFULL;
    constexpr unsigned long long kCurrentThreadPseudoHandle = 0xFFFFFFFFFFFFFFFEULL;
    constexpr unsigned long long kWow64CurrentProcessHandle = 0x00000000FFFFFFFFULL;
    constexpr unsigned long long kWow64CurrentThreadHandle = 0x00000000FFFFFFFEULL;

    inline ProcessorMode ModeFromKernelFlag(bool kernel_mode)
    {
        return kernel_mode ? ProcessorMode::Kernel : ProcessorMode::User;
    }

    inline bool IsUserMode(ProcessorMode mode)
    {
        return mode == ProcessorMode::User;
    }

    inline bool IsCurrentProcessPseudoHandle(unsigned long long handle_value)
    {
        return handle_value == kCurrentProcessPseudoHandle ||
            handle_value == kWow64CurrentProcessHandle;
    }

    inline bool IsCurrentThreadPseudoHandle(unsigned long long handle_value)
    {
        return handle_value == kCurrentThreadPseudoHandle ||
            handle_value == kWow64CurrentThreadHandle;
    }

    inline bool IsProcessDebugObjectHandleQuery(
        ProcessorMode mode,
        unsigned long information_class,
        unsigned long long information_length)
    {
        return IsUserMode(mode) &&
            information_class == kProcessDebugObjectHandle &&
            information_length >= sizeof(unsigned long);
    }

    inline bool ShouldHideProcessDebugObjectHandle(
        ProcessorMode mode,
        unsigned long information_class,
        unsigned long long information_length,
        bool target)
    {
        return target && IsProcessDebugObjectHandleQuery(
            mode,
            information_class,
            information_length);
    }

    inline unsigned long SelectedHandleOutputWidth(
        unsigned long long information_length)
    {
        if (information_length >= sizeof(unsigned long long))
        {
            return sizeof(unsigned long long);
        }
        if (information_length >= sizeof(unsigned long))
        {
            return sizeof(unsigned long);
        }
        return 0;
    }

    inline bool ShouldUseDebugFlagsTrue(
        ProcessorMode mode,
        unsigned long information_class,
        unsigned long long information_length,
        bool target)
    {
        return IsUserMode(mode) &&
            information_class == kProcessDebugFlags &&
            information_length >= sizeof(unsigned long) &&
            target;
    }

    inline bool ShouldUseDebugPortZero(
        ProcessorMode mode,
        unsigned long information_class,
        unsigned long long information_length,
        bool target)
    {
        return IsUserMode(mode) &&
            information_class == kProcessDebugPort &&
            information_length >= sizeof(unsigned long) &&
            target;
    }

    inline bool ShouldHideKernelDebuggerInformation(
        ProcessorMode mode,
        unsigned long information_class,
        unsigned long long information_length,
        bool target)
    {
        return IsUserMode(mode) &&
            information_class == kSystemKernelDebuggerInformation &&
            information_length >= 2 * sizeof(unsigned char) &&
            target;
    }

    inline bool ShouldHideKernelDebuggerInformationEx(
        ProcessorMode mode,
        unsigned long information_class,
        unsigned long long information_length,
        bool target)
    {
        return IsUserMode(mode) &&
            information_class == kSystemKernelDebuggerInformationEx &&
            information_length >= 3 * sizeof(unsigned char) &&
            target;
    }

    inline bool ShouldClearVirtualFirmwareFeature(
        ProcessorMode mode,
        unsigned long information_class,
        unsigned long long information_length,
        bool target)
    {
        return IsUserMode(mode) &&
            information_class == kSystemProcessorFeaturesInformation &&
            information_length >= sizeof(unsigned long long) &&
            target;
    }

    inline bool ShouldHideHypervisorInformation(
        ProcessorMode mode,
        unsigned long information_class,
        unsigned long long information_length,
        unsigned long long hypervisor_query_size,
        bool target)
    {
        return IsUserMode(mode) &&
            information_class == kSystemHypervisorInformation &&
            information_length >= hypervisor_query_size &&
            target;
    }

    inline bool ShouldRewriteHandleObjectTypeIndex(
        ProcessorMode mode,
        unsigned long information_class,
        bool target)
    {
        return IsUserMode(mode) &&
            (information_class == kSystemHandleInformation ||
                information_class == kSystemExtendedHandleInformation) &&
            target;
    }

    inline unsigned long long ClearVirtualFirmwareFeatureBit(
        unsigned long long processor_feature_bits)
    {
        return processor_feature_bits &
            ~(1ULL << kProcessorFeatureVirtualFirmwareEnabled);
    }

    inline bool ShouldHideThreadFromDebuggerQuery(
        ProcessorMode mode,
        unsigned long information_class,
        unsigned long long information_length,
        bool caller_target,
        bool thread_target)
    {
        return IsUserMode(mode) &&
            information_class == kThreadHideFromDebugger &&
            information_length >= sizeof(unsigned char) &&
            caller_target &&
            thread_target;
    }

    inline bool ShouldStripThreadDebugRegisters(
        ProcessorMode mode,
        unsigned long information_class,
        unsigned long long information_length,
        bool caller_target,
        bool thread_target)
    {
        return IsUserMode(mode) &&
            information_class == kThreadWow64Context &&
            information_length >= kWow64ContextDebugRegisterEndOffset &&
            caller_target &&
            thread_target;
    }

    inline bool ShouldStripNativeThreadContext(
        ProcessorMode mode,
        bool caller_target,
        bool thread_target)
    {
        return IsUserMode(mode) && caller_target && thread_target;
    }

    inline bool ShouldStripThreadContextSet(
        ProcessorMode mode,
        bool caller_target)
    {
        return IsUserMode(mode) && caller_target;
    }

    inline bool ShouldStripWow64ContextSet(
        ProcessorMode mode,
        unsigned long information_class,
        unsigned long long information_length,
        bool caller_target)
    {
        return IsUserMode(mode) &&
            information_class == kThreadWow64Context &&
            information_length >= kWow64ContextDebugRegisterEndOffset &&
            caller_target;
    }

    inline bool ShouldSuppressThreadHideFromDebuggerSet(
        ProcessorMode mode,
        unsigned long information_class,
        unsigned long long information_length,
        bool caller_target)
    {
        return IsUserMode(mode) &&
            information_class == kThreadHideFromDebugger &&
            information_length == 0 &&
            caller_target;
    }

    inline bool ShouldStripThreadCreateHideFlag(
        ProcessorMode mode,
        unsigned long create_flags,
        bool caller_target)
    {
        return IsUserMode(mode) &&
            (create_flags & kThreadCreateFlagsHideFromDebugger) != 0 &&
            caller_target;
    }

    inline unsigned int BuildVmcallRipCandidates(
        unsigned long long guest_rip,
        unsigned long long instruction_length,
        unsigned long long candidates[2])
    {
        candidates[0] = guest_rip;
        if (instruction_length == 0 ||
            instruction_length > 15 ||
            guest_rip < instruction_length)
        {
            return 1;
        }

        candidates[1] = guest_rip - instruction_length;
        return candidates[1] == candidates[0] ? 1 : 2;
    }

    // The executing guest address space is authoritative for decoding a
    // VMCALL fetched from an EPT shadow page. The cached system CR3 belongs to
    // the VMM host context and is only a fallback when GUEST_CR3 is unavailable.
    inline unsigned long long SelectVmcallTranslationCr3(
        unsigned long long guest_cr3,
        unsigned long long host_system_cr3)
    {
        return guest_cr3 != 0 ? guest_cr3 : host_system_cr3;
    }

    inline bool IsVmcallInstruction(const unsigned char* instruction)
    {
        return instruction != nullptr &&
            instruction[0] == 0x0F &&
            instruction[1] == 0x01 &&
            instruction[2] == 0xC1;
    }

    inline bool IsVmxCapabilityMsr(unsigned long msr_index)
    {
        // IA32_VMX_BASIC (0x480) through IA32_VMX_PROCBASED_CTLS3 (0x492).
        return msr_index >= 0x480 && msr_index <= 0x492;
    }

    inline unsigned long long AlignUp(
        unsigned long long value,
        unsigned long long alignment)
    {
        if (alignment == 0)
        {
            return value;
        }
        return (value + alignment - 1) & ~(alignment - 1);
    }

    inline unsigned long long ObjectTypeEntrySize(
        unsigned long long fixed_size,
        unsigned long long name_length,
        unsigned long long pointer_size)
    {
        return AlignUp(fixed_size + name_length, pointer_size);
    }
}
