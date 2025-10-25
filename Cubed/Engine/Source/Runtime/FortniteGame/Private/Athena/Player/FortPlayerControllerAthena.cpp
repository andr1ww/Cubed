#include "pch.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Athena/Player/FortPlayerControllerAthena.h"

#include "Offsets.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Plugins/TaskLibrary/Public/TaskLibrary.h"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Kismet/FortKismetLibrary.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Quests/FortQuestManager.h"

void FortPlayerControllerAthena::ServerAttemptAircraftJump(UFortControllerComponent_Aircraft* Comp, FRotator ClientRot)
{
    auto Controller = (AFortPlayerControllerAthena*)Comp->GetOwner();
    auto GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;

	TArray<FGuid> ItemsToRemove;
	for (const auto& Item : Controller->WorldInventory->Inventory.ItemInstances)
	{
		if (Item && Item->CanBeDropped())
			ItemsToRemove.Add(Item->ItemEntry.ItemGuid);
	}

	for (const FGuid& Guid : ItemsToRemove)
	{
		Controller->WorldInventory->Remove(Guid);
	}
    
    GameMode->RestartPlayer(Controller);
    if (Controller->MyFortPawn)
    {
		Controller->MyFortPawn->bIsInAnyStorm = false; //Same for respawning!
		Controller->MyFortPawn->OnRep_IsInAnyStorm();

        Controller->ClientSetRotation(ClientRot, false);
        Controller->MyFortPawn->BeginSkydiving(true);
        
        ForEachMutator<AFortAthenaMutator_InventoryOverride>(
            GetWorld(), 
            AFortAthenaMutator_InventoryOverride::StaticClass(),
            [&](AFortAthenaMutator_InventoryOverride* Mutator)
            {
                const auto& Loadout = Mutator->InventoryLoadouts[std::rand() % Mutator->InventoryLoadouts.Num()];
                for (auto& ItemAndCount : Loadout.LoadoutList)
                {
                    int32 Ammo = 0;
                    if (auto* WeaponDef = Cast<UFortWeaponItemDefinition>(ItemAndCount.Item))
                        if (WeaponDef->GetStats())
                            Ammo = WeaponDef->GetStats()->ClipSize;
                        
                    Controller->WorldInventory->AddItem(ItemAndCount.Item, ItemAndCount.Count.Evaluate(), Ammo);
                }
                
                auto AddResource = [&](EFortResourceType Type) {
                    Controller->WorldInventory->AddItem(UFortKismetLibrary::K2_GetResourceItemDefinition(Type), 500, 0);
                };
                AddResource(EFortResourceType::Wood);
                AddResource(EFortResourceType::Stone);
                AddResource(EFortResourceType::Metal);
            }
        );
    }
}

void FortPlayerControllerAthena::ServerAttemptInteract(UFortControllerComponent_Interaction* Comp, AActor* ReceivingActor, UPrimitiveComponent* InteractComponent, ETInteractionType InteractType, UObject* OptionalData, EInteractionBeingAttempted InteractionBeingAttempted)
{
	if (!ReceivingActor)
		return;

	AController* Controller = Cast<AController>(Comp->GetOwner());

	if (!Controller)
		return;

	AFortPlayerPawn* PlayerPawn = Cast<AFortPlayerPawn>(Controller->Pawn);

	if (!PlayerPawn)
		return;
	
	if (auto ParticipantComponent = (UFortNonPlayerConversationParticipantComponent*) ReceivingActor->GetComponentByClass(UFortNonPlayerConversationParticipantComponent::StaticClass()))
	{
		auto ConversationComponent = (UFortPlayerConversationComponent*)Controller->GetComponentByClass(UFortPlayerConversationComponent::StaticClass());
		UConversationLibrary::StartConversation(ParticipantComponent->ConversationEntryTag, ConversationComponent->GetOwner(), ParticipantComponent->InteractorParticipantTag, ReceivingActor, ParticipantComponent->SelfParticipantTag);
	}

	return ServerAttemptInteractOG(Comp, ReceivingActor, InteractComponent, InteractType, OptionalData, InteractionBeingAttempted);
}

