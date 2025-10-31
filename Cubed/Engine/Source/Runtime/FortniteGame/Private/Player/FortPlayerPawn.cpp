#include "pch.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Player/FortPlayerPawn.h"

#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Kismet/FortKismetLibrary.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Quests/FortQuestManager.h"

void FortPlayerPawn::ServerHandlePickupInfo(AFortPlayerPawn* Pawn, AFortPickup* Pickup, FFortPickupRequestInfo& Params)
{
    if (!Pickup || Pickup->bPickedUp)
        return;

    auto PlayerController = (AFortPlayerControllerAthena*)Pawn->GetController();
    
    if (!PlayerController || !Pickup->PrimaryPickupItemEntry.ItemDefinition)
        return;
    
    Pawn->IncomingPickups.Add(Pickup);
    
    Pickup->PickupLocationData.bPlayPickupSound = Params.bPlayPickupSound;
    Pickup->PickupLocationData.FlyTime = 0.40f;
    Pickup->PickupLocationData.ItemOwner = Pawn;
    Pickup->PickupLocationData.PickupGuid = Pickup->PrimaryPickupItemEntry.ItemGuid;
    Pickup->PickupLocationData.PickupTarget = Pawn;
    Pickup->PickupLocationData.StartDirection = (FVector_NetQuantizeNormal)Params.Direction;
    Pickup->OnRep_PickupLocationData();

    Pickup->bPickedUp = true;
    Pickup->OnRep_bPickedUp();
    return ServerHandlePickupInfoOG(Pawn, Pickup, Params);
}

