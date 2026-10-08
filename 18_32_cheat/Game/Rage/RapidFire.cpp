#include "Game/Features.h"

#include "Game/Rage/NewRapid.hpp"
#include "Game/Rage/RapidFire.hpp"

namespace
{
    float* LoopFields(CWeaponInfo& info)
    {
        return reinterpret_cast<float*>(&info.m_fAnimLoopStart);
    }
}

void RapidFire::Update()
{
    static bool wasOn = false;
    static float snapshot[NewRapid::kGunTypeCount][NewRapid::kSkillCount][NewRapid::kFieldCount]{};

    const auto& entries = NewRapid::GunEntries();

    if (!g_cfg.rapidfire)
    {
        if (wasOn)
        {
            for (const auto& entry : entries)
            {
                float* values = LoopFields(*entry.info);
                for (int index = 0; index < NewRapid::kFieldCount; ++index)
                {
                    values[index] = snapshot[entry.typeIndex][entry.skill][index];
                }
            }
            wasOn = false;
        }
        return;
    }

    if (!wasOn)
    {
        // New Rapid may have scaled the same fields; undo that first so this
        // snapshot captures true original values (and restores them later).
        NewRapid::RestoreTable();
        for (const auto& entry : entries)
        {
            const float* values = LoopFields(*entry.info);
            for (int index = 0; index < NewRapid::kFieldCount; ++index)
            {
                snapshot[entry.typeIndex][entry.skill][index] = values[index];
            }
        }
        wasOn = true;
    }

    // Restore the original instant rapid-fire implementation requested by the
    // user. Zeroing all six attack-loop fields was the previously working path.
    for (const auto& entry : entries)
    {
        float* values = LoopFields(*entry.info);
        for (int index = 0; index < NewRapid::kFieldCount; ++index)
        {
            values[index] = 0.0f;
        }
    }
}
