#pragma once
#include "pch.h"

namespace FortGameModeAthena
{
    static inline uint8_t CurrentTeam = 3;
    static inline uint8_t PlayersOnCurTeam = 0;
    
    DefineOriginal(bool, ReadyToStartMatch, AFortGameModeAthena* GameMode);
    DefineOriginal(APawn*, SpawnDefaultPawnFor, AFortGameModeAthena* GameMode, AFortPlayerControllerAthena* NewPlayer, AActor* StartSpot);
    DefineOriginal(void, HandleStartingNewPlayer, AFortGameModeAthena* GameMode, AFortPlayerControllerAthena* NewPlayer);
    DefineOriginal(void, StartNewSafeZonePhase, AFortGameModeAthena* GameMode, int NewSafeZonePhase);
    DefineOriginal(void, StartAircraftPhase, AFortGameModeAthena* GameMode, char a2);
    DefineOriginal(__int64, InitializeFlightPath, __int64 a1, __int64 a2, __int64 a3, unsigned __int64 a4, unsigned __int64 a5, __int64 a6, float a7, float* a8, float* a9, float a10, float a11);
    void Setup();
}