#include "Game/Features.h"

#include "Game/Rage/DoubleJump.hpp"

#include "Core/Runtime.hpp"

namespace
{
    constexpr ULONG_PTR kInjectedSpaceTag = 0x18325350u;
    volatile LONG physicalSpaceDown = 0;

    void SendSpace(bool down)
    {
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = VK_SPACE;
        input.ki.dwFlags = down ? 0u : KEYEVENTF_KEYUP;
        input.ki.dwExtraInfo = kInjectedSpaceTag;
        SendInput(1, &input, sizeof(input));
    }
}

void DoubleJump::HandleKeyMessage(UINT message, WPARAM key)
{
    if (key != VK_SPACE
        || static_cast<ULONG_PTR>(GetMessageExtraInfo()) == kInjectedSpaceTag)
    {
        return;
    }

    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
    {
        InterlockedExchange(&physicalSpaceDown, 1);
    }
    else if (message == WM_KEYUP || message == WM_SYSKEYUP)
    {
        InterlockedExchange(&physicalSpaceDown, 0);
    }
}

void DoubleJump::Run()
{
    bool injectedDown = false;

    while (Runtime::IsRunning())
    {
        const bool physicallyHeld =
            InterlockedCompareExchange(&physicalSpaceDown, 0, 0) != 0;
        CPed* ped = FindPlayerPed();
        const bool enabled = g_cfg.doublejump && !g_cfg.menu_open
            && physicallyHeld && ped && !ped->m_pVehicle;

        if (enabled)
        {
            // Exact C++ equivalent of the supplied AHK loop:
            // Space Down -> 5 ms -> Space Up -> 5 ms, while physical Space
            // remains held. Tagged injected messages never change hold state.
            SendSpace(true);
            injectedDown = true;
            Sleep(5);
            SendSpace(false);
            injectedDown = false;
            Sleep(5);
            continue;
        }

        if (injectedDown || !physicallyHeld)
        {
            SendSpace(false);
            injectedDown = false;
        }
        Sleep(1);
    }

    SendSpace(false);
}
