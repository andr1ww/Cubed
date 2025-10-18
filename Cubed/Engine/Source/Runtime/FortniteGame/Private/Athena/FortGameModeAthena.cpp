#include "pch.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Athena/FortGameModeAthena.h"

#include "Globals.h"
#include "Offsets.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"
#include "Engine/Source/Runtime/Engine/Classes/Engine/World.h"

bool FortGameModeAthena::ReadyToStartMatch(AFortGameModeAthena* GameMode)
{
    auto GameState = GetGameState();
    if (!GameState || !GameState->MapInfo) return false;
    
    if (GameMode->CurrentPlaylistId == -1)
    {
        auto Playlist = bCreative 
            ? UObject::FindObject<UFortPlaylistAthena>("FortPlaylistAthena Playlist_PlaygroundV2.Playlist_PlaygroundV2")
            : UObject::FindObject<UFortPlaylistAthena>("FortPlaylistAthena Playlist_DefaultSolo.Playlist_DefaultSolo");
        //  : UObject::FindObject<UFortPlaylistAthena>("FortPlaylistAthena Playlist_ShowdownAlt_BlueCheese_Regular_Solo.Playlist_ShowdownAlt_BlueCheese_Regular_Solo");
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
    }

    if (!GameMode->bWorldIsReady)
        GameMode->bWorldIsReady = true;
    
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
        TArray<AActor*> WarmupActors;
        std::vector<std::string> LootClasses = {
            "/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_Warmup.Tiered_Athena_FloorLoot_Warmup_C",
            "/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_01.Tiered_Athena_FloorLoot_01_C"
        };

        for (const auto& ClassPath : LootClasses)
        {
            UClass* WarmupClass = StaticLoadObject<UClass>(ClassPath);
            UGameplayStatics::GetAllActorsOfClass(UWorld::GetWorld(), WarmupClass, &WarmupActors);

            for (auto& WarmupActor : WarmupActors)
                WarmupActor->K2_DestroyActor();

            WarmupActors.Free(); 
        }
        
        if (bCreative)
        {
            if (UCurveTable* AthenaGameDataTable = Cast<AFortGameStateAthena>(GameMode->GameState)->AthenaGameDataTable)
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
    }
    
    return Pawn;
}

void FortGameModeAthena::HandleStartingNewPlayer(AFortGameModeAthena* GameMode, AFortPlayerControllerAthena* NewPlayer) {
    auto PlayerState = Cast<AFortPlayerStateAthena>(NewPlayer->PlayerState);
    auto GameState = GetGameState();

    PlayerState->SquadId = PlayerState->TeamIndex - 3;
    PlayerState->OnRep_SquadId();
    
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

    if (!NewPlayer->MatchReport)
        NewPlayer->MatchReport = (UAthenaPlayerMatchReport*)UGameplayStatics::SpawnObject(UAthenaPlayerMatchReport::StaticClass(), NewPlayer);
}

void FortGameModeAthena::StartNewSafeZonePhase(AFortGameModeAthena* GameMode, int NewSafeZonePhase)
{
    auto GameState = (AFortGameStateAthena*)GameMode->GameState;
    if (!GameState) return StartNewSafeZonePhaseOG(GameMode, NewSafeZonePhase);

    static auto PlaylistName = GetGameState()->CurrentPlaylistInfo.BasePlaylist->PlaylistName.ToString();
    if (PlaylistName.contains("BlueCheese"))
    {
        static bool First = false;
        if (!First) 
        {
            auto& FlightInfo = GetGameState()->FlightPathMidLine;
            float TimeTilDropStart = FlightInfo.TimeTillDropStart;

            float BackUPBuddy = FlightInfo.FlightSpeed * 7.0f;
        
            FVector DropStartLocation = FlightInfo.FlightStartLocation
                + (RotatorToVector(FlightInfo.FlightStartRotation) * FlightInfo.FlightSpeed * FlightInfo.TimeTillDropStart);

            FVector NewStartLocation = DropStartLocation 
                - (RotatorToVector(FlightInfo.FlightStartRotation) * BackUPBuddy);

            FlightInfo.FlightStartLocation = (FVector_NetQuantize100)NewStartLocation;
            FlightInfo.TimeTillDropStart = 7.0f;
            FlightInfo.TimeTillDropEnd = FlightInfo.TimeTillDropEnd - TimeTilDropStart + 7.0f;
            FlightInfo.TimeTillFlightEnd = FlightInfo.TimeTillFlightEnd - TimeTilDropStart + 15.0f;

            if (FlightInfo.TimeTillDropEnd < 0.0f) FlightInfo.TimeTillDropEnd = 0.0f;
            if (FlightInfo.TimeTillFlightEnd < 0.0f) FlightInfo.TimeTillFlightEnd = 0.0f;

            for (auto Aircraft : GameMode->Aircrafts) 
            {
                Aircraft->FlightInfo = GameState->FlightPathMidLine;
                Aircraft->DropStartTime = Aircraft->DropStartTime - TimeTilDropStart + 7.0f;
                Aircraft->DropEndTime = Aircraft->DropEndTime - TimeTilDropStart + 7.0f;
                Aircraft->FlightEndTime = Aircraft->FlightEndTime - TimeTilDropStart + 15.0f;
                Aircraft->K2_TeleportTo(NewStartLocation, FlightInfo.FlightStartRotation);
            }
            First = true;
        }
    }

    FFortSafeZoneDefinition* SafeZoneDefinition = &GameState->MapInfo->SafeZoneDefinition;
    TArray<float>& Durations = *(TArray<float>*)(__int64(SafeZoneDefinition) + 0x248);
    TArray<float>& HoldDurations = *(TArray<float>*)(__int64(SafeZoneDefinition) + 0x238);
    auto Sum = 0.f;
    for (auto& _v : Durations) Sum += _v;
    if (Sum == 0.f)
    {
        UCurveTable* GameData = GameState->CurrentPlaylistInfo.BasePlaylist->GameData.Get();

        if (!GameData)
            GameData = StaticFindObject<UCurveTable>("/Game/Balance/AthenaGameData.AthenaGameData");

        auto ShrinkTime = UKismetStringLibrary::Conv_StringToName(L"Default.SafeZone.ShrinkTime");
        auto HoldTime = UKismetStringLibrary::Conv_StringToName(L"Default.SafeZone.WaitTime");
        
        for (size_t i = 0; i < HoldDurations.Num(); i++)
        {
            float Out;
            EEvaluateCurveTableResult Res;
            UDataTableFunctionLibrary::EvaluateCurveTableRow(GameData, HoldTime, i, &Res, &Out, FString());
            HoldDurations[i] = Out;
        }
        for (size_t i = 0; i < Durations.Num(); i++)
        {
            float Out;
            EEvaluateCurveTableResult Res;
            UDataTableFunctionLibrary::EvaluateCurveTableRow(GameData, ShrinkTime, i, &Res, &Out, FString());
            Durations[i] = Out;
        }
    }

    auto SafeZoneIndicator = GameMode->SafeZoneIndicator;
    SafeZoneIndicator->SafeZoneStartShrinkTime = UGameplayStatics::GetTimeSeconds(GetWorld()) + HoldDurations[GameMode->SafeZonePhase + 1];
    SafeZoneIndicator->SafeZoneFinishShrinkTime = SafeZoneIndicator->SafeZoneStartShrinkTime + Durations[GameMode->SafeZonePhase + 1];
    StartNewSafeZonePhaseOG(GameMode, NewSafeZonePhase);
}

