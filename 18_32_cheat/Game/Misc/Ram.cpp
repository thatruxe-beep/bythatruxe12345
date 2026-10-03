#include "Game/Features.h"

#include "Game/Misc/Ram.hpp"

#include <algorithm>
#include <cmath>

void Ram::Update()
{
    static bool wasActive = false;
    static bool hasOrigin = false;
    static CVehicle* savedVehicle = nullptr;
    static CVector origin{};

    CPed* ped = FindPlayerPed();
    CVehicle* vehicle = ped ? ped->m_pVehicle : nullptr;
    const bool active = g_cfg.ram && vehicle && vehicle->m_pDriver == ped;

    if (active && (!wasActive || !hasOrigin || savedVehicle != vehicle))
    {
        savedVehicle = vehicle;
        origin = vehicle->GetPosition();
        hasOrigin = true;
    }

    if (active && hasOrigin)
    {
        CMatrix& matrix = vehicle->GetMatrix();
        CVector forward = matrix.GetForward();
        const float horizontal = sqrtf(forward.x * forward.x + forward.y * forward.y);
        if (horizontal > 0.001f)
        {
            const float acceleration = std::clamp(g_cfg.rampower, 1.0f, 20.0f) * 0.02f;
            vehicle->m_vecMoveSpeed.x += forward.x / horizontal * acceleration;
            vehicle->m_vecMoveSpeed.y += forward.y / horizontal * acceleration;
        }
    }

    if (!active && wasActive && hasOrigin)
    {
        // Only touch the saved pointer when it is still the player's current
        // vehicle; this avoids dereferencing a destroyed/streamed-out vehicle.
        if (vehicle && vehicle == savedVehicle)
        {
            vehicle->SetPosition(origin.x, origin.y, origin.z);
            vehicle->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
            vehicle->m_vecTurnSpeed = CVector(0.0f, 0.0f, 0.0f);
        }
        hasOrigin = false;
        savedVehicle = nullptr;
    }

    wasActive = active;
}
