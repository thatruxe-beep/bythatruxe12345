#include "Game/Features.h"

#include "Game/Rage/TpMarker.hpp"

#include <algorithm>

namespace
{
    // SA 1.0 US: CMenuManager::m_nTargetBlipIndex — хендл метки, поставленной
    // игроком на карте (M). Младшее слово — индекс в CRadar::ms_RadarTrace.
    constexpr unsigned int kTargetBlipIndex = 0x00BA6774;
    constexpr int kRadarTraceCount = 175;

    bool FindWaypoint(CVector& out)
    {
        // Формат хендла блипа (CRadar::GetNewUniqueBlipIndex, gta_sa 1.0 US):
        //   handle = index | (counter << 16)
        // CRadar::GetActualBlipArrayIndex требует совпадения counter с
        // m_nCounter трейса. Без этой проверки устаревший хендл указывает на
        // чужой блип, занявший слот (например, серверный блип автосалона) —
        // из-за этого телепорт уводил не на метку.
        const unsigned int handle = *reinterpret_cast<unsigned int*>(kTargetBlipIndex);
        const unsigned int index = handle & 0xFFFFu;
        const unsigned int counter = (handle >> 16) & 0xFFFFu;

        if (index != 0 && index < kRadarTraceCount)
        {
            tRadarTrace& blip = CRadar::ms_RadarTrace[index];

            if (blip.m_bInUse
                && blip.m_nCounter == counter
                && blip.m_nRadarSprite == RADAR_SPRITE_WAYPOINT)
            {
                out = blip.m_vecPos;
                return true;
            }
        }

        // Запасной обход: метка с карты всегда имеет спрайт waypoint.
        // Серверные блипы создаются при старте ресурсов (низкие индексы),
        // метка игрока — позже, поэтому берём последнее совпадение.
        for (int i = kRadarTraceCount - 1; i >= 0; --i)
        {
            tRadarTrace& blip = CRadar::ms_RadarTrace[i];

            if (blip.m_bInUse && blip.m_nRadarSprite == RADAR_SPRITE_WAYPOINT)
            {
                out = blip.m_vecPos;
                return true;
            }
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

    void Teleport(CPed* ped, const CVector& pos)
    {
        if (CVehicle* vehicle = ped->m_pVehicle)
        {
            vehicle->SetPosn(pos.x, pos.y, pos.z);
            vehicle->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
            vehicle->m_vecTurnSpeed = CVector(0.0f, 0.0f, 0.0f);
        }
        else
        {
            ped->SetPosn(pos.x, pos.y, pos.z);
            ped->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
        }
    }
}

void TpMarker::Update()
{
    // Отложенная посадка: метка далеко, коллизия ещё не стримнулась — висим
    // высоко, пока под игроком не появится земля, и опускаем на неё.
    static bool pending = false;
    static CVector pendingPos{};
    static unsigned int pendingUntil = 0;

    if (pending)
    {
        CPed* ped = FindPlayerPed();

        if (!ped)
        {
            pending = false;
        }
        else
        {
            CVector landing{};

            if (ResolveLanding(pendingPos.x, pendingPos.y, landing))
            {
                Teleport(ped, landing);
                pending = false;
            }
            else if (CTimer::m_snTimeInMilliseconds > pendingUntil)
            {
                pending = false;
            }
        }
    }

    static bool wasOn = false;
    const bool on = g_cfg.tpmarker;

    if (on && !wasOn)
    {
        // Функция разовая: срабатывает по нажатию и сразу выключается.
        g_cfg.tpmarker = false;

        CPed* ped = FindPlayerPed();
        CVector marker{};

        if (ped && FindWaypoint(marker) && ValidWaypointPos(marker))
        {
            CVector landing{};

            if (ResolveLanding(marker.x, marker.y, landing))
            {
                Teleport(ped, landing);
            }
            else
            {
                Teleport(ped, CVector(marker.x, marker.y, 300.0f));
                pending = true;
                pendingPos = marker;
                pendingUntil = CTimer::m_snTimeInMilliseconds + 2500u;
            }
        }
    }

    wasOn = on;
}
