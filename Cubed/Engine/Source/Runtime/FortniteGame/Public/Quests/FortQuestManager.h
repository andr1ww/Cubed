#pragma once
#include "pch.h"

#include "Logging.h"
#include <unordered_map>

namespace FortQuestManager
{
    static xmap<AFortPlayerControllerAthena*, xmap<xstring, int32>> PlayerConditionCounts;
    
    struct FParseConditionResult
    {
        bool bMatch;
        size_t NextStart;
    };

    static FParseConditionResult ParseCondition(xstring Condition, const FGameplayTagContainer &TargetTags, const FGameplayTagContainer &SourceTags, const FGameplayTagContainer &ContextTags, AFortPlayerControllerAthena* PlayerController = nullptr)
    {
        size_t CondTypeStart = -1, CondTypeEnd = -1, NextCond = -1;

        for (size_t i = 0; i < Condition.length(); i++)
        {
            char c = Condition[i];
            
            if (CondTypeStart == -1 && (c == '>' || c == '<' || c == '=' || c == '!'))
            {
                CondTypeStart = i;
            }
            else if (CondTypeStart != -1 && CondTypeEnd == -1 && (c != '>' && c != '<' && c != '=' && c != '&'))
            {
                CondTypeEnd = i;
            }
            else if (CondTypeEnd != -1 && (c == '=' || c == '&'))
            {
                NextCond = i;
                break;
            }
        }

        if (CondTypeStart == Condition.npos)
        {
            CondTypeStart = Condition.find(" ");

            if (CondTypeStart == Condition.npos)
                return {false, NextCond};

            CondTypeStart++;

            if (CondTypeEnd == Condition.npos)
                CondTypeEnd = Condition.find(" ", CondTypeStart);

            NextCond = Condition.find("&&", CondTypeEnd + 1);
            if (NextCond == Condition.npos)
                NextCond = Condition.find("||", CondTypeEnd + 1);
        }
        else if (CondTypeEnd == Condition.npos)
        {
            CondTypeEnd = CondTypeStart + 1;
        }

        auto Left = Condition.substr(0, CondTypeStart);
        Left.erase(Left.find_last_not_of(" \t\n\r") + 1);
        
        auto CondType = Condition.substr(CondTypeStart, CondTypeEnd - CondTypeStart);
        
        auto Right = Condition.substr(CondTypeEnd, NextCond == Condition.npos ? NextCond : NextCond - CondTypeEnd);
        Right.erase(0, Right.find_first_not_of(" \t\n\r"));
        if (NextCond != Condition.npos)
            Right.erase(Right.find_last_not_of(" \t\n\r") + 1);
        
        if (CondType == "HasTag" || CondType == "MissingTag")
        {
            FGameplayTagContainer Container;

            if (Left == "Target.Tags")
            {
                Container = TargetTags;
            }
            else if (Left == "Source.Tags")
            {
                Container = SourceTags;
            }
            else if (Left == "Context.Tags")
            {
                Container = ContextTags;
            }
            else
            {
                return {false, NextCond};
            }

            FGameplayTag Tag(UKismetStringLibrary::Conv_StringToName(wstring(Right.begin(), Right.end()).c_str()));

            if (CondType == "HasTag")
            {
                return {Container.HasTag(Tag), NextCond};
            }
            
            return {!Container.HasTag(Tag), NextCond};
        }

        if (CondType == "==" || CondType == ">=" || CondType == ">" || CondType == "<=" || CondType == "<")
        {
            if (PlayerController)
            {
                PlayerConditionCounts[PlayerController][Left]++;
                int32 CurrentCount = PlayerConditionCounts[PlayerController][Left];
                int32 RequiredCount = std::stoi(std::string(Right));
                
                bool bResult = false;
                
                if (CondType == "==")
                {
                    bResult = CurrentCount == RequiredCount;
                }
                else if (CondType == ">=")
                {
                    bResult = CurrentCount >= RequiredCount;
                }
                else if (CondType == ">")
                {
                    bResult = CurrentCount > RequiredCount;
                }
                else if (CondType == "<=")
                {
                    bResult = CurrentCount <= RequiredCount;
                }
                else if (CondType == "<")
                {
                    bResult = CurrentCount < RequiredCount;
                }
                
                if (bResult)
                {
                    PlayerConditionCounts[PlayerController][Left] = 0;
                }
                
                return {bResult, NextCond};
            }
            
            return {false, NextCond};
        }

        UE_LOG(LogQuests, Log, "Hit unimplemented condition: %s %s %s\n", Left.c_str(), CondType.c_str(), Right.c_str());
        return {false, NextCond};
    }

    // thanks ploosh for letting me use this and ^ function above
    static bool IsConditionMet(const FString &InCondition, const FGameplayTagContainer &TargetTags, const FGameplayTagContainer &SourceTags, const FGameplayTagContainer &ContextTags, AFortPlayerControllerAthena* PlayerController = nullptr)
    {
        auto Condition = InCondition.ToString();

        if (Condition.empty())
            return true;

        FParseConditionResult Result = ParseCondition(Condition, TargetTags, SourceTags, ContextTags, PlayerController);

        if (Result.NextStart != Condition.npos)
        {
        loop:
            if (Result.NextStart == Condition.npos)
                return Result.bMatch;
            auto Start = Condition.substr(Result.NextStart, 2);

            if (Start == "&&")
            {
                Condition = Condition.substr(Result.NextStart + 2);

                if (Condition.substr(0, 1) == " ")
                    Condition = Condition.substr(1);

                auto LastResult = Result;

                Result = ParseCondition(Condition, TargetTags, SourceTags, ContextTags, PlayerController);

                if (!LastResult.bMatch || !Result.bMatch)
                    Result.bMatch = false;

                goto loop;
            }
            else if (Start == "||")
            {
                Condition = Condition.substr(Result.NextStart + 2);

                if (Condition.substr(0, 1) == " ")
                    Condition = Condition.substr(1);

                auto LastResult = Result;

                Result = ParseCondition(Condition, TargetTags, SourceTags, ContextTags, PlayerController);

                if (LastResult.bMatch || Result.bMatch)
                    Result.bMatch = true;

                goto loop;
            }
            else
            {
                return Result.bMatch;
            }
        }

        return Result.bMatch;
    }
    
    DefineOriginal(void, SendStatEventWithTags,
        UFortQuestManager* QuestManager,
        EFortQuestObjectiveStatEvent Type,
        UObject* TargetObj,
        FGameplayTagContainer &TargetTags,
        FGameplayTagContainer &SourceTags,
        FGameplayTagContainer &ContextTags,
        int32 Count);
    DefineOriginal(void, SendComplexCustomStatEvent,
        UFortQuestManager* QuestManager,
        UObject* TargetObj,
        FGameplayTagContainer &SourceTags,
        FGameplayTagContainer &TargetTags,
        bool *QuestActive, bool *QuestCompleted, int32 Count);
    
    void Setup();
}