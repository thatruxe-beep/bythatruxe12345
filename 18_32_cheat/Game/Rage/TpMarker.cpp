#include "Game/Features.h"

#include "Game/Rage/TpMarker.hpp"

#include "CGame.h"
#include "CStreaming.h"

namespace
{
    // SA 1.0 US: CMenuManager::m_nTargetBlipIndex — хендл метки, поставленной
    // игроком на карте (M). Младшее слово — индекс в CRadar::ms_RadarTrace.
    constexpr unsigned int kTargetBlipIndex = 0x00BA6774;
    constexpr int kRadarTraceCount = 175;
    constexpr int kAreaNormalWorld = 0;     // eAreaCodes::AREA_CODE_NORMAL_WORLD
    constexpr float kHoverHeight = 300.0f;  // высота ожидания, пока грузится коллизия
    constexpr unsigned int kLandingTimeoutMs = 6000u;

    struct PendingTeleport
    {
        bool active = false;
        CVector target{};  // XY метки
        CVector origin{};  // где игрок был до телепорта
        unsigned int untilMs = 0;
    };

    PendingTeleport s_pending;

    bool FindWaypoint(CVector& out)
    {
        // Метку ставит родной фронтенд SA (карта в паузе): он вызывает
        // CRadar::SetCoordBlip, затем SetBlipSprite(RADAR_SPRITE_WAYPOINT),
        // и записывает хендл в CMenuManager::m_nTargetBlipIndex (0xBA6774).
        // Формат хендла (GetNewUniqueBlipIndex):
        //   handle = index | (counter << 16)
        // Валидация повторяет каноничную из plugin-sdk GPS / SAMP-GPS:
        // счётчик хендла обязан совпадать с m_nCounter трейса, а блип должен
        // отображаться. Никакого перебора всех трейсов: сервер может создать
        // свой блип со спрайтом waypoint (так было с автосалоном), и обход
        // телепортировал игрока на него.
        const unsigned int handle = *reinterpret_cast<unsigned int*>(kTargetBlipIndex);
        const unsigned int index = handle & 0xFFFFu;
        const unsigned int counter = (handle >> 16) & 0xFFFFu;

        if (index == 0 || index >= kRadarTraceCount)
        {
            return false;
        }

        tRadarTrace& blip = CRadar::ms_RadarTrace[index];

        if (blip.m_bInUse
            && blip.m_nCounter == counter
            && blip.m_nBlipDisplay != 0
            && blip.m_nRadarSprite == RADAR_SPRITE_WAYPOINT)
        {
            out = blip.m_vecPos;
            return true;
        }

        return false;
    }

    bool ValidWaypointPos(const CVector& pos)
    {
        return pos.x > -4000.0f && pos.x < 4000.0f
            && pos.y > -4000.0f && pos.y < 4000.0f
            && pos.z > -200.0f && pos.z < 600.0f;
    }

    bool ResolveLanding(float x, float y, CVector& out)
    {
        bool found = false;
        CEntity* groundEntity = nullptr;
        const float groundZ = CWorld::FindGroundZFor3DCoord(x, y, 1000.0f, &found, &groundEntity);

        if (found)
        {
            out = CVector(x, y, groundZ + 1.0f);
            return true;
        }

        float waterZ = 0.0f;
        if (CWaterLevel::GetWaterLevelNoWaves(x, y, 1000.0f, &waterZ))
        {
            out = CVector(x, y, waterZ + 1.0f);
            return true;
        }

        return false;
    }

    // Переносим машину, если игрок за рулём/в салоне, иначе самого игрока.
    CPhysical* TeleportTarget(CPed* ped)
    {
        if (CVehicle* vehicle = ped->m_pVehicle)
        {
            return vehicle;
        }

        return ped;
    }

