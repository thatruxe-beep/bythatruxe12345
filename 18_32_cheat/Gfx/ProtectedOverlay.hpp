#pragma once

#include <d3d9.h>
#include <windows.h>

namespace ProtectedOverlay
{
    // Uses an additional swap chain on GTA's existing D3D9 device. Creating a
    // second IDirect3DDevice9 inside MTA trips core.dll's graphics singleton.
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
