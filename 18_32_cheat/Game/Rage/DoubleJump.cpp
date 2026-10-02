#include "Game/Features.h"

#include "Game/Rage/DoubleJump.hpp"

#include "Core/Runtime.hpp"
#include "CPad.h"

void DoubleJump::Run()
{
    while (Runtime::IsRunning())
    {
        if (g_cfg.doublejump
            && (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0)
        {
            CPed* ped = FindPlayerPed();
            if (ped && !ped->m_pVehicle)
            {
                // Keep presenting a fresh jump press to the game while the
                // physical Space key is held. The worker interval is 2 ms as
                // requested and does not modify the Windows key state.
                if (CPad* pad = CPad::GetPad(0))
                {
                    pad->OldState.ButtonSquare = 0;
                    pad->NewState.ButtonSquare = 255;
                }
            }
        }

        Sleep(2);
    }
}