void FortPlayerPawn::OnAboutToEnterBackpack(AFortPickup* Pickup)
{
    auto Pawn = Cast<AFortPlayerPawn>(Pickup->PickupLocationData.PickupTarget);
    if (!Pawn) return OnAboutToEnterBackpackOG(Pickup);
    auto PC = Cast<AFortPlayerControllerAthena>(Pawn->GetController());
    if (!PC) return OnAboutToEnterBackpackOG(Pickup);
    
    AFortAthenaAIBotController* AI = Cast<AFortAthenaAIBotController>(PC);
    AFortInventory* Inv = PC->WorldInventory;
    if (!Inv) return;
    
    auto MyPawn = AI ? Cast<AFortPlayerPawn>(AI->PlayerBotPawn) : PC->MyFortPawn;
    if (!MyPawn) return;
    
    auto& PickupEntry = Pickup->PrimaryPickupItemEntry;
    auto Def = (UFortWorldItemDefinition*)PickupEntry.ItemDefinition;
    auto MaxStack = Def->MaxStackSize.Evaluate();
    auto& Items = Inv->Inventory.ItemInstances;
    
    int Ammo = 0;
    if (auto WDef = Cast<UFortWeaponItemDefinition>(Def))
        if (auto Stats = WDef->GetStats()) Ammo = Stats->ClipSize;
    
    int ItemCount = 0;
    for (auto* I : Items)
        if (GetQuickbar(I->ItemEntry.ItemDefinition) == EFortQuickBars::Primary && 
            I->ItemEntry.ItemDefinition->bInventorySizeLimited)
            ItemCount++;
    
    bool bShouldSwap = (ItemCount - 6) >= 5;
    bool bIsStackable = Def->IsStackable();
    
    if (bIsStackable) {
        UFortWorldItem* ExistingItem = nullptr;
        for (auto* I : Items)
            if (I && I->ItemEntry.ItemDefinition == Def && I->ItemEntry.Count <= MaxStack) 
            { ExistingItem = I; break; }
        
        if (ExistingItem) {
            auto& Entry = ExistingItem->ItemEntry;
            FFortItemEntryStateValue* State = nullptr;
            for (auto& Value : Entry.StateValues)
                if (Value.StateType == EFortItemEntryState::ShouldShowItemToast) 
                { State = &Value; break; }
            
            if (!State) {
                FFortItemEntryStateValue Value{};
                Value.StateType = EFortItemEntryState::ShouldShowItemToast;
                Value.IntValue = true;
                Entry.StateValues.Add(Value);
            } else State->IntValue = true;
            
            int NewCount = Entry.Count + PickupEntry.Count;
            if (NewCount > MaxStack) {
                Entry.Count = MaxStack;
                int Overflow = NewCount - MaxStack;
                if (Def->bAllowMultipleStacks && ItemCount < 5)
                    Inv->AddItem(Def, Overflow, Ammo, PickupEntry.Level);
                else
                    FortKismetLibrary::SpawnPickup(PC->GetViewTarget()->K2_GetActorLocation(), PickupEntry, 
                        EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, MyPawn, Overflow, true, false, true);
            } else Entry.Count = NewCount;
            Inv->ReplaceEntry(Entry);
        } else {
            if (PickupEntry.Count > MaxStack) {
                int Overflow = PickupEntry.Count - MaxStack;
                if (ItemCount == 5 && GetQuickbar(Def) == EFortQuickBars::Primary) {
                    auto CW = MyPawn->CurrentWeapon;
                    if (CW && GetQuickbar(CW->GetWeaponData()) == EFortQuickBars::Primary) {
                        FGuid CWGuid = CW->ItemEntryGuid;
                        for (auto* I : Items)
                            if (I->ItemEntry.ItemGuid == CWGuid) {
                                auto& CWEntry = I->ItemEntry;
                                FortKismetLibrary::SpawnPickup(MyPawn->K2_GetActorLocation() + MyPawn->GetActorForwardVector() * 70.f + FVector(0,0,50), 
                                    CWEntry, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, MyPawn, CWEntry.Count, true, false, true);
                                Inv->Remove(CWGuid);
                                break;
                            }
                    } else {
                        FortKismetLibrary::SpawnPickup(PC->GetViewTarget()->K2_GetActorLocation(), PickupEntry, 
                            EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, MyPawn, MaxStack, true, false, true);
                        OnAboutToEnterBackpackOG(Pickup);
                        return;
                    }
                    Inv->AddItem(Def, MaxStack, Ammo, PickupEntry.Level);
                } else Inv->AddItem(Def, MaxStack, Ammo, PickupEntry.Level);
                
                if (Def->bAllowMultipleStacks && ItemCount < 5)
                    Inv->AddItem(Def, Overflow, Ammo, PickupEntry.Level);
                else
                    FortKismetLibrary::SpawnPickup(PC->GetViewTarget()->K2_GetActorLocation(), PickupEntry, 
                        EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, MyPawn, Overflow, true, false, true);
            } else {
                if (bShouldSwap && GetQuickbar(Def) == EFortQuickBars::Primary) {
                    auto CW = MyPawn->CurrentWeapon;
                    if (CW && GetQuickbar(CW->GetWeaponData()) == EFortQuickBars::Primary) {
                        FGuid CWGuid = CW->ItemEntryGuid;
                        for (auto* I : Items)
                            if (I->ItemEntry.ItemGuid == CWGuid) {
                                auto& CWEntry = I->ItemEntry;
                                FortKismetLibrary::SpawnPickup(MyPawn->K2_GetActorLocation() + MyPawn->GetActorForwardVector() * 70.f + FVector(0,0,50), 
                                    CWEntry, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, MyPawn, CWEntry.Count, true, false, true);
                                Inv->Remove(CWGuid);
                                break;
                            }
                        Inv->AddItem(Def, PickupEntry.Count, Ammo, PickupEntry.Level);
                    } else FortKismetLibrary::SpawnPickup(PC->GetViewTarget()->K2_GetActorLocation(), PickupEntry, 
                        EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, MyPawn, MaxStack, true, false, true);
                } else Inv->AddItem(Def, PickupEntry.Count, Ammo, PickupEntry.Level);
            }
        }
    } else {
        if (ItemCount == 5 && GetQuickbar(Def) == EFortQuickBars::Primary) {
            auto CW = MyPawn->CurrentWeapon;
            if (CW && GetQuickbar(CW->GetWeaponData()) == EFortQuickBars::Primary) {
                FGuid CWGuid = CW->ItemEntryGuid;
                for (auto* I : Items)
                    if (I->ItemEntry.ItemGuid == CWGuid) {
                        auto& CWEntry = I->ItemEntry;
                        FortKismetLibrary::SpawnPickup(MyPawn->K2_GetActorLocation() + MyPawn->GetActorForwardVector() * 70.f + FVector(0,0,50), 
                            CWEntry, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, MyPawn, CWEntry.Count, true, false, true);
                        Inv->Remove(CWGuid);
                        break;
                    }
                Inv->AddItem(Def, PickupEntry.Count, Ammo, PickupEntry.Level);
            } else FortKismetLibrary::SpawnPickup(PC->GetViewTarget()->K2_GetActorLocation(), PickupEntry, 
                EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, MyPawn, MaxStack, true, false, true);
        } else Inv->AddItem(Def, PickupEntry.Count, Ammo, PickupEntry.Level);
    }
    
    OnAboutToEnterBackpackOG(Pickup);
}