void FortPlayerControllerAthena::ServerPlayEmoteItem(AFortPlayerControllerAthena* Controller, UFortMontageItemDefinitionBase* EmoteAsset, float EmoteRandomNumber)
{
    if (!Controller || !Controller->MyFortPawn || !EmoteAsset) return;

    auto AbilitySystemComponent = ((AFortPlayerStateAthena*)Controller->PlayerState)->AbilitySystemComponent;
    static UClass *DanceAbility = StaticLoadObject<UClass>("/Game/Abilities/Emotes/GAB_Emote_Generic.GAB_Emote_Generic_C");
    static UClass *SprayAbility = StaticLoadObject<UClass>("/Game/Abilities/Sprays/GAB_Spray_Generic.GAB_Spray_Generic_C");
    
    FGameplayAbilitySpec NewSpec;
    UClass* Ability = nullptr;
    if (Cast<UAthenaSprayItemDefinition>(EmoteAsset))
    {
        Ability = SprayAbility;
    }
    else if (auto Toy = Cast<UAthenaToyItemDefinition>(EmoteAsset))
    {
        Ability = Toy->ToySpawnAbility.Get();
    }
    else if (auto Emote = Cast<UAthenaDanceItemDefinition>(EmoteAsset))
    {
        auto DA = Emote->CustomDanceAbility.Get();
        Ability = DA ? DA : DanceAbility;
        Controller->MyFortPawn->bMovingEmote = Emote->bMovingEmote;
        Controller->MyFortPawn->bMovingEmoteForwardOnly = Emote->bMoveForwardOnly;
        Controller->MyFortPawn->EmoteWalkSpeed = Emote->WalkForwardSpeed;
    }
    
    if (Ability)
    {
        ((void (*)(FGameplayAbilitySpec*, UObject*, int, int, UObject*))(ImageBase + 0x3D47310))(
            &NewSpec, Ability->DefaultObject, 1, -1, EmoteAsset);
        FGameplayAbilitySpecHandle handle;
        ((void (*)(UFortAbilitySystemComponent*, FGameplayAbilitySpecHandle*, FGameplayAbilitySpec*, void*))(ImageBase + 0x3D54000))(
            AbilitySystemComponent, &handle, &NewSpec, nullptr);
    }
}

