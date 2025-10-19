#include "pch.h"

void UConversationParticipantComponent::ServerNotifyConversationStarted(UConversationInstance* Conversation, FGameplayTag AsParticipant)
{
    AActor* Owner = GetOwner();
    Auth_CurrentConversation = Conversation;
    Auth_Conversations.Add(Conversation);
    ClientUpdateParticipants(Auth_CurrentConversation->Participants);

    ConversationsActive++;
    OnRep_ConversationsActive(ConversationsActive - 1);
    ClientStartConversation(AsParticipant);

    if (Owner->GetRemoteRole() == ENetRole::ROLE_AutonomousProxy)
        ClientUpdateConversations(ConversationsActive);
}

void UConversationParticipantComponent::ServerNotifyConversationEnded(UConversationInstance* Conversation)
{
    AActor* Owner = GetOwner();
    if (Owner->GetLocalRole() == ENetRole::ROLE_Authority)
    {
        if (Auth_CurrentConversation == Conversation)
        {
            Auth_CurrentConversation = nullptr;
            auto ConversationIdx = Auth_Conversations.SearchIndex([&](UConversationInstance*& Instance) {
                return Instance == Conversation;
            });
            Auth_Conversations.Remove(ConversationIdx);

            ConversationsActive--;
            OnRep_ConversationsActive(ConversationsActive + 1);

            if (Owner->GetRemoteRole() == ENetRole::ROLE_AutonomousProxy)
                ClientUpdateConversations(ConversationsActive);
        }
    }
}