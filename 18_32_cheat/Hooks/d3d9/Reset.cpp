#include <d3d9.h>

#include "imgui_impl_dx9.h"

#include "minhook.hpp"

#include "Utils/xorstr.h"
#include "Utils/D3D9.hpp"

#include "Gfx/Blur.hpp"

#include "Reset.hpp"

using tReset = HRESULT(__stdcall*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
tReset oReset = nullptr;

static LPVOID sResetTarget = nullptr;
static volatile LONG sResetCalls = 0;

namespace
{
    struct ResetCallGuard
    {
        ResetCallGuard() { InterlockedIncrement(&sResetCalls); }
        ~ResetCallGuard() { InterlockedDecrement(&sResetCalls); }
    };
}

HRESULT __stdcall hkReset(IDirect3DDevice9* self, D3DPRESENT_PARAMETERS* presentationParameters)
{
    ResetCallGuard callGuard;
    Blur::OnReset();
    ImGui_ImplDX9_InvalidateDeviceObjects();

    HRESULT result = oReset(self, presentationParameters);

    ImGui_ImplDX9_CreateDeviceObjects();

    return result;
}

void Reset::InstallHook()
{
    MH_STATUS status;

    sResetTarget = Utils::get_function_address(16);

    if (!sResetTarget)
    {
        MessageBoxA(NULL, "Failed to find device for d3d9/reset", "18:32 cheat", MB_OK | MB_ICONERROR);
        return;
    }

    status = MH_CreateHook(sResetTarget, &hkReset, reinterpret_cast<LPVOID*>(&oReset));
    if (status != MH_OK)
    {
        MessageBoxA(NULL, "Failed to create hook on function d3d9/reset", "18:32 cheat", MB_OK | MB_ICONERROR);
    }

    status = MH_EnableHook(sResetTarget);
    if (status != MH_OK)
    {
        MessageBoxA(NULL, "Failed to enable hook on function d3d9/reset", "18:32 cheat", MB_OK | MB_ICONERROR);
    }
}

void Reset::RemoveHook()
{
    MH_STATUS status;

    if (sResetTarget == nullptr)
    {
        return;
    }

    status = MH_DisableHook(sResetTarget);
    if (status != MH_OK)
    {
        MessageBoxA(NULL, "Failed to disable hook on function d3d9/reset", "18:32 cheat", MB_OK | MB_ICONERROR);
    }

    for (int i = 0; i < 2000 && InterlockedCompareExchange(&sResetCalls, 0, 0) != 0; ++i)
    {
        Sleep(1);
    }

    status = MH_RemoveHook(sResetTarget);
    if (status != MH_OK && status != MH_ERROR_NOT_CREATED)
    {
        MessageBoxA(NULL, "Failed to remove hook on function d3d9/reset", "18:32 cheat", MB_OK | MB_ICONERROR);
    }
    sResetTarget = nullptr;
    oReset = nullptr;
}
