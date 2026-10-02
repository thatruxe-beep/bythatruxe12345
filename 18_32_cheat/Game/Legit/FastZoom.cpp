#include "Game/Features.h"

#include "Game/Legit/FastZoom.hpp"

#include <algorithm>

void FastZoom::Update()
{
    static bool zoomed = false;
    static float originalFov = 70.0f;

    const bool requested = g_cfg.fastzoom
        && (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
    CCam& camera = TheCamera.m_aCams[TheCamera.m_nActiveCam];

    if (requested)
    {
        if (!zoomed)
        {
            originalFov = camera.m_fFOV;
            zoomed = true;
        }

        camera.m_fFOV = 5.0f;
        *reinterpret_cast<float*>(0x8D5038) = 5.0f;
    }
    else if (zoomed)
    {
        const float restored = std::clamp(originalFov, 5.0f, 120.0f);
        camera.m_fFOV = restored;
        *reinterpret_cast<float*>(0x8D5038) = restored;
        zoomed = false;
    }
}
