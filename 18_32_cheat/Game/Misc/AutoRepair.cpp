#include "Game/Features.h"

#include "Game/Misc/AutoRepair.hpp"

void AutoRepair::Update()
{
    if (g_cfg.autorepair)
    {
        Repair::Update();
    }
}
