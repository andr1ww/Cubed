#include "pch.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Components/FortControllerComponent_IndicatedActorManagement.h"

#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"

void FortControllerComponent_IndicatedActorManagement::Library::AddActorsToIndicatedList(UObject*, FFrame& Stack)
{
    AFortPlayerControllerAthena* InstigatingController;
    TArray<AActor*>* IndicatedActors;
    FIndicatedActorData IndicatedActorData;
    bool bAddAsUnique;
    bool bAllowOwningPlayer;
    Stack.StepCompiledIn(&InstigatingController);
    Stack.StepCompiledIn(&IndicatedActors);
    Stack.StepCompiledIn(&IndicatedActorData);
    Stack.StepCompiledIn(&bAddAsUnique);
    Stack.StepCompiledIn(&bAllowOwningPlayer);
    Stack.IncrementCode();

    if (!InstigatingController) return;

    auto Comp = (UFortControllerComponent_IndicatedActorManagement*)InstigatingController->GetComponentByClass(UFortControllerComponent_IndicatedActorManagement::StaticClass());
    if (!Comp) return;

    for (auto& Actor : *IndicatedActors)
    {
        if (!Actor) return;
        FIndicatedActorInfoEntry Entry{};

        Entry.Actor = Actor;
        Entry.Data = IndicatedActorData;
        Entry.EndTime = UGameplayStatics::GetTimeSeconds(GetWorld()) + 5.f;
        Entry.StartTime = UGameplayStatics::GetTimeSeconds(GetWorld());
        Entry.bRefreshExistingWhenAdded = true;

        Comp->IndicatedActorList.Entries.Add(Entry);
    }
}

void FortControllerComponent_IndicatedActorManagement::Library::AddActorsToStenciledList(UObject*, FFrame& Stack)
{
    AFortPlayerControllerAthena* InstigatingController;
    TArray<class AActor*>* StenciledActors;
    FStenciledActorData StenciledActorData;
    bool bAddAsUnique;
    Stack.StepCompiledIn(&InstigatingController);
    Stack.StepCompiledIn(&StenciledActors);
    Stack.StepCompiledIn(&StenciledActorData);
    Stack.StepCompiledIn(&bAddAsUnique);
    Stack.IncrementCode();
    if (!InstigatingController) return;

    auto Comp = (UFortControllerComponent_IndicatedActorManagement*)InstigatingController->GetComponentByClass(UFortControllerComponent_IndicatedActorManagement::StaticClass());
    if (!Comp) return;

    for (auto& Actor : *StenciledActors)
    {
        if (!Actor) return;
        FStenciledActorInfoEntry Entry{};

        Entry.Actor = Actor;
        Entry.Data = StenciledActorData;
        Entry.EndTime = UGameplayStatics::GetTimeSeconds(GetWorld()) + 5.f;
        Entry.StartTime = UGameplayStatics::GetTimeSeconds(GetWorld());
        Entry.bRefreshExistingWhenAdded = true;

        Comp->StenciledActorList.Entries.Add(Entry);
    }
}

void FortControllerComponent_IndicatedActorManagement::Setup()
{
    UHook* Hook = new UHook();

    Hook->Path = "/Script/FortniteGame.FortniteGame.FortIndicatedActorManagementLibrary.AddActorsToIndicatedList";
    Hook->Detour = Library::AddActorsToIndicatedList;
    UKismetHookingLibrary::Hook(Hook, EHook::Exec);
    
    Hook->Path = "/Script/FortniteGame.FortIndicatedActorManagementLibrary.AddActorsToStenciledList";
    Hook->Detour = Library::AddActorsToStenciledList;
    UKismetHookingLibrary::Hook(Hook, EHook::Exec);

    delete Hook;
}
