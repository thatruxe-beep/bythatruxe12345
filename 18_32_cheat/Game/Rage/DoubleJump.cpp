#include "Game/Features.h"

#include "Game/Rage/DoubleJump.hpp"

#include "Core/Runtime.hpp"

namespace
{
    constexpr ULONG_PTR kInjectedJumpTag = 0x1832D00Bu;
    volatile LONG physicalSpaceDown = 0;

    void InjectSpace(bool down)
    {
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = VK_SPACE;
        input.ki.dwFlags = down ? 0u : KEYEVENTF_KEYUP;
        input.ki.dwExtraInfo = kInjectedJumpTag;
        SendInput(1, &input, sizeof(input));
    }
}

void DoubleJump::HandleKeyMessage(UINT message, WPARAM key)
{
    if (key != VK_SPACE
        || static_cast<ULONG_PTR>(GetMessageExtraInfo()) == kInjectedJumpTag)
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
    bool wasSpamming = false;

    while (Runtime::IsRunning())
    {
        const bool held = InterlockedCompareExchange(&physicalSpaceDown, 0, 0) != 0;
        CPed* ped = FindPlayerPed();
        const bool canJump = g_cfg.doublejump && !g_cfg.menu_open
            && held && ped && !ped->m_pVehicle;

        if (canJump)
        {
            // Generate a complete Space tap every 2 ms while the real key is
            // held. Tagged synthetic messages are ignored by HandleKeyMessage,
            // so releasing the physical key always stops the loop.
            InjectSpace(false);
            InjectSpace(true);
            wasSpamming = true;
        }
        else if (wasSpamming && !held)
        {
            // Ensure the synthetic key cannot remain down after a real release.
            InjectSpace(false);
            wasSpamming = false;
        }

        Sleep(2);
    }

    if (wasSpamming)
    {
        InjectSpace(false);
    }
}
