#include "Game/Features.h"

#include "Game/Rage/Damager.hpp"

#include <limits>

void Damager::Update()
{
    static bool applied = false;
    static unsigned short originalDamage[17][4]{};

    auto forEachWeapon = [](auto&& callback)
    {
        // GTA SA firearm weapon types, from pistol through minigun.
        for (int type = 22; type <= 38; ++type)
        {
            for (int skill = 0; skill < 4; ++skill)
            {
                CWeaponInfo* info = CWeaponInfo::GetWeaponInfo(
                    static_cast<eWeaponType>(type), static_cast<unsigned char>(skill));
                if (info)
                {
                    callback(*info, type - 22, skill);
                }
            }
        }
    };

    if (!g_cfg.damager)
    {
        if (applied)
        {
            forEachWeapon([&](CWeaponInfo& info, int type, int skill)
            {
                info.m_nDamage = originalDamage[type][skill];
            });
            applied = false;
        }
        return;
    }

    if (!applied)
    {
        forEachWeapon([&](CWeaponInfo& info, int type, int skill)
        {
            originalDamage[type][skill] = info.m_nDamage;
        });
        applied = true;
    }

    // Keep enforcing the multiplier because MTA can refresh weapon data.
    forEachWeapon([&](CWeaponInfo& info, int type, int skill)
    {
        const unsigned int doubled = static_cast<unsigned int>(originalDamage[type][skill]) * 2u;
        const unsigned int maximum = std::numeric_limits<unsigned short>::max();
        info.m_nDamage = static_cast<unsigned short>(doubled > maximum ? maximum : doubled);
    });
}
