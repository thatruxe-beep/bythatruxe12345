// Win32-часть лицензии: хранение состояния (реестр + файл), время,
// машина состояний и завершение игры при неверном ключе.

#include "Core/License.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>

namespace
{
    // Секрет для подписи ключей (XOR-маска, чтобы не лежал в открытом виде).
    const unsigned char kSecretMasked[32] = {
        0xB8, 0x62, 0x27, 0x50, 0x57, 0x15, 0x0F, 0x5A,
        0x29, 0x33, 0x00, 0xEA, 0x6F, 0x39, 0x0A, 0xAA,
        0xE3, 0x48, 0x74, 0x70, 0xFD, 0xA2, 0x16, 0x7D,
        0xE5, 0x9E, 0xF2, 0xBF, 0xF1, 0x1E, 0x5F, 0xE1,
    };

    const uint8_t* Secret()
    {
        static uint8_t secret[32];
        static bool initialized = false;
        if (!initialized)
        {
            for (int i = 0; i < 32; ++i)
                secret[i] = kSecretMasked[i] ^ 0x5A;
            initialized = true;
        }
        return secret;
    }

    constexpr int64_t kRollbackToleranceSeconds = 300; // 5 минут на перевод часов
    constexpr int64_t kWrongKeyExitDelay = 2;          // секунды до закрытия игры
    constexpr int64_t kBlockedExitDelay = 3;

    int64_t NowUnix()
    {
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        ULARGE_INTEGER large;
        large.LowPart = ft.dwLowDateTime;
        large.HighPart = ft.dwHighDateTime;
        return static_cast<int64_t>((large.QuadPart - 116444736000000000ull) / 10000000ull);
    }

    // ------------------------------------------------------------- хранилище

