#include "Game/Features.h"

#include "Game/Misc/Ram.hpp"

#include <algorithm>
#include <cmath>

namespace
{
    // Гравитация движка за кадр² (GAME_GRAVITY).
    constexpr float kGravity = 0.008f;
    // Небольшая поправка на сопротивление воздуха: подброс чуть выше расчётного.
    constexpr float kLaunchBoost = 1.1f;
    // Минимальная скорость машины (ед./кадр, ~30 fps), при которой таран ловит педа.
    constexpr float kMinRamSpeed = 0.15f;
    // Сколько пешеход едет у бампера, прежде чем его подбросить.
    constexpr unsigned int kGripMs = 220u;
    // Зона перед машиной (метры от центра машины), в которой ловим пешехода.
    constexpr float kGripNearDistance = 1.5f;
    constexpr float kGripFarDistance = 3.6f;
    constexpr float kGripHalfWidth = 1.1f;
    constexpr float kGripMinHeight = -1.8f;
    constexpr float kGripMaxHeight = 1.2f;

    struct Grip
    {
        CPed* ped = nullptr;
        int ref = 0;
        CVehicle* vehicle = nullptr;
        float distance = 0.0f; // вперёд от центра машины
        float height = 0.0f;   // высота корня педа, держим постоянной
        unsigned int startMs = 0;
    };

    Grip s_grip;

    void ReleaseGrip()
    {
        s_grip = Grip{};
    }

    // Пешеход захватывается, когда машина едет вперёд и он стоит перед бампером.
    // Захваченный пешеход едет вместе с машиной, затем подбрасывается вверх.
    // Локальный игрок и другие игроки исключены: только NPC.
    void UpdateGrip(CPed* local, CVehicle* vehicle, float fx, float fy)
    {
        const CVector center = vehicle->GetPosition();
        const CVector velocity = vehicle->m_vecMoveSpeed;
        const float speedForward = velocity.x * fx + velocity.y * fy;
        const unsigned int now = CTimer::m_snTimeInMilliseconds;

        if (s_grip.ped)
        {
            CPed* gripped = CPools::GetPed(s_grip.ref);
            if (!gripped || gripped != s_grip.ped || s_grip.vehicle != vehicle
                || gripped->m_fHealth <= 0.0f)
            {
                ReleaseGrip();
                return;
            }

            // Машина остановилась: пешехода отпускаем без подброса, иначе он
            // взлетел бы вертикально с места.
            if (speedForward < kMinRamSpeed * 0.5f)
            {
                ReleaseGrip();
                return;
            }

            gripped->SetPosn(center.x + fx * s_grip.distance,
                center.y + fy * s_grip.distance, s_grip.height);
            gripped->m_vecMoveSpeed = velocity;
            gripped->m_vecTurnSpeed = CVector(0.0f, 0.0f, 0.0f);

            if (now - s_grip.startMs >= kGripMs)
            {
                // std::max<float>, а не std::max(...): windows.h определяет макрос max.
                const float height = std::max<float>(1.0f, g_cfg.ramlaunchheight);
                const float lift = std::sqrt(2.0f * kGravity * height) * kLaunchBoost;

                gripped->m_vecMoveSpeed = CVector(fx * speedForward, fy * speedForward, lift);
                gripped->bIsStanding = false;
                ReleaseGrip();
            }
            return;
        }

        if (velocity.Magnitude() < kMinRamSpeed || speedForward < kMinRamSpeed * 0.5f)
        {
            return;
        }

        // Правый вектор машины в горизонтали: (fy, -fx).
        const float rx = fy;
        const float ry = -fx;

        CPed* best = nullptr;
        float bestAhead = 0.0f;
        float bestHeight = 0.0f;

        const int poolSize = CPools::ms_pPedPool->m_nSize;
        for (int i = 0; i < poolSize; ++i)
        {
            CPed* ped = CPools::ms_pPedPool->GetAt(i);
            if (!ped || ped == local || ped->m_fHealth <= 0.0f
                || ped->m_pVehicle != nullptr || ped->IsPlayer())
            {
                continue;
            }

            const CVector& position = ped->GetPosition();
            const float dx = position.x - center.x;
            const float dy = position.y - center.y;
            const float ahead = dx * fx + dy * fy;
            const float lateral = dx * rx + dy * ry;
            const float height = position.z - center.z;

            if (ahead < kGripNearDistance || ahead > kGripFarDistance
                || std::fabs(lateral) > kGripHalfWidth
                || height < kGripMinHeight || height > kGripMaxHeight)
            {
                continue;
            }

            if (!best || ahead < bestAhead)
            {
                best = ped;
                bestAhead = ahead;
                bestHeight = position.z;
            }
        }

        if (best)
        {
            s_grip.ped = best;
            s_grip.ref = CPools::GetPedRef(best);
            s_grip.vehicle = vehicle;
            s_grip.distance = bestAhead;
            s_grip.height = bestHeight;
            s_grip.startMs = now;
        }
    }
}

void Ram::Update()
{
    static bool wasActive = false;
    static bool hasOrigin = false;
    static CVehicle* savedVehicle = nullptr;
    static CVector origin{};

    CPed* ped = FindPlayerPed();
    CVehicle* vehicle = ped ? ped->m_pVehicle : nullptr;
    const bool active = g_cfg.ram && vehicle && vehicle->m_pDriver == ped;

    if (active && (!wasActive || !hasOrigin || savedVehicle != vehicle))
    {
        savedVehicle = vehicle;
        origin = vehicle->GetPosition();
        hasOrigin = true;
    }

    bool gripAllowed = false;
    if (active && hasOrigin)
    {
        CMatrix& matrix = vehicle->GetMatrix();
        CVector forward = matrix.GetForward();
        const float horizontal = sqrtf(forward.x * forward.x + forward.y * forward.y);
        if (horizontal > 0.001f)
        {
            const float fx = forward.x / horizontal;
            const float fy = forward.y / horizontal;
            const float acceleration = std::clamp(g_cfg.rampower, 1.0f, 20.0f) * 0.02f;
            vehicle->m_vecMoveSpeed.x += fx * acceleration;
            vehicle->m_vecMoveSpeed.y += fy * acceleration;

            if (g_cfg.ramlaunch)
            {
                gripAllowed = true;
                UpdateGrip(ped, vehicle, fx, fy);
            }
        }
    }

    if (!gripAllowed)
    {
        ReleaseGrip();
    }

    if (!active && wasActive && hasOrigin)
    {
        // Only touch the saved pointer when it is still the player's current
        // vehicle; this avoids dereferencing a destroyed/streamed-out vehicle.
        if (vehicle && vehicle == savedVehicle)
        {
            vehicle->SetPosition(origin.x, origin.y, origin.z);
            vehicle->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
            vehicle->m_vecTurnSpeed = CVector(0.0f, 0.0f, 0.0f);
        }
        hasOrigin = false;
        savedVehicle = nullptr;
    }

    wasActive = active;
}