void FortPlayerControllerAthena::ServerLoadPlotForPortal(AFortPlayerControllerAthena* Controller, FFrame& Stack)
{
    AFortAthenaCreativePortal* Portal;
    FString PlotItemId;
    Stack.StepCompiledIn(&Portal);
    Stack.StepCompiledIn(&PlotItemId);
    Stack.IncrementCode();
    
    auto PlotItem = Controller->CreativeModeProfile->FindPlotById(PlotItemId);
    if (!PlotItem) return;
    
    auto PlayerState = Controller->PlayerState;
    
    if (!Portal || !Portal->LinkedVolume)
       return;
            
    Portal->GetLinkedVolume()->bNeverAllowSaving = false;
    Portal->GetLinkedVolume()->VolumeState = ESpatialLoadingState::Initializing;
    Portal->GetLinkedVolume()->OnRep_VolumeState();
    Portal->GetLinkedVolume()->ServerClearVolume();
	
    auto LevelStreamComponent = (UPlaysetLevelStreamComponent*)Portal->GetLinkedVolume()->GetComponentByClass(UPlaysetLevelStreamComponent::StaticClass());
    UnLoadPlaysetOG(LevelStreamComponent);

	auto LevelSaveComponent = (UFortLevelSaveComponent*)Portal->GetLinkedVolume()->GetComponentByClass(UFortLevelSaveComponent::StaticClass());
	LevelSaveComponent->AccountIdOfOwner = PlayerState->UniqueId;
	LevelSaveComponent->bIsLoaded = false;
	LevelSaveComponent->OnRep_IsActive();
	ResetLevelRecordOG(LevelSaveComponent);
	LevelSaveComponent->StopPeriodicSaveTimer();
    
    Portal->OwningPlayer = PlayerState->UniqueId;
    Portal->OnRep_OwningPlayer();
        
    Portal->IslandInfo.AltTitle = Portal->LoadingText;
    Portal->OnRep_IslandInfo();

    Portal->bPortalOpen = false;
    Portal->OnRep_PortalOpen();

	auto it = Creative::PlotDefinitionsByMcpId.find(PlotItemId.ToString());
	if (it != Creative::PlotDefinitionsByMcpId.end() && it->second && it->second->BasePlayset)
	{
		LevelSaveComponent->bLoadPlaysetFromPlot = true;

		LoadPlaysetOG(LevelStreamComponent);
	}

	std::string QueueId = "PortalLoading_" + std::to_string(rand());
	float currentTime = static_cast<float>(
std::chrono::duration<double>(
	std::chrono::steady_clock::now().time_since_epoch()
).count()
);
    
	UTaskLibrary::Initialize(QueueId, currentTime, UTaskLibrary::ExecutionMode::SeparateThread);
        
    UTaskLibrary::QueueTask(QueueId, currentTime, 10.0f, [
    	QueueId, PlotItem, Controller, Portal, PlayerState, PlotItemId]()
    {
	    Portal->GetLinkedVolume()->VolumeState = ESpatialLoadingState::Ready;
		Portal->GetLinkedVolume()->OnRep_VolumeState();
            
		auto TargetIsland = Controller->CreativeIslands.Search([PlotItemId](FCreativeIslandData& IslandData)
			{ return IslandData.McpId == PlotItemId; });
        
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
            
		Portal->bUserInitiatedLoad = true;
		Portal->bInErrorState = false;

		Portal->bPortalOpen = true;
		Portal->OnRep_PortalOpen();
            
		Controller->OwnedPortal = Portal;
            
		auto LevelSaveComponent = (UFortLevelSaveComponent*)Portal->GetLinkedVolume()->GetComponentByClass(UFortLevelSaveComponent::StaticClass());
		LevelSaveComponent->AccountIdOfOwner = PlayerState->UniqueId;
		LevelSaveComponent->bIsLoaded = true;
		LevelSaveComponent->OnRep_IsActive();
		LevelSaveComponent->StartPeriodicSaveTimer();
		LevelSaveComponent->LoadedPlot = (UFortCreativeRealEstatePlotItem*)PlotItem->Instance;
            
		static auto MinigameSettingsMachine = StaticLoadObject<UClass>("/Game/Athena/Items/Gameplay/MinigameSettingsControl/MinigameSettingsMachine.MinigameSettingsMachine_C");
		auto Transform = Portal->GetLinkedVolume()->GetTransform();
		GetWorld()->SpawnActor<AActor>(MinigameSettingsMachine, Transform.Translation, FRotator(), Portal->LinkedVolume);
    	
		UTaskLibrary::Shutdown(QueueId);
    });
}

