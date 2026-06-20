#pragma once

#ifndef _DRIVER_IMAGE_PATH_H
#define _DRIVER_IMAGE_PATH_H

#include <string>

namespace DriverImagePath
{
    std::wstring Normalize(std::wstring path);
    bool Same(const std::wstring& left, const std::wstring& right);
    std::wstring ToNativeImagePath(const std::wstring& path);
}

#endif // !_DRIVER_IMAGE_PATH_H
