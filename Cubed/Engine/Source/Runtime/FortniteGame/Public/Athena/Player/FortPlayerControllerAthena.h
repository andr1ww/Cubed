#pragma once
#include "pch.h"

namespace FortPlayerControllerAthena
{
    DefineOriginal(void, ServerAttemptAircraftJump, UFortControllerComponent_Aircraft* Comp, FRotator ClientRot);
    void ServerPlayEmoteItem(AFortPlayerControllerAthena* Controller, UFortMontageItemDefinitionBase* EmoteAsset, float EmoteRandomNumber);
    DefineOriginal(void, ClientOnPawnDied, AFortPlayerControllerAthena* Controller, FFortPlayerDeathReport& DeathReport);
    
    void Setup();
}