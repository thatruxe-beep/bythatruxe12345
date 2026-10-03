#include "Game/Features.h"

#include "Game/Rage/GodMode.hpp"

namespace
{
    struct ProofState
    {
        bool bullet = false;
        bool fire = false;
        bool collision = false;
        bool melee = false;
        bool explosion = false;
        bool invulnerable = false;
    };

    template <typename T>
    ProofState CaptureProofs(T* entity)
    {
        return {
            !!entity->bBulletProof,
            !!entity->bFireProof,
            !!entity->bCollisionProof,
            !!entity->bMeleeProof,
            !!entity->bExplosionProof,
            !!entity->bInvulnerable
        };
    }

    template <typename T>
    void SetProofs(T* entity, const ProofState& state)
    {
        entity->bBulletProof = state.bullet;
        entity->bFireProof = state.fire;
        entity->bCollisionProof = state.collision;
        entity->bMeleeProof = state.melee;
        entity->bExplosionProof = state.explosion;
        entity->bInvulnerable = state.invulnerable;
    }

    template <typename T>
    void EnableProofs(T* entity)
    {
        SetProofs(entity, { true, true, true, true, true, true });
    }
}

void GodMode::Update()
{
    static bool applied = false;
    static CPed* savedPed = nullptr;
    static CVehicle* savedVehicle = nullptr;
    static ProofState pedProofs{};
    static ProofState vehicleProofs{};

    CPed* ped = FindPlayerPed(-1);
    CVehicle* vehicle = FindPlayerVehicle(0, false);

    if (!g_cfg.godmode)
    {
        if (applied)
        {
            // Restore only live/current entities. This avoids writing through a
            // stale pool pointer after respawn or a vehicle despawn.
            if (ped && ped == savedPed) SetProofs(ped, pedProofs);
            if (vehicle && vehicle == savedVehicle) SetProofs(vehicle, vehicleProofs);
        }

        applied = false;
        savedPed = nullptr;
        savedVehicle = nullptr;
        return;
    }

    if (!ped)
    {
        return;
    }

    if (!applied || savedPed != ped)
    {
        savedPed = ped;
        pedProofs = CaptureProofs(ped);
    }
    EnableProofs(ped);

    if (vehicle)
    {
        if (!applied || savedVehicle != vehicle)
        {
            savedVehicle = vehicle;
            vehicleProofs = CaptureProofs(vehicle);
        }
        EnableProofs(vehicle);
    }

    applied = true;
}
