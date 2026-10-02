#include "Game/Features.h"

#include "Game/Legit/FastZoom.hpp"

void FastZoom::Update()
{
    static bool middleWasDown = false;
    const bool middleDown = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
    const bool aiming = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

    if (g_cfg.fastzoom && aiming && middleDown && !middleWasDown)
    {
        // Exact C++ equivalent of AHK: Send {WheelUp 40}. The game performs
        // its own scope interpolation, so there is no direct FOV write and no
        // fight with the camera code that could produce shaking.
        INPUT wheelInputs[40]{};
        for (INPUT& input : wheelInputs)
        {
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_WHEEL;
            input.mi.mouseData = WHEEL_DELTA;
        }
        SendInput(40, wheelInputs, sizeof(INPUT));
    }

    middleWasDown = middleDown;
}
