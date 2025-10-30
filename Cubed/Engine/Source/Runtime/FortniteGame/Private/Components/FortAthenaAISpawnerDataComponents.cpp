#include "pch.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Components/FortAthenaAISpawnerDataComponents.h"

#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"

void FortAthenaAISpawnerDataComponents::OnPawnAISpawned(AFortAthenaAIBotController* C, AFortPlayerPawn* Pawn)
{
    auto Controller = Cast<AFortAthenaAIBotController>(Pawn->GetController());
    *reinterpret_cast<bool*>(__int64(Controller->CachedAIServicePlayerBots) + 0x688) = true; //bCanActivateBrain
    OnPawnAISpawnedOG(C, Pawn);

    static xvector<UAthenaCharacterItemDefinition*> Characters;
    static xvector<UAthenaDanceItemDefinition*> Dances;
    if (Characters.size() == 0)
    {
        Characters = GetObjectsOfClass<UAthenaCharacterItemDefinition>();
        Dances = GetObjectsOfClass<UAthenaDanceItemDefinition>();
    }

    for (auto& Dance : Dances) {
        if (IsValidPointer(Dance)) 
            if (!Dance->IsA(UAthenaSprayItemDefinition::StaticClass())) Controller->CosmeticLoadoutBC.Dances.Add(Dance);
    }

    static UClass* PhoebeClass = StaticLoadObject<UClass>("/Game/Athena/AI/Phoebe/BP_PhoebePlayerController.BP_PhoebePlayerController_C");
    
    if (Pawn->Controller->Class == PhoebeClass)
    {
        auto PlayerState = Cast<AFortPlayerStateAthena>(Controller->PlayerState);
        if (Characters.size() != 0) {
            UAthenaCharacterItemDefinition* CID = nullptr;
            while (!IsValidPointer(CID)) {
                auto ok = Characters[UKismetMathLibrary::RandomIntegerInRange(0, Characters.size() - 1)];
                if (IsValidPointer(ok))
                    if (ok->BaseCharacterParts.Num() >= 1) CID = ok;
            }
        
            if (CID)
            {
                for (size_t i = 0; i < CID->BaseCharacterParts.Num(); i++)
                {
                    UCustomCharacterPart* Part = CID->BaseCharacterParts[i].Get();
                    if (Part)
                    {
                        PlayerState->CharacterData.Parts[(uintptr_t)Part->CharacterPartType] = Part;
                        Pawn->CharacterParts[(uintptr_t)Part->CharacterPartType] = Part;
                        Pawn->ServerChoosePart(Part->CharacterPartType, Part);
                    } 
                }

                PlayerState->OnRep_CharacterData();
            }
        }
    } else
    {
	    UFortKismetLibrary::UpdatePlayerCustomCharacterPartsVisualization((AFortPlayerState*)Controller->PlayerState);
    }
}

void FortAthenaAISpawnerDataComponents::OnSpawned_InventoryBase(UFortAthenaAISpawnerDataComponent_AIBotInventory* Base, AFortPlayerPawn* Pawn)
{
	auto Controller = (AFortAthenaAIBotController*)Pawn->Controller;

	if (!Controller->Inventory)
	{
		if (Controller->Inventory = GetWorld()->SpawnActor<AFortInventory>(AFortInventory::StaticClass(), {}, {}, Controller))
		{
			Controller->Inventory->Owner = Controller;
			Controller->Inventory->OnRep_Owner();
		}
	}
	
	for (auto& Item : Base->Items)
	{
		UFortWorldItem* WorldItem = Cast<UFortWorldItem>(Item.Item->CreateTemporaryItemInstanceBP(Item.Count, 0));
		WorldItem->OwnerInventory = Controller->Inventory;
		FFortItemEntry& Entry = WorldItem->ItemEntry;
		Entry.LoadedAmmo = 9999;
		Controller->Inventory->Inventory.ReplicatedEntries.Add(Entry);
		Controller->Inventory->Inventory.ItemInstances.Add(WorldItem);
		Controller->Inventory->Inventory.MarkItemDirty(Entry);
		Controller->Inventory->HandleInventoryLocalUpdate();
		if (Cast<UFortWeaponRangedItemDefinition>(Entry.ItemDefinition))
			Controller->EquipWeapon(WorldItem);
	}

	AFortWeapon* CurrentWeapon = ((AFortPlayerPawn*)Controller->Pawn)->CurrentWeapon;
	Controller->CacheWeaponUsedToCalculateType = CurrentWeapon;
	Controller->CachedWeaponUsedToCalculateProjectileData = CurrentWeapon;
}

