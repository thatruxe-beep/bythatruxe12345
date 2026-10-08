#pragma once

#include <vector>

class CWeaponInfo;

class NewRapid
{
public:
    // Таблица оружия, с которой работают оба рапида: ids 22..38 (Colt 45 ..
    // Minigun), четыре уровня скилла eWeaponSkill и шесть float'ов цикла
    // атаки, начиная с CWeaponInfo::m_fAnimLoopStart.
    static constexpr int kGunTypeCount = 17;
    static constexpr int kSkillCount = 4;
    static constexpr int kFieldCount = 6;

    struct GunEntry
    {
        CWeaponInfo* info;
        int typeIndex; // 0..16 -> weapon id 22..38
        int skill;     // 0..3  -> eWeaponSkill
    };

    static void Update();
    // Undo any scaled weapon-table values. Used by Old Rapid Fire before it
    // takes its own snapshot of the original table.
    static void RestoreTable();
    // Все записи таблицы оружия ровно по одному разу. Оружие без уровней
    // скилла (RPG, миниган и т.д.) имеет только STD. Запрос других скиллов
    // для него адресует записи чужого оружия или выходит за конец таблицы.
    static const std::vector<GunEntry>& GunEntries();
};
