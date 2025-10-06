#pragma once
#include "pch.h"

namespace FortGameModeAthena
{
    DefineOriginal(bool, ReadyToStartMatch, AFortGameModeAthena* GameMode);
    DefineOriginal(APawn*, SpawnDefaultPawnFor, AFortGameModeAthena* GameMode, AFortPlayerControllerAthena* NewPlayer, AActor* StartSpot);
    DefineOriginal(void, HandleStartingNewPlayer, AFortGameModeAthena* GameMode, AFortPlayerControllerAthena* NewPlayer);
    DefineOriginal(void, StartNewSafeZonePhase, AFortGameModeAthena* GameMode, int NewSafeZonePhase);
    DefineOriginal(void, StartAircraftPhase, AFortGameModeAthena* GameMode, char a2);

    void Setup();
}