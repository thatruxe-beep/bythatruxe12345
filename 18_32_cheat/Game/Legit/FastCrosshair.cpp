#include "Game/Features.h"

#include "Game/Legit/FastCrosshair.hpp"

void FastCrosshair::Update()
{
    static bool applied = false;
    static bool captured = false;
    static unsigned char original = 0;
    auto* instruction = reinterpret_cast<unsigned char*>(0x0058E1D9);

    if (g_cfg.fastcross == applied)
    {
        return;
    }

    if (g_cfg.fastcross)
    {
        if (!captured)
        {
            original = *instruction;
            captured = true;
        }

        const unsigned char jump = 0xEB;
        PatchBytes(instruction, &jump, sizeof(jump));
        applied = true;
        return;
    }

    if (captured)
    {
        PatchBytes(instruction, &original, sizeof(original));
    }
    applied = false;
}
