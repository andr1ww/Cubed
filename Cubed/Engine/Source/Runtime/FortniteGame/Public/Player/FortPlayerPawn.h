#pragma once
#include "pch.h"

#include "Engine/Source/Runtime/CoreUObject/Public/UObject/Stack.h"

namespace FortPlayerPawn
{
    DefineOriginal(void, OnAboutToEnterBackpack, AFortPickup *Pickup);
    DefineOriginal(void, ServerHandlePickupInfo, AFortPlayerPawn* Pawn, AFortPickup* Pickup, FFortPickupRequestInfo& Params)
    DefineOriginal(void, MovingEmoteStopped, AFortPawn*, FFrame& Stack);
    DefineOriginal(void, ServerSendZiplineState, AFortPlayerPawn* Pawn, FFrame& Stack);
    DefineOriginal(void, OnRep_IsInAnyStorm, AFortPlayerPawn* Pawn);

    void Setup();
}