void FortAthenaAISpawnerDataComponents::OnSpawned_CosmeticLoadout(UFortAthenaAISpawnerDataComponent_CosmeticLoadout* Base, AFortPlayerPawn* Pawn)
{
	((void (*)(APlayerState*, APawn*))(ImageBase + 0x526E7D4))(Pawn->PlayerState, Pawn);
}

void FortAthenaAISpawnerDataComponents::OnSpawned_Conversation(UFortAthenaAISpawnerDataComponent_AIBotConversation* AIBotConversation, AFortAIPawn* Pawn)
{
		auto Controller = (AFortAthenaAIBotController*)Pawn->Controller;

	UFortNPCConversationParticipantComponent* NPCConversationParticipantComponent = (UFortNPCConversationParticipantComponent*)Controller->GetComponentByClass(AIBotConversation->ConversationComponentClass);

	if (!NPCConversationParticipantComponent)
		NPCConversationParticipantComponent = (UFortNPCConversationParticipantComponent*)Controller->AddComponentByClass(AIBotConversation->ConversationComponentClass, false, FTransform(), false);

	NPCConversationParticipantComponent->PlayerPawnOwner = (AFortPlayerPawn*)Pawn;
	NPCConversationParticipantComponent->BotControllerOwner = Controller;

	NPCConversationParticipantComponent->ConversationEntryTag = AIBotConversation->ConversationEntryTag;
	NPCConversationParticipantComponent->CharacterData = AIBotConversation->CharacterData;

	if (NPCConversationParticipantComponent->ServiceProviderIDTag.TagName == FName(0))
		NPCConversationParticipantComponent->ServiceProviderIDTag.TagName = NPCConversationParticipantComponent->ConversationEntryTag.TagName;

	if (NPCConversationParticipantComponent->SalesInventory)
	{
		for (const auto& [RowName, RowPtr] : NPCConversationParticipantComponent->SalesInventory->RowMap)
		{
			FNPCSaleInventoryRow* Row = (FNPCSaleInventoryRow*)RowPtr;

			if (!RowName.IsValid() || !Row)
				continue;

			if (Row->NPC.TagName == NPCConversationParticipantComponent->ServiceProviderIDTag.TagName)
			{
				NPCConversationParticipantComponent->SupportedSales.Add(*Row);
			}
		}
	}

	if (NPCConversationParticipantComponent->Services)
	{
		for (const auto& [RowName, RowPtr] : NPCConversationParticipantComponent->Services->RowMap)
		{
			FNPCDynamicServiceRow* Row = (FNPCDynamicServiceRow*)RowPtr;

			if (!RowName.IsValid() || !Row)
				continue;

			if (Row->NPC == NPCConversationParticipantComponent->ServiceProviderIDTag)
			{
				NPCConversationParticipantComponent->SupportedServices.GameplayTags.Add(Row->ServiceTag);
			}
		}
	}

	NPCConversationParticipantComponent->bCanStartConversation = true;
	NPCConversationParticipantComponent->OnRep_CanStartConversation();
}

void FortAthenaAISpawnerDataComponents::Setup()
{
    UHook* Hook = new UHook();

    Hook->Address = ImageBase + 0x47E939C; 
    Hook->Detour = OnPawnAISpawned;
    Hook->Original = (void**)&OnPawnAISpawnedOG;
    UKismetHookingLibrary::Hook(Hook, Address);
    
    Hook->Address = ImageBase + 0x129ED4C;
    UKismetHookingLibrary::Hook(Hook, RTrue);

	Hook->Address = 0x5a;
	Hook->Detour = OnSpawned_Conversation;
	Hook->Class = UFortAthenaAISpawnerDataComponent_ConversationBase::StaticClass();
	UKismetHookingLibrary::Hook(Hook, EveryVFT);

	Hook->Address = 0x5a;
	Hook->Detour = OnSpawned_InventoryBase;
	Hook->Class = UFortAthenaAISpawnerDataComponent_InventoryBase::StaticClass();
	UKismetHookingLibrary::Hook(Hook, EveryVFT);

	Hook->Address = 0x5a;
	Hook->Detour = OnSpawned_CosmeticLoadout;
	Hook->Class = UFortAthenaAISpawnerDataComponent_CosmeticBase::StaticClass();
	UKismetHookingLibrary::Hook(Hook, EveryVFT);

    delete Hook;
}
