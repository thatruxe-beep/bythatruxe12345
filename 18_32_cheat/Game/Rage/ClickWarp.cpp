#include "Game/Features.h"

#include "Game/Rage/ClickWarp.hpp"

#include "Hooks/core/ForceCursorVisible.hpp"

#include <d3d9.h>

#include "imgui.h"

#include <cmath>
#include <utility>

namespace
{
    constexpr float kReach = 300.0f;    // дальность луча прицела
    constexpr float kSkyProbe = 150.0f; // шаг вперёд при прицеле в небо

    struct Mat4
    {
        float m[4][4];
    };

    Mat4 Identity()
    {
        Mat4 r{};
        for (int i = 0; i < 4; ++i)
        {
            r.m[i][i] = 1.0f;
        }
        return r;
    }

    Mat4 FromD3D(const D3DMATRIX& t)
    {
        Mat4 r;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                r.m[i][j] = t.m[i][j];
            }
        }
        return r;
    }

    Mat4 Multiply(const Mat4& a, const Mat4& b)
    {
        Mat4 r{};
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k)
                {
                    sum += a.m[i][k] * b.m[k][j];
                }
                r.m[i][j] = sum;
            }
        }
        return r;
    }

    // Обращение 4x4 методом Гаусса-Жордана; при вырожденной матрице — единичная.
    Mat4 Invert(const Mat4& src)
    {
        float a[4][8];
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                a[i][j] = src.m[i][j];
                a[i][4 + j] = (i == j) ? 1.0f : 0.0f;
            }
        }

        for (int col = 0; col < 4; ++col)
        {
            int pivot = col;
            for (int row = col + 1; row < 4; ++row)
            {
                if (std::fabs(a[row][col]) > std::fabs(a[pivot][col]))
                {
                    pivot = row;
                }
            }
            if (std::fabs(a[pivot][col]) < 1e-9f)
            {
                return Identity();
            }
            if (pivot != col)
            {
                for (int j = 0; j < 8; ++j)
                {
                    std::swap(a[col][j], a[pivot][j]);
                }
            }

            const float div = a[col][col];
            for (int j = 0; j < 8; ++j)
            {
                a[col][j] /= div;
            }

            for (int row = 0; row < 4; ++row)
            {
                if (row == col)
                {
                    continue;
                }
                const float factor = a[row][col];
                if (factor != 0.0f)
                {
                    for (int j = 0; j < 8; ++j)
                    {
                        a[row][j] -= factor * a[col][j];
                    }
                }
            }
        }

        Mat4 r;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                r.m[i][j] = a[i][4 + j];
            }
        }
        return r;
    }

    // Экранная точка -> мировая (D3D: row-vector, world = ndc * inv(view*proj)).
    bool Unproject(const Mat4& invViewProj, const D3DVIEWPORT9& vp,
        float sx, float sy, float sz, CVector& out)
    {
        if (vp.Width == 0.0f || vp.Height == 0.0f)
        {
            return false;
        }

        const float ndcX = 2.0f * (sx - vp.X) / vp.Width - 1.0f;
        const float ndcY = 1.0f - 2.0f * (sy - vp.Y) / vp.Height;

        const float w = ndcX * invViewProj.m[0][3] + ndcY * invViewProj.m[1][3]
            + sz * invViewProj.m[2][3] + invViewProj.m[3][3];
        if (std::fabs(w) < 1e-6f)
        {
            return false;
        }

        const float x = ndcX * invViewProj.m[0][0] + ndcY * invViewProj.m[1][0]
            + sz * invViewProj.m[2][0] + invViewProj.m[3][0];
        const float y = ndcX * invViewProj.m[0][1] + ndcY * invViewProj.m[1][1]
            + sz * invViewProj.m[2][1] + invViewProj.m[3][1];
        const float z = ndcX * invViewProj.m[0][2] + ndcY * invViewProj.m[1][2]
            + sz * invViewProj.m[2][2] + invViewProj.m[3][2];

        out = CVector(x / w, y / w, z / w);
        return true;
    }

    void DrawCrosshair(ImDrawList* draw, const ImVec2& center, float s, ImU32 accent)
    {
        draw->AddCircleFilled(center, 1.7f * s, accent);

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

        const char* hint = "СКМ — телепорт";
        const ImVec2 hintSize = ImGui::CalcTextSize(hint);
        const ImVec2 hintPos(center.x - hintSize.x * 0.5f, center.y + 20.0f * s);
        draw->AddText(ImVec2(hintPos.x + 1.0f, hintPos.y + 1.0f), IM_COL32(0, 0, 0, 200), hint);
        draw->AddText(hintPos, IM_COL32(255, 255, 255, 170), hint);
    }
}

