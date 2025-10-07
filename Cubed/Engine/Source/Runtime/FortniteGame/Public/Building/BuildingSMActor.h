#pragma once
#include "pch.h"

namespace BuildingSMActor
{
    bool AttemptSpawnResources(ABuildingSMActor* BuildingSMActor, AFortPlayerPawn* Pawn, float DamageDealt, bool WeakSpot);
    
    void Setup();
}