void FortPlayerControllerAthena::ClientOnPawnDied(AFortPlayerControllerAthena* PlayerController, FFortPlayerDeathReport& DeathReport)
{
	if (!PlayerController)
		return ClientOnPawnDiedOG(PlayerController, DeathReport);

	auto GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;
	auto GameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;
	auto PlayerState = (AFortPlayerStateAthena*)PlayerController->PlayerState;
	
	if (!PlayerState || !GameState || !GameMode) return;

	auto KillerPlayerState = (AFortPlayerStateAthena*)DeathReport.KillerPlayerState;
	auto KillerPawn = Cast<AFortPlayerPawnAthena>(DeathReport.KillerPawn);
	auto VictimPawn = (AFortPlayerPawnAthena*)PlayerController->MyFortPawn;

	TArray<FFortQuestObjectiveCompletion> Advance;
	TArray<FAthenaAccolades> Accolades;
	TArray<FString> ShuffledLoadoutUsed;
	TArray<FSecondaryXpGained> secondaryXp;
	TArray<FFortCreateItemDetail> GrantedItems;
	TArray<FFortTransientQuestGrant> GrantedTransientQuests;
	TArray<FAthenaSeasonItemMCPState> SeasonItemStates; 
	static auto PlaylistId = UKismetStringLibrary::Conv_NameToString(GameState->CurrentPlaylistInfo.BasePlaylist->PlaylistName);
	
	PlayerController->AthenaProfile->EndBattleRoyaleGameV2(Advance, PlaylistId, PlayerController->MatchReport->MatchStats,
		100, 0, 0, 0, 1.1f,
		true, true, Accolades, ShuffledLoadoutUsed,
		0, ShuffledLoadoutUsed, ShuffledLoadoutUsed, secondaryXp,
		GrantedItems, GrantedTransientQuests, PlaylistId, SeasonItemStates,
		UGameplayStatics::GetTimeSeconds(GetWorld()) + PlayerState->TimeOfPawnCreation,
		{});
	
	FVector DeathLocation = VictimPawn ? VictimPawn->K2_GetActorLocation() : FVector{0,0,0};

	if (!KillerPlayerState && VictimPawn)
		KillerPlayerState = Cast<AFortPlayerStateAthena>(Cast<AFortPlayerControllerAthena>(VictimPawn->Controller)->PlayerState);
	
	PlayerState->PawnDeathLocation = DeathLocation;
	PlayerState->DeathInfo.bDBNO = false;
	PlayerState->DeathInfo.DeathLocation = DeathLocation;
	PlayerState->DeathInfo.DeathTags = DeathReport.Tags;
	PlayerState->DeathInfo.DeathCause = AFortPlayerStateAthena::ToDeathCause(DeathReport.Tags, false);
	PlayerState->DeathInfo.Downer = KillerPlayerState;
	PlayerState->DeathInfo.FinisherOrDowner = KillerPlayerState;
	EDeathCause CachedDeathCause = PlayerState->DeathInfo.DeathCause;
	
	if (VictimPawn) {
		PlayerState->DeathInfo.Distance = (PlayerState->DeathInfo.DeathCause != EDeathCause::FallDamage) 
			? (KillerPawn && KillerPawn->Class->GetFunction("Actor", "GetDistanceTo") ? KillerPawn->GetDistanceTo(VictimPawn) : 0.0f)
			: VictimPawn->LastFallDistance;
	}
	
	PlayerState->DeathInfo.bInitialized = true;
	PlayerState->OnRep_DeathInfo();
	
	bool bRespawningAllowed = GameState && PlayerState ? GameState->IsRespawningAllowed(PlayerState) : false;

	if (!bRespawningAllowed && 
		PlayerController->WorldInventory && PlayerController->MyFortPawn)
	{
		static UClass* Types[] = {
			UFortResourceItemDefinition::StaticClass(),
			UFortWeaponRangedItemDefinition::StaticClass(),
			UFortConsumableItemDefinition::StaticClass(),
			UFortAmmoItemDefinition::StaticClass()
		};
		
		auto Location = PlayerController->MyFortPawn->K2_GetActorLocation();
		bool bFoundMats = false;
		
		for (auto& entry : PlayerController->WorldInventory->Inventory.ReplicatedEntries) {
			auto ItemDef = entry.ItemDefinition;
			if (ItemDef->IsA(UFortWeaponMeleeItemDefinition::StaticClass())) continue;
			
			bool bAllowedType = false;
			for (auto Type : Types) {
				if (ItemDef->IsA(Type)) {
					bAllowedType = true;
					if (!bFoundMats && Type == UFortResourceItemDefinition::StaticClass())
						bFoundMats = true;
					break;
				}
			}
			
			if (bAllowedType)
			{
				FortKismetLibrary::SpawnPickup(Location, entry, 
	EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::PlayerElimination, PlayerController->MyFortPawn, -1,
	true, false, true);
			}
		}
		
		if (!bFoundMats) {
			static auto Wood = StaticFindObject<UFortWorldItemDefinition>("/Game/Items/ResourcePickups/WoodItemData.WoodItemData");
			static auto Stone = StaticFindObject<UFortWorldItemDefinition>("/Game/Items/ResourcePickups/StoneItemData.StoneItemData");
			static auto Metal = StaticFindObject<UFortWorldItemDefinition>("/Game/Items/ResourcePickups/MetalItemData.MetalItemData");

			FortKismetLibrary::SpawnPickup(Location, FortKismetLibrary::ConstructItemEntry(Wood, 50, 0), 
EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::PlayerElimination, PlayerController->MyFortPawn, -1,
true, false, true);

			FortKismetLibrary::SpawnPickup(Location, FortKismetLibrary::ConstructItemEntry(Stone, 50, 0), 
EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::PlayerElimination, PlayerController->MyFortPawn, -1,
true, false, true);

			FortKismetLibrary::SpawnPickup(Location, FortKismetLibrary::ConstructItemEntry(Metal, 50, 0), 
EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::PlayerElimination, PlayerController->MyFortPawn, -1,
true, false, true);
		}
	}

	if (PlayerState->PlayerTeam->TeamMembers.Num() > 1 && PlayerState->DeathInfo.bDBNO) {
		for (auto& Member : PlayerState->PlayerTeam->TeamMembers) {
			auto MemberController = (AFortPlayerControllerAthena*)Member;
			if (MemberController != PlayerController && !MemberController->bMarkedAlive) {
				((void (*)(AFortGameModeAthena*, AFortPlayerController*, APlayerState*, AFortPawn*, UFortWeaponItemDefinition*, EDeathCause, char))(ImageBase + 0x4A81A2C))
					(GameMode, PlayerController, KillerPlayerState, KillerPawn, 
					 DeathReport.DamageCauser->Index ? Cast<AFortWeapon>(DeathReport.DamageCauser)->WeaponData : nullptr, 
					 PlayerState->DeathInfo.DeathCause, 0);
				PlayerController->bMarkedAlive = false;
				return ClientOnPawnDiedOG(PlayerController, DeathReport);
			}
		}
	}

	if (!KillerPlayerState) KillerPlayerState = PlayerState;
	if (!KillerPawn) KillerPawn = PlayerController->MyFortPawn ? Cast<AFortPlayerPawnAthena>(PlayerController->MyFortPawn) : nullptr;

	if (KillerPlayerState && KillerPawn && KillerPawn->Controller && KillerPawn->Controller != PlayerController) {
		KillerPlayerState->KillScore++;
		KillerPlayerState->OnRep_Kills();
		KillerPlayerState->TeamKillScore++;
		KillerPlayerState->OnRep_TeamKillScore();
		KillerPlayerState->ClientReportTeamKill(KillerPlayerState->TeamKillScore);
		
		for (auto Member : ((AFortPlayerStateAthena*)KillerPlayerState)->PlayerTeam->TeamMembers) {
			if ((AFortPlayerStateAthena*)Member->PlayerState != KillerPlayerState) {
				((AFortPlayerStateAthena*)Member->PlayerState)->TeamKillScore++;
				((AFortPlayerStateAthena*)Member->PlayerState)->OnRep_TeamKillScore();
				((AFortPlayerStateAthena*)Member->PlayerState)->ClientReportTeamKill(((AFortPlayerStateAthena*)Member->PlayerState)->TeamKillScore);
			}
		}
		
		KillerPlayerState->ClientReportKill(PlayerState);
		if (auto CPlayerController = Cast<AFortPlayerControllerAthena>(KillerPlayerState->GetOwner())) {
			if (CPlayerController->MatchReport)
				CPlayerController->MatchReport->MatchStats.Stats[3] = KillerPlayerState->KillScore;
		}

		static auto PlaylistName = GameState->CurrentPlaylistInfo.BasePlaylist->PlaylistName.ToString();
		
		if (PlaylistName.contains("BlueCheese") && KillerPawn && KillerPawn != PlayerController->MyFortPawn) {
			static FGameplayTag EarnedElim = { UKismetStringLibrary::Conv_StringToName(L"Event.EarnedElimination") };
			FGameplayEventData Data{};
			Data.EventTag = EarnedElim;
			Data.ContextHandle = KillerPlayerState->AbilitySystemComponent->MakeEffectContext();
			Data.Instigator = KillerPlayerState->GetOwner();
			Data.Target = PlayerState;
			Data.TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromActor(PlayerState);

			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(((AFortPlayerController*)KillerPawn->GetController())->MyFortPawn, EarnedElim, Data);

			auto Pawn = KillerPlayerState->GetCurrentPawn();
			if (Pawn && !Pawn->IsActorBeingDestroyed()) {
				auto Health = Pawn->GetHealth();
				auto Shield = Pawn->GetShield();

				if (Health >= 100.0f) {
					Shield = Shield + 50.0f;
				} else if (Health + 50.0f > 100.0f) {
					float HealthOverflow = (Health + 50.0f) - 100.0f;
					Health = 100.0f;
					Shield = Shield + HealthOverflow;
				} else {
					Health += 50.0f;
				}
    
				Pawn->SetHealth(Health);
				Pawn->SetShield(Shield);
			}
		}
		
		auto QuestManager = ((AFortPlayerControllerAthena*)KillerPlayerState->GetOwner())->GetQuestManager(ESubGame::Athena);
		FGameplayTagContainer ContextTags;
		FGameplayTagContainer TargetTags;
		FGameplayTagContainer SourceTags;
		QuestManager->GetSourceAndContextTags(&SourceTags, &ContextTags);
		ContextTags.AppendTags(GameState->CurrentPlaylistInfo.BasePlaylist->GameplayTagContainer);

		static auto HomeBaseClassTag = FGameplayTag(UKismetStringLibrary::Conv_StringToName(L"Homebase.Class"));
		TargetTags.GameplayTags.Add(HomeBaseClassTag);
		TargetTags.ParentTags.Add(HomeBaseClassTag);
		
		FortQuestManager::SendStatEventWithTags(QuestManager, EFortQuestObjectiveStatEvent::Kill, NULL, TargetTags, SourceTags,
							  ContextTags, 1);

		auto DeadQuestManager = PlayerController->GetQuestManager(ESubGame::Athena);
		FGameplayTagContainer ContextTags2;
		FGameplayTagContainer TargetTags2;
		FGameplayTagContainer SourceTags2;
		DeadQuestManager->GetSourceAndContextTags(&SourceTags2, &ContextTags2);
		ContextTags.AppendTags(GameState->CurrentPlaylistInfo.BasePlaylist->GameplayTagContainer);

		TargetTags2.GameplayTags.Add(HomeBaseClassTag);
		TargetTags2.ParentTags.Add(HomeBaseClassTag);
		FortQuestManager::SendStatEventWithTags(DeadQuestManager, EFortQuestObjectiveStatEvent::AthenaRank, NULL, TargetTags2, SourceTags2,
							  ContextTags2, 1);
	}
	
	if (!bRespawningAllowed && 
		(PlayerController->MyFortPawn ? !PlayerController->MyFortPawn->IsDBNO() : true))
	{
		((void (*)(AFortGameModeAthena*, AFortPlayerController*, APlayerState*, AFortPawn*, UFortWeaponItemDefinition*, EDeathCause, char))(ImageBase + 0x4A81A2C))
			(GameMode, PlayerController, KillerPlayerState, KillerPawn, 
			 DeathReport.DamageCauser ? Cast<AFortWeapon>(DeathReport.DamageCauser) ? Cast<AFortWeapon>(DeathReport.DamageCauser)->WeaponData : nullptr : nullptr, 
			 PlayerState->DeathInfo.DeathCause, 0);

		FAthenaRewardResult KResult = PlayerController->MatchReport->EndOfMatchResults;

		for (auto& AlivePlayer : GameMode->AlivePlayers) {
			if (!AlivePlayer || AlivePlayer == PlayerController)
				continue;
			
			auto QuestManager = AlivePlayer->GetQuestManager(ESubGame::Athena);
			FGameplayTagContainer ContextTags;
			FGameplayTagContainer TargetTags;
			FGameplayTagContainer SourceTags;
			QuestManager->GetSourceAndContextTags(&SourceTags, &ContextTags);
			ContextTags.AppendTags(GameState->CurrentPlaylistInfo.BasePlaylist->GameplayTagContainer);
			FortQuestManager::SendStatEventWithTags(QuestManager, EFortQuestObjectiveStatEvent::AthenaOutlive, NULL, TargetTags, SourceTags,
								  ContextTags, 1);
		}
		
		if (PlayerController->MatchReport)
		{
			KResult.TotalBookXpGained = PlayerController->XPComponent->TotalXpEarned;
			KResult.TotalSeasonXpGained = PlayerController->XPComponent->TotalXpEarned;
			PlayerController->MatchReport->EndOfMatchResults = KResult;
			PlayerController->ClientSendEndBattleRoyaleMatchForPlayer(true, PlayerController->MatchReport->EndOfMatchResults);

			PlayerState->Place = GameState->PlayersLeft + 1;
			PlayerState->OnRep_Place();

			FAthenaMatchStats& Stats = PlayerController->MatchReport->MatchStats;
			FAthenaMatchTeamStats& TeamStats = PlayerController->MatchReport->TeamStats;

			if (PlayerState && PlayerState->KillScore && PlayerState->SquadId) {
				Stats.Stats[3] = PlayerState->KillScore;
				Stats.Stats[8] = PlayerState->SquadId;
				PlayerController->ClientSendMatchStatsForPlayer(Stats);
			}

			TeamStats.Place = PlayerState->Place;
			TeamStats.TotalPlayers = GameState->TotalPlayers;
			PlayerController->ClientSendTeamStatsForPlayer(TeamStats);
		}
		
		int AlivePlayersCount = 0;
		AFortPlayerControllerAthena* LastAliveController = nullptr;

		for (auto& AlivePC : GameMode->AlivePlayers) {
			if (AlivePC && AlivePC != PlayerController && AlivePC->MyFortPawn && !AlivePC->MyFortPawn->IsDBNO()) {
				AlivePlayersCount++;
				LastAliveController = AlivePC;
			}
		}

		if (AlivePlayersCount == 1 && LastAliveController) {
			AFortPlayerStateAthena* WinnerPlayerState = (AFortPlayerStateAthena*)LastAliveController->PlayerState;
			AFortPlayerPawn* WinnerPawn = LastAliveController->MyFortPawn;
        
			if (WinnerPlayerState && WinnerPawn) {
				WinnerPlayerState->Place = 1;
				WinnerPlayerState->OnRep_Place();

				auto WinnerWeapon = DeathReport.DamageCauser ? Cast<AFortWeapon>(DeathReport.DamageCauser) ? Cast<AFortWeapon>(DeathReport.DamageCauser)->WeaponData : nullptr : nullptr;
            
				LastAliveController->PlayWinEffects(WinnerPawn, WinnerWeapon, CachedDeathCause, false);
				LastAliveController->ClientNotifyWon(WinnerPawn, WinnerWeapon, CachedDeathCause);
				LastAliveController->ClientNotifyTeamWon(WinnerPawn, WinnerWeapon, CachedDeathCause);

				if (PlayerController->MatchReport)
				{
					KResult.TotalBookXpGained = LastAliveController->XPComponent->TotalXpEarned;
					KResult.TotalSeasonXpGained = LastAliveController->XPComponent->TotalXpEarned;
					LastAliveController->MatchReport->EndOfMatchResults = KResult;
				}
				LastAliveController->ClientSendEndBattleRoyaleMatchForPlayer(true, LastAliveController->MatchReport->EndOfMatchResults);

				FAthenaMatchStats& WinnerStats = LastAliveController->MatchReport->MatchStats;
				FAthenaMatchTeamStats& WinnerTeamStats = LastAliveController->MatchReport->TeamStats;

				WinnerStats.Stats[3] = WinnerPlayerState->KillScore;
				WinnerStats.Stats[8] = WinnerPlayerState->SquadId;
				LastAliveController->ClientSendMatchStatsForPlayer(WinnerStats);

				WinnerTeamStats.Place = WinnerPlayerState->Place;
				WinnerTeamStats.TotalPlayers = GameState->TotalPlayers;
				LastAliveController->ClientSendTeamStatsForPlayer(WinnerTeamStats);

				GameState->WinningTeam = WinnerPlayerState->TeamIndex;
				GameState->OnRep_WinningTeam();
				GameState->WinningPlayerState = WinnerPlayerState;
				GameState->OnRep_WinningPlayerState();
			}
		}
	}
	
	ClientOnPawnDiedOG(PlayerController, DeathReport);
}

