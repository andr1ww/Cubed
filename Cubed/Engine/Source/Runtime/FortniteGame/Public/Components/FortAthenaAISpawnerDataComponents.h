#pragma once
#include "pch.h"

namespace FortAthenaAISpawnerDataComponents
{
    DefineOriginal(void, OnPawnAISpawned,
    AFortAthenaAIBotController* Controller,
        AFortPlayerPawn *Pawn);

    DefineOriginal(void, OnSpawned_InventoryBase,
        UFortAthenaAISpawnerDataComponent_AIBotCosmeticBase* Base,
        AFortPlayerPawn *Pawn);

    void Setup();
}