#include "pch.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Components/FortControllerComponent_IndicatedActorManagement.h"

#include "Globals.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"

void FortControllerComponent_IndicatedActorManagement::Library::AddActorsToIndicatedList(UObject*, FFrame& Stack)
{
    AFortPlayerControllerAthena* InstigatingController;
    TArray<AActor*>& IndicatedActors = Stack.StepCompiledInRef<TArray<AActor*>>();
    FIndicatedActorData IndicatedActorData;
    bool bAddAsUnique;
    bool bAllowOwningPlayer;
    
    Stack.StepCompiledIn(&InstigatingController);
    Stack.StepCompiledIn(&IndicatedActorData);
    Stack.StepCompiledIn(&bAddAsUnique);
    Stack.StepCompiledIn(&bAllowOwningPlayer);
    Stack.IncrementCode();
    
    UE_LOG(LogServer, Log, __FUNCTION__);

    if (!IsValidPointer(InstigatingController)) return;

    if (!InstigatingController->Class->GetFunction("Actor", "GetComponentByClass")) return;
    auto Comp = InstigatingController->GetComponentByClass(UFortControllerComponent_IndicatedActorManagement::StaticClass());
    for (auto Actor : IndicatedActors) 
    {
        if (!Actor) continue;
        
        FIndicatedActorInfoEntry Entry{};
        Entry.Actor = Actor;
        Entry.Data = IndicatedActorData;
        Entry.EndTime = UGameplayStatics::GetTimeSeconds(GetWorld()) + 5.f;
        Entry.StartTime = UGameplayStatics::GetTimeSeconds(GetWorld());
        Entry.bRefreshExistingWhenAdded = true;

        ((UFortControllerComponent_IndicatedActorManagement*)Comp)->IndicatedActorList.Entries.Add(Entry);
    }
}

void FortControllerComponent_IndicatedActorManagement::Library::AddActorsToStenciledList(UObject*, FFrame& Stack)
{
    AFortPlayerControllerAthena* InstigatingController;
    TArray<AActor*>& StenciledActors = Stack.StepCompiledInRef<TArray<AActor*>>();
    FStenciledActorData StenciledActorData;
    bool bAddAsUnique;
    Stack.StepCompiledIn(&InstigatingController);
    Stack.StepCompiledIn(&StenciledActorData);
    Stack.StepCompiledIn(&bAddAsUnique);
    Stack.IncrementCode();
    UE_LOG(LogServer, Log, __FUNCTION__);
    if (!InstigatingController) return;

    auto Comp = (UFortControllerComponent_IndicatedActorManagement*)InstigatingController->GetComponentByClass(UFortControllerComponent_IndicatedActorManagement::StaticClass());
    if (!Comp) return;

    for (auto& Actor : StenciledActors)
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

void FortControllerComponent_IndicatedActorManagement::AddActorsToIndicatedList(UFortControllerComponent_IndicatedActorManagement* Comp, FFrame& Stack)
{
    TArray<class AActor*>* IndicatedActors;
    FIndicatedActorData Data;
    bool bAddAsUnique;
    bool bAllowOwningPlayer;
    Stack.StepCompiledIn(&IndicatedActors);
    Stack.StepCompiledIn(&Data);
    Stack.StepCompiledIn(&bAddAsUnique);
    Stack.StepCompiledIn(&bAllowOwningPlayer);
    Stack.IncrementCode();
    
    if (!Comp) return;

    for (auto& Actor : *IndicatedActors)
    {
        if (!Actor) return;
        FIndicatedActorInfoEntry Entry{};

        Entry.Actor = Actor;
        Entry.Data = Data;
        Entry.EndTime = UGameplayStatics::GetTimeSeconds(GetWorld()) + 5.f;
        Entry.StartTime = UGameplayStatics::GetTimeSeconds(GetWorld());
        Entry.bRefreshExistingWhenAdded = true;

        Comp->IndicatedActorList.Entries.Add(Entry);
    }
}

void FortControllerComponent_IndicatedActorManagement::AddActorsToStenciledList(UFortControllerComponent_IndicatedActorManagement* Comp, FFrame& Stack)
{
    TArray<AActor*>* StenciledActors;
    FStenciledActorData Data;
    bool bAddAsUnique;
    Stack.StepCompiledIn(&StenciledActors);
    Stack.StepCompiledIn(&Data);
    Stack.StepCompiledIn(&bAddAsUnique);
    
    if (!Comp) return;

    for (auto& Actor : *StenciledActors)
    {
        if (!Actor) return;
        FStenciledActorInfoEntry Entry{};

        Entry.Actor = Actor;
        Entry.Data = Data;
        Entry.EndTime = UGameplayStatics::GetTimeSeconds(GetWorld()) + 5.f;
        Entry.StartTime = UGameplayStatics::GetTimeSeconds(GetWorld());
        Entry.bRefreshExistingWhenAdded = true;

        Comp->StenciledActorList.Entries.Add(Entry);
    }
}

void FortControllerComponent_IndicatedActorManagement::Setup()
{
    UHook* Hook = new UHook();

    Hook->Path = "/Script/FortniteGame.FortIndicatedActorManagementLibrary.AddActorsToIndicatedList";
    Hook->Detour = Library::AddActorsToIndicatedList;
 //   UKismetHookingLibrary::Hook(Hook, EHook::Exec);
    
    Hook->Path = "/Script/FortniteGame.FortIndicatedActorManagementLibrary.AddActorsToStenciledList";
    Hook->Detour = Library::AddActorsToStenciledList;
   // UKismetHookingLibrary::Hook(Hook, EHook::Exec);
    
    delete Hook;
}
