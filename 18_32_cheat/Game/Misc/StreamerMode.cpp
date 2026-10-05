#include "Game/Features.h"

#include "Game/Misc/StreamerMode.hpp"

#include "Gfx/ProtectedOverlay.hpp"

#include <cstdarg>
#include <cstdio>
#include <fstream>

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
}

void StreamerMode::Update(IDirect3DDevice9* device, HWND gameWindow)
{
    if (!sessionLogged)
    {
        sessionLogged = true;
        LogOpen();
        LogF("session start, streamer=%d, build " __DATE__ " " __TIME__,
            g_cfg.streamer ? 1 : 0);
    }

    if (g_cfg.streamer)
    {
        if (!overlayActive && !overlayFailed && gameWindow && device)
        {
            // One attempt per activation: retrying every frame would spin
            // window creation while capture protection is unsupported
            // (e.g. Windows 10 older than 2004).
            LogF("enabling: overlay init");
            overlayActive = ProtectedOverlay::Initialize(gameWindow, device);
            overlayFailed = !overlayActive;
            LogF("init %s gle=%lu", overlayActive ? "ok" : "failed", GetLastError());
        }
    }
    else if (overlayActive || overlayFailed)
    {
        LogF("disabling: overlay shutdown");
        ProtectedOverlay::Shutdown();
        overlayActive = false;
        overlayFailed = false;
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
    LogF("reset: releasing chain");
    ProtectedOverlay::BeforeDeviceReset();
}

void StreamerMode::OnAfterDeviceReset()
{
    ProtectedOverlay::AfterDeviceReset();
    LogF("reset: chain rebuilt");
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
