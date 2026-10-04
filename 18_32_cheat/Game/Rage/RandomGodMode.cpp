#include "Game/Features.h"

#include "Game/Rage/RandomGodMode.hpp"

#include "minhook.hpp"

#include <algorithm>

// SA 1.0 US: CPedDamageResponseCalculator::ComputeDamageResponse.
// Адрес сверен по трём независимым источникам: plugin-sdk
// (CPedDamageResponseCalculator.cpp), MTA:SA client
// (CPedDamageResponseCalculatorSA.h, 0x4b5ac0) и декомпиляции gta_sa.exe.
constexpr DWORD kComputeDamageResponse = 0x004B5AC0;

namespace
{
    typedef void(__thiscall* ComputeDamageResponse_t)(void*, CPed*, CPedDamageResponse&, bool);
    ComputeDamageResponse_t callComputeDamageResponse = nullptr;

    unsigned int randomState = 0x1832u;

    void __fastcall hkComputeDamageResponse(void* self, void* edx,
        CPed* ped, CPedDamageResponse& response, bool bSpeak)
    {
        callComputeDamageResponse(self, ped, response, bSpeak);

        if (!g_cfg.randomgodmode)
        {
            return;
        }

        CPed* local = FindPlayerPed();

        if (!local || ped != local)
        {
            return;
        }

        if (response.m_fDamageHealth <= 0.0f && response.m_fDamageArmor <= 0.0f
            && !response.m_bForceDeath)
        {
            return;
        }

        randomState = randomState * 1664525u + 1013904223u;
        const int roll = static_cast<int>(randomState % 100u) + 1;
        const int chance = std::clamp(g_cfg.randomgodmode_chance, 1, 100);

        if (roll <= chance)
        {
            // Урон отменяется в момент нанесения: событие урона не уходит на
            // сервер, поэтому сервер не пересинхронизирует HP вниз (прежний
            // способ "восстановить значение на следующий кадр" серверная
            // синхронизация просто затирала).
            response.m_fDamageHealth = 0.0f;
            response.m_fDamageArmor = 0.0f;
            response.m_bHealthZero = false;
            response.m_bForceDeath = false;
        }
    }
}

void RandomGodMode::InstallHook()
{
    if (callComputeDamageResponse != nullptr)
    {
        return;
    }

    LPVOID target = reinterpret_cast<LPVOID>(kComputeDamageResponse);
    const MH_STATUS status = MH_CreateHook(target,
        reinterpret_cast<LPVOID>(&hkComputeDamageResponse),
        reinterpret_cast<LPVOID*>(&callComputeDamageResponse));

    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED)
    {
        callComputeDamageResponse = nullptr;
        return;
    }

    MH_EnableHook(target);
}

void RandomGodMode::RemoveHook()
{
    if (callComputeDamageResponse == nullptr)
    {
        return;
    }

    LPVOID target = reinterpret_cast<LPVOID>(kComputeDamageResponse);
    MH_DisableHook(target);
    MH_RemoveHook(target);
    callComputeDamageResponse = nullptr;
}
