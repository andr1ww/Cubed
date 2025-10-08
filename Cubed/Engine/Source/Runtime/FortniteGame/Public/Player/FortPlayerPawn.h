#pragma once
#include "pch.h"

namespace FortPlayerPawn
{
    DefineOriginal(void, OnAboutToEnterBackpack, AFortPickup *Pickup);
    DefineOriginal(void, ServerHandlePickupInfo, AFortPlayerPawn* Pawn, AFortPickup* Pickup, FFortPickupRequestInfo& Params)
    
    void Setup();
}