#pragma once

#ifndef CHttpRequestTool_H_
#define CHttpRequestTool_H_

#include <list>
#include <string>

#include "../../Crypter/xorstr.hpp"

namespace Help
{
    class CHttpRequestTool
    {
    public:
        explicit CHttpRequestTool(
            std::string agent = static_cast<const char*>(_xor_(
                "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/74.0.3729.108 Safari/537.36")));
        ~CHttpRequestTool() = default;

        std::string HttpGet(std::string url, int timeOut);
        std::string HttpPost(
            std::string url,
            std::string postData,
            std::list<std::string> headers,
            int timeOut);

    private:
        std::string ExecuteRequest(
            const wchar_t* method,
            const std::string& url,
            const std::string& postData,
            const std::list<std::string>& headers,
            int timeOut);

        std::string agent_;
    };
}

#endif
