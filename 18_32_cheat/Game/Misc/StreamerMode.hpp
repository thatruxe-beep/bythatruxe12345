#pragma once

#include <windows.h>

struct IDirect3DDevice9;

class StreamerMode
{
public:
    // Keeps the protected overlay in sync with the g_cfg.streamer toggle.
    // Must be called on the render thread (from the Present hook), because
    // the overlay window is created and destroyed there.
    static void Update(IDirect3DDevice9* device, HWND gameWindow);
    // True while the overlay exists: UI draw data must be redirected to it
    // instead of the game back buffer, so captures stay clean.
    static bool IsActive();
    static bool BeginFrame();
    static void EndFrame();
    static void OnBeforeDeviceReset();
    static void OnAfterDeviceReset();
    static void Shutdown();
    // Диагностика: строка в C:\18_32_cheat\overlay.log (журнал открывается
    // с первого кадра Present). Используется и другими модулями.
    static void Log(const char* format, ...);
};
