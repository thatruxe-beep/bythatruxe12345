#include "Game/Features.h"

#include "Game/Legit/FastZoom.hpp"

#include <algorithm>

void FastZoom::Update()
{
    static bool middleWasDown = false;
    static bool zoomRequested = false;
    static bool zoomApplied = false;
    static float originalFov = 70.0f;

    const bool aiming = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    const bool middleDown = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
    CCam& camera = TheCamera.m_aCams[TheCamera.m_nActiveCam];

    // One MMB click toggles maximum zoom, but only while the player is
    // actually holding the aim button. Leaving aim always resets the zoom.
    if (g_cfg.fastzoom && aiming && middleDown && !middleWasDown)
    {
        zoomRequested = !zoomRequested;
    }

    if (!g_cfg.fastzoom || !aiming)
    {
        zoomRequested = false;
    }

    if (zoomRequested)
    {
        if (!zoomApplied)
        {
            originalFov = camera.m_fFOV;
            zoomApplied = true;
        }
        camera.m_fFOV = 5.0f;
        *reinterpret_cast<float*>(0x8D5038) = 5.0f;
    }
    else if (zoomApplied)
    {
        const float restored = std::clamp(originalFov, 5.0f, 120.0f);
        camera.m_fFOV = restored;
        *reinterpret_cast<float*>(0x8D5038) = restored;
        zoomApplied = false;
    }

    middleWasDown = middleDown;
}
