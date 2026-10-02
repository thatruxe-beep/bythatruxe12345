#include "Game/Features.h"

#include "Game/Rage/RapidFire.hpp"

#include <algorithm>
#include <cmath>

namespace
{
    struct RapidSnapshot
    {
        float loopStart{};
        float loopEnd{};
        unsigned int loopFire{};
        unsigned int loop2Start{};
        unsigned int loop2End{};
        unsigned int loop2Fire{};
        float breakout{};
    };

    unsigned int ScaleTime(unsigned int value, float multiplier)
    {
        if (value == 0u)
        {
            return 0u;
        }
        const unsigned int scaled = static_cast<unsigned int>(std::lround(value / multiplier));
        return scaled == 0u ? 1u : scaled;
    }
}

void RapidFire::Update()
{
    static bool captured = false;
    static RapidSnapshot original[17][4]{};

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
        if (captured)
        {
            each([&](CWeaponInfo& info, int type, int skill)
            {
                const RapidSnapshot& value = original[type][skill];
                info.m_fAnimLoopStart = value.loopStart;
                info.m_fAnimLoopEnd = value.loopEnd;
                info.m_nAnimLoopFire = value.loopFire;
                info.m_nAnimLoop2Start = value.loop2Start;
                info.m_nAnimLoop2End = value.loop2End;
                info.m_nAnimLoop2Fire = value.loop2Fire;
                info.m_fBreakoutTime = value.breakout;
            });
            captured = false;
        }
        return;
    }

    if (!captured)
    {
        each([&](CWeaponInfo& info, int type, int skill)
        {
            RapidSnapshot& value = original[type][skill];
            value.loopStart = info.m_fAnimLoopStart;
            value.loopEnd = info.m_fAnimLoopEnd;
            value.loopFire = info.m_nAnimLoopFire;
            value.loop2Start = info.m_nAnimLoop2Start;
            value.loop2End = info.m_nAnimLoop2End;
            value.loop2Fire = info.m_nAnimLoop2Fire;
            value.breakout = info.m_fBreakoutTime;
        });
        captured = true;
    }

    const float multiplier = std::clamp(
        std::round(g_cfg.rapidfire_multiplier * 10.0f) / 10.0f, 1.0f, 10.0f);

    each([&](CWeaponInfo& info, int type, int skill)
    {
        const RapidSnapshot& value = original[type][skill];
        info.m_fAnimLoopStart = value.loopStart / multiplier;
        info.m_fAnimLoopEnd = value.loopEnd / multiplier;
        info.m_nAnimLoopFire = ScaleTime(value.loopFire, multiplier);
        info.m_nAnimLoop2Start = ScaleTime(value.loop2Start, multiplier);
        info.m_nAnimLoop2End = ScaleTime(value.loop2End, multiplier);
        info.m_nAnimLoop2Fire = ScaleTime(value.loop2Fire, multiplier);
        info.m_fBreakoutTime = value.breakout / multiplier;
    });
}
