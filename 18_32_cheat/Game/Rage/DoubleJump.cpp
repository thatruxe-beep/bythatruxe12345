#include "Game/Features.h"

#include "Game/Rage/DoubleJump.hpp"

void DoubleJump::Update()
{
    static bool wasPressed = false;
    static bool pendingBoost = false;

    const bool pressed = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
    CPed* ped = FindPlayerPed();

    if (!g_cfg.doublejump || !ped || ped->m_pVehicle)
    {
        pendingBoost = false;
        wasPressed = pressed;
        return;
    }

    if (pressed && !wasPressed)
    {
        pendingBoost = true;
    }

    // Apply after GTA has produced the real jump velocity, not on the input
    // frame before the jump task starts.
    if (pendingBoost && ped->m_vecMoveSpeed.z > 0.03f)
    {
        ped->m_vecMoveSpeed.x *= 2.0f;
        ped->m_vecMoveSpeed.y *= 2.0f;
        pendingBoost = false;
    }

    wasPressed = pressed;
}
