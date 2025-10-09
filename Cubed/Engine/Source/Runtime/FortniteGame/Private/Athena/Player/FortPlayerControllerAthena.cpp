#include "pch.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Athena/Player/FortPlayerControllerAthena.h"

#include "Offsets.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Kismet/FortKismetLibrary.h"

void FortPlayerControllerAthena::ServerAttemptAircraftJump(UFortControllerComponent_Aircraft* Comp, FRotator ClientRot)
{
    auto Controller = (AFortPlayerControllerAthena*)Comp->GetOwner();
    auto GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;

    for (int32 i = 0; i < Controller->WorldInventory->Inventory.ItemInstances.Num(); i++)
    {
        if (Controller->WorldInventory->Inventory.ItemInstances.IsValidIndex(i))
        {
            auto& Item = Controller->WorldInventory->Inventory.ItemInstances[i];
            if (Item->CanBeDropped())
                Controller->WorldInventory->Remove(Item->ItemEntry.ItemGuid);
        }
    }
    
    GameMode->RestartPlayer(Controller);
    if (Controller->MyFortPawn)
    {
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
				FortKismetLibrary::SpawnPickup(Location, &entry, 
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
	}
	
	if (!bRespawningAllowed && 
		(PlayerController->MyFortPawn ? !PlayerController->MyFortPawn->IsDBNO() : true))
	{
		((void (*)(AFortGameModeAthena*, AFortPlayerController*, APlayerState*, AFortPawn*, UFortWeaponItemDefinition*, EDeathCause, char))(ImageBase + 0x4A81A2C))
			(GameMode, PlayerController, KillerPlayerState, KillerPawn, 
			 DeathReport.DamageCauser ? Cast<AFortWeapon>(DeathReport.DamageCauser) ? Cast<AFortWeapon>(DeathReport.DamageCauser)->WeaponData : nullptr : nullptr, 
			 PlayerState->DeathInfo.DeathCause, 0);

		FAthenaRewardResult KResult = PlayerController->MatchReport->EndOfMatchResults;

		if (PlayerController->MatchReport)
		{
			KResult.TotalBookXpGained = PlayerController->XPComponent->TotalXpEarned;
			KResult.TotalSeasonXpGained = PlayerController->XPComponent->TotalXpEarned;
			PlayerController->MatchReport->EndOfMatchResults = KResult;
			PlayerController->ClientSendEndBattleRoyaleMatchForPlayer(true, PlayerController->MatchReport->EndOfMatchResults);

			PlayerState->Place = GameState->PlayersLeft;
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

				auto WinnerWeapon = DeathReport.DamageCauser->Index ? Cast<AFortWeapon>(DeathReport.DamageCauser)->WeaponData : nullptr;
            
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

void FortPlayerControllerAthena::Setup()
{
    UHook* Hook = new UHook();

    Hook->Address = Runtime::Vfts::ServerAttemptAircraftJump;
    Hook->Class = UFortControllerComponent_Aircraft::StaticClass();
    Hook->Detour = ServerAttemptAircraftJump;
    UKismetHookingLibrary::Hook(Hook, EHook::VFT);

    Hook->Address = 0x1D9;
    Hook->Class = AFortPlayerControllerAthena::StaticClass();
    Hook->Detour = ServerPlayEmoteItem;
    UKismetHookingLibrary::Hook(Hook, EHook::EveryVFT);

	Hook->Address = ImageBase + 0x5711728;
	Hook->Original = (void**)&ClientOnPawnDiedOG;
	Hook->Detour = ClientOnPawnDied;
	UKismetHookingLibrary::Hook(Hook, EHook::Address);
}