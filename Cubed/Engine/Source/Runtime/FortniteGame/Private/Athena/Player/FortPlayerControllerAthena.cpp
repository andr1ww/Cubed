#include "pch.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Athena/Player/FortPlayerControllerAthena.h"

#include "Offsets.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"

void FortPlayerControllerAthena::ServerAttemptAircraftJump(UFortControllerComponent_Aircraft* Comp, FRotator ClientRot)
{
    auto Controller = (AFortPlayerControllerAthena*)Comp->GetOwner();
    auto GameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;

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
                if (Mutator->InventoryLoadouts.Num() == 0) return;
                
                const auto& Loadout = Mutator->InventoryLoadouts[std::rand() % Mutator->InventoryLoadouts.Num()];
                static const FName ExcludedRow = UKismetStringLibrary::Conv_StringToName(L"BlueCheese.Evergreen.Solo.AllLoadouts.NutsBolts");
                
                for (auto& ItemAndCount : Loadout.LoadoutList)
                {
                    if (ItemAndCount.Item && ItemAndCount.Count.Curve.RowName != ExcludedRow)
                    {
                        int32 Ammo = 0;
                        if (auto* WeaponDef = Cast<UFortWeaponItemDefinition>(ItemAndCount.Item))
                            if (WeaponDef->GetStats())
                                Ammo = WeaponDef->GetStats()->ClipSize;
                        
                        Controller->WorldInventory->AddItem(ItemAndCount.Item, ItemAndCount.Count.Evaluate(), Ammo);
                    }
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

void FortPlayerControllerAthena::Setup()
{
    UHook* Hook = new UHook();

    Hook->Address = Runtime::Vfts::ServerAttemptAircraftJump;
    Hook->Class = UFortControllerComponent_Aircraft::StaticClass();
    Hook->Detour = ServerAttemptAircraftJump;
    UKismetHookingLibrary::Hook(Hook, EHook::VFT);
}