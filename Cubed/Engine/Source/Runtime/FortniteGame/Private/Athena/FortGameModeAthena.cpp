#include "pch.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Athena/FortGameModeAthena.h"

#include "Logging.h"
#include "Offsets.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/CoreUObject/Public/Templates/Casts.h"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"
#include "Engine/Source/Runtime/Engine/Classes/Engine/World.h"

bool FortGameModeAthena::ReadyToStartMatch(AFortGameModeAthena* GameMode)
{
    auto GameState = Cast<AFortGameStateAthena>(GameMode->GameState);
    if (!GameState || !GameState->MapInfo) return false;

    if (GameMode->CurrentPlaylistId == -1)
    {
        auto Playlist = UObject::FindObject<UFortPlaylistAthena>(
            "FortPlaylistAthena Playlist_DefaultSolo.Playlist_DefaultSolo");
        if (!Playlist) return false;

     //   GameMode->SpawningPolicyManager = GetWorld()->SpawnActor<AFortAthenaSpawningPolicyManager>(AFortAthenaSpawningPolicyManager::StaticClass(), { 0, 0, -99999 }, {});
       // GameMode->SpawningPolicyManager->GameModeAthena = GameMode;
       // GameMode->SpawningPolicyManager->GameStateAthena = GameState;

        Playlist->GarbageCollectionFrequency = 99999999999999.f;
        GameState->CurrentPlaylistInfo.BasePlaylist = Playlist;
        GameState->CurrentPlaylistInfo.OverridePlaylist = Playlist;
        GameState->CurrentPlaylistInfo.PlaylistReplicationKey++;
        GameState->CurrentPlaylistInfo.MarkArrayDirty();

        GameMode->bDisableGCOnServerDuringMatch = true;
        GameMode->bPlaylistHotfixChangedGCDisabling = true;

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
    }

    if (!GameMode->bWorldIsReady) GameMode->bWorldIsReady = true;

    if (!GetWorld()->NetDriver)
        World::Listen(GetWorld(), {});
    
    if (GetWorld()->NetDriver)
        SetConsoleTitleA("Cubed | Ready on Port 7777 | Joinable = true");
    
    return GameMode->AlivePlayers.Num() > 0;
}

APawn* FortGameModeAthena::SpawnDefaultPawnFor(AFortGameModeAthena* GameMode, AFortPlayerControllerAthena* NewPlayer, AActor* StartSpot)
{
    FTransform T = StartSpot->GetTransform();
    T.Translation.Z += 250.f;
    
    APawn* Pawn = GameMode->SpawnDefaultPawnAtTransform(NewPlayer, T);

    AFortInventory* Inventory = NewPlayer->WorldInventory;
    for (int i = 0; i < GameMode->StartingItems.Num(); i++)
    {
        auto Item = GameMode->StartingItems[i];
        if (Item.Count <= 0) continue;
        
        Inventory->AddItem(Item.Item, Item.Count, 0);
    }

    Inventory->AddItem(NewPlayer->CosmeticLoadoutPC.Pickaxe->WeaponDefinition, 1, 0);

    static bool bFirst = false;
    if (!bFirst)
    {
        bFirst = true;
        auto GameState = Cast<AFortGameStateAthena>(GameMode->GameState);
        if (UCurveTable* AthenaGameDataTable = GameState->AthenaGameDataTable)
        {
            static FName DefaultSafeZoneDamageName = UKismetStringLibrary::Conv_StringToName(FString(L"Default.SafeZone.Damage"));

            for (const auto& [RowName, RowPtr] : ((UDataTable*)AthenaGameDataTable)->RowMap) 
            {
                if (RowName != DefaultSafeZoneDamageName)
                    continue;

                FSimpleCurve* Row = (FSimpleCurve*)RowPtr;

                if (!Row)
                    continue;

                for (auto& Key : Row->Keys)
                {
                    FSimpleCurveKey* KeyPtr = &Key;

                    if (KeyPtr->Time == 0.f)
                        KeyPtr->Value = 0.f;
                }
            } 
        }
    }
    
    return Pawn;
}

