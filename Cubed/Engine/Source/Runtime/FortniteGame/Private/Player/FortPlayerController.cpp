#include "pch.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Player/FortPlayerController.h"

#include "Logging.h"
#include "Offsets.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/GameplayAbilities/Public/AbilitySystemComponent.h"

void FortPlayerController::ServerAcknowledgePossession(AFortPlayerControllerAthena* Controller, APawn* Pawn)
{
    auto PlayerState = (AFortPlayerStateZone*)Controller->PlayerState;
    Controller->AcknowledgedPawn = Pawn;

    PlayerState->HeroType = Controller->CosmeticLoadoutPC.Character->HeroDefinition;
    UFortKismetLibrary::UpdatePlayerCustomCharacterPartsVisualization(PlayerState);
    
    TScriptInterface<IAbilitySystemInterface> AbilitySystemInterfaceActor{};
    AbilitySystemInterfaceActor.ObjectPointer = PlayerState;
    AbilitySystemInterfaceActor.InterfacePointer = PlayerState->GetInterfaceAddress<IAbilitySystemInterface>();
    
    static auto AbilitySet = GetAssetManager()->GameDataBR->PlayerAbilitySetBR.Get();
    UFortKismetLibrary::EquipFortAbilitySet(AbilitySystemInterfaceActor, AbilitySet, nullptr);
    
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
}