void FortPlayerPawn::MovingEmoteStopped(AFortPawn* Pawn, FFrame& Stack)
{
    Pawn->bMovingEmote = false;
    Pawn->bMovingEmoteForwardOnly = false;
    return MovingEmoteStoppedOG(Pawn, Stack);
}

void FortPlayerPawn::ServerSendZiplineState(AFortPlayerPawn* Pawn, FFrame& Stack)
{
    FZiplinePawnState State;
    Stack.StepCompiledIn(&State);
    Stack.IncrementCode();
    Pawn->ZiplineState = State;
    reinterpret_cast<void(*)(AFortPlayerPawn*)>(ImageBase + 0x51EEBEC)(Pawn); // OnRep_ZiplineState
    
    if (State.bJumped)
    {
        {
            auto Velocity = Pawn->CharacterMovement->Velocity;
            auto VelocityX = Velocity.X * -0.5f;
            auto VelocityY = Velocity.Y * -0.5f;
            Pawn->LaunchCharacterJump(FVector{ VelocityX >= -750 ? fminf(VelocityX, 750) : -750, VelocityY >= -750 ? fminf(VelocityY, 750) : -750, 1200 },
                false, false, true, true);
        }
    }
}

void FortPlayerPawn::OnRep_IsInAnyStorm(AFortPlayerPawn* Pawn) {
    Pawn->bIsInsideSafeZone = !Pawn->bIsInAnyStorm;
    Pawn->OnRep_IsInsideSafeZone();
    return OnRep_IsInAnyStormOG(Pawn);
}

void FortPlayerPawn::EndSkydiving(AFortPlayerPawn* Pawn)
{
    EndSkydivingOG(Pawn);   
    auto Controller = (AFortPlayerControllerAthena*)Pawn->GetController();
    if (Controller->IsA(AFortAthenaAIBotController::StaticClass())) return;
    
    auto QuestManager = Controller ? Controller->GetQuestManager(ESubGame::Athena) : nullptr;
    if (!QuestManager) return;
    FGameplayTagContainer ContextTags;
    FGameplayTagContainer TargetTags;
    FGameplayTagContainer SourceTags;
    QuestManager->GetSourceAndContextTags(&SourceTags, &ContextTags);
    
    FortQuestManager::SendStatEventWithTags(QuestManager, EFortQuestObjectiveStatEvent::Land, NULL, TargetTags, SourceTags,
                          ContextTags, 1);
}

void FortPlayerPawn::UpdatePlayerDistanceTraveled(AFortPlayerPawn* Pawn, __int64 a2)
{
    if (!Pawn) return;
    UpdatePlayerDistanceTraveledOG(Pawn, a2);
    auto Controller = (AFortPlayerControllerAthena*)Pawn->GetController();
    if (!IsValidPointer(Controller)) return;
    auto QuestManager = Controller ? Controller->GetQuestManager(ESubGame::Athena) : nullptr;
    if (!QuestManager) return;
    FGameplayTagContainer ContextTags;
    FGameplayTagContainer TargetTags;
    FGameplayTagContainer SourceTags;
    QuestManager->GetSourceAndContextTags(&SourceTags, &ContextTags);
    
    FortQuestManager::SendStatEventWithTags(QuestManager, EFortQuestObjectiveStatEvent::DistanceTraveled, NULL, TargetTags, SourceTags,
                          ContextTags, 1 /* idk what to put here yet */);
}

