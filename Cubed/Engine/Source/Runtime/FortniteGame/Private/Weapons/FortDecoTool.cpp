#include "pch.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Weapons/FortDecoTool.h"

#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"

void FortDecoTool::SpawnDecoInternal(AFortDecoTool* DecoTool, FVector Location, FRotator Rotation, ABuildingSMActor* AttachedActor)
{
    auto ItemDefinition = Cast<UFortDecoItemDefinition>(DecoTool->ItemDefinition);
    
    if (auto ContextTrap = Cast<AFortDecoTool_ContextTrap>(DecoTool)) {
        auto& TrapDef = ContextTrap->ContextTrapItemDefinition;
        switch ((int)AttachedActor->BuildingAttachmentType) {
        case 0: case 6: ItemDefinition = TrapDef->FloorTrap; break;
        case 7: case 2: ItemDefinition = TrapDef->CeilingTrap; break;
        case 1: ItemDefinition = TrapDef->WallTrap; break;
        case 8: ItemDefinition = TrapDef->StairTrap; break;
        }
    }

    if (!ItemDefinition) return;

    auto NewTrap = GetWorld()->SpawnActor<ABuildingActor>(ItemDefinition->BlueprintClass.Get(), Location, Rotation, AttachedActor);
    AttachedActor->AttachBuildingActorToMe(NewTrap, true);
    AttachedActor->bHiddenDueToTrapPlacement = ItemDefinition->bReplacesBuildingWhenPlaced;
    if (ItemDefinition->bReplacesBuildingWhenPlaced) AttachedActor->bActorEnableCollision = false;
    AttachedActor->ForceNetUpdate();

    auto Pawn = (APawn*)DecoTool->Owner;
    if (!Pawn) return;
    auto PlayerController = (AFortPlayerControllerAthena*)Pawn->Controller;
    if (!PlayerController) return;

    auto Entry = PlayerController->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry) {
        return entry.ItemDefinition == DecoTool->ItemDefinition;
    });
    if (!Entry) return;

    if (--Entry->Count <= 0)
        PlayerController->WorldInventory->Remove(Entry->ItemGuid);
    else
        PlayerController->WorldInventory->ReplaceEntry(*Entry);

    auto TeamIndex = ((AFortPlayerStateAthena*)PlayerController->PlayerState)->TeamIndex;
    if (NewTrap->TeamIndex != TeamIndex) {
        NewTrap->TeamIndex = TeamIndex;
        NewTrap->Team = EFortTeam(TeamIndex);
        NewTrap->SetTeam(TeamIndex);
    }
}

void FortDecoTool::Setup()
{
    UHook* Hook = new UHook();

    Hook->Address = 0x168;
    Hook->Class = AFortDecoTool::StaticClass();
    Hook->Detour = SpawnDecoInternal;
    UKismetHookingLibrary::Hook(Hook, EveryVFT);

    delete Hook;
}