    std::string WideToUtf8(const std::wstring& text)
    {
        if (text.empty())
            return std::string();
        const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                               nullptr, 0, nullptr, nullptr);
        std::string out(static_cast<size_t>(length), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &out[0], length,
                            nullptr, nullptr);
        return out;
    }

    std::wstring Utf8ToWide(const std::string& text)
    {
        if (text.empty())
            return std::wstring();
        const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                               nullptr, 0);
        std::wstring out(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &out[0], length);
        return out;
    }

    std::string LoadRegistryBlob()
    {
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\18_32_cheat", 0, KEY_READ, &key) != ERROR_SUCCESS)
            return std::string();

        wchar_t buffer[4096] = {};
        DWORD size = sizeof(buffer) - sizeof(wchar_t);
        DWORD type = 0;
        const LSTATUS status = RegQueryValueExW(key, L"License", nullptr, &type,
                                                reinterpret_cast<LPBYTE>(buffer), &size);
        RegCloseKey(key);
        if (status != ERROR_SUCCESS || type != REG_SZ)
            return std::string();
        buffer[size / sizeof(wchar_t)] = L'\0';
        return WideToUtf8(buffer);
    }

    std::string LicenseFilePath()
    {
        wchar_t appdata[MAX_PATH] = {};
        const DWORD length = GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
        if (length == 0 || length >= MAX_PATH)
            return std::string();
        std::wstring path = std::wstring(appdata) + L"\\18_32_cheat";
        CreateDirectoryW(path.c_str(), nullptr);
        return WideToUtf8(path) + "\\license.dat";
    }

    std::string LoadFileBlob()
    {
        const std::string path = LicenseFilePath();
        if (path.empty())
            return std::string();

        HANDLE file = CreateFileW(Utf8ToWide(path).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return std::string();

        char buffer[8192] = {};
        DWORD read = 0;
        const BOOL ok = ReadFile(file, buffer, sizeof(buffer) - 1, &read, nullptr);
        CloseHandle(file);
        if (!ok)
            return std::string();
        buffer[read] = '\0';
        return std::string(buffer);
    }

    void SaveBlob(const std::string& blob)
    {
        const std::wstring wide = Utf8ToWide(blob);

        HKEY key = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\18_32_cheat", 0, nullptr, 0,
                            KEY_WRITE, nullptr, &key, nullptr) == ERROR_SUCCESS)
        {
            RegSetValueExW(key, L"License", 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(wide.c_str()),
                           static_cast<DWORD>((wide.size() + 1) * sizeof(wchar_t)));
            RegCloseKey(key);
        }

        const std::string path = LicenseFilePath();
        if (!path.empty())
        {
            HANDLE file = CreateFileW(Utf8ToWide(path).c_str(), GENERIC_WRITE, 0, nullptr,
                                      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file != INVALID_HANDLE_VALUE)
            {
                DWORD written = 0;
                WriteFile(file, blob.c_str(), static_cast<DWORD>(blob.size()), &written, nullptr);
                CloseHandle(file);
            }
        }
    }

    // ------------------------------------------------------------ состояние

    std::mutex g_mutex;
    license::State g_state;
    std::atomic<int> g_phase{ static_cast<int>(License::Phase::NeedKey) };
    char g_key_buffer[64] = {};
    uint32_t g_current_minutes = 0;
    int64_t g_activation_unix = 0;
    int64_t g_exit_at_unix = 0; // 0 — таймер закрытия не запущен
    uint64_t g_machine_hwid = 0;

    // 40-битный HWID: серийный номер системного диска + имя ПК.
    uint64_t ComputeMachineHwid()
    {
        DWORD volumeSerial = 0;
        GetVolumeInformationW(L"C:\\", nullptr, 0, &volumeSerial, nullptr, nullptr, 0);

        wchar_t computer[MAX_COMPUTERNAME_LENGTH + 2] = {};
        DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
        if (!GetComputerNameW(computer, &size))
        {
            size = 0;
        }

        uint64_t hash = 1469598103934665603ull; // FNV-1a 64
        auto mix = [&hash](const void* data, size_t length) {
            const unsigned char* bytes = static_cast<const unsigned char*>(data);
            for (size_t i = 0; i < length; ++i)
            {
                hash ^= bytes[i];
                hash *= 1099511628211ull;
            }
        };
        mix(&volumeSerial, sizeof(volumeSerial));
        mix(computer, size * sizeof(wchar_t));

        return hash & 0xFFFFFFFFFFull; // 40 бит — ровно столько зашито в ключ v2
    }

    bool KeyMatchesThisPc(const license::KeyInfo& info)
    {
        return !info.hwid_bound || info.hwid == g_machine_hwid;
    }

    int64_t ExpiryUnix()
    {
        return g_activation_unix + static_cast<int64_t>(g_current_minutes) * 60;
    }

    const license::ActivationRecord* FindRecord(uint32_t serial)
    {
        for (const license::ActivationRecord& record : g_state.records)
        {
            if (record.serial == serial)
                return &record;
        }
        return nullptr;
    }

    void RecomputePhase()
    {
        const int64_t now = NowUnix();
        g_exit_at_unix = 0;

        if (g_state.current_key.empty())
        {
            g_phase = static_cast<int>(License::Phase::NeedKey);
            return;
        }

        license::KeyInfo info;
        if (!license::ParseKey(g_state.current_key, Secret(), info))
        {
            g_phase = static_cast<int>(License::Phase::NeedKey);
            return;
        }

        // Ключ v2 работает только на том ПК, под который выдан.
        if (!KeyMatchesThisPc(info))
        {
            g_phase = static_cast<int>(License::Phase::HwidMismatch);
            return;
        }

        const license::ActivationRecord* record = FindRecord(info.serial);
        if (record == nullptr)
        {
            // Ключ без записи активации не должен был сюда попасть.
            g_phase = static_cast<int>(License::Phase::NeedKey);
            return;
        }

        g_current_minutes = info.minutes;
        g_activation_unix = record->activation_unix;

        if (ExpiryUnix() <= now)
        {
            g_phase = static_cast<int>(License::Phase::Expired);
            return;
        }
        g_phase = static_cast<int>(License::Phase::Authorized);
    }

    void SaveState()
    {
        SaveBlob(license::SerializeState(g_state, Secret()));
    }

    void LoadState()
    {
        license::State fromRegistry;
        license::State fromFile;
        const bool haveRegistry = license::DeserializeState(LoadRegistryBlob(), Secret(), fromRegistry);
        const bool haveFile = license::DeserializeState(LoadFileBlob(), Secret(), fromFile);

        // Берём состояние с большей историей активаций — удаление одной из
        // копий не сбрасывает уже начатые сроки.
        if (haveRegistry && haveFile)
        {
            g_state = (fromFile.records.size() > fromRegistry.records.size()) ? fromFile : fromRegistry;

            // Объединяем истории и берём самое позднее время проверки.
            for (const license::ActivationRecord& record : fromFile.records)
            {
                if (FindRecord(record.serial) == nullptr)
                    g_state.records.push_back(record);
            }
            for (const license::ActivationRecord& record : fromRegistry.records)
            {
                if (FindRecord(record.serial) == nullptr)
                    g_state.records.push_back(record);
            }
            if (fromRegistry.last_seen_unix > g_state.last_seen_unix)
                g_state.last_seen_unix = fromRegistry.last_seen_unix;
            if (fromFile.last_seen_unix > g_state.last_seen_unix)
                g_state.last_seen_unix = fromFile.last_seen_unix;
        }
        else if (haveRegistry)
        {
            g_state = fromRegistry;
        }
        else if (haveFile)
        {
            g_state = fromFile;
        }
        else
        {
            g_state = license::State();
            g_state.last_seen_unix = NowUnix();
        }
    }
}

namespace License
{
    void Initialize()
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_machine_hwid = ComputeMachineHwid();
        LoadState();

        const int64_t now = NowUnix();

        // Перевод системного времени назад — блокировка.
        if (g_state.last_seen_unix > now + kRollbackToleranceSeconds)
        {
            g_phase = static_cast<int>(Phase::Blocked);
            g_exit_at_unix = now + kBlockedExitDelay;
            return;
        }

        RecomputePhase();

