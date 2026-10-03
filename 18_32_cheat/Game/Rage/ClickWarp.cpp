#include "Game/Features.h"

#include "Game/Rage/ClickWarp.hpp"

#include "imgui.h"

namespace
{
    constexpr float kReach = 300.0f;   // дальность луча прицела
    constexpr float kSkyProbe = 150.0f; // шаг вперёд при прицеле в небо

    void DrawCrosshair(ImDrawList* draw, const ImVec2& center, float s, ImU32 accent)
    {
        // Точка в центре
        draw->AddCircleFilled(center, 1.7f * s, accent);

        // Четыре засечки с чёрной обводкой (читаемость на любом фоне)
        const float gap = 6.0f * s;
        const float len = 7.0f * s;
        const ImVec2 ticks[4][2] = {
            { ImVec2(center.x, center.y - gap - len), ImVec2(center.x, center.y - gap) },
            { ImVec2(center.x, center.y + gap), ImVec2(center.x, center.y + gap + len) },
            { ImVec2(center.x - gap - len, center.y), ImVec2(center.x - gap, center.y) },
            { ImVec2(center.x + gap, center.y), ImVec2(center.x + gap + len, center.y) },
        };

        for (const auto& tick : ticks)
        {
            draw->AddLine(tick[0], tick[1], IM_COL32(0, 0, 0, 200), 3.2f * s);
        }
        for (const auto& tick : ticks)
        {
            draw->AddLine(tick[0], tick[1], accent, 1.4f * s);
        }

        // Подсказка под прицелом
        const char* hint = "СКМ — телепорт";
        const ImVec2 hintSize = ImGui::CalcTextSize(hint);
        const ImVec2 hintPos(center.x - hintSize.x * 0.5f, center.y + 20.0f * s);
        draw->AddText(ImVec2(hintPos.x + 1.0f, hintPos.y + 1.0f), IM_COL32(0, 0, 0, 200), hint);
        draw->AddText(hintPos, IM_COL32(255, 255, 255, 170), hint);
    }
}

void ClickWarp::Update()
{
    if (!g_cfg.clickwarp || g_cfg.menu_open)
    {
        return;
    }

    CPed* ped = FindPlayerPed();
    if (!ped)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const float s = g_cfg.ui_scale / 100.0f;
    const ImU32 accent = static_cast<ImU32>(g_cfg.accent.to_color().as_imcolor());

    DrawCrosshair(draw, center, s, accent);

    if (!(GetAsyncKeyState(VK_MBUTTON) & 1))
    {
        return;
    }

    // Луч из активной камеры через центр экрана.
    CCam& cam = TheCamera.m_aCams[TheCamera.m_nActiveCam];
    CVector direction = cam.m_vecFront;
    const float length = VecLength(direction);
    if (length < 0.0001f)
    {
        return;
    }
    direction = CVector(direction.x / length, direction.y / length, direction.z / length);

    const CVector origin = cam.m_vecSource;
    const CVector target(
        origin.x + direction.x * kReach,
        origin.y + direction.y * kReach,
        origin.z + direction.z * kReach);

    CColPoint hit;
    CEntity* hitEntity = nullptr;
    CVector destination(0.0f, 0.0f, 0.0f);
    bool haveDestination = false;

    if (CWorld::ProcessLineOfSight(origin, target, hit, hitEntity,
            true, true, false, true, false, true, false, false))
    {
        // Точка попадания: чуть отступаем от поверхности и поднимаем над землёй.
        const CVector& point = hit.m_vecPoint;
        const CVector& normal = hit.m_vecNormal;
        destination = CVector(
            point.x + normal.x * 0.5f,
            point.y + normal.y * 0.5f,
            point.z + normal.z * 0.5f + 1.0f);
        haveDestination = true;
    }
    else
    {
        // Прицел в небо: шаг вперёд по направлению камеры и падение на землю.
        const float probeX = origin.x + direction.x * kSkyProbe;
        const float probeY = origin.y + direction.y * kSkyProbe;
        const float probeZ = origin.z + direction.z * kSkyProbe;
        bool found = false;
        CEntity* groundEntity = nullptr;
        const float groundZ = CWorld::FindGroundZFor3DCoord(probeX, probeY, probeZ, &found, &groundEntity);
        if (found)
        {
            destination = CVector(probeX, probeY, groundZ + 1.0f);
            haveDestination = true;
        }
    }

    if (!haveDestination)
    {
        return;
    }

    if (CVehicle* vehicle = ped->m_pVehicle)
    {
        // В машине варпается машина вместе с игроком.
        vehicle->SetPosn(destination.x, destination.y, destination.z);
        vehicle->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
        vehicle->m_vecTurnSpeed = CVector(0.0f, 0.0f, 0.0f);
    }
    else
    {
        ped->SetPosn(destination.x, destination.y, destination.z);
        ped->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
    }
}
