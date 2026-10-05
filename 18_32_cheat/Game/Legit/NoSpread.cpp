#include "Game/Features.h"

#include "Game/Legit/NoSpread.hpp"

void NoSpread::Update()
{
    // NoRecoil (merged here): patch the camera recoil push once per toggle.
    // 0x008D610F is "mov esi, 0.75f" inside the recoil code - the patched
    // "mov esi, 0" removes the recoil impulse entirely.
    static bool recoilApplied = false;

    if (g_cfg.norecoil != recoilApplied)
    {
        recoilApplied = g_cfg.norecoil;

        const unsigned char on[] = { 0xBE, 0x00, 0x00, 0x00, 0x00 };
        const unsigned char off[] = { 0xBE, 0x00, 0x00, 0x40, 0x3F };
        PatchBytes(reinterpret_cast<void*>(0x008D610F), recoilApplied ? on : off, 5);
    }

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

    each([](CWeaponInfo* wi, int ti, int si)
    {
        // ti = ID - 22, дробовики 25/26/27 -> ti 3/4/5: им каждый кадр
        // возвращается снапшотная (родная) точность, иначе сервер
        // (setWeaponProperty) может сам выставить им максимальную точность -
        // выглядело бы, будто NoSpread работает и на дробовиках.
        const bool isShotgun = ti >= 3 && ti <= 5;
        *reinterpret_cast<int*>(&wi->m_fAccuracy) = isShotgun ? snap[ti][si] : 1265353216;
    });
}
