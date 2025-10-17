#include "pch.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Player/FortPlayerController.h"

#include "Globals.h"
#include "Offsets.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Kismet/FortKismetLibrary.h"
#include "Engine/Source/Runtime/GameplayAbilities/Public/AbilitySystemComponent.h"

void FortPlayerController::ServerAcknowledgePossession(AFortPlayerControllerAthena* Controller, APawn* Pawn)
{
    auto PlayerState = (AFortPlayerStateZone*)Controller->PlayerState;
    Controller->AcknowledgedPawn = Pawn;

    PlayerState->HeroType = Controller->CosmeticLoadoutPC.Character->HeroDefinition;
    
    UFortKismetLibrary::UpdatePlayerCustomCharacterPartsVisualization(PlayerState);
    
    reinterpret_cast<void(*)(void*)>(ImageBase + 0x4EC023C)(PlayerState->GetInterfaceAddress<IAbilitySystemInterface>()); // ability stuff
    
    Controller->XPComponent->bRegisteredWithQuestManager = true;
    Controller->XPComponent->OnRep_bRegisteredWithQuestManager();
    Controller->GetQuestManager(ESubGame::Athena)->InitializeQuestAbilities(Pawn);
}

void FortPlayerController::ServerExecuteInventoryItem(AFortPlayerController* Controller, FGuid ItemGuid)
{
    if (!Controller || !Controller->MyFortPawn) return;
    FFortItemEntry* Entry = Controller->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry) {
        return entry.ItemGuid == ItemGuid;
    });
    
    if (Controller->MyFortPawn)
    {
        if (Entry->ItemDefinition->IsA(UFortDecoItemDefinition::StaticClass()))
        {
            auto DecoItemDefinition = Cast<UFortDecoItemDefinition>(Entry->ItemDefinition);
            if (!DecoItemDefinition) return;
    
            Controller->MyFortPawn->PickUpActor(nullptr, DecoItemDefinition);
            Controller->MyFortPawn->CurrentWeapon->ItemEntryGuid = ItemGuid;

            if (auto ContextTrap = Cast<AFortDecoTool_ContextTrap>(Controller->MyFortPawn->CurrentWeapon))
            {
                ContextTrap->SetContextTrapItemDefinition(Cast<UFortContextTrapItemDefinition>(DecoItemDefinition));
            }
            return;
        }
   
        Controller->MyFortPawn->EquipWeaponDefinition((UFortWeaponItemDefinition*)Entry->ItemDefinition, Entry->ItemGuid, Entry->TrackerGuid, false); 
    }
}

void FortPlayerController::ServerCreateBuildingActor(AFortPlayerControllerAthena* Controller, FCreateBuildingActorData& CreateBuildingData)
{
    AFortGameStateAthena* GameState = Cast<AFortGameStateAthena>(UWorld::GetWorld()->GameState);
    TArray<ABuildingActor*> BuildingActors;

    for (auto &BuildableClass : GameState->AllPlayerBuildableClassesIndexLookup)
    {
        if (BuildableClass.Value() == CreateBuildingData.BuildingClassHandle)
            CreateBuildingData.BuildingClassData.BuildingClass = BuildableClass.Key();
    }
    
    if (auto BuildingClass = CreateBuildingData.BuildingClassData.BuildingClass)
    {
        EFortBuildPreviewMarkerOptionalAdjustment Adjustment;

        EFortStructuralGridQueryResults QueryResult = GameState->StructuralSupportSystem->CanAddBuildingActorClassToGrid(
            UWorld::GetWorld(),
            BuildingClass,
            CreateBuildingData.BuildLoc,
            CreateBuildingData.BuildRot,
            CreateBuildingData.bMirrored,
            &BuildingActors,
            &Adjustment,
            false);

        if (QueryResult == EFortStructuralGridQueryResults::CanAdd)
        {
            for (int32 i = 0; i < BuildingActors.Num(); i++)
            {
                ABuildingActor *ExistingBuilding = BuildingActors[i];
                if (!ExistingBuilding)
                    continue;

                ExistingBuilding->K2_DestroyActor();
            }

            static void *(*CanAffordToPlaceBuildableClass)(AFortPlayerController *, FBuildingClassData *) = decltype(CanAffordToPlaceBuildableClass)(
              ImageBase + 0x520C100);
            if (!CanAffordToPlaceBuildableClass(Controller, &CreateBuildingData.BuildingClassData))
                return;

            static void* (*PayBuildableClassPlacementCost)(AFortPlayerController*, FBuildingClassData*) = decltype(PayBuildableClassPlacementCost)(
                ImageBase + 0x52315DC);
            PayBuildableClassPlacementCost(Controller, &CreateBuildingData.BuildingClassData);
            
            ABuildingSMActor* BuildingSMActor = UWorld::GetWorld()->SpawnActor<ABuildingSMActor>(BuildingClass, CreateBuildingData.BuildLoc, CreateBuildingData.BuildRot);
            if (BuildingSMActor)
            {
                BuildingSMActor->CurrentBuildingLevel = CreateBuildingData.BuildingClassData.UpgradeLevel = 0;

                BuildingSMActor->InitializeKismetSpawnedBuildingActor(BuildingSMActor, Controller, true, nullptr);
                BuildingSMActor->bPlayerPlaced = true;

                AFortPlayerStateAthena *PlayerState = Cast<AFortPlayerStateAthena>(Controller->PlayerState);
                if (PlayerState)
                {
                    BuildingSMActor->Team = EFortTeam(PlayerState->TeamIndex);
                    BuildingSMActor->OnRep_Team();
                    BuildingSMActor->SetTeam(PlayerState->TeamIndex);
                }
            } 
        }
    }
}