void FortGameModeAthena::HandleStartingNewPlayer(AFortGameModeAthena* GameMode, AFortPlayerControllerAthena* NewPlayer) {
    auto PlayerState = Cast<AFortPlayerStateAthena>(NewPlayer->PlayerState);
    auto GameState = Cast<AFortGameStateAthena>(GameMode->GameState);

    FGameMemberInfo Member;
    Member.MostRecentArrayReplicationKey = -1;
    Member.ReplicationID = -1;
    Member.ReplicationKey = -1;
    Member.TeamIndex = PlayerState->TeamIndex; 
    Member.SquadId = PlayerState->SquadId;
    Member.MemberUniqueId = PlayerState->UniqueId;

    GameState->GameMemberInfoArray.Members.Add(Member);
    GameState->GameMemberInfoArray.MarkItemDirty(Member);

    PlayerState->SeasonLevelUIDisplay = NewPlayer->XPComponent->CurrentLevel;
    PlayerState->OnRep_SeasonLevelUIDisplay();
}

void FortGameModeAthena::StartNewSafeZonePhase(AFortGameModeAthena* GameMode, int NewSafeZonePhase)
{
    auto GameState = (AFortGameStateAthena*)GameMode->GameState;
    if (!GameState) return;

    FFortSafeZoneDefinition* SafeZoneDefinition = &GameState->MapInfo->SafeZoneDefinition;
    TArray<float>& Durations = *(TArray<float>*)(__int64(SafeZoneDefinition) + 0x248);
    TArray<float>& HoldDurations = *(TArray<float>*)(__int64(SafeZoneDefinition) + 0x238);

    int SafeZonePhase = GameMode->SafeZonePhase + 1;

    float ZoneDuration = 0.f;
    float ZoneHoldDuration = 0.f;

    if (SafeZonePhase >= 0 && SafeZonePhase < Durations.Num())
        ZoneDuration = Durations[SafeZonePhase];
    if (SafeZonePhase >= 0 && SafeZonePhase < HoldDurations.Num())
        ZoneHoldDuration = HoldDurations[SafeZonePhase];

    float CurrentTime = UGameplayStatics::GetTimeSeconds(GetWorld());
    GameMode->SafeZoneIndicator->SafeZoneStartShrinkTime  = CurrentTime + ZoneHoldDuration;
    GameMode->SafeZoneIndicator->SafeZoneFinishShrinkTime = GameMode->SafeZoneIndicator->SafeZoneStartShrinkTime + ZoneDuration;

    StartNewSafeZonePhaseOG(GameMode, NewSafeZonePhase);
}

void FortGameModeAthena::Setup()
{
    UHook* Hook = new UHook();

    Hook->Address = Runtime::Vfts::ReadyToStartMatch;
    Hook->Class = AFortGameModeAthena::StaticClass();
    Hook->Detour = ReadyToStartMatch;
    UKismetHookingLibrary::Hook(Hook, EHook::VFT);

    Hook->Address = Runtime::Vfts::SpawnDefaultPawnFor;
    Hook->Class = AGameModeBase::StaticClass();
    Hook->Detour = SpawnDefaultPawnFor;
    UKismetHookingLibrary::Hook(Hook, EHook::EveryVFT);

    Hook->Address = Runtime::Vfts::HandleStartingNewPlayer;
    Hook->Class = AGameModeBase::StaticClass();
    Hook->Detour = HandleStartingNewPlayer;
    UKismetHookingLibrary::Hook(Hook, EHook::EveryVFT);

    Hook->Address = ImageBase + 0x4a90298;
    Hook->Original = (void**)&StartNewSafeZonePhaseOG;
    Hook->Detour = StartNewSafeZonePhase;
    UKismetHookingLibrary::Hook(Hook, EHook::Address);
}
