#include "Game/Features.h"

#include "Game/Rage/InfAmmo.hpp"

void InfAmmo::Update()
{
    static bool applied = false;
    static BYTE original = 0;
    auto* flag = reinterpret_cast<BYTE*>(0x969178);

    if (g_cfg.infammo)
    {
        if (!applied)
        {
            original = *flag;
            applied = true;
        }
        *flag = 1;
    }
    else if (applied)
    {
        *flag = original;
        applied = false;
    }
}