void FortPlayerController::ServerBeginEditingBuildingActor(AFortPlayerController* PlayerController, ABuildingSMActor* BuildingActorToEdit)
{
    AFortPlayerStateZone* PlayerStateZone = Cast<AFortPlayerStateZone>(PlayerController->PlayerState);
    if (!PlayerStateZone || !BuildingActorToEdit || !PlayerController->MyFortPawn)
        return;

    static void* (*SetEditingPlayer)(ABuildingSMActor*, AFortPlayerController*) = decltype(SetEditingPlayer)(ImageBase + 0x4CA0FA8);
    SetEditingPlayer(BuildingActorToEdit, PlayerController);

    auto AssetManager = GetAssetManager();
    static UFortEditToolItemDefinition* EditToolItemDefinition = AssetManager->GameDataCommon->EditToolItem.Get();
    if (!EditToolItemDefinition)
    {
        const FString& AssetPathName = UKismetStringLibrary::Conv_NameToString(AssetManager->GameDataCommon->EditToolItem.ObjectID.AssetPathName);
        EditToolItemDefinition = StaticLoadObject<UFortEditToolItemDefinition>(AssetPathName.ToString());
    }

    auto ItemEntry = PlayerController->WorldInventory->Inventory.ReplicatedEntries.Search([](FFortItemEntry &entry)
                                                                            { return entry.ItemDefinition == EditToolItemDefinition; });
    if (ItemEntry && 
        EditToolItemDefinition && 
        PlayerController->MyFortPawn->EquipWeaponDefinition(EditToolItemDefinition, ItemEntry->ItemGuid, ItemEntry->TrackerGuid, false))
    {
        AFortWeap_EditingTool* EditingTool = Cast<AFortWeap_EditingTool>(PlayerController->MyFortPawn->CurrentWeapon);

        if (EditingTool)
        {
            EditingTool->EditActor = BuildingActorToEdit;
            EditingTool->OnRep_EditActor();
        }
    }
}

void FortPlayerController::ServerEditBuildingActor(AFortPlayerController* PlayerController, ABuildingSMActor* BuildingActorToEdit, TSubclassOf<ABuildingSMActor> BuildingClass, uint8 Rot, bool Mirrored)
{
    if (!PlayerController || !BuildingActorToEdit || !BuildingActorToEdit->IsA(ABuildingSMActor::StaticClass()) || !PlayerController->MyFortPawn)
        return;
    
    static void* (*SetEditingPlayer)(ABuildingSMActor*, AFortPlayerController*) = decltype(SetEditingPlayer)(ImageBase + 0x4CA0FA8);
    SetEditingPlayer(BuildingActorToEdit, nullptr);

    static auto ReplaceBuildingActor = (ABuildingSMActor * (*)(ABuildingSMActor*, unsigned int, UObject*, unsigned int, int, bool, AFortPlayerController*))(
        ImageBase + 0x4c9f210);
    
    int32 CurrentBuildingLevel = BuildingActorToEdit->CurrentBuildingLevel;
    ABuildingSMActor* NewBuild = ReplaceBuildingActor(BuildingActorToEdit, 1, BuildingClass, CurrentBuildingLevel, Rot, Mirrored, PlayerController);
    if (NewBuild) NewBuild->bPlayerPlaced = true;
}

