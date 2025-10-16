#include "pch.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Quests/FortQuestManager.h"

#include "Logging.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"

static void ProgressQuest(AFortPlayerControllerAthena* PlayerController, UFortQuestManager* QuestManager,
                          UFortQuestItem* QuestItem, UFortQuestItemDefinition* QuestDefinition,
                          FFortMcpQuestObjectiveInfo* Obj, int32 IncrementCount)
{
    static xmap<AFortPlayerControllerAthena*, std::vector<FFortMcpQuestObjectiveInfo>> ObjCompArray;
    auto Count = QuestManager->GetObjectiveCompletionCount(QuestDefinition, Obj->BackendName);

    int32 NewCount = Count + IncrementCount;

    QuestDefinition->ObjectiveCompletionCount = NewCount;

    xstring BackendName = Obj->BackendName.ToString().c_str();

    bool thisObjectiveCompleted = (NewCount >= Obj->Count);
    bool allObjsCompleted = false;

    if (thisObjectiveCompleted && QuestDefinition->Objectives.Num() == 1)
    {
        allObjsCompleted = true;
    }

    if (thisObjectiveCompleted && QuestDefinition->Objectives.Num() > 1)
    {
        bool alreadyExists = false;
        for (auto& ObjComp : ObjCompArray[PlayerController])
        {
            if (ObjComp.BackendName == Obj->BackendName)
            {
                alreadyExists = true;
                break;
            }
        }
        if (!alreadyExists)
        {
            ObjCompArray[PlayerController].push_back(*Obj);
        }

        int32 totalObjectives = QuestDefinition->Objectives.Num();

        auto CompletionCount = 0;
        for (auto& QuestObj : QuestDefinition->Objectives)
        {
            bool Found = false;
            for (auto& ObjComp : ObjCompArray[PlayerController])
            {
                if (QuestObj.BackendName == ObjComp.BackendName)
                {
                    Found = true;
                    break;
                }
            }
            if (Found)
            {
                CompletionCount++;
            }
        }

        allObjsCompleted = (CompletionCount == totalObjectives);
    }

    auto PlayerState = (AFortPlayerStateAthena*)PlayerController->PlayerState;
    if (PlayerState)
    {
        for (const auto& TeamMember : PlayerState->PlayerTeam->TeamMembers)
        {
            auto TeamMemberPlayerController = (AFortPlayerControllerAthena*)TeamMember;
            if (TeamMemberPlayerController->IsA(AFortAthenaAIBotController::StaticClass()))
                continue;

            FFortUpdatedObjectiveStat Stat{};
            Stat.BackendName = Obj->BackendName;
            Stat.CurrentStage = QuestItem->CurrentStage;
            Stat.Quest = QuestDefinition;
            Stat.ShadowStatValue = Obj->Count;
            Stat.StatDelta = 1;
            Stat.StatValue = Stat.ShadowStatValue;
            
            for (auto& UpdatedObj : TeamMemberPlayerController->UpdatedObjectiveStats)
            {
                if (UpdatedObj.BackendName.ComparisonIndex == Stat.BackendName.ComparisonIndex)
                {
                    UpdatedObj.CurrentStage = Stat.CurrentStage;
                    UpdatedObj.ShadowStatValue = Stat.ShadowStatValue;
                    UpdatedObj.StatDelta = Stat.StatDelta;
                    UpdatedObj.StatValue = Stat.StatValue;
                    TeamMemberPlayerController->OnRep_UpdatedObjectiveStats();
                    return;
                }
            }
            TeamMemberPlayerController->UpdatedObjectiveStats.Add(Stat);
            TeamMemberPlayerController->OnRep_UpdatedObjectiveStats();

            FFortDisplayQuestUpdateData Data{};
            Data.QuestOwner = (AFortPlayerState*)TeamMemberPlayerController->PlayerState;
            Data.ObjectiveUpdated = Stat;
            QuestManager->DisplayQuestUpdateData.Add(Data);

            UFortQuestObjectiveInfo* CurrentObjective = nullptr;
            for (auto obj : QuestItem->Objectives)
                if (obj->BackendName == Obj->BackendName)
                {
                    CurrentObjective = obj;
                    break;
                }

            CurrentObjective->AchievedCount = NewCount;
            CurrentObjective->QuestOwner = (AFortPlayerState*)TeamMemberPlayerController->PlayerState;
            CurrentObjective->DisplayDynamicQuestUpdate();

            QuestManager->HandleQuestObjectiveUpdated(
    TeamMemberPlayerController,
    QuestDefinition,
    Obj->BackendName,
    NewCount,
    IncrementCount,
    TeamMemberPlayerController == PlayerController ? nullptr : PlayerState,
    thisObjectiveCompleted,
    allObjsCompleted);
            QuestManager->ForceTriggerQuestsUpdated();

            TArray<FFortQuestObjectiveCompletion> Progress;
            FFortQuestObjectiveCompletion Completion;
            Completion.StatName = UKismetStringLibrary::Conv_NameToString(Obj->BackendName);
            Completion.Count = NewCount;
            Progress.Add(Completion);

            PlayerController->XPComponent->ClientQuestsUpdated(Progress);
            PlayerController->XPComponent->QuestObjectiveUpdated(PlayerController, QuestDefinition, Obj->BackendName,
                                                                 NewCount, thisObjectiveCompleted,
                                                                 allObjsCompleted);
        }
    }

    if (allObjsCompleted)
    {
        int32 XPPlayerControllerCount = 0;

        if (auto RewardsTable = QuestDefinition->RewardsTable)
        {
            static auto Name = UKismetStringLibrary::Conv_StringToName(L"Default");
            auto DefaultRow = RewardsTable->Search([](FName& RName, uint8* Row) { return RName == Name; });
            XPPlayerControllerCount = (*(FFortQuestRewardTableRow**)DefaultRow)->Quantity;
        }

        if (XPPlayerControllerCount > 0)  {
            for (const auto& TeamMember : PlayerState->PlayerTeam->TeamMembers)
            {
                auto TeamMemberPlayerController = (AFortPlayerControllerAthena*)TeamMember;
                if (!TeamMemberPlayerController || TeamMemberPlayerController->IsA(
                    AFortAthenaAIBotController::StaticClass()))
                    continue;

                auto* XPComp = TeamMemberPlayerController->XPComponent;
                if (!XPComp) continue;
                
                FXPEventInfo Info;
                Info.EventXpValue = XPPlayerControllerCount;
                Info.RestedValuePortion = Info.EventXpValue;
                Info.RestedXPRemaining = Info.EventXpValue;
                Info.QuestDef = QuestDefinition;
                Info.TotalXpEarnedInMatch = Info.EventXpValue + XPComp->TotalXpEarned;
                Info.Priority = EXPEventPriorityType::TopCenter;
                Info.SimulatedText = UKismetTextLibrary::Conv_StringToText(L"Objective completed");
                Info.SimulatedName = QuestDefinition->GetDisplayName(true);
                Info.Accolade = UKismetSystemLibrary::GetPrimaryAssetIdFromObject(QuestDefinition);
                Info.EventName = QuestDefinition->Name;
                Info.SeasonBoostValuePortion = 0;
                Info.PlayerController = PlayerController;

                XPComp->ChallengeXp += XPPlayerControllerCount;
                XPComp->TotalXpEarned += XPPlayerControllerCount;
                XPComp->OnXpUpdated(XPComp->CombatXp, XPComp->SurvivalXp, XPComp->MedalBonusXP, XPComp->ChallengeXp, XPComp->MatchXp, XPComp->TotalXpEarned);

                XPComp->InMatchProfileVer++;
                XPComp->OnInMatchProfileUpdate(XPComp->InMatchProfileVer);
                XPComp->OnProfileUpdated();

                XPComp->OnXpEvent(Info);
            }
        
            QuestManager->ClaimQuestReward(QuestItem);
        }
    }
}

