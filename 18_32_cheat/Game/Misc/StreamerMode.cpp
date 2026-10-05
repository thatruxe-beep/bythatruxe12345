#include "Game/Features.h"

#include "Game/Misc/StreamerMode.hpp"

#include "Gfx/ProtectedOverlay.hpp"

#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <psapi.h>

namespace
{
    bool overlayActive = false;
    bool overlayFailed = false;
    bool sessionLogged = false;
    bool beginHidden = false;

    // Diagnostics for the 0xE0000008 investigation: every overlay lifecycle
    // step and every frame phase is appended (and flushed) to
    // C:\18_32_cheat\overlay.log, so the last line after a crash shows what
    // the render thread was doing - or that the crash happened elsewhere.
    // 0xE0000008 is Chromium's kOomExceptionCode: libcef (loaded inside the
    // 32-bit gta_sa.exe process) raises it when an allocation fatally fails,
    // so the log also tracks process memory and the largest contiguous free
    // block of the 2 GB address space.
    std::ofstream g_log;
    unsigned long long g_frame = 0;

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

        // Walk the 32-bit address space and sum the free regions plus the
        // largest contiguous one: allocation failures in a 32-bit process
        // are usually caused by fragmentation, not by committed bytes.
        unsigned long long totalFree = 0;
        unsigned long long largestFree = 0;
        MEMORY_BASIC_INFORMATION info{};

        for (uintptr_t address = 0; address < 0x7FFF0000u;)
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

            address += info.RegionSize;
        }

        LogF("%s: mem private=%lluMB vfree=%lluMB largest=%lluMB",
            context,
            (unsigned long long)counters.PrivateUsage >> 20,
            totalFree >> 20,
            largestFree >> 20);
    }
}

void StreamerMode::Update(IDirect3DDevice9* device, HWND gameWindow)
{
    if (!sessionLogged)
    {
        sessionLogged = true;
        LogOpen();
        LogF("session start, streamer=%d, build " __DATE__ " " __TIME__,
            g_cfg.streamer ? 1 : 0);
        LogMemory("session");
    }

    if (g_cfg.streamer)
    {
        if (!overlayActive && !overlayFailed && gameWindow && device)
        {
            // One attempt per activation: retrying every frame would spin
            // window creation while capture protection is unsupported
            // (e.g. Windows 10 older than 2004).
            LogMemory("pre-init");
            LogF("enabling: overlay init");
            overlayActive = ProtectedOverlay::Initialize(gameWindow, device);
            overlayFailed = !overlayActive;
            LogF("init %s gle=%lu", overlayActive ? "ok" : "failed", GetLastError());
            LogMemory("post-init");
        }
    }
    else if (overlayActive || overlayFailed)
    {
        LogF("disabling: overlay shutdown");
        ProtectedOverlay::Shutdown();
        overlayActive = false;
        overlayFailed = false;
        LogMemory("post-shutdown");
    }
}

bool StreamerMode::IsActive()
{
    return overlayActive;
}

bool StreamerMode::BeginFrame()
{
    if (!overlayActive)
    {
        return false;
    }

    ++g_frame;

    // Memory snapshot twice a minute: enough to see a leak or a ceiling
    // approach without flooding the log.
    if (g_frame % 3600 == 0)
    {
        LogMemory("periodic");
    }

    // While the overlay stays hidden (game unfocused/minimized) only the
    // transition is logged, not every skipped frame.
    if (!beginHidden)
    {
        LogF("F#%llu b-", g_frame);
    }

    const bool ok = ProtectedOverlay::BeginFrame();

    if (ok)
    {
        if (beginHidden)
        {
            LogF("F#%llu visible again", g_frame);
            beginHidden = false;
        }
        LogF("F#%llu b+", g_frame);
    }
    else if (!beginHidden)
    {
        LogF("F#%llu bX (hidden: unfocused/minimized)", g_frame);
        beginHidden = true;
    }

    return ok;
}

void StreamerMode::EndFrame()
{
    LogF("F#%llu e-", g_frame);
    ProtectedOverlay::EndFrame();
    LogF("F#%llu e+", g_frame);
}

void StreamerMode::OnBeforeDeviceReset()
{
    LogF("reset: releasing resources");
    ProtectedOverlay::BeforeDeviceReset();
}

void StreamerMode::OnAfterDeviceReset()
{
    ProtectedOverlay::AfterDeviceReset();
    LogF("reset: done");
}

void StreamerMode::Shutdown()
{
    if (overlayActive || overlayFailed)
    {
        LogF("shutdown");
        ProtectedOverlay::Shutdown();
        overlayActive = false;
        overlayFailed = false;
    }
}
