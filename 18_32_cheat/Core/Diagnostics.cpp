#include "Core/Diagnostics.hpp"

#include "Core/Config.hpp"

#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <windows.h>
#include <psapi.h>

namespace
{
    std::ofstream g_log;
    bool g_sessionStarted = false;
    DWORD g_nextTick = 0;

    void LogOpen()
    {
        if (!g_log.is_open())
        {
            g_log.open(config_t::ConfigDir() + "\\overlay.log", std::ios::trunc);
        }
    }

    void LogF(const char* format, ...)
    {
        if (!g_log.is_open())
        {
            return;
        }

        char line[192];
        int length = snprintf(line, sizeof(line), "[%lu] ", GetTickCount());

        if (length <= 0 || length >= (int)sizeof(line) - 2)
        {
            return;
        }

        va_list args;
        va_start(args, format);
        const int written = vsnprintf(line + length, sizeof(line) - length - 1, format, args);
        va_end(args);

        if (written < 0)
        {
            return;
        }

        const int room = (int)sizeof(line) - length - 2;
        length += written < room ? written : room;
        line[length] = '\n';
        line[length + 1] = '\0';

        g_log << line;
        g_log.flush();
    }

    void LogMemory(const char* context)
    {
        if (!g_log.is_open())
        {
            return;
        }

        PROCESS_MEMORY_COUNTERS_EX counters{};
        counters.cb = sizeof(counters);

        if (!GetProcessMemoryInfo(GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
        {
            LogF("%s: mem query failed gle=%lu", context, GetLastError());
            return;
        }

        // Обход всего пользовательского адресного пространства: у
        // LargeAddressAware-процесса 4 ГБ. Аллокации падают из-за
        // фрагментации, поэтому важен крупнейший непрерывный блок.
        unsigned long long totalFree = 0;
        unsigned long long largestFree = 0;
        MEMORY_BASIC_INFORMATION info{};

        for (uintptr_t address = 0;;)
        {
            if (VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) == 0)
            {
                break;
            }

            if (info.State == MEM_FREE)
            {
                totalFree += info.RegionSize;

                if (info.RegionSize > largestFree)
                {
                    largestFree = info.RegionSize;
                }
            }

            const uintptr_t next = address + info.RegionSize;

            if (next <= address || next > 0xFFFFF000ULL)
            {
                break;
            }

            address = next;
        }

        char largest[24];

        if (largestFree >= (1ULL << 20))
        {
            snprintf(largest, sizeof(largest), "%lluMB", largestFree >> 20);
        }
        else
        {
            snprintf(largest, sizeof(largest), "%lluKB", largestFree >> 10);
        }

        LogF("%s: mem private=%lluMB vfree=%lluMB largest=%s%s",
            context,
            (unsigned long long)counters.PrivateUsage >> 20,
            totalFree >> 20,
            largest,
            largestFree < (16ULL << 20) ? " LOW!" : "");
    }

    void LogSystemInfo()
    {
        // Флаг LargeAddressAware решает, получает ли 32-битный процесс
        // 2 или 4 ГБ адресного пространства — главный рычаг против
        // 0xE0000008. Читается прямо из PE-заголовков главного exe.
        int laa = -1;
        const unsigned char* base = reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));

        if (base && base[0] == 'M' && base[1] == 'Z')
        {
            const unsigned int e_lfanew = *reinterpret_cast<const unsigned int*>(base + 0x3C);

            if (e_lfanew >= 0x40 && e_lfanew < 0x1000)
            {
                // IMAGE_NT_HEADERS: Signature(4) IMAGE_FILE_HEADER: ... Characteristics at +18
                const unsigned long characteristics =
                    *reinterpret_cast<const unsigned long*>(base + e_lfanew + 4 + 18);
                laa = (characteristics & 0x0020UL) != 0 ? 1 : 0;
            }
        }

        // Лимит commit: когда RAM + файл подкачки исчерпаны, аллокации
        // падают даже при свободном адресном пространстве.
        MEMORYSTATUSEX status{};
        status.dwLength = sizeof(status);
        GlobalMemoryStatusEx(&status);

        LogF("exe laa=%d (address space %s), libcef=%d",
            laa,
            laa == 1 ? "4GB" : (laa == 0 ? "2GB" : "?"),
            GetModuleHandleW(L"libcef.dll") != nullptr ? 1 : 0);
        LogF("commit limit=%lluMB avail=%lluMB, phys avail=%lluMB",
            (unsigned long long)status.ullTotalPageFile >> 20,
            (unsigned long long)status.ullAvailPageFile >> 20,
            (unsigned long long)status.ullAvailPhys >> 20);
    }
}

void Diagnostics::Frame()
{
    if (!g_sessionStarted)
    {
        g_sessionStarted = true;
        LogOpen();
        LogF("session start, build " __DATE__ " " __TIME__);
        LogSystemInfo();
        LogMemory("session");
    }

    const DWORD now = GetTickCount();

    if (g_nextTick == 0)
    {
        g_nextTick = now + 30000;
        return;
    }

    if (now >= g_nextTick)
    {
        LogMemory("tick");
        g_nextTick = now + 30000;
    }
}

void Diagnostics::Log(const char* format, ...)
{
    if (!g_log.is_open())
    {
        return;
    }

    char text[176];
    va_list args;
    va_start(args, format);
    const int written = vsnprintf(text, sizeof(text), format, args);
    va_end(args);

    if (written <= 0)
    {
        return;
    }

    LogF("%s", text);
}
