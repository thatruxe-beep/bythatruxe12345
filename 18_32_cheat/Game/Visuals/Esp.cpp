#include "Game/Features.h"

#include "Game/Visuals/Esp.hpp"

#include "Menu/Menu.hpp"

#include "imgui.h"
#include "CColModel.h"
#include "CPools.h"
#include "CSprite.h"
#include "ePedBones.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    constexpr float kMaximumHealth = 300.0f;
    constexpr float kMaximumArmor = 128.0f;

    bool WorldToScreen(const RwV3d& world, ImVec2& screen)
    {
        RwV3d projected{};
        float width = 0.0f;
        float height = 0.0f;

        if (!CSprite::CalcScreenCoors(world, &projected, &width, &height, false, false))
        {
            return false;
        }

        screen = ImVec2(projected.x, projected.y);
        return true;
    }

    void DrawOutlinedLine(ImDrawList* draw, const ImVec2& from, const ImVec2& to,
        ImU32 color, float thickness)
    {
        draw->AddLine(from, to, IM_COL32(0, 0, 0, 190), thickness + 2.0f);
        draw->AddLine(from, to, color, thickness);
    }

    void DrawCenteredText(ImDrawList* draw, float centerX, float y,
        ImU32 color, const char* text)
    {
        const ImVec2 size = ImGui::CalcTextSize(text);
        const ImVec2 pos(centerX - size.x * 0.5f, y);
        draw->AddText(pos + ImVec2(1.0f, 1.0f), IM_COL32(0, 0, 0, 230), text);
        draw->AddText(pos, color, text);
    }
}