void FortPlayerPawn::ServerReviveFromDBNO(AFortPlayerPawn* Pawn, AFortPlayerController* EventInstigator)
{
	if (!Pawn || !EventInstigator) return;

	auto PlayerState = reinterpret_cast<AFortPlayerStateAthena*>(Pawn->PlayerState);
	auto Controller = reinterpret_cast<AFortPlayerControllerAthena*>(Pawn->Controller);

	if (!PlayerState) return;

	static UClass* Class = UGAB_AthenaDBNO_C::StaticClass();

	if (!Class) return;

	FGameplayEventData EventData{};
	EventData.EventTag = Pawn->EventReviveTag; 
	EventData.ContextHandle = PlayerState->AbilitySystemComponent->MakeEffectContext();
	EventData.Instigator = EventInstigator;
	EventData.InstigatorTags = ((AFortPlayerPawnAthena*)EventInstigator->Pawn)->GameplayTags;
	EventData.Target = Pawn;
	EventData.TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromActor(Pawn);
	EventData.TargetTags = Pawn->GameplayTags;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Pawn, Pawn->EventReviveTag, EventData);

	for (auto& Item : PlayerState->AbilitySystemComponent->ActivatableAbilities.Items)
	{
		if (!Item.Ability) continue; 
		if (Item.Ability->Class == Class)
		{
			PlayerState->AbilitySystemComponent->ClientCancelAbility(Item.Handle, Item.ActivationInfo);
			PlayerState->AbilitySystemComponent->ClientEndAbility(Item.Handle, Item.ActivationInfo);
			PlayerState->AbilitySystemComponent->ServerEndAbility(Item.Handle, Item.ActivationInfo, Item.ActivationInfo.PredictionKeyWhenActivated);
			PlayerState->AbilitySystemComponent->ServerCancelAbility(Item.Handle, Item.ActivationInfo);
		}
	}
    
	Pawn->bIsDBNO = false;
	Pawn->OnRep_IsDBNO();
	Pawn->SetHealth(30);
	PlayerState->DeathInfo = {};
	PlayerState->OnRep_DeathInfo();
	Controller->ClientOnPawnRevived(EventInstigator);
}

void FortPlayerPawn::Setup()
{
    UHook* Hook = new UHook();

    Hook->Address = ImageBase + 0x202F5A0;
    Hook->Original = (void**)&OnRep_IsInAnyStormOG;
    Hook->Detour = OnRep_IsInAnyStorm;
    UKismetHookingLibrary::Hook(Hook, EHook::Address);

    Hook->Address = 0x20F;
    Hook->Original = (void**)&ServerHandlePickupInfoOG;
    Hook->Class = AFortPlayerPawnAthena::StaticClass();
    Hook->Detour = ServerHandlePickupInfo;
    UKismetHookingLibrary::Hook(Hook, EHook::VFT);

    Hook->Address = 0x1F3;
    Hook->Class = AFortPlayerPawn::StaticClass();
    Hook->Detour = ServerReviveFromDBNO;
    UKismetHookingLibrary::Hook(Hook, EveryVFT);

    Hook->Address = ImageBase + 0x1B3426C;
    Hook->Detour = OnAboutToEnterBackpack;
    Hook->Original = (void**)&OnAboutToEnterBackpackOG;
    UKismetHookingLibrary::Hook(Hook, EHook::Address);

    Hook->Path = "/Script/FortniteGame.FortPawn.MovingEmoteStopped";
    Hook->Original = reinterpret_cast<void**>(&MovingEmoteStoppedOG);
    Hook->Detour = MovingEmoteStopped;
    UKismetHookingLibrary::Hook(Hook, EHook::Exec);

    Hook->Path = "/Script/FortniteGame.FortPlayerPawn.ServerSendZiplineState";
    Hook->Detour = ServerSendZiplineState;
    UKismetHookingLibrary::Hook(Hook, EHook::Exec);

    Hook->Address = ImageBase + 0x1890154;
    Hook->Original = (void**)&EndSkydivingOG;
    Hook->Detour = EndSkydiving;
    UKismetHookingLibrary::Hook(Hook, EHook::Address);

    Hook->Address = ImageBase + 0x51FCFFC;
    Hook->Original = (void**)&UpdatePlayerDistanceTraveledOG;
    Hook->Detour = UpdatePlayerDistanceTraveled;
    UKismetHookingLibrary::Hook(Hook, EHook::Address);
    
    delete Hook;
}