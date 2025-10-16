#include "pch.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Building/BuildingSMActor.h"

#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Quests/FortQuestManager.h"

bool BuildingSMActor::AttemptSpawnResources(ABuildingSMActor* BuildingSMActor, AFortPlayerPawn* Pawn, float DamageDealt, bool WeakSpot)
{
    static UCurveTable* AthenaResourceRates = StaticLoadObject<UCurveTable>("/Game/Athena/Balance/DataTables/AthenaResourceRates.AthenaResourceRates");

    EEvaluateCurveTableResult STFU;
    float Result;
    UDataTableFunctionLibrary::EvaluateCurveTableRow(AthenaResourceRates, BuildingSMActor->BuildingResourceAmountOverride.RowName, 0, &STFU, &Result, FString());
 
    int ResourceCount = (int)round(Result / (BuildingSMActor->GetMaxHealth() / DamageDealt));
    
    if (ResourceCount == 0)
        return false;
    
    UFortResourceItemDefinition* ItemDefinition = UFortKismetLibrary::K2_GetResourceItemDefinition(BuildingSMActor->ResourceType);

    if (!ItemDefinition)
        return false;

    auto Controller = (AFortPlayerController*)Pawn->GetController();
    auto ItemEntry = Controller->WorldInventory->Inventory.ReplicatedEntries.Search([ItemDefinition](FFortItemEntry &entry)
    {
        return entry.ItemDefinition == ItemDefinition;
    });
    
    if (ItemEntry)
    {
        ItemEntry->Count += ResourceCount;
        Controller->WorldInventory->ReplaceEntry(*ItemEntry);
    } else
    {
        Controller->WorldInventory->AddItem(ItemDefinition, ResourceCount, 0);
    }

    Controller->ClientReportDamagedResourceBuilding(BuildingSMActor, BuildingSMActor->ResourceType, ResourceCount, BuildingSMActor->GetHealth() - DamageDealt <= 0, WeakSpot);

    auto QuestManager = Controller ? Controller->GetQuestManager(ESubGame::Athena) : nullptr;
    if (!QuestManager) return true;
    FGameplayTagContainer ContextTags;
    FGameplayTagContainer TargetTags;
    FGameplayTagContainer SourceTags;
    QuestManager->GetSourceAndContextTags(&SourceTags, &ContextTags);
    TargetTags.AppendTags(BuildingSMActor->StaticGameplayTags);
    TargetTags.AppendTags(BuildingSMActor->ConstTags);
    
    FortQuestManager::SendStatEventWithTags(QuestManager, EFortQuestObjectiveStatEvent::Damage, BuildingSMActor, TargetTags, SourceTags,
                          ContextTags, ResourceCount /* prob wrong but wtv */);
    return true;
}

void BuildingSMActor::Setup()
{
    UHook* Hook = new UHook();

    Hook->Address = 0x16B;
    Hook->Class = ABuildingSMActor::StaticClass();
    Hook->Detour = AttemptSpawnResources;
    UKismetHookingLibrary::Hook(Hook, EHook::EveryVFT);

    free(Hook);
}