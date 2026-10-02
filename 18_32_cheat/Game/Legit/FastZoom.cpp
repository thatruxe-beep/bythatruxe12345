#include "Game/Features.h"

#include "Game/Legit/FastZoom.hpp"

#include <algorithm>

namespace
{
    constexpr float kZoomFov = 5.0f;
    volatile LONG zoomActive = 0;

    void ApplyZoomToCamera()
    {
        CCam& camera = TheCamera.m_aCams[TheCamera.m_nActiveCam];
        camera.m_fFOV = kZoomFov;
        camera.m_fFOVSpeed = 0.0f;
        TheCamera.m_fFOVDuringInter = kZoomFov;
        TheCamera.m_fFOVWhenInterPol = kZoomFov;
        TheCamera.m_fFOVSpeedAtStartInter = 0.0f;
        TheCamera.m_fStartingFOVForInterPol = kZoomFov;
        TheCamera.m_fFOVNew = kZoomFov;
        TheCamera.m_bFOVLerpProcessed = true;
        *reinterpret_cast<float*>(0x8D5038) = kZoomFov;
    }
}

void FastZoom::Update()
{
    static bool middleWasDown = false;
    static bool zoomRequested = false;
    static bool saved = false;
    static int savedCameraIndex = 0;
    static float savedGlobalFov = 70.0f;
    static float savedCameraFov = 70.0f;
    static float savedCameraFovSpeed = 0.0f;
    static float savedDuringInter = 70.0f;
    static float savedWhenInterpol = 70.0f;
    static float savedSpeedAtInter = 0.0f;
    static float savedStartingInter = 70.0f;
    static float savedFovNew = 70.0f;
    static bool savedLerpProcessed = false;

    const bool aiming = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    const bool middleDown = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;

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
        if (!saved)
        {
            savedCameraIndex = TheCamera.m_nActiveCam;
            CCam& camera = TheCamera.m_aCams[savedCameraIndex];
            savedGlobalFov = *reinterpret_cast<float*>(0x8D5038);
            savedCameraFov = camera.m_fFOV;
            savedCameraFovSpeed = camera.m_fFOVSpeed;
            savedDuringInter = TheCamera.m_fFOVDuringInter;
            savedWhenInterpol = TheCamera.m_fFOVWhenInterPol;
            savedSpeedAtInter = TheCamera.m_fFOVSpeedAtStartInter;
            savedStartingInter = TheCamera.m_fStartingFOVForInterPol;
            savedFovNew = TheCamera.m_fFOVNew;
            savedLerpProcessed = TheCamera.m_bFOVLerpProcessed;
            saved = true;
        }

        InterlockedExchange(&zoomActive, 1);
        ApplyZoomToCamera();
    }
    else if (saved)
    {
        InterlockedExchange(&zoomActive, 0);
        CCam& camera = TheCamera.m_aCams[savedCameraIndex];
        camera.m_fFOV = std::clamp(savedCameraFov, 5.0f, 120.0f);
        camera.m_fFOVSpeed = savedCameraFovSpeed;
        TheCamera.m_fFOVDuringInter = savedDuringInter;
        TheCamera.m_fFOVWhenInterPol = savedWhenInterpol;
        TheCamera.m_fFOVSpeedAtStartInter = savedSpeedAtInter;
        TheCamera.m_fStartingFOVForInterPol = savedStartingInter;
        TheCamera.m_fFOVNew = savedFovNew;
        TheCamera.m_bFOVLerpProcessed = savedLerpProcessed;
        *reinterpret_cast<float*>(0x8D5038) = std::clamp(savedGlobalFov, 5.0f, 120.0f);
        saved = false;
    }

    middleWasDown = middleDown;
}

void FastZoom::Enforce()
{
    if (InterlockedCompareExchange(&zoomActive, 0, 0) != 0)
    {
        ApplyZoomToCamera();
    }
}
