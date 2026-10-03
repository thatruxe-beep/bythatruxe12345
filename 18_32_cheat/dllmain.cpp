// 18:32 cheat entry point

#include <windows.h>

#include "Core/Config.hpp"
#include "Core/Runtime.hpp"
#include "Game/Features.h"
#include "Hooks/Hooks.hpp"
#include "Menu/Menu.hpp"

namespace
{
    DWORD WINAPI AirBreakThread(LPVOID)
    {
        AirBreak::Run();
        return 0;
    }

    DWORD WINAPI MainThread(LPVOID)
    {
        config_t::EnsureDir();
        Hooks::InstallHooks();

        HANDLE airThread = CreateThread(nullptr, 0, AirBreakThread, nullptr, 0, nullptr);

        while (Runtime::IsRunning())
        {
            Sleep(25);
        }

        if (airThread)
        {
            WaitForSingleObject(airThread, 2000);
            CloseHandle(airThread);
        }

        Hooks::RemoveHooks();
        delete menu;
        menu = nullptr;
        Sleep(150);
        FreeLibraryAndExitThread(Runtime::GetModule(), 0);
        return 0;
    }
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
        Runtime::Initialize(module);
        HANDLE thread = CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
        if (thread)
        {
            CloseHandle(thread);
        }
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        Runtime::RequestUnload();
    }

    return TRUE;
}