void FortPlayerController::ServerEndEditingBuildingActor(AFortPlayerController* PlayerController, ABuildingSMActor* BuildingActorToEdit)
{
    if (!BuildingActorToEdit ||
        BuildingActorToEdit->EditingPlayer != PlayerController->PlayerState ||
        BuildingActorToEdit->bDestroyed || !PlayerController->MyFortPawn)
        return;

    static void* (*SetEditingPlayer)(ABuildingSMActor*, AFortPlayerController*) = decltype(SetEditingPlayer)(ImageBase + 0x4CA0FA8);
    SetEditingPlayer(BuildingActorToEdit, nullptr);

    AFortWeap_EditingTool* EditingTool = Cast<AFortWeap_EditingTool>(PlayerController->MyFortPawn->CurrentWeapon);
    if (EditingTool)
    {
        EditingTool->EditActor = BuildingActorToEdit;
        EditingTool->OnRep_EditActor();
    }
}

void FortPlayerController::ServerAttemptInventoryDrop(AFortPlayerController* Controller, const struct FGuid& ItemGuid, int32 Count, bool bTrash)
{
    auto PlayerController = (AFortPlayerControllerAthena*)Controller;
    if (!PlayerController || !PlayerController->Pawn)
        return;
    auto ItemEntry = PlayerController->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry)
        { return entry.ItemGuid == ItemGuid; });

    if (!ItemEntry || (ItemEntry->Count - Count) < 0)
        return;

    ItemEntry->Count -= Count;
    FVector DropLocation = FVector(0, 0, 0);
    if (PlayerController->Pawn)
    {
        DropLocation = PlayerController->Pawn->K2_GetActorLocation() + PlayerController->Pawn->GetActorForwardVector() * 70.f + FVector(0, 0, 50);
    }
    FortKismetLibrary::SpawnPickup(
        DropLocation, *ItemEntry, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, PlayerController->MyFortPawn, Count, true,
        true, false);
    
    PlayerController->WorldInventory->ReplaceEntry(*ItemEntry);
}

