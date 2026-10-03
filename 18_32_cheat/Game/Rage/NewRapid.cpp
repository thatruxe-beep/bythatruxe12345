#include "Game/Features.h"

#include "Game/Rage/NewRapid.hpp"

#include <algorithm>
#include <cmath>

namespace
{
    // Six attack-loop fields of CWeaponInfo starting at m_fAnimLoopStart.
    // Old Rapid Fire zeroes them for an instant re-arm; New Rapid scales the
    // whole loop window by 1/multiplier so the weapon re-arms proportionally
    // faster while keeping the normal shooting rhythm at multiplier 1.0.
    constexpr int kFieldCount = 6;
    constexpr int kFirstWeaponId = 22; // Colt 45
    constexpr int kLastWeaponId = 38;  // Minigun
    constexpr int kSkillCount = 4;

    float s_snapshot[kLastWeaponId - kFirstWeaponId + 1][kSkillCount][kFieldCount] = {};
    bool s_applied = false;
    float s_appliedScale = 1.0f;

    template <typename F>
    void EachWeaponInfo(F&& callback)
    {
        for (int type = kFirstWeaponId; type <= kLastWeaponId; ++type)
        {
            for (int skill = 0; skill < kSkillCount; ++skill)
            {
                if (CWeaponInfo* info = CWeaponInfo::GetWeaponInfo(
                        static_cast<eWeaponType>(type), static_cast<unsigned char>(skill)))
                {
                    callback(*info, type - kFirstWeaponId, skill);
                }
            }
        }
    }

    void TakeSnapshot()
    {
        EachWeaponInfo([](CWeaponInfo& info, int type, int skill)
        {
            float* values = reinterpret_cast<float*>(&info.m_fAnimLoopStart);
            for (int index = 0; index < kFieldCount; ++index)
            {
                s_snapshot[type][skill][index] = values[index];
            }
        });
    }

    void RestoreSnapshot()
    {
        EachWeaponInfo([](CWeaponInfo& info, int type, int skill)
        {
            float* values = reinterpret_cast<float*>(&info.m_fAnimLoopStart);
            for (int index = 0; index < kFieldCount; ++index)
            {
                values[index] = s_snapshot[type][skill][index];
            }
        });
    }

    void ApplyScale(float multiplier)
    {
        EachWeaponInfo([multiplier](CWeaponInfo& info, int type, int skill)
        {
            float* values = reinterpret_cast<float*>(&info.m_fAnimLoopStart);
            for (int index = 0; index < kFieldCount; ++index)
            {
                values[index] = s_snapshot[type][skill][index] / multiplier;
            }
        });
    }
}

void NewRapid::Update()
{
    // Old Rapid Fire owns the same table cells and has priority: it rewrites
    // the fields to zero every frame, which would overwrite any scaled copy.
    const bool enabled = g_cfg.newrapid && !g_cfg.rapidfire;
    const float multiplier = std::clamp(
        std::round(g_cfg.rapidfire_multiplier * 10.0f) / 10.0f, 1.0f, 10.0f);

    if (!enabled)
    {
        if (s_applied)
        {
            RestoreSnapshot();
            s_applied = false;
            s_appliedScale = 1.0f;
        }
        return;
    }

    if (!s_applied)
    {
        // The table holds original values here: Old Rapid restores its own
        // snapshot on disable and calls RestoreTable() before snapshotting,
        // so this snapshot never captures scaled copies.
        TakeSnapshot();
        s_applied = true;
        s_appliedScale = 1.0f;
    }

    if (multiplier == s_appliedScale)
    {
        return;
    }

    if (multiplier <= 1.0f)
    {
        RestoreSnapshot();
    }
    else
    {
        ApplyScale(multiplier);
    }
    s_appliedScale = multiplier;
}

void NewRapid::RestoreTable()
{
    // Called by Old Rapid Fire before it takes its own snapshot, so it never
    // captures scaled values as "originals" (and later restores garbage).
    if (s_applied)
    {
        RestoreSnapshot();
        s_applied = false;
        s_appliedScale = 1.0f;
    }
}
