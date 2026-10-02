#include "Game/Features.h"

#include "Game/Rage/NewRapid.hpp"

#include "CWeapon.h"

#include <algorithm>
#include <cmath>

void NewRapid::Update()
{
    static CWeapon* trackedWeapon = nullptr;
    static unsigned int nextPulse = 0u;

    CPed* ped = FindPlayerPed();
    if (!g_cfg.newrapid || !ped || ped->m_pVehicle)
    {
        trackedWeapon = nullptr;
        nextPulse = 0u;
        return;
    }

    CWeapon* weapon = ped->GetWeapon();
    const int weaponType = weapon ? static_cast<int>(weapon->m_eWeaponType) : -1;
    // Firearm IDs in GTA SA run from Colt 45 (22) through Minigun (38).
    // Numeric bounds keep this compatible with plugin-sdk revisions that use
    // different enum symbol prefixes.
    if (!weapon || weaponType < 22 || weaponType > 38)
    {
        return;
    }

    if (weapon != trackedWeapon)
    {
        trackedWeapon = weapon;
        nextPulse = 0u;
    }

    const float multiplier = std::clamp(
        std::round(g_cfg.rapidfire_multiplier * 10.0f) / 10.0f,
        1.0f, 10.0f);
    if (multiplier <= 1.0f || (GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0)
    {
        nextPulse = 0u;
        return;
    }

    const unsigned int now = CTimer::m_snTimeInMilliseconds;
    if (nextPulse != 0u && now < nextPulse)
    {
        return;
    }

    CWeaponInfo* info = CWeaponInfo::GetWeaponInfo(weapon->m_eWeaponType, 1);
    float baseInterval = 180.0f;
    if (info)
    {
        const float animationInterval = (info->m_fAnimLoopEnd - info->m_fAnimLoopStart) * 1000.0f;
        if (std::isfinite(animationInterval) && animationInterval > 0.0f)
        {
            baseInterval = std::clamp(animationInterval, 60.0f, 700.0f);
        }
    }

    const float scaledInterval = baseInterval / multiplier;
    const unsigned int interval = static_cast<unsigned int>(
        scaledInterval < 5.0f ? 5.0f : scaledInterval);

    // Re-arm the local weapon at a controlled cadence. Unlike the previous
    // implementation this does not corrupt the global weapon animation table.
    weapon->m_nTimeForNextShot = now;
    weapon->m_nState = WEAPONSTATE_READY;
    nextPulse = now + interval;
}
