#include "CommonInternal.h"

namespace Common
{
    //int转wstring
    std::wstring IntToWString(int value)
    {
        return std::to_wstring(value);
    }

    //wstring转int
    int WStringToInt(const std::wstring& str)
    {
        return std::stoi(str);
    }
}
