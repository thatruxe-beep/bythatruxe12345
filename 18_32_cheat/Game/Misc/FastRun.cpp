#include "Game/Features.h"

#include "Game/Misc/FastRun.hpp"

#include <algorithm>

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

    if (g_cfg.fastbeg)
    {
        // MTA may restore animation-group speeds every frame, so keep applying
        // the selected multiplier while the feature is enabled.
        ApplyRunSpeed(std::clamp(g_cfg.fastbegs, 1.0f, 10.0f));
    }
    else if (wasEnabled)
    {
        ApplyRunSpeed(1.0f);
    }

    wasEnabled = g_cfg.fastbeg;
}
