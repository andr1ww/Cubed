#pragma once
#include "pch.h"

#include "Engine/Source/Runtime/CoreUObject/Public/UObject/Stack.h"

namespace FortPlayerControllerAthena
{
    DefineOriginal(void, ServerAttemptAircraftJump, UFortControllerComponent_Aircraft* Comp, FRotator ClientRot);
    void ServerPlayEmoteItem(AFortPlayerControllerAthena* Controller, UFortMontageItemDefinitionBase* EmoteAsset, float EmoteRandomNumber);
    DefineOriginal(void, ClientOnPawnDied, AFortPlayerControllerAthena* Controller, FFortPlayerDeathReport& DeathReport);
    void ServerLoadPlotForPortal(AFortPlayerControllerAthena* Controller, FFrame& Stack);
    
    void ServerCheat(AFortPlayerControllerAthena* PC, FString FCommand);

    void Setup();
}
