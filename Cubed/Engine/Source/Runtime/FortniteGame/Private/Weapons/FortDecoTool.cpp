#include "pch.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Weapons/FortDecoTool.h"

#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"

void FortDecoTool::SpawnDecoInternal(AFortDecoTool* DecoTool, FVector Location, FRotator Rotation, ABuildingSMActor* AttachedActor)
{
    auto ItemDefinition = Cast<UFortDecoItemDefinition>(DecoTool->ItemDefinition);
    
    if (auto ContextTrap = Cast<AFortDecoTool_ContextTrap>(DecoTool)) {
        auto& TrapDef = ContextTrap->ContextTrapItemDefinition;
        switch (AttachedActor->BuildingAttachmentType) {
        case EBuildingAttachmentType::ATTACH_Floor: case EBuildingAttachmentType::ATTACH_FloorAndStairs: ItemDefinition = TrapDef->FloorTrap; break;
        case EBuildingAttachmentType::ATTACH_CeilingAndStairs: case EBuildingAttachmentType::ATTACH_Ceiling: ItemDefinition = TrapDef->CeilingTrap; break;
        case EBuildingAttachmentType::ATTACH_Wall: ItemDefinition = TrapDef->WallTrap; break;
        case EBuildingAttachmentType::ATTACH_StairsBothSides: ItemDefinition = TrapDef->StairTrap; break;
        }
    }

    if (!ItemDefinition) return;

    auto NewTrap = GetWorld()->SpawnActor<ABuildingTrap>(ItemDefinition->BlueprintClass.Get(), Location, Rotation, AttachedActor);
    AttachedActor->AttachBuildingActorToMe(NewTrap, true);
    AttachedActor->bHiddenDueToTrapPlacement = ItemDefinition->bReplacesBuildingWhenPlaced;
    if (ItemDefinition->bReplacesBuildingWhenPlaced) AttachedActor->bActorEnableCollision = false;
    AttachedActor->ForceNetUpdate();
    NewTrap->OnBuildingActorInitialized(EFortBuildingInitializationReason::TrapTool, EFortBuildingPersistentState::New);
    
    auto Pawn = Cast<APawn>(DecoTool->Owner);
    if (!Pawn) return;
    
    auto PlayerController = Cast<AFortPlayerControllerAthena>(Pawn->Controller);
    if (!PlayerController) return;

    NewTrap->AttachedTo = AttachedActor;
    NewTrap->SetAttachedTo(AttachedActor);
    NewTrap->OnRep_AttachedTo();
    if (auto TrapData = Cast<UFortTrapItemDefinition>(ItemDefinition))
        NewTrap->TrapData = TrapData;

    NewTrap->InitializeKismetSpawnedBuildingActor(NewTrap, PlayerController, true, nullptr);
    NewTrap->OnPlaced();

    auto Entry = PlayerController->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry) {
        return entry.ItemDefinition == DecoTool->ItemDefinition;
    });
    if (!Entry) return;

    if (--Entry->Count <= 0)
        PlayerController->WorldInventory->Remove(Entry->ItemGuid);
    else
        PlayerController->WorldInventory->ReplaceEntry(*Entry);

    auto TeamIndex = Cast<AFortPlayerStateAthena>(PlayerController->PlayerState)->TeamIndex;
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
