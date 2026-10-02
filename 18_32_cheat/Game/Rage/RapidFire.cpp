#include "Game/Features.h"

#include "Game/Rage/RapidFire.hpp"

#include "CWeapon.h"

#include <algorithm>
#include <cmath>

void RapidFire::Update()
{
    static CWeapon* trackedWeapon = nullptr;
    static unsigned int adjustedDeadline = 0u;

    if (!g_cfg.rapidfire)
    {
        trackedWeapon = nullptr;
        adjustedDeadline = 0u;
        return;
    }

    CPed* ped = FindPlayerPed();
    if (!ped)
    {
        return;
    }

    CWeapon* weapon = ped->GetWeapon();
    if (!weapon)
    {
        return;
    }

    if (weapon != trackedWeapon)
    {
        trackedWeapon = weapon;
        adjustedDeadline = weapon->m_nTimeForNextShot;
    }

    const unsigned int now = CTimer::m_snTimeInMilliseconds;
    const unsigned int deadline = weapon->m_nTimeForNextShot;

    // GTA writes a fresh absolute deadline after every shot. Shorten only
    // that newly-created interval; repeatedly dividing it every frame makes
    // the weapon state invalid and can prevent firing entirely.
    if (deadline != adjustedDeadline && deadline > now)
    {
        const float multiplier = std::clamp(
            std::round(g_cfg.rapidfire_multiplier * 10.0f) / 10.0f,
            1.0f, 10.0f);
        const unsigned int remaining = deadline - now;
        const unsigned int shortened = static_cast<unsigned int>(
            std::lround(static_cast<float>(remaining) / multiplier));
        adjustedDeadline = now + (shortened == 0u ? 1u : shortened);
        weapon->m_nTimeForNextShot = adjustedDeadline;
    }
    else
    {
        adjustedDeadline = deadline;
    }
}
