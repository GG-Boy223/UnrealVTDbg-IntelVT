#include "CommonInternal.h"

namespace Common
{
    //确认CPU型号
    void ConfirmCPUVendor()
    {
        std::array<int, 4> cpui;

        // Calling __cpuid with 0x0 as the function_id argument
        // gets the number of the highest valid function ID.
        __cpuid(cpui.data(), 0);

        // Capture vendor string
        char vendor[0x20];
        memset(vendor, 0, sizeof(vendor));
        *reinterpret_cast<int*>(vendor) = cpui[ebx];
        *reinterpret_cast<int*>(vendor + 4) = cpui[edx];
        *reinterpret_cast<int*>(vendor + 8) = cpui[ecx];
        std::string vendor_ = vendor;
        if (vendor_ == "GenuineIntel")
        {
            isIntel = true;
        }
        else if (vendor_ == "AuthenticAMD")
        {
            isAMD = true;
        }
    }
}
