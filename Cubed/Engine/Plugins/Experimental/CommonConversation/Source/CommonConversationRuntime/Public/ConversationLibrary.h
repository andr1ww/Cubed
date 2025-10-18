#pragma once
#include "pch.h"

#include "Engine/Source/Runtime/CoreUObject/Public/UObject/Stack.h"


namespace ConversationLibrary
{
    template <typename T>
static T PickWeighted(vector<T>& Map, float (*RandFunc)(float), bool bCheckZero = true) {
        float TotalWeight = std::accumulate(Map.begin(), Map.end(), 0.0f, [&](float acc, T& p) { return acc + p.Weight.Evaluate(); });
        float RandomNumber = RandFunc(TotalWeight);

        for (auto& Element : Map)
        {
            float Weight = Element.Weight.Evaluate();
            if (bCheckZero && Weight == 0)
                continue;

            if (RandomNumber <= Weight) return Element;

            RandomNumber -= Weight;
        }

        return T();
    }
    
    static UConversationInstance* StartConversation(UObject*, FFrame&, UConversationInstance**);
    FConversationTaskResult ExecuteTaskNodeWithSideEffects(UConversationTaskNode* Node, FConversationContext& Context);
    FConversationTaskResult ExecuteTaskNode(UConversationTaskNode* Node, FConversationContext& InContext);
    static void MakeConversationParticipant(UObject*, FFrame&);
    static void AdvanceConversation(UObject*, FFrame&, FConversationTaskResult*);
    static void AdvanceConversationWithChoice(UObject*, FFrame&, FConversationTaskResult*);
    static void PauseConversationAndSendClientChoices(UObject*, FFrame&, FConversationTaskResult*);
    static void ReturnToLastClientChoice(UObject*, FFrame&, FConversationTaskResult*);
    static void ReturnToCurrentClientChoice(UObject*, FFrame&, FConversationTaskResult*);
    static void ReturnToConversationStart(UObject*, FFrame&, FConversationTaskResult*);
    static void AbortConversation(UObject*, FFrame&, FConversationTaskResult*);
    extern xmap<uint32, AActor*> ConversationParticipantEntry;
    
    void Setup();
}