        if (!g_state.current_key.empty())
        {
            strncpy_s(g_key_buffer, g_state.current_key.c_str(), _TRUNCATE);
        }
    }

    Phase GetPhase()
    {
        return static_cast<Phase>(g_phase.load());
    }

    bool Authorized()
    {
        return g_phase.load() == static_cast<int>(Phase::Authorized);
    }

    bool NeedsInputOverlay()
    {
        return g_phase.load() != static_cast<int>(Phase::Authorized);
    }

    char* KeyBuffer()
    {
        return g_key_buffer;
    }

    void SubmitKey()
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const int64_t now = NowUnix();

        license::KeyInfo info;
        const std::string normalized = license::NormalizeKeyText(g_key_buffer);
        if (!license::ParseKey(normalized, Secret(), info))
        {
            // Неверный ключ — игра закрывается.
            g_phase = static_cast<int>(Phase::WrongKey);
            g_exit_at_unix = now + kWrongKeyExitDelay;
            return;
        }

        // Чужой ключ: привязан к другому компьютеру — не активируем,
        // даём ввести правильный. Игра при этом не закрывается.
        if (!KeyMatchesThisPc(info))
        {
            g_phase = static_cast<int>(Phase::HwidMismatch);
            return;
        }

        // Срок начинается с первого ввода и не обновляется при повторном вводе.
        const license::ActivationRecord* existing = FindRecord(info.serial);
        if (existing == nullptr)
        {
            license::ActivationRecord record;
            record.serial = info.serial;
            record.activation_unix = now;
            if (g_state.records.size() >= license::kMaxRecords)
                g_state.records.erase(g_state.records.begin());
            g_state.records.push_back(record);
            g_activation_unix = now;
        }
        else
        {
            g_activation_unix = existing->activation_unix;
        }

        g_state.current_key = normalized;
        g_current_minutes = info.minutes;
        g_state.last_seen_unix = now;

        SaveState();

        if (ExpiryUnix() <= now)
        {
            g_phase = static_cast<int>(Phase::Expired);
            return;
        }
        g_phase = static_cast<int>(Phase::Authorized);
    }

    void Tick()
    {
        std::lock_guard<std::mutex> lock(g_mutex);

        if (g_phase.load() == static_cast<int>(Phase::Authorized))
        {
            const int64_t now = NowUnix();

            if (ExpiryUnix() <= now)
            {
                // Подписка закончилась прямо во время игры.
                g_phase = static_cast<int>(Phase::Expired);
                return;
            }

            // Обновляем метку времени не чаще раза в 10 минут.
            if (now > g_state.last_seen_unix + 600)
            {
                g_state.last_seen_unix = now;
                SaveState();
            }
        }
    }

    int64_t RemainingSeconds()
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_state.current_key.empty() || g_current_minutes == 0)
            return 0;
        const int64_t remaining = ExpiryUnix() - NowUnix();
        return remaining > 0 ? remaining : 0;
    }

    void FormatRemaining(char* out, size_t size)
    {
        int64_t seconds = RemainingSeconds();
        if (seconds < 0)
            seconds = 0;

        const int64_t days = seconds / 86400;
        if (days >= 1)
        {
            _snprintf_s(out, size, _TRUNCATE, "%lld дн. %02d:%02d",
                        static_cast<long long>(days),
                        static_cast<int>((seconds % 86400) / 3600),
                        static_cast<int>((seconds % 3600) / 60));
        }
        else
        {
            _snprintf_s(out, size, _TRUNCATE, "%02d:%02d:%02d",
                        static_cast<int>(seconds / 3600),
                        static_cast<int>((seconds % 3600) / 60),
                        static_cast<int>(seconds % 60));
        }
    }

    void FormatExpiry(char* out, size_t size)
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        out[0] = '\0';
        if (g_state.current_key.empty() || g_current_minutes == 0)
            return;

        const time_t moment = static_cast<time_t>(ExpiryUnix());
        tm local = {};
        localtime_s(&local, &moment);
        _snprintf_s(out, size, _TRUNCATE, "%02d.%02d.%04d %02d:%02d",
                    local.tm_mday, local.tm_mon + 1, local.tm_year + 1900,
                    local.tm_hour, local.tm_min);
    }

    void FormattedKey(char* out, size_t size)
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const std::string& key = g_state.current_key;
        if (key.empty())
        {
            _snprintf_s(out, size, _TRUNCATE, "-");
            return;
        }

        size_t written = 0;
        for (size_t i = 0; i < key.size() && written + 1 < size; ++i)
        {
            if (i > 0 && (i % 4) == 0 && written + 1 < size)
                out[written++] = '-';
            out[written++] = key[i];
        }
        out[written] = '\0';
    }

    void GetMachineHwidText(char* out, size_t size)
    {
        _snprintf_s(out, size, _TRUNCATE, "%010llX",
                    static_cast<unsigned long long>(g_machine_hwid));
    }

    int SecondsToExit()
    {
        const int64_t exit_at = g_exit_at_unix;
        if (exit_at == 0)
            return -1;
        const int64_t left = exit_at - NowUnix();
        return left > 0 ? static_cast<int>(left) : 0;
    }

    void EnforceExit()
    {
        if (g_exit_at_unix != 0 && NowUnix() >= g_exit_at_unix)
        {
            TerminateProcess(GetCurrentProcess(), 1);
        }
    }
}