void FortPlayerController::ServerLoadingScreenDropped(AFortPlayerControllerAthena* Controller)
{
    auto GS = Cast<AFortGameStateAthena>(GetWorld()->GameState);
    auto GM = Cast<AFortGameModeAthena>(GetWorld()->AuthorityGameMode);
    if (!Controller || !GM || !GS) return ServerLoadingScreenDroppedOG(Controller);

    auto PlayerState = Cast<AFortPlayerStateAthena>(Controller->PlayerState);
    if(!PlayerState) return ServerLoadingScreenDroppedOG(Controller);

    if (bCreative)
    {
        auto CreativePortalManager = GS->CreativePortalManager;
        xmap<xstring, UFortCreativeRealEstatePlotItemDefinition*> PlotDefinitionsByMcpId;
        
        auto State = Controller->CreativeModeProfile->InternalGetState();
        UE_LOG(LogServer, Log, "ItemContainerNum: %d", State->ItemsContainer.Items.Num());
        
        if (State->ItemsContainer.Items.Num() > 0)
        {
            for (auto& Item : State->ItemsContainer.Items)
            {
                if (auto McpItem = Item.Value().Get())
                {
                    UE_LOG(LogServer, Log, "Item: %s, Count: %d", McpItem->TemplateId.ToString().c_str(), McpItem->Quantity);
                    UFortCreativeRealEstatePlotItem* Instance = nullptr;
                    FText AltName;
                    xstring PlotId = McpItem->TemplateId.ToString().substr(
                        McpItem->TemplateId.ToString().find(':') + 1
                    );

                    auto PlotItemDefinition = StaticLoadObject<UFortCreativeRealEstatePlotItemDefinition>(
                        "/Game/Playgrounds/Items/Plots/" + PlotId + "." + PlotId);
                    if (!PlotItemDefinition) continue;
                    
                    PlotDefinitionsByMcpId[McpItem->InstanceId.ToString()] = PlotItemDefinition;

                    AltName = PlotItemDefinition->GetDisplayName(true);
                    
                    Instance = (UFortCreativeRealEstatePlotItem*)PlotItemDefinition->CreateTemporaryItemInstanceBP(1, 0);
                    Instance->InstanceID = McpItem->InstanceId;
                    McpItem->Instance = Instance;

                    if (Instance)
                    {
                        auto CreativeIsland = Controller->CreativeIslands.Search([McpItem](FCreativeIslandData& IslandData)
                            { return IslandData.McpId == McpItem->InstanceId; });
                        bool bIsNew = false;
                        if (!CreativeIsland)
                        {
                            bIsNew = true;
                            CreativeIsland = new FCreativeIslandData();
                        }

                        reinterpret_cast<void(*)(UFortCreativeRealEstatePlotItem*, FMcpItem*)>(ImageBase + 0x1F165E4)(Instance, McpItem);

                        auto IslandTitle = UKismetTextLibrary::Conv_StringToText(Instance->IslandTitle);
                        CreativeIsland->McpId = McpItem->InstanceId;
                        CreativeIsland->IslandName = IslandTitle.ToString().empty() ? AltName : IslandTitle;
                        CreativeIsland->LastLoadedDate = Instance->LastUsedDate;
                        CreativeIsland->PublishedIslandCode = Instance->IslandCode;
                        CreativeIsland->bIsDeleted = false;

                        if (bIsNew)
                            Controller->CreativeIslands.Add(*CreativeIsland);
                
                        Controller->OnRep_CreativeIslands();
                    }
                }
            }
        }

        AFortAthenaCreativePortal* Portal = nullptr;

        FString TargetMcpId;
        if (Controller->CreativeIslands.Num() > 0)
        {
            FCreativeIslandData* MostRecentIsland = nullptr;
    
            for (auto& Island : Controller->CreativeIslands)
            {
                if (!MostRecentIsland || Island.LastLoadedDate > MostRecentIsland->LastLoadedDate)
                {
                    MostRecentIsland = &Island;
                }
            }
    
            if (MostRecentIsland)
            {
                TargetMcpId = MostRecentIsland->McpId;
            }
        }

        if (TargetMcpId.ToString().empty())
            return;

        if (!Portal)
        {
            for (auto& Portal1 : CreativePortalManager->AllPortals)
            {
                if (!Portal1->LinkedVolume || Portal1->LinkedVolume->VolumeState == ESpatialLoadingState::Ready)
                {
                    continue;
                }
                Portal = Portal1;
                break;
            }
        }

        if (!Portal)
            return;

        Portal->GetLinkedVolume()->bNeverAllowSaving = false;
        Portal->GetLinkedVolume()->VolumeState = ESpatialLoadingState::Ready;
        Portal->GetLinkedVolume()->OnRep_VolumeState();

        Portal->OwningPlayer = PlayerState->UniqueId;
        Portal->OnRep_OwningPlayer();
        
        auto TargetIsland = Controller->CreativeIslands.Search([TargetMcpId](FCreativeIslandData& IslandData)
            { return IslandData.McpId == TargetMcpId; });
    
        if (TargetIsland)
        {
            Portal->IslandInfo.AltTitle = TargetIsland->IslandName;
            Portal->IslandInfo.CreatorName = TargetIsland->IslandName.GetStringRef();
            Portal->IslandInfo.SupportCode = TargetIsland->PublishedIslandCode.ToString().empty() ? L"1111-1111-1111" : TargetIsland->PublishedIslandCode;
        }
        else
        {
            Portal->IslandInfo.CreatorName = PlayerState->GetPlayerName();
            Portal->IslandInfo.SupportCode = L"Ok";
        }

        Portal->CachedEditIslandData = *TargetIsland;
        
        Portal->IslandInfo.Version = 1.0f;
        Portal->OnRep_IslandInfo();

        Portal->bPortalOpen = true;
        Portal->OnRep_PortalOpen();

        Portal->PlayersReady.Add(PlayerState->UniqueId);
        Portal->OnRep_PlayersReady();

        Portal->bUserInitiatedLoad = true;
        Portal->bInErrorState = false;

        Controller->OwnedPortal = Portal;

        auto LevelSaveComponent = (UFortLevelSaveComponent*)Portal->GetLinkedVolume()->GetComponentByClass(UFortLevelSaveComponent::StaticClass());
        LevelSaveComponent->AccountIdOfOwner = PlayerState->UniqueId;
        LevelSaveComponent->bIsLoaded = true;
        LevelSaveComponent->OnRep_IsActive();

        Controller->CreativePlotLinkedVolume = Portal->GetLinkedVolume();
        Controller->OnRep_CreativePlotLinkedVolume();

        static auto MinigameSettingsMachine = StaticLoadObject<UClass>("/Game/Athena/Items/Gameplay/MinigameSettingsControl/MinigameSettingsMachine.MinigameSettingsMachine_C");
        auto OK = Portal->GetLinkedVolume()->GetTransform();
        GetWorld()->SpawnActor<AActor>(MinigameSettingsMachine, OK.Translation, FRotator(), Portal->LinkedVolume);

        auto it = PlotDefinitionsByMcpId.find(TargetMcpId.ToString());
        if (it != PlotDefinitionsByMcpId.end() && it->second && it->second->BasePlayset)
        {
            auto LevelStreamComponent = (UPlaysetLevelStreamComponent*)Portal->GetLinkedVolume()->GetComponentByClass(UPlaysetLevelStreamComponent::StaticClass());
            LevelStreamComponent->SetPlayset(it->second->BasePlayset);
            LoadPlaysetOG(LevelStreamComponent);
        }
    }
    
    return ServerLoadingScreenDroppedOG(Controller);
}

