// Сетевая часть лицензии: активация ключа на сервере.
// WinHTTP без внешних зависимостей; время работы — один запрос на активацию.

#include "Core/LicenseNet.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

#include <cstdlib>
#include <cstring>

#pragma comment(lib, "winhttp.lib")

namespace
{
    // Адрес сервера по умолчанию. Продавец меняет на свой публичный адрес
    // (или кладёт файл server.txt рядом с license.dat).
    const char* kDefaultServerUrl = "http://127.0.0.1:8000";

    std::wstring ToWide(const std::string& text)
    {
        if (text.empty())
            return std::wstring();
        const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                               static_cast<int>(text.size()), nullptr, 0);
        std::wstring wide(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                            &wide[0], length);
        return wide;
    }

    std::string Trim(const std::string& text)
    {
        size_t begin = 0;
        size_t end = text.size();
        while (begin < end && (text[begin] == ' ' || text[begin] == '\r'
            || text[begin] == '\n' || text[begin] == '\t'))
        {
            ++begin;
        }
        while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\r'
            || text[end - 1] == '\n' || text[end - 1] == '\t'))
        {
            --end;
        }
        return text.substr(begin, end - begin);
    }

    struct Endpoint
    {
        std::wstring host;
        unsigned short port = 80;
        bool secure = false;
    };

    bool ParseEndpoint(const std::string& url, Endpoint& out)
    {
        std::string rest = url;
        out.secure = false;
        out.port = 80;

        if (rest.rfind("http://", 0) == 0)
        {
            rest = rest.substr(7);
        }
        else if (rest.rfind("https://", 0) == 0)
        {
            rest = rest.substr(8);
            out.secure = true;
            out.port = 443;
        }

        const size_t slash = rest.find('/');
        if (slash != std::string::npos)
        {
            rest = rest.substr(0, slash);
        }

        std::string host = rest;
        const size_t colon = rest.rfind(':');
        if (colon != std::string::npos)
        {
            host = rest.substr(0, colon);
            const int port = atoi(rest.substr(colon + 1).c_str());
            if (port > 0)
            {
                out.port = static_cast<unsigned short>(port);
            }
        }

        if (host.empty())
        {
            return false;
        }
        out.host = ToWide(host);
        return true;
    }

    // -- минимальный разбор плоского JSON-ответа ------------------------

    size_t FindValue(const std::string& json, const char* field)
    {
        std::string needle = "\"";
        needle += field;
        needle += "\"";
        size_t pos = json.find(needle);
        if (pos == std::string::npos)
        {
            return std::string::npos;
        }
        pos = json.find(':', pos + needle.size());
        if (pos == std::string::npos)
        {
            return std::string::npos;
        }
        ++pos;
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'))
        {
            ++pos;
        }
        return pos;
    }

    bool JsonGetString(const std::string& json, const char* field, std::string& out)
    {
        const size_t pos = FindValue(json, field);
        if (pos == std::string::npos || pos >= json.size() || json[pos] != '"')
        {
            return false;
        }
        out.clear();
        for (size_t i = pos + 1; i < json.size() && json[i] != '"'; ++i)
        {
            out += json[i];
        }
        return true;
    }

    bool JsonGetBool(const std::string& json, const char* field, bool& out)
    {
        const size_t pos = FindValue(json, field);
        if (pos == std::string::npos)
        {
            return false;
        }
        if (json.compare(pos, 4, "true") == 0)
        {
            out = true;
            return true;
        }
        if (json.compare(pos, 5, "false") == 0)
        {
            out = false;
            return true;
        }
        return false;
    }

    int JsonGetInt(const std::string& json, const char* field, int& out)
    {
        const size_t pos = FindValue(json, field);
        if (pos == std::string::npos || pos >= json.size())
        {
            return false;
        }
        out = atoi(json.c_str() + pos);
        return true;
    }
}

namespace LicenseNet
{
    std::string ServerUrl()
    {
        wchar_t appdata[MAX_PATH] = {};
        const DWORD length = GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
        if (length > 0 && length < MAX_PATH)
        {
            const std::wstring path = std::wstring(appdata) + L"\\18_32_cheat\\server.txt";
            HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file != INVALID_HANDLE_VALUE)
            {
                char buffer[256] = {};
                DWORD read = 0;
                const BOOL ok = ReadFile(file, buffer, sizeof(buffer) - 1, &read, nullptr);
                CloseHandle(file);
                if (ok && read > 0)
                {
                    buffer[read] = '\0';
                    const std::string url = Trim(buffer);
                    if (!url.empty())
                    {
                        return url;
                    }
                }
            }
        }
        return kDefaultServerUrl;
    }

    bool Activate(const std::string& key, const std::string& hwid, ActivateResult& out)
    {
        out = ActivateResult();

        Endpoint endpoint;
        if (!ParseEndpoint(ServerUrl(), endpoint))
        {
            return false;
        }

        const std::string body = "{\"key\":\"" + key + "\",\"hwid\":\"" + hwid + "\"}";

        HINTERNET session = WinHttpOpen(L"1832/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session)
        {
            return false;
        }

        // Короткие таймауты: активация не должна подвешивать кадр надолго.
        WinHttpSetTimeouts(session, 2000, 2000, 2000, 4000);

        HINTERNET connect = WinHttpConnect(session, endpoint.host.c_str(), endpoint.port, 0);
        if (!connect)
        {
            WinHttpCloseHandle(session);
            return false;
        }

        HINTERNET request = WinHttpOpenRequest(connect, L"POST", L"/api/activate", nullptr,
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
                                    WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
                                    WINHTTP_NO_HEADER_INDEX))
            {
                out.httpCode = status;
            }

            std::string response;
            DWORD available = 0;
            while (WinHttpQueryDataAvailable(request, &available) && available > 0)
            {
                std::string chunk(static_cast<size_t>(available), '\0');
                DWORD readBytes = 0;
                if (!WinHttpReadData(request, &chunk[0], available, &readBytes) || readBytes == 0)
                {
                    break;
                }
                chunk.resize(readBytes);
                response += chunk;
                available = 0;
            }

            JsonGetString(response, "status", out.status);
            JsonGetBool(response, "first", out.first);
            JsonGetInt(response, "days_left", out.daysLeft);
            ok = !out.status.empty();
        }

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return ok;
    }
}
