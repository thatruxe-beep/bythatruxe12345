#include "Game/Features.h"

#include "Game/Rage/InfAmmo.hpp"

#include <array>

namespace
{
    constexpr unsigned int kInfiniteAmmoAmount = 9999;
    constexpr int kWeaponSlotCount = 13;

    struct AmmoSnapshot
    {
        eWeaponType type = WEAPON_UNARMED;
        unsigned int total = 0;
        bool captured = false;
    };

    std::array<AmmoSnapshot, kWeaponSlotCount> ammoSnapshots{};
    CPed* snapshotOwner = nullptr;

    void ClearSnapshots()
    {
        for (auto& snapshot : ammoSnapshots)
        {
            snapshot = AmmoSnapshot{};
        }
        snapshotOwner = nullptr;
    }

    bool UsesAmmo(eWeaponType type)
    {
        const int value = static_cast<int>(type);
        // GTA SA firearms, explosives, detonator, spray can, extinguisher and camera.
        return value >= 22 && value <= 43;
    }
}

void InfAmmo::Update()
{
    static bool flagApplied = false;
    static BYTE originalFlag = 0;
    auto* flag = reinterpret_cast<BYTE*>(0x969178);
    CPed* ped = FindPlayerPed();

    if (g_cfg.infammo)
    {
        if (!flagApplied)
        {
            originalFlag = *flag;
            flagApplied = true;
        }
        *flag = 1;

        if (!ped)
        {
            return;
        }

        if (snapshotOwner != ped)
        {
            ClearSnapshots();
            snapshotOwner = ped;
        }

        for (int slot = 0; slot < kWeaponSlotCount; ++slot)
        {
            CWeapon& weapon = ped->m_aWeapons[slot];
            AmmoSnapshot& snapshot = ammoSnapshots[slot];

            if (!UsesAmmo(weapon.m_eWeaponType))
            {
                snapshot = AmmoSnapshot{};
                continue;
            }

            if (!snapshot.captured || snapshot.type != weapon.m_eWeaponType)
            {
                snapshot.type = weapon.m_eWeaponType;
                snapshot.total = weapon.m_nAmmoTotal;
                snapshot.captured = true;
            }

            weapon.m_nAmmoTotal = kInfiniteAmmoAmount;
        }
    }
    else
    {
        if (flagApplied)
        {
            *flag = originalFlag;
            flagApplied = false;
        }

        if (ped && ped == snapshotOwner)
        {
            for (int slot = 0; slot < kWeaponSlotCount; ++slot)
            {
                CWeapon& weapon = ped->m_aWeapons[slot];
                const AmmoSnapshot& snapshot = ammoSnapshots[slot];
                if (snapshot.captured && snapshot.type == weapon.m_eWeaponType)
                {
                    weapon.m_nAmmoTotal = snapshot.total;
                }
            }
        }

        ClearSnapshots();
    }
}