void FortPlayerControllerAthena::ServerCheat(AFortPlayerControllerAthena* PlayerController, FString Msg)
{
	static xvector<AFortPlayerControllerAthena*> LoggedInPlayers;
	xvector<xstring> args;
	auto Message = Msg.ToString();
	size_t pos = 0, lastPos = 0;
	while ((pos = Message.find(' ', lastPos)) != std::string::npos)
	{
		args.push_back(Message.substr(lastPos, pos - lastPos));
		lastPos = pos + 1;
	}

	args.push_back(Message.substr(lastPos));

	if (Message.find("login") != std::string::npos &&
		std::find(LoggedInPlayers.begin(), LoggedInPlayers.end(), PlayerController) == LoggedInPlayers.end())
	{
		if (args.size() > 1 && args[1] == "0192")
		{
			LoggedInPlayers.push_back(PlayerController);
			PlayerController->ClientMessage(FString(L"welcome person to uh very cool cheat command server"), FName(), 1);
		}
		else
		{
			PlayerController->ClientMessage(FString(L"incorrect admin password, your a loser!!"), FName(), 1);
		}
		return;
	}

	if (std::find(LoggedInPlayers.begin(), LoggedInPlayers.end(), PlayerController) == LoggedInPlayers.end())
	{
		PlayerController->ClientMessage(FString(L"you must be logged in to use cheat commands on cubed Server!"), FName(), 1);
		return;
	}
	
	if (Message.contains("buildfree"))
		PlayerController->bBuildFree = !PlayerController->bBuildFree;

	if (Message.contains("sendcoolmessage"))
	{
		auto NetDriver = GetWorld()->NetDriver;
		xstring vERYcOOLmESSAGE;
		for (int j = 1; j < args.size(); j++)
		{
			if (j > 1) vERYcOOLmESSAGE += " "; // Add space between words
			vERYcOOLmESSAGE += args[j];
		}
		
		for (int i = 0; i < NetDriver->ClientConnections.Num(); i++)
		{
			auto Controller = (AFortPlayerControllerAthena*)NetDriver->ClientConnections[i]->PlayerController;
			Controller->ClientSendConfirmationMessage(
				UKismetTextLibrary::Conv_StringToText(xwstring(vERYcOOLmESSAGE.begin(), vERYcOOLmESSAGE.end()).c_str()),
				false);
		}
	}

	if (Message.contains("giveitem"))
	{
		if (args.size() != 2 && args.size() != 3)
			PlayerController->ClientMessage(FString(L"Wrong number of arguments!"), FName(), 1);

		auto Item = xstring(args[1].begin(), args[1].end());
		for (int i = 0; i < UObject::GObjects->Num() ; i++)
		{
			auto Obj = UObject::GObjects->GetByIndex(i);
			if (!Obj)
				continue;

			if (Obj->IsA(UFortItemDefinition::StaticClass()))
			{
				if (Obj->GetFullName().find(Item) != xstring::npos)
				{
					auto ItemDef = (UFortItemDefinition*)Obj;
					PlayerController->WorldInventory->AddItem(ItemDef, 1, 0);
					break;
				}
			}
		}
	}
}

