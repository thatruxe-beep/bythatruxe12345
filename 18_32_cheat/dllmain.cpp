// 18:32 cheat entry point

#include <thread>
#include <windows.h>

#include "Hooks/Hooks.hpp"
#include "Core/Config.hpp"
#include "Game/Features.h"

#include "Utils/xorstr.h"
#include "../thirdparty/virtualiser/VirtualizerSDK.h"

void Main()
{
    config_t::EnsureDir();
    Hooks::InstallHooks();
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
    //  ✵✵✵ ВОР ✵✵✵
        std::thread main(Main);
        main.detach();
        std::thread air(AirBreak::Run);
        air.detach();
    }

    return TRUE;
}
