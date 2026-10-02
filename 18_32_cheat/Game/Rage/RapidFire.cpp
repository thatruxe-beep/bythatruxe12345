#include "Game/Features.h"

#include "Game/Rage/RapidFire.hpp"

void RapidFire::Update()
{
    static bool wasOn = false;
    static float snapshot[17][4][6]{};

    auto each = [](auto&& callback)
    {
        for (int type = 22; type <= 38; ++type)
        {
            for (int skill = 0; skill < 4; ++skill)
            {
                if (CWeaponInfo* info = CWeaponInfo::GetWeaponInfo(
                    static_cast<eWeaponType>(type), static_cast<unsigned char>(skill)))
                {
                    callback(*info, type - 22, skill);
                }
            }
        }
    };

    if (!g_cfg.rapidfire)
    {
        if (wasOn)
        {
            each([&](CWeaponInfo& info, int type, int skill)
            {
                float* values = reinterpret_cast<float*>(&info.m_fAnimLoopStart);
                for (int index = 0; index < 6; ++index)
                {
                    values[index] = snapshot[type][skill][index];
                }
            });
            wasOn = false;
        }
        return;
    }

    if (!wasOn)
    {
        each([&](CWeaponInfo& info, int type, int skill)
        {
            float* values = reinterpret_cast<float*>(&info.m_fAnimLoopStart);
            for (int index = 0; index < 6; ++index)
            {
                snapshot[type][skill][index] = values[index];
            }
        });
        wasOn = true;
    }

    // Restore the original instant rapid-fire implementation requested by the
    // user. Zeroing all six attack-loop fields was the previously working path.
    each([](CWeaponInfo& info, int, int)
    {
        float* values = reinterpret_cast<float*>(&info.m_fAnimLoopStart);
        for (int index = 0; index < 6; ++index)
        {
            values[index] = 0.0f;
        }
    });
}
