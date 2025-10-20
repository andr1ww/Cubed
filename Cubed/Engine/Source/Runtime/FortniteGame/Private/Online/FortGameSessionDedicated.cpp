#include "pch.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Online/FortGameSessionDedicated.h"

#include "Globals.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"

bool FortGameSessionDedicated::HandleMatchAssignmentV2(__int64 a1, FMatchmakingDedicatedServerMatchAssignment* MatchAssignment)
{
    auto GameInstance = Cast<UFortGameInstance>(UWorld::GetWorld()->OwningGameInstance);
    auto GameMode = Cast<AFortGameModeAthena>(UWorld::GetWorld()->AuthorityGameMode);
    auto GameState = Cast<AFortGameStateAthena>(GameMode->GameState);
    
    UFortPlaylistAthena* Playlist = nullptr;
    
    for (auto& PlaylistAthena : GameInstance->PlaylistManager->PreloadedPlaylists)
    {
        if (PlaylistAthena->PlaylistName == MatchAssignment->PlaylistName)
            Playlist = PlaylistAthena;
    }

    if (MatchAssignment->PlaylistName.ToString().contains("PlaygroundV2"))
        bCreative = true;
    
    if (!Playlist) return false;

    if (!bCreative)
    {
        GameMode->AIDirector = GetWorld()->SpawnActor<AAthenaAIDirector>(AAthenaAIDirector::StaticClass(), { 0, 0, -99999 }, {});
        if (!GameMode->AIDirector) return false;
        GameMode->AIDirector->Activate();
        if (!GameMode->AIGoalManager)
            GameMode->AIGoalManager = GetWorld()->SpawnActor<AFortAIGoalManager>(AFortAIGoalManager::StaticClass(), { 0, 0, -99999 }, {});
        
        GameMode->SpawningPolicyManager = GetWorld()->SpawnActor<AFortAthenaSpawningPolicyManager>(AFortAthenaSpawningPolicyManager::StaticClass(), { 0, 0, -99999 }, {});
        GameMode->SpawningPolicyManager->GameModeAthena = GameMode;
        GameMode->SpawningPolicyManager->GameStateAthena = GameState;
    }
        
    Playlist->GarbageCollectionFrequency = 99999999999999.f;
    GameState->CurrentPlaylistInfo.BasePlaylist = Playlist;
    GameState->CurrentPlaylistInfo.OverridePlaylist = Playlist;
    GameState->CurrentPlaylistInfo.PlaylistReplicationKey++;
    GameState->CurrentPlaylistInfo.MarkArrayDirty();

    GameMode->bDisableGCOnServerDuringMatch = true;
    GameMode->bPlaylistHotfixChangedGCDisabling = true;

    GameState->DefaultParachuteDeployTraceForGroundDistance = Playlist->PlaylistName.ToString().contains("BlueCheese") ? 50 : 10000; 
    GameMode->CurrentPlaylistId = GameState->CurrentPlaylistId = Playlist->PlaylistId;
    GameMode->CurrentPlaylistName = Playlist->PlaylistName;
    GameMode->bAllowSpectateAfterDeath = true;
    GameMode->WarmupRequiredPlayerCount = 1;
    GameMode->GameSession->MaxPlayers = Playlist->MaxPlayers;

    GameState->AirCraftBehavior = Playlist->AirCraftBehavior;
    GameState->CachedSafeZoneStartUp = Playlist->SafeZoneStartUp;

    GameState->OnRep_CurrentPlaylistId();
    GameState->OnRep_CurrentPlaylistInfo();

    UAthenaAISettings* AISettingsInstance = nullptr;
    for (int i = 0; i < UObject::GObjects->Num(); i++)
    {
        auto Object = UObject::GObjects->GetByIndex(i);
        if (Object && Object->Class == UAthenaAISettings::StaticClass())
        {
            AISettingsInstance = (UAthenaAISettings*)Object;
            break;
        }
    }

    if (AISettingsInstance) {
        GameMode->AISettings = AISettingsInstance;
    
        GameMode->AISettings->AIServices.Free();
        GameMode->AISettings->AIServices.Add(StaticLoadObject<UClass>("/Game/Athena/AI/Phoebe/AIServices/BP_Phoebe_AIService_Loot.BP_Phoebe_AIService_Loot_C"));
        GameMode->AISettings->AIServices.Add(UAthenaAIServicePlayerBots::StaticClass());
        GameMode->AISettings->AIServices.Add(UAthenaAIServiceVehicle::StaticClass());
    
        GameMode->AISettings->bAllowAIGoalManager = true;
        GameMode->AISettings->MaxFootstepHearingRange = 3000.0f;
        GameMode->AISettings->ReducedDeAggroRange = 3500.0f;
    } else {
        GameMode->AISettings = Playlist->AISettings;
    }
        
    for (auto& AdditionalLevel : Playlist->AdditionalLevels)
    {
        bool Success = false;

        ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(UWorld::GetWorld(), AdditionalLevel, FVector(), FRotator(), &Success, FString());
        FAdditionalLevelStreamed Level{};
        Level.bIsServerOnly = false;
        Level.LevelName = AdditionalLevel.ObjectID.AssetPathName;
        if (Success) GameState->AdditionalPlaylistLevelsStreamed.Add(Level);
    }

    for (auto& AdditionalLevel : Playlist->AdditionalLevelsServerOnly)
    {
        bool Success = false;

        ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(UWorld::GetWorld(), AdditionalLevel, FVector(), FRotator(), &Success, FString());
        FAdditionalLevelStreamed Level{};
        Level.bIsServerOnly = false;
        Level.LevelName = AdditionalLevel.ObjectID.AssetPathName;
        if (Success) GameState->AdditionalPlaylistLevelsStreamed.Add(Level);
    }

    GameState->OnFinishedShowingAdditionalPlaylistLevel();
    GameState->OnRep_AdditionalPlaylistLevelsStreamed();
    
    return HandleMatchAssignmentV2OG(a1, MatchAssignment);
}

void FortGameSessionDedicated::Setup()
{
    UHook* Hook = new UHook();

    Hook->Address = ImageBase + 0x51655C4;
    Hook->Detour = HandleMatchAssignmentV2;
    Hook->Original = (void**)&HandleMatchAssignmentV2OG;
    UKismetHookingLibrary::Hook(Hook, Address);
}