void FortQuestManager::SendComplexCustomStatEvent(UFortQuestManager* QuestManager, UObject* TargetObj,
                                                  FGameplayTagContainer& SourceTags, FGameplayTagContainer& TargetTags,
                                                  bool* QuestActive, bool* QuestCompleted, int32 Count)
{
    FGameplayTagContainer ContextTags;
    QuestManager->GetSourceAndContextTags(nullptr, &ContextTags);
    
    SendStatEventWithTags(QuestManager, EFortQuestObjectiveStatEvent::ComplexCustom, TargetObj, TargetTags, SourceTags,
                          ContextTags, Count);
    return SendComplexCustomStatEventOG(QuestManager, TargetObj, SourceTags, TargetTags, QuestActive, QuestCompleted,
                                        Count);
}

void FortQuestManager::SendStatEventWithTags(UFortQuestManager* QuestManager,
                                             EFortQuestObjectiveStatEvent Type,
                                             UObject* TargetObj,
                                             FGameplayTagContainer& TargetTags,
                                             FGameplayTagContainer& SourceTags,
                                             FGameplayTagContainer& ContextTags,
                                             int32 Count)
{
    if (Count <= 0) Count = 1;
    if (QuestManager)
    {
        auto Controller = (AFortPlayerControllerAthena*)QuestManager->GetPlayerControllerBP();
        UE_LOG(LogQuests, Log, "QuestManager called for player %s with type: %d and count: %d",
               Controller ? Controller->PlayerState->GetPlayerName().ToString().c_str() : "Unknown", Type, Count);
        if (!Controller) return;

        static auto Playlist = ((AFortGameStateAthena*)UWorld::GetWorld()->GameState)->CurrentPlaylistInfo.BasePlaylist;
        if (Playlist) {
            for (auto& Tag : Playlist->GameplayTagContainer.GameplayTags) { 
                // this might fix some playlist tags?!?!??!
                ContextTags.GameplayTags.Add(Tag);
            }
        }

        static UDataTable* AthenaObjectiveStatXPTable = StaticLoadObject<UDataTable>(
            "/Game/Athena/Items/Quests/AthenaObjectiveStatXPTable.AthenaObjectiveStatXPTable");
        static xmap<UFortAccoladeItemDefinition*, bool> OnceOnlyMap;

        if (AthenaObjectiveStatXPTable)
        {
            for (const auto& [RowName, RowPtr] : AthenaObjectiveStatXPTable->RowMap)
            {
                auto Row = (FFortQuestObjectiveStatXPTableRow*)RowPtr;

                static FName AccoladeTypeName = UKismetStringLibrary::Conv_StringToName(L"Accolades");
                if (Row->AccoladePrimaryAssetId.PrimaryAssetType.Name == AccoladeTypeName)
                {
                    if (Row->Type != Type || (Row->CountThreshhold > 0 && Row->CountThreshhold < Count))
                        continue;

                    if (!TargetTags.HasAll(Row->TargetTags))
                        continue;

                    if (!SourceTags.HasAll(Row->SourceTags))
                        continue;

                    if (!ContextTags.HasAll(Row->ContextTags))
                    {
                        static FGameplayTag PlaylistTag = FGameplayTag(UKismetStringLibrary::Conv_StringToName(L"Athena.Playlist"));
                        ContextTags.GameplayTags.Add(PlaylistTag);
                        if (!ContextTags.HasAll(Row->ContextTags))
                        {
                            continue;
                        }
                    }

                    if (!IsConditionMet(Row->Condition, TargetTags, SourceTags, ContextTags, Controller))
                        continue;
                    
                    auto AccoladeToGive = (UFortAccoladeItemDefinition*)
                        UKismetSystemLibrary::GetObjectFromPrimaryAssetId(Row->AccoladePrimaryAssetId);

                    if (!AccoladeToGive)
                    {
                        UE_LOG(LogQuests, Warning, "Failed to find accolade %s for player %s",
                               Row->AccoladePrimaryAssetId.PrimaryAssetName.ToString().c_str(),
                               Controller->PlayerState->GetPlayerName().ToString().c_str());
                        continue;
                    }

                    if (Row->bOnceOnly)
                    {
                        if (OnceOnlyMap[AccoladeToGive])
                            continue;

                        OnceOnlyMap[AccoladeToGive] = true;
                    }

                    float XpValue = 0;
                    TMap<FName, uint8*>& RowMap = *reinterpret_cast<TMap<FName, uint8*>*>(__int64(AccoladeToGive->XpRewardAmount.Curve.CurveTable) + 0x30);

                    for (auto& Pair : RowMap)
                    {
                        if (Pair.Key().ToString() == AccoladeToGive->XpRewardAmount.Curve.RowName.ToString())
                        {
                            FSimpleCurve* Curve = (FSimpleCurve*)Pair.Value();
                            XpValue = Curve->Keys[0].Value;
                        }
                    }

                    FXPEventInfo Info{};
                    Info.EventXpValue = (int32)XpValue;
                    Info.RestedValuePortion = Info.EventXpValue;
                    Info.RestedXPRemaining = Info.EventXpValue;
                    Info.TotalXpEarnedInMatch = Info.EventXpValue + Controller->XPComponent->TotalXpEarned;
                    Info.Priority = AccoladeToGive->Priority;
                    Info.SimulatedText = AccoladeToGive->GetShortDescription();
                    Info.SimulatedName = AccoladeToGive->GetDisplayName(true);
                    Info.Accolade = UKismetSystemLibrary::GetPrimaryAssetIdFromObject(AccoladeToGive);
                    Info.EventName = AccoladeToGive->Name;
                    Info.SeasonBoostValuePortion = 0;
                    Info.PlayerController = Controller;

                    Controller->XPComponent->MatchXp += Info.EventXpValue;
                    Controller->XPComponent->TotalXpEarned += Info.EventXpValue;

                    for (auto& TSRemove : AccoladeToGive->AccoladeToReplace)
                    {
                        UFortAccoladeItemDefinition* AccoladeToRemove = TSRemove.Get();
        
                        auto AthenaIdx = Controller->XPComponent->PlayerAccolades.SearchIndex([AccoladeToRemove](FAthenaAccolades& item) 
                            { return item.AccoladeDef == AccoladeToRemove; });
                        auto MedalIdx = Controller->XPComponent->MedalsEarned.SearchIndex([AccoladeToRemove](UFortAccoladeItemDefinition* item) 
                            { return item == AccoladeToRemove; });

                        if (AthenaIdx != -1) Controller->XPComponent->PlayerAccolades.Remove(AthenaIdx);
                        if (MedalIdx != -1) Controller->XPComponent->MedalsEarned.Remove(MedalIdx);
                    }

                    FAthenaAccolades AthenaAccolade{};
                    AthenaAccolade.AccoladeDef = AccoladeToGive;
                    AthenaAccolade.Count = 1;
                    AthenaAccolade.TemplateId = AccoladeToGive->Name.GetRawWString().c_str();
                    Controller->XPComponent->PlayerAccolades.Add(AthenaAccolade);

                    if (AccoladeToGive->AccoladeType == EFortAccoladeType::Medal)
                    {
                        Controller->XPComponent->MedalsEarned.Add(AccoladeToGive);
                        Controller->XPComponent->ClientMedalsRecived(Controller->XPComponent->PlayerAccolades);
                    }

                    Controller->XPComponent->OnXpEvent(Info);
                }
            }
        }

        TArray<UFortQuestItem*> CurrentQuests;
        QuestManager->GetCurrentQuests(&CurrentQuests);

        for (int i = 0; i < CurrentQuests.Num(); i++)
        {
            if (!CurrentQuests.IsValidIndex(i) || !CurrentQuests[i])
                continue;

            auto CurrentQuest = CurrentQuests[i];

            if (CurrentQuest->HasCompletedQuest())
                continue;

            auto QuestDef = CurrentQuest->GetQuestDefinitionBP();

            if (!QuestDef)
                continue;

            if (QuestManager->HasCompletedQuest(QuestDef))
                continue;

            auto& Objectives = QuestDef->Objectives;
            for (int j = 0; j < Objectives.Num(); j++)
            {
                if (!Objectives.IsValidIndex(j))
                    continue;

                FFortMcpQuestObjectiveInfo* Objective = &Objectives[j];

                if (QuestManager->HasCompletedObjectiveWithName(QuestDef, Objective->BackendName) ||
                    QuestManager->HasCompletedObjective(QuestDef, Objective->ObjectiveStatHandle) ||
                    CurrentQuest->HasCompletedObjectiveWithName(Objective->BackendName) ||
                    CurrentQuest->HasCompletedObjective(Objective->ObjectiveStatHandle))
                {
                    continue;
                }

                auto& InlineStats = Objective->InlineObjectiveStats;
                for (int k = 0; k < InlineStats.Num(); k++)
                {
                    if (!InlineStats.IsValidIndex(k))
                        continue;

                    FFortQuestObjectiveStat& ObjectiveStat = InlineStats[k];

                    if (ObjectiveStat.Type != Type)
                        continue;

                    bool bFoundQuest = true;

                    auto& TagConditions = ObjectiveStat.TagConditions;
                    for (int l = 0; l < TagConditions.Num(); l++)
                    {
                        if (!TagConditions.IsValidIndex(l))
                            continue;

                        FInlineObjectiveStatTagCheckEntry TagCondition = TagConditions[l];

                        if (!TagCondition.Require || !bFoundQuest)
                            continue;

                        switch (TagCondition.Type)
                        {
                        case EInlineObjectiveStatTagCheckEntryType::Target:
                            {
                                if (!ObjectiveStat.bHasInclusiveTargetTags)
                                    break;

                                if (!TargetTags.HasTag(TagCondition.Tag))
                                {
                                    bFoundQuest = false;
                                }

                                break;
                            }
                        case EInlineObjectiveStatTagCheckEntryType::Source:
                            {
                                if (!ObjectiveStat.bHasInclusiveSourceTags)
                                    break;

                                if (!SourceTags.HasTag(TagCondition.Tag))
                                {
                                    bFoundQuest = false;
                                }

                                break;
                            }
                        case EInlineObjectiveStatTagCheckEntryType::Context:
                            {
                                if (!ObjectiveStat.bHasInclusiveContextTags)
                                    break;

                                static FGameplayTag PlaylistTag = FGameplayTag(UKismetStringLibrary::Conv_StringToName(L"Athena.Playlist"));

                                if (!ContextTags.HasTag(TagCondition.Tag) && TagCondition.Tag.TagName != PlaylistTag.TagName)
                                {
                                    bFoundQuest = false;
                                }

                                break;
                            }
                        case EInlineObjectiveStatTagCheckEntryType::EInlineObjectiveStatTagCheckEntryType_MAX:
                            {
                                break;
                            }
                        default:
                            break;
                        }
                    }

                    if (!IsConditionMet(ObjectiveStat.Condition, TargetTags, SourceTags, ContextTags, Controller))
                       continue;
                    
                    if (bFoundQuest)
                        ProgressQuest(Controller, QuestManager, CurrentQuest, QuestDef, Objective, Count);
                }
            }
        }
    }
}

void FortQuestManager::Setup()
{
    UHook* Hook = new UHook();
    
    Hook->Address = ImageBase + 0x52A1BF4;
    Hook->Detour = SendComplexCustomStatEvent;
    Hook->Original = (void**)&SendComplexCustomStatEventOG;
    UKismetHookingLibrary::Hook(Hook, EHook::Address);
}