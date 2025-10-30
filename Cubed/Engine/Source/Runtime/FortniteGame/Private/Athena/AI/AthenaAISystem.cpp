#include "pch.h"

#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"

void UAthenaAISystem::SpawnAI(string NPCName)
{
    std::string Path = std::string("/NPCLibrary/") + "NPCs/" + NPCName + "/" + NPCName + "/" +
                       "BP_AIBotSpawnerData_NPC_" + NPCName + "." + "BP_AIBotSpawnerData_NPC_" + NPCName + "_C";
    UE_LOG(LogTemp, Log, "Path: %s", Path.c_str());
    auto Class = StaticLoadObject<UClass>(Path.c_str());
    if (!Class) return;
    
    TArray<AFortAthenaPatrolPath*> PossibleSpawnPaths;
    for (auto* Actor : GetAll(AFortAthenaPatrolPathPointProvider::StaticClass()))
    {
        AFortAthenaPatrolPathPointProvider* FortAthenaPatrolPathPointProvider = (AFortAthenaPatrolPathPointProvider*)Actor;   
        if (FortAthenaPatrolPathPointProvider->FiltersTags.GameplayTags.Num() == 0)
            continue;
        auto PathName = FortAthenaPatrolPathPointProvider->FiltersTags.GameplayTags[0].TagName.ToString();
        if (PathName.substr(PathName.rfind(L'.') + 1) == xstring(NPCName))
            PossibleSpawnPaths.Add(FortAthenaPatrolPathPointProvider->AssociatedPatrolPath);
    }
    for (auto* Actor : GetAll(AFortAthenaPatrolPath::StaticClass()))
    {
        AFortAthenaPatrolPath* PatrolPath = (AFortAthenaPatrolPath*)Actor;   
        if (PatrolPath->GameplayTags.GameplayTags.Num() == 0)
            continue;
        auto PathName = PatrolPath->GameplayTags.GameplayTags[0].TagName.ToString();
        if (PathName.substr(PathName.rfind(L'.') + 1) == xstring(NPCName))
            PossibleSpawnPaths.Add(PatrolPath);
    }

    if (PossibleSpawnPaths.Num() > 0)
    {
        auto PatrolPath = PossibleSpawnPaths[(int)ceil((float)rand() / RAND_MAX) * PossibleSpawnPaths.Num() - 1];
        auto ComponentList = UFortAthenaAISpawnerData::CreateComponentListFromClass(Class, GetWorld());
    
        auto Transform = IsValidPointer(PatrolPath->PatrolPoints[0]) ? PatrolPath->PatrolPoints[0]->GetTransform() : FTransform{};
        AISpawner->RequestSpawn(ComponentList, Transform);
    } 
}
