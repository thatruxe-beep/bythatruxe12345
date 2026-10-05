#include "Game/Features.h"

#include "Game/Misc/StreamerMode.hpp"

#include "Gfx/ProtectedOverlay.hpp"

namespace
{
    bool overlayActive = false;
    bool overlayFailed = false;
}

void StreamerMode::Update(IDirect3DDevice9* device, HWND gameWindow)
{
    if (g_cfg.streamer)
    {
        if (!overlayActive && !overlayFailed && gameWindow && device)
        {
            // One attempt per activation: retrying every frame would spin
            // window creation while capture protection is unsupported
            // (e.g. Windows 10 older than 2004).
            overlayActive = ProtectedOverlay::Initialize(gameWindow, device);
            overlayFailed = !overlayActive;
        }
    }
    else if (overlayActive || overlayFailed)
    {
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

    return ProtectedOverlay::BeginFrame();
}

void StreamerMode::EndFrame()
{
    ProtectedOverlay::EndFrame();
}

void StreamerMode::OnBeforeDeviceReset()
{
    ProtectedOverlay::BeforeDeviceReset();
}

void StreamerMode::OnAfterDeviceReset()
{
    ProtectedOverlay::AfterDeviceReset();
}

void StreamerMode::Shutdown()
{
    if (overlayActive || overlayFailed)
    {
        ProtectedOverlay::Shutdown();
        overlayActive = false;
        overlayFailed = false;
    }
}
