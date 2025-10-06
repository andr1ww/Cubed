#pragma once
#include "pch.h"

namespace FortPlayerController
{
    DefineOriginal(void, ServerAcknowledgePossession, AFortPlayerControllerAthena* Controller, APawn* Pawn);
    DefineOriginal(void, ServerExecuteInventoryItem, AFortPlayerController* Controller, FGuid ItemGuid);
    
    void Setup();
}