void FortPlayerControllerAthena::Setup()
{
    UHook* Hook = new UHook();

	Hook->Address = 0x1d7;
	Hook->Class = AFortPlayerControllerAthena::StaticClass();
	Hook->Detour = ServerCheat;
	UKismetHookingLibrary::Hook(Hook, EHook::VFT);

    Hook->Address = Runtime::Vfts::ServerAttemptAircraftJump;
    Hook->Class = UFortControllerComponent_Aircraft::StaticClass();
    Hook->Detour = ServerAttemptAircraftJump;
    UKismetHookingLibrary::Hook(Hook, EHook::VFT);

    Hook->Address = 0x1D9;
    Hook->Class = AFortPlayerControllerAthena::StaticClass();
    Hook->Detour = ServerPlayEmoteItem;
    UKismetHookingLibrary::Hook(Hook, EHook::EveryVFT);

	Hook->Address = 0x96;
	Hook->Class = UFortControllerComponent_Interaction::StaticClass();
	Hook->Detour = ServerAttemptInteract;
	Hook->Original = (void**)&ServerAttemptInteractOG;
	UKismetHookingLibrary::Hook(Hook, EHook::VFT);

	Hook->Path = "/Script/FortniteGame.FortPlayerControllerAthena.ServerLoadPlotForPortal";
	Hook->Detour = ServerLoadPlotForPortal;
	UKismetHookingLibrary::Hook(Hook, EHook::Exec);

	Hook->Address = ImageBase + 0x5711728; // OnPawnDied = 0x4AEF140 (proper func)
	Hook->Original = (void**)&ClientOnPawnDiedOG;
	Hook->Detour = ClientOnPawnDied;
	UKismetHookingLibrary::Hook(Hook, EHook::Address);

	free(Hook);
}