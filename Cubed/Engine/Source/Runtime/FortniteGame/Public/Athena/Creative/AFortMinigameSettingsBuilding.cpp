#include "pch.h"
#include "AFortMinigameSettingsBuilding.h"

#include "Offsets.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"

void FortMinigameSettingsBuilding::BeginPlay(AFortMinigameSettingsBuilding* the)
{
    the->SettingsVolume = (AFortVolume*)the->GetOwner();
    BeginPlayOG(the);
}

void FortMinigameSettingsBuilding::Setup()
{
    UHook* Hook = new UHook();



    Hook->Address = ImageBase + 0x501CA94;
    Hook->Original = (void**)&BeginPlayOG;
    Hook->Detour = BeginPlay;
    UKismetHookingLibrary::Hook(Hook, EHook::Address);
    
    free(Hook);
}
