#include "Game/Features.h"

#include "Game/Legit/NoSpread.hpp"

#include "Core/Diagnostics.hpp"

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
    // Ванильная точность дробовиков (колонка Accuracy из data\weapon.dat),
    // по уровню навыка 0..3. Значения из оригинального weapon.dat зашиты
    // прямо в код, чтобы защита работала даже без файла; если файл игры
    // читается — он переопределяет их. Зачем всё это: сервер
    // (setWeaponProperty) может поднять точность дробовиков при подключении,
    // и тогда снапшот "родной" точности в момент включения NoSpread уже
    // испорчен. Слот навыка 3 (только для Colt45) дробовикам не существует —
    // туда дублируется уровень 2.
    float vanillaAccuracy[3][4]
    {
        { 1.0f, 1.2f, 1.4f, 1.4f },  // 25 SHOTGUN (в weapon.dat зовётся SHOTGUN)
        { 0.7f, 0.8f, 0.9f, 0.9f },  // 26 SAWNOFF
        { 1.4f, 1.8f, 2.0f, 2.0f },  // 27 SPAS12 (combat shotgun)
    };

    bool vanillaReady = false;

    // Формат строки оружия в weapons.dat (токены после '$'):
    // 0 имя, 1 тип огня, 2 дальность прицеливания, 3 дальность, 4 модель,
    // 5 модель2, 6 перезарядка, 7 группа анимации, 8 магазин, 9 урон,
    // 10-12 сдвиг дула, 13 уровень навыка, 14 требуемый навык, 15 ТОЧНОСТЬ.
    void LoadVanillaShotgunAccuracy()
    {
        wchar_t widePath[MAX_PATH]{};

        if (GetModuleFileNameW(nullptr, widePath, MAX_PATH) == 0)
        {
            return;
        }

        wchar_t* slash = wcsrchr(widePath, L'\\');

        if (!slash)
        {
            return;
        }

        slash[1] = L'\0';

        char directory[MAX_PATH]{};

        if (WideCharToMultiByte(CP_ACP, 0, widePath, -1, directory, sizeof(directory), nullptr, nullptr) <= 0)
        {
            return;
        }

        // Игра сама открывает DATA\WEAPON.DAT (CWeaponInfo::LoadWeaponData) —
        // имя файла в единственном числе.
        std::ifstream file(std::string(directory) + "data\\weapon.dat");

        if (!file.is_open())
        {
            return;
        }

        std::string line;

        while (std::getline(file, line))
        {
            if (line.empty() || line[0] != '$')
            {
                continue; // строки оружия начинаются с '$'
            }

            std::istringstream stream(line.substr(1));
            std::string token[16];
            int count = 0;

            while (count < 16 && stream >> token[count])
            {
                ++count;
            }

            if (count < 16)
            {
                continue;
            }

            int index = -1;

            // Имена из weapon.dat (сверено с реверсом CWeaponInfo::FindWeaponType):
            // CHROMEGUN и SHOTGSPA — имена МОДЕЛЕЙ из default.ide, их тут нет.
            if (token[0] == "SHOTGUN")       index = 0; // 25 дробовик
            else if (token[0] == "SAWNOFF")  index = 1; // 26 обрез
            else if (token[0] == "SPAS12")   index = 2; // 27 combat shotgun
            else
            {
                continue;
            }

            const int skill = atoi(token[13].c_str());

            if (skill < 0 || skill > 3)
            {
                continue;
            }

            const float accuracy = static_cast<float>(atof(token[15].c_str()));

            if (accuracy > 0.0f && accuracy <= 2.5f)
            {
                vanillaAccuracy[index][skill] = accuracy;
            }
        }
    }
}

void NoSpread::Update()
{
    // NoRecoil (влит сюда): патч импульса отдачи камеры при переключении.
    // 0x008D610F - "mov esi, 0.75f" в коде отдачи, патчим на "mov esi, 0".
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

    if (!vanillaReady)
    {
        vanillaReady = true;
        LoadVanillaShotgunAccuracy();
        Diagnostics::Log(
            "nospread: weapons.dat 25=%.2f/%.2f/%.2f/%.2f 26=%.2f/%.2f/%.2f/%.2f 27=%.2f/%.2f/%.2f/%.2f",
            vanillaAccuracy[0][0], vanillaAccuracy[0][1], vanillaAccuracy[0][2], vanillaAccuracy[0][3],
            vanillaAccuracy[1][0], vanillaAccuracy[1][1], vanillaAccuracy[1][2], vanillaAccuracy[1][3],
            vanillaAccuracy[2][0], vanillaAccuracy[2][1], vanillaAccuracy[2][2], vanillaAccuracy[2][3]);
    }

    auto each = [](auto fn)
    {
        // Идентификаторы оружия GTA SA: 22 (Colt 45) .. 38 (Minigun),
        // числовые ID, не зависящие от ревизий enum в plugin-sdk.
        constexpr int kFirstWeaponId = 22;
        constexpr int kLastWeaponId = 38;

        for (int t = kFirstWeaponId; t <= kLastWeaponId; t++)
        {
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

        Diagnostics::Log(
            "nospread: on; live 25=%.2f/%.2f/%.2f/%.2f 26=%.2f/%.2f/%.2f/%.2f 27=%.2f/%.2f/%.2f/%.2f",
            *reinterpret_cast<float*>(&snap[3][0]), *reinterpret_cast<float*>(&snap[3][1]),
            *reinterpret_cast<float*>(&snap[3][2]), *reinterpret_cast<float*>(&snap[3][3]),
            *reinterpret_cast<float*>(&snap[4][0]), *reinterpret_cast<float*>(&snap[4][1]),
            *reinterpret_cast<float*>(&snap[4][2]), *reinterpret_cast<float*>(&snap[4][3]),
            *reinterpret_cast<float*>(&snap[5][0]), *reinterpret_cast<float*>(&snap[5][1]),
            *reinterpret_cast<float*>(&snap[5][2]), *reinterpret_cast<float*>(&snap[5][3]));
    }

    each([](CWeaponInfo* wi, int ti, int si)
    {
        // ti = ID - 22; дробовики 25/26/27 -> ti 3/4/5.
        const bool isShotgun = ti >= 3 && ti <= 5;

        if (!isShotgun)
        {
            *reinterpret_cast<int*>(&wi->m_fAccuracy) = 1265353216;
            return;
        }

        // Дробовики: принудительно ванильная точность из weapons.dat —
        // ни NoSpread, ни сервер не должны собирать дробь в точку.
        // Если файл не прочитался — снапшот на момент включения.
        const float vanilla = vanillaAccuracy[ti - 3][si];

        if (vanilla > 0.0f)
        {
            wi->m_fAccuracy = vanilla;
        }
        else
        {
            *reinterpret_cast<int*>(&wi->m_fAccuracy) = snap[ti][si];
        }
    });
}
