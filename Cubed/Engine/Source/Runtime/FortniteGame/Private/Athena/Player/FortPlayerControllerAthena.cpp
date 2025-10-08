#include "pch.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Athena/Player/FortPlayerControllerAthena.h"

#include "Offsets.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"

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
}