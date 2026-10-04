#include "Game/Features.h"

#include "Game/Legit/NoSpread.hpp"

void NoSpread::Update()
{
    static bool wasOn = false;
    static int snap[17][4] = {};

    auto each = [](auto fn)
    {
        // Идентификаторы оружия GTA SA: 22 (Colt 45) .. 38 (Minigun).
        // Дробовики: 25 (дробовик), 26 (обрез), 27 (Combat Shotgun) —
        // числовые ID, не зависящие от ревизий enum в plugin-sdk.
        // NoSpread их не трогает: у дробовиков всегда родной разброс дроби.
        constexpr int kFirstWeaponId = 22;
        constexpr int kLastWeaponId = 38;
        constexpr int kFirstShotgunId = 25;
        constexpr int kLastShotgunId = 27;

        for (int t = kFirstWeaponId; t <= kLastWeaponId; t++)
        {
            if (t >= kFirstShotgunId && t <= kLastShotgunId)
            {
                continue;
            }

            for (int s = 0; s < 4; s++)
            {
                if (CWeaponInfo* wi = CWeaponInfo::GetWeaponInfo((eWeaponType)t, (unsigned char)s))
                {
                    fn(wi, t - 22, s);
                }
            }
        }
    };

    if (!g_cfg.nospread)
    {
        if (wasOn)
        {
            each([&](CWeaponInfo* wi, int ti, int si)
            {
                *reinterpret_cast<int*>(&wi->m_fAccuracy) = snap[ti][si];
            });
            wasOn = false;
        }

        return;
    }

    if (!wasOn)
    {
        each([&](CWeaponInfo* wi, int ti, int si)
        {
            snap[ti][si] = *reinterpret_cast<int*>(&wi->m_fAccuracy);
        });
        wasOn = true;
    }

    each([](CWeaponInfo* wi, int, int)
    {
        *reinterpret_cast<int*>(&wi->m_fAccuracy) = 1265353216;
    });
}
