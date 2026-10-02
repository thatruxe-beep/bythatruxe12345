#include "Game/Features.h"

#include "Game/Rage/RandomGodMode.hpp"

#include <algorithm>

void RandomGodMode::Update()
{
    static CPed* trackedPed = nullptr;
    static float previousHealth = 0.0f;
    static float previousArmor = 0.0f;
    static unsigned int randomState = 0x1832u;

    CPed* ped = FindPlayerPed();
    if (!ped)
    {
        trackedPed = nullptr;
        return;
    }

    if (ped != trackedPed || !g_cfg.randomgodmode)
    {
        trackedPed = ped;
        previousHealth = ped->m_fHealth;
        previousArmor = ped->m_fArmour;
        return;
    }

    const bool tookDamage = ped->m_fHealth + 0.01f < previousHealth
        || ped->m_fArmour + 0.01f < previousArmor;

    if (tookDamage)
    {
        randomState = randomState * 1664525u + 1013904223u;
        const int roll = static_cast<int>(randomState % 100u) + 1;
        const int chance = std::clamp(g_cfg.randomgodmode_chance, 1, 100);
        if (roll <= chance)
        {
            ped->m_fHealth = previousHealth;
            ped->m_fArmour = previousArmor;
        }
    }

    previousHealth = ped->m_fHealth;
    previousArmor = ped->m_fArmour;
}