void FortPlayerController::GetPlayerViewPoint(AFortPlayerControllerAthena* Controller, FVector& Loc, FRotator& Rot)
{
    static auto SFName = UKismetStringLibrary::Conv_StringToName(FString(L"Spectating"));
    if (Controller->StateName == SFName)
    {
        Loc = Controller->LastSpectatorSyncLocation;
        Rot = Controller->LastSpectatorSyncRotation;
    }
    else if (Controller->GetViewTarget())
    {
        Loc = Controller->GetViewTarget()->K2_GetActorLocation();
        Rot = Controller->GetControlRotation();
    }
    else return GetPlayerViewPointOG(Controller, Loc, Rot);
}

void FortPlayerController::Setup()
{
    UHook* Hook = new UHook();

    Hook->Address = Runtime::Vfts::ServerAcknowledgePossesion;
    Hook->Class = AFortPlayerController::StaticClass();
    Hook->Detour = ServerAcknowledgePossession;
    UKismetHookingLibrary::Hook(Hook, EHook::EveryVFT);

    Hook->Address = Runtime::Vfts::ServerExecuteInventoryItem;
    Hook->Class = AFortPlayerController::StaticClass();
    Hook->Detour = ServerExecuteInventoryItem;
    UKismetHookingLibrary::Hook(Hook, EHook::EveryVFT);

    Hook->Address = 0x241;
    Hook->Class = AFortPlayerController::StaticClass();
    Hook->Detour = ServerCreateBuildingActor;
    UKismetHookingLibrary::Hook(Hook, EHook::EveryVFT);
    
    Hook->Address = 0x248;
    Hook->Class = AFortPlayerController::StaticClass();
    Hook->Detour = ServerBeginEditingBuildingActor;
    UKismetHookingLibrary::Hook(Hook, EHook::EveryVFT);

    Hook->Address = 0x243;
    Hook->Class = AFortPlayerController::StaticClass();
    Hook->Detour = ServerEditBuildingActor;
    UKismetHookingLibrary::Hook(Hook, EHook::EveryVFT);

    Hook->Address = 0x246;
    Hook->Class = AFortPlayerController::StaticClass();
    Hook->Detour = ServerEndEditingBuildingActor;
    UKismetHookingLibrary::Hook(Hook, EHook::EveryVFT);

    Hook->Address = 0x22D;
    Hook->Class = AFortPlayerControllerAthena::StaticClass();
    Hook->Detour = ServerAttemptInventoryDrop;
    UKismetHookingLibrary::Hook(Hook, EHook::VFT);

    Hook->Address = ImageBase + 0xe1747c;
    Hook->Original = (void**)&GetPlayerViewPointOG;
    Hook->Detour = GetPlayerViewPoint;
    UKismetHookingLibrary::Hook(Hook, EHook::Address);

    Hook->Address = 0x282;
    Hook->Class = AFortPlayerControllerAthena::StaticClass();
    Hook->Original = (void**)&ServerLoadingScreenDroppedOG;
    Hook->Detour = ServerLoadingScreenDropped;
    UKismetHookingLibrary::Hook(Hook, EHook::VFT);
    
    free(Hook);
}
