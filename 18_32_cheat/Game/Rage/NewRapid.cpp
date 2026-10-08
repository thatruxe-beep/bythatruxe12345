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
    constexpr int kFirstGunId = 22;        // Colt 45
    constexpr int kLastGunId = 38;         // Minigun
    constexpr int kFirstSkilledGunId = 22; // Colt 45 - первое оружие со скиллом
    constexpr int kLastSkilledGunId = 32;  // Tec-9 - последнее оружие со скиллом
    constexpr int kStdSkill = 1;           // eWeaponSkill::STD

    float s_snapshot[NewRapid::kGunTypeCount][NewRapid::kSkillCount][NewRapid::kFieldCount] = {};
    bool s_applied = false;

    float* LoopFields(CWeaponInfo& info)
    {
        return reinterpret_cast<float*>(&info.m_fAnimLoopStart);
    }

    void TakeSnapshot()
    {
        for (const auto& entry : NewRapid::GunEntries())
        {
            const float* values = LoopFields(*entry.info);
            for (int index = 0; index < NewRapid::kFieldCount; ++index)
            {
                s_snapshot[entry.typeIndex][entry.skill][index] = values[index];
            }
        }
    }

    // Пишет исходные значения, поделённые на multiplier (1.0 — исходные).
    void WriteFromSnapshot(float multiplier)
    {
        for (const auto& entry : NewRapid::GunEntries())
        {
            float* values = LoopFields(*entry.info);
            for (int index = 0; index < NewRapid::kFieldCount; ++index)
            {
                values[index] = s_snapshot[entry.typeIndex][entry.skill][index] / multiplier;
            }
        }
    }
}

const std::vector<NewRapid::GunEntry>& NewRapid::GunEntries()
{
    // Адреса записей фиксированы, поэтому список строится один раз. Пары без
    // отдельной записи не добавляем: индексы >= 80 лежат за концом aWeaponInfo
    // и попадают в соседние глобалы игры (trapDisplay, GsubSysInfo и др.).
    static const std::vector<GunEntry> entries = []
    {
        std::vector<GunEntry> list;
        for (int id = kFirstGunId; id <= kLastGunId; ++id)
        {
            const bool hasSkills = id >= kFirstSkilledGunId && id <= kLastSkilledGunId;
            for (int skill = 0; skill < kSkillCount; ++skill)
            {
                if (!hasSkills && skill != kStdSkill)
                {
                    continue;
                }

                if (CWeaponInfo* info = CWeaponInfo::GetWeaponInfo(
                        static_cast<eWeaponType>(id), static_cast<unsigned char>(skill)))
                {
                    list.push_back({ info, id - kFirstGunId, skill });
                }
            }
        }
        return list;
    }();

    return entries;
}

void NewRapid::Update()
{
    // Old Rapid Fire owns the same table cells and has priority: it rewrites
    // the fields to zero every frame, which would overwrite any scaled copy.
    const bool enabled = g_cfg.newrapid && !g_cfg.rapidfire;
    const float multiplier = std::clamp(
        std::round(g_cfg.rapidfire_multiplier * 10.0f) / 10.0f, 1.0f, 10.0f);

    if (!enabled || multiplier <= 1.0f)
    {
        RestoreTable();
        return;
    }

    if (!s_applied)
    {
        // The table holds original values here: Old Rapid restores its own
        // snapshot on disable and calls RestoreTable() before snapshotting,
        // so this snapshot never captures scaled copies.
        TakeSnapshot();
        s_applied = true;
    }

    // Записывается каждый кадр из исходного снимка: таблица всегда совпадает
    // с текущим множителем, даже если значения были перезаписаны.
    WriteFromSnapshot(multiplier);
}

void NewRapid::RestoreTable()
{
    // Called by Old Rapid Fire before it takes its own snapshot, so it never
    // captures scaled values as "originals" (and later restores garbage).
    if (!s_applied)
    {
        return;
    }

    WriteFromSnapshot(1.0f);
    s_applied = false;
}