void FortGameModeAthena::OnAircraftExitedDropZone(AFortGameModeAthena* GameMode, AFortAthenaAircraft* FortAthenaAircraft)
{
    for (auto& Player : GameMode->AlivePlayers)
    {
        if (Player->IsInAircraft())
            Player->GetAircraftComponent()->ServerAttemptAircraftJump({}); // we want this cause we do safezone stuff there
    }
    
    return OnAircraftExitedDropZoneOG(GameMode, FortAthenaAircraft);
}

void FortGameModeAthena::StartAircraftPhase(AFortGameModeAthena* GameMode, char a2)
{
    StartAircraftPhaseOG(GameMode, a2);
    if (!bCreative)
    {
        auto GameState = GetGameState();

        GameState->GamePhase = EAthenaGamePhase::Aircraft;
        GameState->GamePhaseStep = EAthenaGamePhaseStep::BusLocked;
        GameState->OnRep_GamePhase(EAthenaGamePhase::Warmup);
    
        std::thread([GameMode]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(7000));
            for (auto& Player : GameMode->AliveBots)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                Player->PlayerBotPawn->K2_TeleportTo(GameMode->Aircrafts[0]->K2_GetActorLocation(), GameMode->Aircrafts[0]->K2_GetActorRotation());
                Player->ThankBusDriver();
                Player->PlayerBotPawn->BeginSkydiving(true);
            }
        }).detach();
    }
}

__int64 FortGameModeAthena::InitializeFlightPath(
    __int64 a1,
    __int64 a2,
    __int64 a3,
    unsigned __int64 a4,
    unsigned __int64 a5,
    __int64 a6,
    float a7,
    float* a8,
    float* a9,
    float a10,
    float a11) 
{
    static auto PlaylistName = GetGameState()->CurrentPlaylistInfo.BasePlaylist->PlaylistName.ToString();
    if (PlaylistName.contains("BlueCheese")) 
    {
        if (GetGameMode()->SafeZoneLocations.Num() > 0)
        {
            TArray<AActor*> FoundPOIActors;
            UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABuildingSMActor::StaticClass(), &FoundPOIActors);
            if (FoundPOIActors.Num() > 0)
            {
                auto Location = FoundPOIActors[std::rand() % FoundPOIActors.Num() - 1]->K2_GetActorLocation();
                Location.Z = 0;
                for (int i = 0; i < GetGameMode()->SafeZoneLocations.Num(); i++)
                {
                    GetGameMode()->SafeZoneLocations[i] = Location + GetGameMode()->SafeZoneLocations[i];
                }
            }
        }
    }
    return InitializeFlightPathOG(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11);
}

UClass** FortGameModeAthena::GetGameSessionClass(AFortGameModeAthena* GameMode, UClass** ClassRet)
{
    *ClassRet = AFortGameSessionDedicatedAthena::StaticClass();
    GameMode->GameSessionClass = *ClassRet;
    return ClassRet;
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

    Hook->Address = ImageBase + 0x4A30BC4;
    Hook->Original = (void**)&InitializeFlightPathOG;
    Hook->Detour = InitializeFlightPath;
    UKismetHookingLibrary::Hook(Hook, EHook::Address);

    Hook->Address = ImageBase + 0x4A76B34;
    Hook->Original = (void**)&OnAircraftExitedDropZoneOG;
    Hook->Detour = OnAircraftExitedDropZone;
    UKismetHookingLibrary::Hook(Hook, EHook::Address);

    Hook->Address = ImageBase + 0x1172A0C;
    Hook->Detour = GetGameSessionClass;
    if (bGameSessions) UKismetHookingLibrary::Hook(Hook, EHook::Address);
    
    free(Hook);
}
