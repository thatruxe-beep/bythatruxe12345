#include "Game/Features.h"

#include "Game/Legit/NoSpread.hpp"

void NoSpread::Update()
{
    static bool wasOn = false;
    static int snap[17][4] = {};
    static int spreadSnap[17][4] = {};

    // Идентификаторы оружия GTA SA: 22 (Colt 45) .. 38 (Minigun).
    // Дробовики: 25 (Shotgun), 26 (Sawn-off), 27 (Combat shotgun) —
    // числовые границы не зависят от ревизий enum в plugin-sdk.
    constexpr int kFirstWeaponId = 22;
    constexpr int kLastWeaponId = 38;
    constexpr int kFirstShotgunId = 25;
    constexpr int kLastShotgunId = 27;

    auto each = [&](auto fn)
    {
        for (int t = kFirstWeaponId; t <= kLastWeaponId; t++)
        {
            for (int s = 0; s < 4; s++)
            {
                if (CWeaponInfo* wi = CWeaponInfo::GetWeaponInfo((eWeaponType)t, (unsigned char)s))
                {
                    fn(wi, t - kFirstWeaponId, s);
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
            spreadSnap[ti][si] = *reinterpret_cast<int*>(&wi->m_fSpread);
        });
        wasOn = true;
    }

    // Дробовики обязаны сохранять родной разброс: восстанавливаем их
    // точность и угол разлёта дроби каждый кадр, пока NoSpread включён —
    // даже если что-то извне обнулило эти поля.
    each([&](CWeaponInfo* wi, int ti, int si)
    {
        if (ti + kFirstWeaponId >= kFirstShotgunId && ti + kFirstWeaponId <= kLastShotgunId)
        {
            *reinterpret_cast<int*>(&wi->m_fAccuracy) = snap[ti][si];
            *reinterpret_cast<int*>(&wi->m_fSpread) = spreadSnap[ti][si];
            return;
        }

        *reinterpret_cast<int*>(&wi->m_fAccuracy) = 1265353216;
    });
}
