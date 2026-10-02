#include "Http.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

#include <cstdlib>

#pragma comment(lib, "winhttp.lib")

namespace
{
std::wstring ToWide(const std::string& text)
{
    if (text.empty())
        return std::wstring();
    int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &wide[0], length);
    return wide;
}
}

bool ParseEndpoint(const std::string& url, HttpEndpoint& endpoint)
{
    std::string rest = url;
    endpoint.secure = false;
    endpoint.port = 80;

    if (rest.rfind("http://", 0) == 0)
    {
        rest = rest.substr(7);
    }
    else if (rest.rfind("https://", 0) == 0)
    {
        rest = rest.substr(8);
        endpoint.secure = true;
        endpoint.port = 443;
    }

    const size_t slash = rest.find('/');
    if (slash != std::string::npos)
        rest = rest.substr(0, slash);

    std::string host = rest;
    const size_t colon = rest.rfind(':');
    if (colon != std::string::npos)
    {
        host = rest.substr(0, colon);
        const int port = atoi(rest.substr(colon + 1).c_str());
        if (port > 0)
            endpoint.port = static_cast<unsigned short>(port);
    }

    if (host.empty())
        return false;
    endpoint.host = host;
    return true;
}

bool HttpPostJson(const HttpEndpoint& endpoint, const std::string& path,
                  const std::string& body, unsigned& statusCode, std::string& response)
{
    statusCode = 0;
    response.clear();

    HINTERNET session = WinHttpOpen(L"1832-loader/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)
        return false;

    // Локальный сервер — таймауты короткие, чтобы интерфейс не подвисал.
    WinHttpSetTimeouts(session, 3000, 3000, 3000, 5000);

    HINTERNET connect = WinHttpConnect(session, ToWide(endpoint.host).c_str(), endpoint.port, 0);
    if (!connect)
    {
        WinHttpCloseHandle(session);
        return false;
    }

    HINTERNET request = WinHttpOpenRequest(connect, L"POST", ToWide(path).c_str(), nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           endpoint.secure ? WINHTTP_FLAG_SECURE : 0);
    if (!request)
    {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    const wchar_t* headers = L"Content-Type: application/json\r\n";
    const bool sent = WinHttpSendRequest(request, headers, static_cast<DWORD>(-1),
                                         const_cast<char*>(body.c_str()),
                                         static_cast<DWORD>(body.size()),
                                         static_cast<DWORD>(body.size()), 0) != FALSE;
    const bool received = sent && WinHttpReceiveResponse(request, nullptr);

    bool ok = false;
    if (received)
    {
        DWORD status = 0;
        DWORD size = sizeof(status);
        if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX))
        {
            statusCode = status;
        }

        DWORD available = 0;
        while (WinHttpQueryDataAvailable(request, &available) && available > 0)
        {
            std::string chunk(static_cast<size_t>(available), '\0');
            DWORD read = 0;
            if (!WinHttpReadData(request, &chunk[0], available, &read) || read == 0)
                break;
            chunk.resize(read);
            response += chunk;
            available = 0;
        }
        ok = true;
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return ok;
}
