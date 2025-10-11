#include "pch.h"
#include "FortAthenaCreativePortal.h"

#include "Offsets.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"


void FortAthenaCreativePortal::TeleportPlayerToLinkedVolumeHook(UObject* Context, FFrame& Stack, void* Ret)
{
	auto Portal = (AFortAthenaCreativePortal*)Context;
	if (!Portal)
		return;

	AFortPlayerPawnAthena* Pawn = nullptr;
	bool bUseSpawnTags;

	Stack.StepCompiledIn(&Pawn);
	Stack.StepCompiledIn(&bUseSpawnTags);
	Stack.IncrementCode();


	if (!Pawn || !Pawn->Controller || !Portal->LinkedVolume)
		return;



	auto PC = Cast<AFortPlayerControllerAthena>(Pawn->Controller);

	if (!PC)
		return;

	auto LinkedVolume = Portal->GetLinkedVolume();
	if (!LinkedVolume) return;

	auto MG = PC->GetMinigame();
	FVector Location;
	FRotator Rotation;
	bool bGround;


	auto PS = Cast<AFortPlayerStateAthena>(PC->PlayerState);
	if (MG && PS)
	{
		MG->DetermineSpawnLocation(PS, &Location, &Rotation, &bGround);

	}
	else {
		Location = LinkedVolume->K2_GetActorLocation();
		Rotation = LinkedVolume->K2_GetActorRotation();
		Location.Z += 15500;
	}

	PS->RespawnData.RespawnLocation = Location;
	PC->ClientStartRespawnPreparation(Location, Rotation, 0, UKismetStringLibrary::Conv_StringToName(L"CreativePortal"), PC->ClientIslandTravelText);//ProcessEvent(fn1, &f);
	//PC->ClientClearDeathNotification();
	Pawn->K2_TeleportTo(Location, Rotation);
	Portal->NotifyTeleportedPlayerPawn(Pawn, true);
	
}

void FortAthenaCreativePortal::Setup()
{
    UHook* Hook = new UHook();

    Hook->Path = "/Script/FortniteGame.FortAthenaCreativePortal.TeleportPlayerToLinkedVolume";
    
    Hook->Detour = TeleportPlayerToLinkedVolumeHook;
    UKismetHookingLibrary::Hook(Hook, EHook::Exec);
    free(Hook);
}
