#include "Game/Features.h"

#include "Game/Misc/SpeedHack.hpp"

#include <algorithm>

void SpeedHack::Update()
{
    if (!g_cfg.speedhack || g_cfg.airbreake)
    {
        return;
    }

    static DWORD lastTick = 0;
    const DWORD now = GetTickCount();

    if (now - lastTick < 50)
    {
        return;
    }

    lastTick = now;
    CPed* ped = FindPlayerPed();

    if (!ped || !ped->m_pVehicle || ped->m_pVehicle->m_pDriver != ped)
    {
        return;
    }

    CVector& velocity = ped->m_pVehicle->m_vecMoveSpeed;
    const float length = VecLength(velocity);

    if (length <= 0.01f)
    {
        return;
    }

    const float boost = std::clamp(g_cfg.MaxSpd, 0.0f, 30.0f) / 180.0f;
    velocity.x += velocity.x / length * boost;
    velocity.y += velocity.y / length * boost;
    velocity.z += velocity.z / length * boost;
}
