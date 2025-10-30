#pragma once
#include "pch.h"

namespace FortAthenaAISpawnerDataComponents
{
    DefineOriginal(void, OnPawnAISpawned,
    AFortAthenaAIBotController* Controller,
        AFortPlayerPawn *Pawn);

    DefineOriginal(void, OnSpawned_InventoryBase,
        UFortAthenaAISpawnerDataComponent_AIBotInventory* Base,
        AFortPlayerPawn *Pawn);

    DefineOriginal(void, OnSpawned_CosmeticLoadout,
    UFortAthenaAISpawnerDataComponent_CosmeticLoadout* Base,
    AFortPlayerPawn *Pawn);
    
    DefineOriginal(void, OnSpawned_Conversation, UFortAthenaAISpawnerDataComponent_AIBotConversation*, AFortAIPawn*);

    
    void Setup();
}