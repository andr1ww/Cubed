#include "pch.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Athena/AI/Systems/AthenaAIServicePlayerBots.h"

#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"

void AthenaAIServicePlayerBots::InitializeMMRInfos()
{
    UAthenaAIServicePlayerBots* AIServicePlayerBots = UAthenaAIBlueprintLibrary::GetAIServicePlayerBots(GetWorld());
    static UClass* AISpawnerData = StaticLoadObject<UClass>("/Game/Athena/AI/Phoebe/BP_AISpawnerData_Phoebe.BP_AISpawnerData_Phoebe_C");
    
    AIServicePlayerBots->DefaultBotAISpawnerData = AISpawnerData;
    FMMRSpawningInfo NewSpawningInfo{};
    NewSpawningInfo.BotSpawningDataInfoTargetELO = 6700.f;
    NewSpawningInfo.BotSpawningDataInfoWeight = 100.f;
    NewSpawningInfo.AISpawnerData = AISpawnerData;
    NewSpawningInfo.NumBotsToSpawn = 55;

    AIServicePlayerBots->CachedMMRSpawningInfo.SpawningInfos.Add(NewSpawningInfo);
    AIServicePlayerBots->GamePhaseToStartSpawning = EAthenaGamePhase::Warmup;
    AIServicePlayerBots->bWaitForNavmeshToBeLoaded = false;

    static bool bSet = false;
    if (!bSet)
    {
        bSet = true;
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFortPoiVolume::StaticClass(), &AllActors);
        for (auto& Actor : AllActors)
        {
            AFortPoiVolume* POIVolume = static_cast<AFortPoiVolume*>(Actor);
            if (POIVolume)
            {
                FCachedPOIVolumeLocations Ok{};
                Ok.POIVolume = POIVolume;
                AIServicePlayerBots->CachedValidPOIVolumeLocations.Add(Ok);
            }
        }
    }
}

void AthenaAIServicePlayerBots::Setup()
{
    UHook* Hook = new UHook();
    Hook->Swap = ImageBase + 0x5592D18;
    Hook->Address = ImageBase + 0x49D649C;
    Hook->Offset = 3;
    Hook->Detour = InitializeMMRInfos;
    UKismetHookingLibrary::Hook(Hook, EHook::ModifyCustom);

    UKismetHookingLibrary::PatchBytes(ImageBase + 0x49DE47B, { 
        0xE9, 0xA4, 0x00, 0x00, 0x00,  // jmp
        0x90                            // nop
    });
}
