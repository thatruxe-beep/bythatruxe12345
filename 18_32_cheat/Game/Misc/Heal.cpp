#include "Game/Features.h"

#include "Game/Misc/Heal.hpp"

#include <algorithm>

void Heal::Update()
{
    CPed* ped = FindPlayerPed();

    if (!ped)
    {
        return;
    }

    // Match 18:32 cheat's heal amount while never reducing a higher
    // server/plugin supplied health value.
    ped->m_fHealth = (std::max)(ped->m_fHealth, 200.0f);
}
