#include "pch.h"
#include "Engine/Source/Runtime/Engine/Classes/Engine/NetDriver.h"

#include "Offsets.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"
#include <fstream>

void AddStatObjective(AFortPlayerControllerAthena* PC, FFortUpdatedObjectiveStat& Obj)
{
    for (auto& UpdatedObj : PC->UpdatedObjectiveStats)
    {
        if (UpdatedObj.BackendName.ComparisonIndex == Obj.BackendName.ComparisonIndex)
        {
            UpdatedObj.CurrentStage = Obj.CurrentStage;
            UpdatedObj.ShadowStatValue = Obj.ShadowStatValue;
            UpdatedObj.StatDelta = Obj.StatDelta;
            UpdatedObj.StatValue = Obj.StatValue;
            PC->OnRep_UpdatedObjectiveStats();
            return;
        }
    }
    PC->UpdatedObjectiveStats.Add(Obj);
    PC->OnRep_UpdatedObjectiveStats();
}

void NetDriver::TickFlush(UNetDriver* NetDriver, float DeltaTime)
{
    if (NetDriver->ReplicationDriver)
        Runtime::Funcs::ServerReplicateActors(NetDriver->ReplicationDriver, DeltaTime);

    static bool bStartedBus = false;
    if (GetAsyncKeyState(VK_F5) & 0x1 && !bStartedBus) {
        bStartedBus = true;
        UKismetSystemLibrary::ExecuteConsoleCommand(NetDriver->World, L"startaircraft", nullptr);
    }

    if (GetAsyncKeyState(VK_F7) & 1)
    {
        std::ofstream QuestsLog("Quests.log");
        AFortPlayerControllerAthena* PC = reinterpret_cast<AFortPlayerControllerAthena*>(NetDriver->ClientConnections[0]->PlayerController);
        UFortQuestManager* QuestManager = PC->GetQuestManager(ESubGame::Athena);
        if (QuestManager)
        {
            QuestsLog << "CurrentQuests: %d" << QuestManager->CurrentQuests.Num() << "\n";

            for (int i = 0; i < QuestManager->CurrentQuests.Num(); i++)
            {
                UFortQuestItem* CurrentQuest = QuestManager->CurrentQuests[i];
                UFortQuestItemDefinition* QuestItemDefinition = CurrentQuest->GetQuestDefinitionBP();
                QuestsLog << "QuestDef: %s" << QuestItemDefinition->GetFullName().c_str() << "\n";
                QuestsLog << "Description: %s" << QuestItemDefinition->Description.ToString().c_str() << "\n";
                for (int i = 0; i < QuestItemDefinition->Objectives.Num(); i++)
                {
                    FFortMcpQuestObjectiveInfo& ObjectiveInfo = QuestItemDefinition->Objectives[i];
                    QuestsLog << "Objective[" << i << "]" << "\n";
                    QuestsLog << "BackendName: " << ObjectiveInfo.BackendName.ToString().c_str() << "\n";
                    QuestsLog << "ItemEvent: " << (int)ObjectiveInfo.ItemEvent << "\n";
                    QuestsLog << "Count: " << ObjectiveInfo.Count << "\n";
                    for (int f = 0; f < ObjectiveInfo.InlineObjectiveStats.Num(); f++)
                    {
                        auto& InlineObjectiveStat = ObjectiveInfo.InlineObjectiveStats[f];
                        QuestsLog << "InlineObjectiveStat[" << f << "]" << "\n";
                        QuestsLog << "Type: " << (int)InlineObjectiveStat.Type;
                        for (int z = 0; z < InlineObjectiveStat.TagConditions.Num(); z++)
                        {
                            auto& TagCondition = InlineObjectiveStat.TagConditions[z];
                            QuestsLog << "TagCondition[" << z << "]" << "\n";
                            QuestsLog << "Tag: " << TagCondition.Tag.TagName.ToString().c_str() << "\n";
                            QuestsLog << "Type: " << (int)TagCondition.Type << "\n";
                        }
                    }
                }

                FFortUpdatedObjectiveStat Stat{};
                Stat.BackendName = QuestItemDefinition->Objectives[0].BackendName;
                Stat.CurrentStage = CurrentQuest->CurrentStage;
                Stat.Quest = QuestItemDefinition;
                Stat.ShadowStatValue = 1;
                Stat.StatDelta = 1;
                Stat.StatValue = Stat.ShadowStatValue;
                AddStatObjective(PC, Stat);
                
                QuestManager->HandleQuestObjectiveUpdated(PC, QuestItemDefinition, QuestItemDefinition->Objectives[0].BackendName, 1, 1, nullptr, false, false);
            }
        }
    }
    
    return TickFlushOG(NetDriver, DeltaTime);
}

void NetDriver::Setup()
{
    UHook* Hook = new UHook();
    
    Hook->Address = ImageBase + Runtime::Offsets::TickFlush;
    Hook->Original = (void**)&TickFlushOG;
    Hook->Detour = TickFlush;
    UKismetHookingLibrary::Hook(Hook, EHook::Address);

    free(Hook);
}