void Esp::Update()
{
    if (!g_cfg.wh)
    {
        return;
    }

    CPed* local = FindPlayerPed();

    if (!local || !CPools::ms_pPedPool)
    {
        return;
    }

    const CVector localPosition = local->GetPosition();
    const float scale = menu->GetScale();
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImDrawList* draw = ImGui::GetForegroundDrawList();

    const ImU32 boxColor = static_cast<ImU32>(g_cfg.whcol.to_color().as_imcolor());
    const ImU32 boxFill = static_cast<ImU32>(g_cfg.whcol.to_color(18).as_imcolor());
    const ImU32 hpColor = static_cast<ImU32>(g_cfg.hpcol.to_color().as_imcolor());
    const ImU32 armorColor = static_cast<ImU32>(g_cfg.armorcol.to_color().as_imcolor());
    const ImU32 textColor = static_cast<ImU32>(g_cfg.distcol.to_color().as_imcolor());
    const ImU32 skeletonColor = static_cast<ImU32>(g_cfg.skelcol.to_color().as_imcolor());
    const ImU32 tracerColor = static_cast<ImU32>(g_cfg.snapcol.to_color().as_imcolor());
    const ImU32 backgroundColor = IM_COL32(0, 0, 0, 190);

    CMatrix& camera = TheCamera.m_mCameraMatrix;
    const int poolSize = CPools::ms_pPedPool->m_nSize;

    for (int i = 0; i < poolSize; ++i)
    {
        CPed* ped = CPools::ms_pPedPool->GetAt(i);

        if (!ped || ped == local || ped->m_fHealth <= 0.0f)
        {
            continue;
        }

        const CVector position = ped->GetPosition();
        const CVector delta(position.x - localPosition.x,
            position.y - localPosition.y, position.z - localPosition.z);
        const float distance = VecLength(delta);

        if (distance > g_cfg.whDistance)
        {
            continue;
        }

        const float cameraDepth = delta.x * camera.at.x
            + delta.y * camera.at.y + delta.z * camera.at.z;

        if (cameraDepth <= 0.0f)
        {
            continue;
        }

        RwV3d headWorld{};
        ped->GetBonePosition(headWorld, BONE_HEAD, true);
        headWorld.z += 0.12f;

        float feetZ = position.z;
        if (CColModel* collision = ped->GetColModel())
        {
            const float minimumZ = collision->m_boundBox.m_vecMin.z;
            if (minimumZ >= -2.0f && minimumZ <= 0.5f)
            {
                feetZ += minimumZ;
            }
        }

        const RwV3d feetWorld = { position.x, position.y, feetZ };
        ImVec2 headScreen{};
        ImVec2 feetScreen{};

        if (!WorldToScreen(headWorld, headScreen) || !WorldToScreen(feetWorld, feetScreen))
        {
            continue;
        }

        const float boxHeight = feetScreen.y - headScreen.y;
        if (boxHeight < 4.0f)
        {
            continue;
        }

        const float boxWidth = boxHeight * 0.46f;
        const float centerX = (headScreen.x + feetScreen.x) * 0.5f;
        const ImVec2 boxMin(centerX - boxWidth * 0.5f, headScreen.y);
        const ImVec2 boxMax(centerX + boxWidth * 0.5f, feetScreen.y);

        if (g_cfg.wh_flags & WH_SNAP)
        {
            DrawOutlinedLine(draw,
                ImVec2(displaySize.x * 0.5f, displaySize.y - 1.0f),
                ImVec2(centerX, boxMax.y), tracerColor, 1.5f);
        }

        if (g_cfg.wh_flags & WH_BOX)
        {
            draw->AddRectFilled(boxMin, boxMax, boxFill);
            draw->AddRect(boxMin - ImVec2(1.0f, 1.0f),
                boxMax + ImVec2(1.0f, 1.0f), backgroundColor);
            draw->AddRect(boxMin, boxMax, boxColor);
            draw->AddRect(boxMin + ImVec2(1.0f, 1.0f),
                boxMax - ImVec2(1.0f, 1.0f), backgroundColor);
        }

        if (g_cfg.wh_flags & WH_HP)
        {
            const float healthFraction = std::clamp(ped->m_fHealth / kMaximumHealth, 0.0f, 1.0f);
            const ImVec2 barMin(boxMin.x - 7.0f * scale, boxMin.y);
            const ImVec2 barMax(boxMin.x - 3.0f * scale, boxMax.y);
            draw->AddRectFilled(barMin - ImVec2(1.0f, 1.0f),
                barMax + ImVec2(1.0f, 1.0f), backgroundColor);
            draw->AddRectFilled(barMin, barMax, IM_COL32(18, 18, 18, 210));
            draw->AddRectFilled(
                ImVec2(barMin.x, barMax.y - (barMax.y - barMin.y) * healthFraction),
                barMax, hpColor);
        }

        if (g_cfg.wh_flags & WH_ARMOR)
        {
            const float armorFraction = std::clamp(ped->m_fArmour / kMaximumArmor, 0.0f, 1.0f);
            const ImVec2 barMin(boxMax.x + 3.0f * scale, boxMin.y);
            const ImVec2 barMax(boxMax.x + 7.0f * scale, boxMax.y);
            draw->AddRectFilled(barMin - ImVec2(1.0f, 1.0f),
                barMax + ImVec2(1.0f, 1.0f), backgroundColor);
            draw->AddRectFilled(barMin, barMax, IM_COL32(18, 18, 18, 210));
            draw->AddRectFilled(
                ImVec2(barMin.x, barMax.y - (barMax.y - barMin.y) * armorFraction),
                barMax, armorColor);
        }

        if (g_cfg.wh_flags & WH_TEXT)
        {
            char values[48]{};
            const int health = (int)std::lround(std::clamp(ped->m_fHealth, 0.0f, kMaximumHealth));
            const int armor = (int)std::lround(std::clamp(ped->m_fArmour, 0.0f, kMaximumArmor));
            snprintf(values, sizeof(values), "HP: %d  Armor: %d", health, armor);
            const float y = boxMin.y - ImGui::GetTextLineHeight() - 4.0f * scale;
            DrawCenteredText(draw, centerX, y, textColor, values);
        }

        if (g_cfg.wh_flags & WH_DIST)
        {
            char value[24]{};
            snprintf(value, sizeof(value), "%d m", (int)std::lround(distance));
            DrawCenteredText(draw, centerX, boxMax.y + 3.0f * scale, textColor, value);
        }

        if (g_cfg.wh_flags & WH_SKELETON)
        {
            static constexpr int segments[][2] = {
                { BONE_PELVIS, BONE_SPINE1 }, { BONE_SPINE1, BONE_UPPERTORSO },
                { BONE_UPPERTORSO, BONE_NECK }, { BONE_NECK, BONE_HEAD },
                { BONE_UPPERTORSO, BONE_RIGHTSHOULDER },
                { BONE_RIGHTSHOULDER, BONE_RIGHTELBOW },
                { BONE_RIGHTELBOW, BONE_RIGHTWRIST },
                { BONE_UPPERTORSO, BONE_LEFTSHOULDER },
                { BONE_LEFTSHOULDER, BONE_LEFTELBOW },
                { BONE_LEFTELBOW, BONE_LEFTWRIST },
                { BONE_PELVIS, BONE_RIGHTHIP }, { BONE_RIGHTHIP, BONE_RIGHTKNEE },
                { BONE_RIGHTKNEE, BONE_RIGHTANKLE },
                { BONE_PELVIS, BONE_LEFTHIP }, { BONE_LEFTHIP, BONE_LEFTKNEE },
                { BONE_LEFTKNEE, BONE_LEFTANKLE },
            };

            for (const auto& segment : segments)
            {
                RwV3d firstWorld{};
                RwV3d secondWorld{};
                ped->GetBonePosition(firstWorld, (unsigned int)segment[0], true);
                ped->GetBonePosition(secondWorld, (unsigned int)segment[1], true);

                ImVec2 firstScreen{};
                ImVec2 secondScreen{};
                if (WorldToScreen(firstWorld, firstScreen)
                    && WorldToScreen(secondWorld, secondScreen))
                {
                    DrawOutlinedLine(draw, firstScreen, secondScreen, skeletonColor, 1.0f);
                }
            }
        }
    }
}