    // Перенос как у самой игры (CPed::Teleport / CAutomobile::Teleport):
    // сущность выводится из секторов мира, перемещается и возвращается.
    // Одного SetPosn недостаточно: сектора остаются старыми, и физика
    // на новом месте работает некорректно.
    void MoveTo(CPed* ped, const CVector& pos)
    {
        CPhysical* target = TeleportTarget(ped);

        CWorld::Remove(target);
        target->SetPosn(pos.x, pos.y, pos.z);
        CWorld::Add(target);

        target->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
        target->m_vecTurnSpeed = CVector(0.0f, 0.0f, 0.0f);
    }

    // Удержание на высоте, пока ждём землю: без смены секторов, только
    // позиция и нулевая скорость, иначе гравитация тянет игрока вниз.
    void HoldAt(CPed* ped, const CVector& pos)
    {
        CPhysical* target = TeleportTarget(ped);

        target->SetPosn(pos.x, pos.y, pos.z);
        target->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
        target->m_vecTurnSpeed = CVector(0.0f, 0.0f, 0.0f);
    }

    // Если игрок был внутри интерьера, метка находится снаружи: выходим
    // из зоны интерьера, иначе стриминг и коллизия останутся внутренними.
    void LeaveInteriorArea(CPed* ped)
    {
        if (ped->m_nAreaCode == static_cast<unsigned char>(kAreaNormalWorld))
        {
            return;
        }

        ped->m_nAreaCode = static_cast<unsigned char>(kAreaNormalWorld);
        if (CVehicle* vehicle = ped->m_pVehicle)
        {
            vehicle->m_nAreaCode = static_cast<unsigned char>(kAreaNormalWorld);
        }

        CGame::currArea = kAreaNormalWorld;
        CStreaming::RemoveBuildingsNotInArea(kAreaNormalWorld);
    }

    void StartTeleport(CPed* ped, const CVector& marker)
    {
        if (!s_pending.active)
        {
            s_pending.origin = TeleportTarget(ped)->GetPosition();
        }

        // Сначала подгружаем мир под точкой назначения, как делает сама игра
        // при пропуске (CGameLogic, SKIP_IN_PROGRESS). Без коллизии поиск
        // земли не находит поверхность, и игрок оказывается в воздухе.
        const CVector destination(marker.x, marker.y, 0.0f);
        CStreaming::LoadSceneCollision(&destination);
        CStreaming::LoadScene(&destination);
        LeaveInteriorArea(ped);

        CVector landing{};
        if (ResolveLanding(marker.x, marker.y, landing))
        {
            MoveTo(ped, landing);
            s_pending.active = false;
            return;
        }

        // Земля ещё не найдена: держим игрока на высоте и ждём посадку.
        s_pending.active = true;
        s_pending.target = CVector(marker.x, marker.y, 0.0f);
        s_pending.untilMs = CTimer::m_snTimeInMilliseconds + kLandingTimeoutMs;
        HoldAt(ped, CVector(marker.x, marker.y, kHoverHeight));
    }

    void UpdatePending()
    {
        if (!s_pending.active)
        {
            return;
        }

        CPed* ped = FindPlayerPed();
        if (!ped)
        {
            s_pending.active = false;
            return;
        }

        CVector landing{};
        if (ResolveLanding(s_pending.target.x, s_pending.target.y, landing))
        {
            MoveTo(ped, landing);
            s_pending.active = false;
        }
        else if (CTimer::m_snTimeInMilliseconds >= s_pending.untilMs)
        {
            // Земля так и не появилась: возвращаем на место старта, а не
            // бросаем игрока с высоты.
            MoveTo(ped, s_pending.origin);
            s_pending.active = false;
        }
        else
        {
            HoldAt(ped, CVector(s_pending.target.x, s_pending.target.y, kHoverHeight));
        }
    }
}

void TpMarker::Update()
{
    UpdatePending();

    // Функция разовая: срабатывает по нажатию и сразу выключается.
    static bool wasOn = false;
    const bool on = g_cfg.tpmarker;

    if (on && !wasOn)
    {
        g_cfg.tpmarker = false;

        CPed* ped = FindPlayerPed();
        CVector marker{};

        if (ped && FindWaypoint(marker) && ValidWaypointPos(marker))
        {
            StartTeleport(ped, marker);
        }
    }

    wasOn = on;
}