void ClickWarp::Update(IDirect3DDevice9* device)
{
    static bool sCursorFreed = false;

    if (!g_cfg.clickwarp || g_cfg.menu_open || !device)
    {
        if (sCursorFreed && !g_cfg.menu_open)
        {
            if (Cself && callForceCursorVisible)
            {
                callForceCursorVisible(Cself, false, false);
            }
            sCursorFreed = false;
        }
        return;
    }

    CPed* ped = FindPlayerPed();
    if (!ped)
    {
        return;
    }

    // Свободный прицел: показываем курсор и освобождаем управление (как у
    // открытого меню), чтобы прицел ходил по экрану, а камера не крутилась.
    if (Cself && callForceCursorVisible)
    {
        callForceCursorVisible(Cself, true, true);
        sCursorFreed = true;
    }

    // Прицел следует за мышью.
    D3DDEVICE_CREATION_PARAMETERS creation{};
    if (FAILED(device->GetCreationParameters(&creation)))
    {
        return;
    }

    POINT p{};
    GetCursorPos(&p);
    if (creation.hFocusWindow)
    {
        ScreenToClient(creation.hFocusWindow, &p);
    }
    const ImVec2 crosshair(static_cast<float>(p.x), static_cast<float>(p.y));

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const float s = g_cfg.ui_scale / 100.0f;
    const ImU32 accent = static_cast<ImU32>(g_cfg.accent.to_color().as_imcolor());
    DrawCrosshair(draw, crosshair, s, accent);

    if (!(GetAsyncKeyState(VK_MBUTTON) & 1))
    {
        return;
    }

    // Луч из камеры через точку прицела: экран -> мир.
    D3DVIEWPORT9 vp{};
    D3DMATRIX view{};
    D3DMATRIX proj{};
    if (FAILED(device->GetViewport(&vp))
        || FAILED(device->GetTransform(D3DTS_VIEW, &view))
        || FAILED(device->GetTransform(D3DTS_PROJECTION, &proj)))
    {
        return;
    }

    const Mat4 invViewProj = Invert(Multiply(FromD3D(view), FromD3D(proj)));
    CVector nearPoint(0.0f, 0.0f, 0.0f);
    CVector farPoint(0.0f, 0.0f, 0.0f);
    if (!Unproject(invViewProj, vp, crosshair.x, crosshair.y, 0.0f, nearPoint)
        || !Unproject(invViewProj, vp, crosshair.x, crosshair.y, 1.0f, farPoint))
    {
        return;
    }

    CVector direction(
        farPoint.x - nearPoint.x,
        farPoint.y - nearPoint.y,
        farPoint.z - nearPoint.z);
    const float length = VecLength(direction);
    if (length < 0.0001f)
    {
        return;
    }
    direction = CVector(direction.x / length, direction.y / length, direction.z / length);

    const CVector target(
        nearPoint.x + direction.x * kReach,
        nearPoint.y + direction.y * kReach,
        nearPoint.z + direction.z * kReach);

    CColPoint hit;
    CEntity* hitEntity = nullptr;
    CVector destination(0.0f, 0.0f, 0.0f);
    bool haveDestination = false;

    if (CWorld::ProcessLineOfSight(nearPoint, target, hit, hitEntity,
            true, true, false, true, false, true, false, false))
    {
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
        // Прицел в небо: шаг вперёд по лучу и падение на землю.
        const float probeX = nearPoint.x + direction.x * kSkyProbe;
        const float probeY = nearPoint.y + direction.y * kSkyProbe;
        const float probeZ = nearPoint.z + direction.z * kSkyProbe;
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
