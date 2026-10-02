#include "Game/Features.h"

#include "Game/Misc/FastRun.hpp"

#include <algorithm>
#include <cmath>

void FastRun::ApplyRunSpeed(float speed)
{
    CPed* ped = FindPlayerPed();

    if (!ped)
    {
        return;
    }

    plugin::Command<0x0393>(ped, "WOMAN_RUN", speed);
    plugin::Command<0x0393>(ped, "WOMAN_RUNBUSY", speed);
    plugin::Command<0x0393>(ped, "WOMAN_RUNPANIC", speed);
    plugin::Command<0x0393>(ped, "WOMAN_RUNSEXY", speed);
    plugin::Command<0x0393>(ped, "SPRINT_CIVI", speed);
    plugin::Command<0x0393>(ped, "SPRINT_PANIC", speed);
    plugin::Command<0x0393>(ped, "SWAT_RUN", speed);
    plugin::Command<0x0393>(ped, "FATSPRINT", speed);
}

void FastRun::Update()
{
    static bool wasEnabled = false;
    static float appliedSpeed = -1.0f;
    const float requestedSpeed = g_cfg.fastbeg
        ? std::clamp(g_cfg.fastbegs, 1.0f, 10.0f)
        : 1.0f;

    // Animation group updates are expensive and do not need to run every frame.
    if (wasEnabled == g_cfg.fastbeg && fabsf(appliedSpeed - requestedSpeed) < 0.001f)
    {
        return;
    }

    ApplyRunSpeed(requestedSpeed);
    appliedSpeed = requestedSpeed;
    wasEnabled = g_cfg.fastbeg;
}
