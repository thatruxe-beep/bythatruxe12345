#include "Loader.hpp"

#include "Http.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace
{
std::wstring Utf8ToWide(const std::string& text)
{
    if (text.empty())
        return std::wstring();
    int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &wide[0], length);
    return wide;
}

std::string WideToUtf8(const std::wstring& text)
{
    if (text.empty())
        return std::string();
    int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string narrow(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &narrow[0], length, nullptr, nullptr);
    return narrow;
}

std::string NormalizeKey(const std::string& key)
{
    std::string out;
    out.reserve(key.size());
    for (char c : key)
    {
        if (c != ' ' && c != '\t')
            out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return out;
}

// -- крошечный разбор плоского JSON-ответа сервера -------------------

size_t FindValue(const std::string& json, const char* field)
{
    std::string needle = "\"";
    needle += field;
    needle += "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos)
        return std::string::npos;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos)
        return std::string::npos;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'))
        ++pos;
    return pos;
}

bool JsonGetString(const std::string& json, const char* field, std::string& out)
{
    size_t pos = FindValue(json, field);
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '"')
        return false;
    ++pos;
    out.clear();
    while (pos < json.size() && json[pos] != '"')
        out += json[pos++];
    return true;
}

bool JsonGetInt(const std::string& json, const char* field, int& out)
{
    size_t pos = FindValue(json, field);
    if (pos == std::string::npos || pos >= json.size())
        return false;
    out = atoi(json.c_str() + pos);
    return true;
}

bool JsonGetBool(const std::string& json, const char* field, bool& out)
{
    size_t pos = FindValue(json, field);
    if (pos == std::string::npos)
        return false;
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
}

// -- настройки (loader.ini) -------------------------------------------

void LoadSettings(LoaderModel& model, const std::string& iniPath)
{
    const std::wstring path = Utf8ToWide(iniPath);
    wchar_t buffer[512] = {};

    GetPrivateProfileStringW(L"loader", L"server", L"http://127.0.0.1:8000", buffer, 512, path.c_str());
    model.server = WideToUtf8(buffer);

    GetPrivateProfileStringW(L"loader", L"game", L"", buffer, 512, path.c_str());
    model.gamePath = WideToUtf8(buffer);

    GetPrivateProfileStringW(L"loader", L"key", L"", buffer, 512, path.c_str());
    const std::string key = NormalizeKey(WideToUtf8(buffer));
    strncpy_s(model.key, key.c_str(), _TRUNCATE);
}

void SaveSettings(const LoaderModel& model, const std::string& iniPath)
{
    const std::wstring path = Utf8ToWide(iniPath);
    WritePrivateProfileStringW(L"loader", L"server", Utf8ToWide(model.server).c_str(), path.c_str());
    WritePrivateProfileStringW(L"loader", L"game", Utf8ToWide(model.gamePath).c_str(), path.c_str());
    WritePrivateProfileStringW(L"loader", L"key", Utf8ToWide(NormalizeKey(model.key)).c_str(), path.c_str());
}

// -- запрос к серверу ---------------------------------------------------

void PerformLicenseRequest(LoaderModel& model, bool activate)
{
    HttpEndpoint endpoint;
    if (!ParseEndpoint(model.server, endpoint))
    {
        model.state = LicenseState::ServerError;
        model.statusText = "Неверный адрес сервера в loader.ini.";
        return;
    }

    const std::string key = NormalizeKey(model.key);
    const std::string body = "{\"key\":\"" + key + "\",\"hwid\":\"" + model.machineId + "\"}";

    unsigned statusCode = 0;
    std::string response;
    if (!HttpPostJson(endpoint, activate ? "/api/activate" : "/api/status", body, statusCode, response))
    {
        model.state = LicenseState::ServerError;
        model.statusText = "Сервер лицензий недоступен. Запустите license-сервер и повторите.";
        return;
    }

    std::string status;
    if (!JsonGetString(response, "status", status))
    {
        model.state = LicenseState::ServerError;
        model.statusText = "Некорректный ответ сервера.";
        return;
    }

    if (status == "ok")
    {
        bool first = false;
        JsonGetBool(response, "first", first);
        model.state = LicenseState::Active;
        model.firstActivation = activate && first;
        JsonGetInt(response, "days_left", model.daysLeft);
        JsonGetInt(response, "duration_days", model.durationDays);
        JsonGetString(response, "expires_at", model.expiresAt);
        JsonGetString(response, "activated_at", model.activatedAt);
        model.statusText.clear();
    }
    else if (status == "unused")
    {
        model.state = LicenseState::Unused;
        model.firstActivation = false;
        JsonGetInt(response, "duration_days", model.durationDays);
        model.daysLeft = model.durationDays;
        model.statusText = "Срок начнётся с момента активации.";
    }
    else if (status == "expired")
    {
        model.state = LicenseState::Expired;
        model.statusText = "Доступ заблокирован. Продление: t.me/thatruxe";
    }
    else if (status == "revoked")
    {
        model.state = LicenseState::Revoked;
        model.statusText = "Ключ отозван. Вопросы: t.me/thatruxe";
    }
    else if (status == "hwid_mismatch")
    {
        model.state = LicenseState::HwidMismatch;
        model.statusText = "Ключ привязан к другому компьютеру. Вопросы: t.me/thatruxe";
    }
    else if (status == "invalid")
    {
        model.state = LicenseState::Invalid;
        model.statusText = "Такого ключа нет. Проверьте ввод.";
    }
    else
    {
        model.state = LicenseState::ServerError;
        model.statusText = "Сервер вернул неизвестный статус.";
    }
}

// -- запуск игры --------------------------------------------------------

bool LaunchGame(LoaderModel& model, std::string& error)
{
    error.clear();
    if (!model.CanLaunch())
    {
        error = "Нет активной подписки.";
        return false;
    }
    if (model.gamePath.empty())
    {
        error = "Укажите путь к gta_sa.exe в loader.ini (game=...).";
        return false;
    }
    if (GetFileAttributesW(Utf8ToWide(model.gamePath).c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        error = "gta_sa.exe не найден: " + model.gamePath;
        return false;
    }

    std::wstring exe = Utf8ToWide(model.gamePath);
    std::wstring directory = exe;
    const size_t slash = directory.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
        directory.resize(slash);

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(exe.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr,
                        directory.c_str(), &si, &pi))
    {
        error = "Не удалось запустить игру (код " + std::to_string(GetLastError()) + ").";
        return false;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

// -- фоновый контроллер ---------------------------------------------------

LoaderController::~LoaderController()
{
    if (thread_.joinable())
        thread_.join();
}

bool LoaderController::Busy() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return busy_;
}

void LoaderController::Start(LoaderModel model, bool activate)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (busy_)
        return;
    if (thread_.joinable())
        thread_.join();

    busy_ = true;
    hasResult_ = false;
    activate_ = activate;
    result_ = std::move(model);

    thread_ = std::thread([this] { Run(); });
}

bool LoaderController::Pump(LoaderModel& model)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!hasResult_)
        return false;
    hasResult_ = false;
    model = result_;
    return true;
}

void LoaderController::Run()
{
    PerformLicenseRequest(result_, activate_);

    std::lock_guard<std::mutex> lock(mutex_);
    busy_ = false;
    hasResult_ = true;
}
