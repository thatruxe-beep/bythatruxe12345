#include "minhook.hpp"

#include "../../thirdparty/virtualiser/VirtualizerSDK.h"

#include "Utils/xorstr.h"

#include "Collision/Collision.hpp"
#include "LdrDll/LdrDll.hpp"
#include "core/ForceCursorVisible.hpp"
#include "d3d9/Present.hpp"
#include "d3d9/Reset.hpp"

#include "Core/Config.hpp"
#include "Game/Features.h"
#include "Menu/Menu.hpp"

#include "Hooks.hpp"

namespace
{
    bool hooksInstalled = false;

    void RestoreFeatureState()
    {
        g_cfg.menu_open = false;
        g_cfg.rapidfire = false;
        g_cfg.gamespeed = false;
        g_cfg.fastrot = false;
        g_cfg.waterdrive = false;
        g_cfg.carfly = false;
        g_cfg.nobikefall = false;
        g_cfg.aspect = false;
        g_cfg.fov = false;
        g_cfg.fastcross = false;
        g_cfg.norecoil = false;
        g_cfg.nospread = false;
        g_cfg.trigger = false;
        g_cfg.nocamcol = false;
        g_cfg.camhack = false;
        g_cfg.camhackteleport = false;
        g_cfg.godmode = false;
        g_cfg.nofall = false;
        g_cfg.fastbeg = false;
        g_cfg.speedhack = false;
        g_cfg.airbreake = false;
        g_cfg.nightmode = false;
        g_cfg.customtime = false;
        g_cfg.customcolor = false;
        g_cfg.fullbright = false;
        g_cfg.skychange = false;
        g_cfg.fogchange = false;
        g_cfg.suncolor = false;
        g_cfg.removals = false;
        g_cfg.carcolor = false;
        g_cfg.nocol = false;
        g_cfg.damager = false;

        World::Update();
        RapidFire::Update();
        Damager::Update();
        GameSpeed::Update();
        FastRot::Update();
        VehicleFlags::Update();
        NoBikeFall::Update();
        Aspect::Update();
        Fov::Update();
        FastCrosshair::Update();
        NoRecoil::Update();
        NoSpread::Update();
        NoCamCol::Update();
        CamHack::Update();
        GodMode::Update();
        NoFall::Update();
        FastRun::Update();
    }
}

void Hooks::InstallHooks()
{
    if (hooksInstalled)
    {
        return;
    }

    const MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
    {
        MessageBoxA(NULL, "Failed to initialize MinHook", "18:32 cheat", MB_OK | MB_ICONERROR);
        return;
    }

    hooksInstalled = true;
    VIRTUALIZER_START;
    LdrDll::InstallHook();
    Collision::InstallHook();
    Present::InstallHook();
    Reset::InstallHook();
    VIRTUALIZER_END;
}

void Hooks::RemoveHooks()
{
    if (!hooksInstalled)
    {
        return;
    }

    // Stop new render callbacks first, then wait for a callback already in
    // progress to leave this DLL before destroying ImGui and releasing code.
    Present::RemoveHook();
    Reset::RemoveHook();
    Sleep(100);

    RestoreFeatureState();

    // The MTA cursor is restored by the unload button on the render thread.
    // Calling its GUI routine from this worker thread can crash inside USER32.
    Present::Shutdown();
    LdrDll::RemoveHook();
    Collision::RemoveHook();
    ForceCursor::RemoveHook();

    const MH_STATUS status = MH_Uninitialize();
    if (status != MH_OK && status != MH_ERROR_NOT_INITIALIZED)
    {
        MessageBoxA(NULL, "Failed to uninitialize MinHook", "18:32 cheat", MB_OK | MB_ICONERROR);
    }
    hooksInstalled = false;
}
