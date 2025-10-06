#include "pch.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Athena/Player/FortPlayerControllerAthena.h"

#include "Offsets.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"

void FortPlayerControllerAthena::ServerAttemptAircraftJump(UFortControllerComponent_Aircraft* Comp, FRotator ClientRot)
{
    auto Controller = (AFortPlayerControllerAthena*)Comp->GetOwner();
    auto GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;

    GameMode->RestartPlayer(Controller);
    if (Controller->MyFortPawn)
    {
        Controller->ClientSetRotation(ClientRot, false);
        Controller->MyFortPawn->BeginSkydiving(true);
    }
}

void FortPlayerControllerAthena::Setup()
{
    UHook* Hook = new UHook();

    Hook->Address = Runtime::Vfts::ServerAttemptAircraftJump;
    Hook->Class = UFortControllerComponent_Aircraft::StaticClass();
    Hook->Detour = ServerAttemptAircraftJump;
    UKismetHookingLibrary::Hook(Hook, EHook::VFT);
}