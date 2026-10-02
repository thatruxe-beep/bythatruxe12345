#include "Game/Features.h"

#include "Game/Rage/DoubleJump.hpp"

#include "Core/Runtime.hpp"
#include "CPad.h"

void DoubleJump::Run()
{
    while (Runtime::IsRunning())
    {
        const bool held = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
        CPed* ped = FindPlayerPed();

        if (g_cfg.doublejump && !g_cfg.menu_open
            && held && ped && !ped->m_pVehicle)
        {
            // GTA reads both the keyboard buffers and the translated pad
            // state. Refresh both every 2 ms and force OldState low so each
            // game tick sees Space as a new press instead of one long hold.
            CPad::OldKeyState.standardKeys[VK_SPACE] = 0;
            CPad::NewKeyState.standardKeys[VK_SPACE] = 255;

            if (CPad* pad = CPad::GetPad(0))
            {
                pad->OldState.ButtonSquare = 0;
                pad->NewState.ButtonSquare = 255;
                pad->PCTempKeyState.ButtonSquare = 255;
            }
        }

        Sleep(2);
    }
}
