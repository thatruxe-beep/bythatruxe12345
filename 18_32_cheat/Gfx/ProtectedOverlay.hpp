#pragma once

#include <d3d9.h>
#include <windows.h>

namespace ProtectedOverlay
{
    // Renders the cheat UI into an offscreen render-target texture on GTA's
    // existing D3D9 device (creating a second IDirect3DDevice9 inside MTA
    // trips core.dll's graphics singleton). The finished frame is downloaded
    // with GetRenderTargetData and composited into a top-level layered window
    // marked WDA_EXCLUDEFROMCAPTURE via UpdateLayeredWindow. No additional
    // swap chain and no second Present call: the UI never touches the game
    // back buffer, so OBS game capture stays clean, and window/display
    // capture (OBS, Discord screen share) excludes the overlay window.
    bool Initialize(HWND gameWindow, IDirect3DDevice9* gameDevice);
    bool BeginFrame();
    void EndFrame();
    void BeforeDeviceReset();
    void AfterDeviceReset();
    void Shutdown();
    HWND GetWindow();
    IDirect3DDevice9* GetDevice();
    void SetVisible(bool visible);
}
