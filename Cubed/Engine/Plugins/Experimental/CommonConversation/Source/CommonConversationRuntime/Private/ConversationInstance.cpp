#include "pch.h"

#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationLibrary.h"

struct FConversationBranchPointBuilder/* : public FNoncopyable*/
{
	TArray<FConversationBranchPoint> BranchPoints;
	inline void AddChoice(FConversationContext& InContext, FClientConversationOptionEntry& InChoice)
	{
		FConversationBranchPoint BranchPoint;
		BranchPoint.ClientChoice = InChoice;
		if (!UKismetGuidLibrary::IsValid_Guid(BranchPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID))
		{
			BranchPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID = InContext.TaskBeingConsidered->Compiled_NodeGUID;
		}
		BranchPoint.ReturnScopeStack = InContext.ReturnScopeStack;

		BranchPoints.Add(BranchPoint);
	}
};

UConversationParticipantComponent* FConversationParticipantEntry::InternalGetParticipantComponent()
{
    if (Actor)
        return (UConversationParticipantComponent*)Actor->GetComponentByClass(UConversationParticipantComponent::StaticClass());

    return nullptr;
}

FConversationContext UConversationInstance::ConstructServerContext(UConversationTaskNode* InTaskBeingConsidered)
{
    FConversationContext Context;
    Context.ActiveConversation = this;
    Context.TaskBeingConsidered = InTaskBeingConsidered;
    Context.bServer = true;
    Context.ConversationRegistry = ConversationRegistryFromWorld(GetWorld());

    return Context;
}

void UConversationInstance::ServerRemoveParticipant(FGameplayTag ParticipantID)
{
    for (int i = 0; i < Participants.List.Num(); i++)
    {
        auto& Participant = Participants.List[i];
        if (Participant.ParticipantID.TagName == ParticipantID.TagName)
        {
            if (bConversationStarted)
                Participant.ParticipantComponent->ServerNotifyConversationEnded(this);

            Participants.List.Remove(i);
            break;
        }
    }
}

void UConversationInstance::ServerAbortConversation()
{
    if (!this)
        return;

    if (bConversationStarted)
    {
        TArray<FConversationParticipantEntry> ListCopy = Participants.List;
        for (const FConversationParticipantEntry& ParticipantEntry : ListCopy)
            ServerRemoveParticipant(ParticipantEntry.ParticipantID);
    }

    StartingEntryGameplayTag = FGameplayTag();
    StartingBranchPoint = FConversationBranchPoint();
    CurrentBranchPoint = FConversationBranchPoint();
    CurrentBranchPoints.ResetNum();
    ClientBranchPoints.ResetNum();
    CurrentUserChoices.ResetNum();
    bConversationStarted = false;
}

TArray<FGuid> UConversationInstance::ConstructBranches(const TArray<FGuid>& SourceList, EConversationRequirementResult MaximumRequirementResult)
{
    FConversationContext Context = ConstructServerContext(nullptr);

    TArray<FGuid> EnabledPaths;
    for (FGuid& TestGUID : SourceList)
    {
        UConversationNode* TestNode = GetRuntimeNodeFromGUID(ConversationRegistryFromWorld(GetWorld()), TestGUID);
        if (UConversationTaskNode* TaskNode = Cast<UConversationTaskNode>(TestNode))
        {
            const EConversationRequirementResult RequirementResult = CheckRequirements(TaskNode, Context);
            if ((int64)RequirementResult <= (int64)MaximumRequirementResult)
                EnabledPaths.Add(TestGUID);
        }
    }

    return EnabledPaths;
}

void UConversationInstance::ModifyCurrentConversationNode(FConversationBranchPoint& NewBranchPoint)
{
	CurrentBranchPoint = NewBranchPoint;

	for (auto& Handle : NewBranchPoint.ReturnScopeStack)
	{
		FConversationChoiceReference Ref;
		Ref.NodeReference = Handle;
		ScopeStack.Add(Ref);
	}

	OnCurrentConversationNodeModified();
}

void UConversationInstance::ModifyCurrentConversationNode(const FConversationChoiceReference& NewChoice)
{
	FConversationBranchPoint BranchPoint;
	BranchPoint.ClientChoice.ChoiceReference = NewChoice;
	ModifyCurrentConversationNode(BranchPoint);
}

void UConversationInstance::ServerAdvanceConversation(FAdvanceConversationRequest InChoicePicked)
{
	if (bConversationStarted && UKismetGuidLibrary::IsValid_Guid(CurrentBranchPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID))
	{
		TArray<FConversationBranchPoint> CandidateDestinations;

		if (UKismetGuidLibrary::IsValid_Guid(InChoicePicked.Choice.NodeReference.NodeGUID))
		{
			FConversationBranchPoint* BranchPoint = CurrentBranchPoints.Search([&](FConversationBranchPoint& Point) {
				return Point.ClientChoice.ChoiceType == EConversationChoiceType::ServerOnly ? false : Point.ClientChoice.ChoiceReference.NodeReference.NodeGUID == InChoicePicked.Choice.NodeReference.NodeGUID;
			});
			if (BranchPoint)
			{
				CandidateDestinations.ResetNum();
				CandidateDestinations.Add(*BranchPoint);
			}
			else
			{
				ServerAbortConversation();
				return;
			}
		}
		else
		{
			if (CurrentBranchPoints.Num() == 0 && ScopeStack.Num() > 0)
			{
				ModifyCurrentConversationNode(ScopeStack[ScopeStack.Num() - 1]);
				return;
			}

			for (const FConversationBranchPoint& BranchPoint : CurrentBranchPoints)
			{
				if (BranchPoint.ClientChoice.ChoiceType != EConversationChoiceType::UserChoiceUnavailable)
				{
					CandidateDestinations.Add(BranchPoint);
				}
			}
		}

		TArray<FConversationBranchPoint> ValidDestinations;
		{
			FConversationContext Context = ConstructServerContext(nullptr);
			for (FConversationBranchPoint& BranchPoint : CandidateDestinations)
			{
				if (UConversationTaskNode* TaskNode = Cast<UConversationTaskNode>(GetRuntimeNodeFromGUID(Context.ConversationRegistry, BranchPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID)))
				{
					EConversationRequirementResult Result = TaskNode->bIgnoreRequirementsWhileAdvancingConversations ? EConversationRequirementResult::Passed : CheckRequirements(TaskNode, Context);
										
					if (Result == EConversationRequirementResult::Passed)
						ValidDestinations.Add(BranchPoint);
				}
				else
					ValidDestinations.Add(BranchPoint);
			}
		}

		if (ValidDestinations.Num() == 0)
		{
			ServerAbortConversation();
			return;
		}
		else
		{
			const int32 StartingIndex = UKismetMathLibrary::RandomIntegerInRange(0, ValidDestinations.Num() - 1);
			FConversationBranchPoint& TargetChoice = ValidDestinations[StartingIndex];
			ModifyCurrentConversationNode(TargetChoice);
		}
	}
}

void UConversationInstance::UpdateNextChoices(FConversationContext& Context)
{
	TArray<FConversationBranchPoint> AllChoices;

	if (UConversationTaskNode* TaskNode = Cast<UConversationTaskNode>(GetRuntimeNodeFromGUID(Context.ConversationRegistry, CurrentBranchPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID)))
	{
		FConversationContext ChoiceContext = Context;
		ChoiceContext.TaskBeingConsidered = TaskNode;

		TArray<FGuid> ArrayWithGUID;
		ArrayWithGUID.Add(CurrentBranchPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID);
		const TArray<FGuid> CandidateDestinations = ChoiceContext.ConversationRegistry->GetConnectedNodeIDs(ArrayWithGUID);

		FConversationBranchPointBuilder BranchBuilder;
		AllChoices = BranchBuilder.BranchPoints;
	}
}

void UConversationInstance::OnCurrentConversationNodeModified()
{
	FConversationContext AnonContext = ConstructServerContext(nullptr);
	UConversationNode* CurrentNode = GetRuntimeNodeFromGUID(AnonContext.ConversationRegistry, CurrentBranchPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID);
	if (UConversationTaskNode* TaskNode = Cast<UConversationTaskNode>(CurrentNode))
	{
		FConversationContext Context = AnonContext;
		Context.TaskBeingConsidered = TaskNode;

		FConversationTaskResult TaskResult = ConversationLibrary::ExecuteTaskNodeWithSideEffects(TaskNode, Context);

		if (ScopeStack.Num() > 0 && ScopeStack[ScopeStack.Num() - 1].NodeReference.NodeGUID == TaskNode->Compiled_NodeGUID)
			ScopeStack.Remove(ScopeStack.Num() - 1);
		
		UpdateNextChoices(Context);

		if (TaskResult.Type == EConversationTaskResultType::AbortConversation)
		{
			ServerAbortConversation();
		}
		else if (TaskResult.Type == EConversationTaskResultType::AdvanceConversation)
		{
			ServerAdvanceConversation(FAdvanceConversationRequest());
		}
		else if (TaskResult.Type == EConversationTaskResultType::AdvanceConversationWithChoice)
		{
			ModifyCurrentConversationNode(TaskResult.AdvanceToChoice.Choice);
		}
		else if (TaskResult.Type == EConversationTaskResultType::PauseConversationAndSendClientChoices)
		{
			FClientConversationMessagePayload LastMessage = FClientConversationMessagePayload();
			LastMessage.Message = TaskResult.Message;
			LastMessage.Options = CurrentUserChoices;
			LastMessage.CurrentNode = Context.ActiveConversation->CurrentBranchPoint.ClientChoice.ChoiceReference.NodeReference;
			LastMessage.Participants = Participants;

			ClientBranchPoints.Add(FCheckpoint(CurrentBranchPoint, ScopeStack));

			for (FConversationParticipantEntry& KVP : LastMessage.Participants.List)
			{
				if (UConversationParticipantComponent* ParticipantComponent = KVP.ParticipantComponent)
				{
					ParticipantComponent->LastMessage = LastMessage;

					if (ParticipantComponent->GetOwner()->GetRemoteRole() == ENetRole::ROLE_AutonomousProxy)
						ParticipantComponent->ClientUpdateConversation(LastMessage);
				}
			}
		}
		else if (TaskResult.Type == EConversationTaskResultType::ReturnToLastClientChoice)
		{
			if (ClientBranchPoints.Num() > 1)
			{
				ClientBranchPoints.Remove(ClientBranchPoints.Num() - 1);

				FCheckpoint& Checkpoint = ClientBranchPoints[ClientBranchPoints.Num() - 1];
				ScopeStack = Checkpoint.ScopeStack;
				ModifyCurrentConversationNode(Checkpoint.ClientBranchPoint);
			}
		}
		else if (TaskResult.Type == EConversationTaskResultType::ReturnToCurrentClientChoice)
		{
			if (ClientBranchPoints.Num() > 0)
			{
				FCheckpoint Checkpoint = ClientBranchPoints[ClientBranchPoints.Num() - 1];
				ClientBranchPoints.Remove(ClientBranchPoints.Num() - 1);
				ScopeStack = Checkpoint.ScopeStack;
				ModifyCurrentConversationNode(Checkpoint.ClientBranchPoint);
			}
		}
		else if (TaskResult.Type == EConversationTaskResultType::ReturnToConversationStart)
		{
			FGameplayTag EntryStartPointGameplayTagCache = StartingEntryGameplayTag;
			FConversationBranchPoint StartingBranchPointCache = StartingBranchPoint;
			
			CurrentBranchPoint = FConversationBranchPoint();
			CurrentBranchPoints.ResetNum();
			ClientBranchPoints.ResetNum();
			CurrentUserChoices.ResetNum();

			StartingEntryGameplayTag = EntryStartPointGameplayTagCache;
			StartingBranchPoint = StartingBranchPointCache;

			ModifyCurrentConversationNode(StartingBranchPoint);
		}
	}
	else
	{
		ServerAbortConversation();
	}
}

void UConversationInstance::ServerStartConversation(FGameplayTag Entry)
{
    StartingEntryGameplayTag = FGameplayTag();
    StartingBranchPoint = FConversationBranchPoint();
    CurrentBranchPoint = FConversationBranchPoint();
    CurrentBranchPoints.ResetNum();
    ClientBranchPoints.ResetNum();
    CurrentUserChoices.ResetNum();
    StartingEntryGameplayTag = Entry;
    
    UConversationRegistry* ConversationRegistry = ConversationRegistryFromWorld(GetWorld());
    TArray<FGuid> PotentialStartingPoints = ConversationRegistry->GetConnectedNodeIDs(Entry);
    if (PotentialStartingPoints.Num() == 0)
    {
        ServerAbortConversation();
        return;
    }

    TArray<FGuid> StartingPoints = ConstructBranches(PotentialStartingPoints, EConversationRequirementResult::FailedButVisible);

    if (StartingPoints.Num() == 0)
    {
        ServerAbortConversation();
        return;
    }
    
    FConversationBranchPoint StartingPoint{};
    StartingPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID = StartingPoints[
        UKismetMathLibrary::RandomIntegerInRange(0, StartingPoints.Num() - 1)];

    StartingBranchPoint = StartingPoint;
    CurrentBranchPoint = StartingPoint;

    for (FConversationParticipantEntry& ParticipantEntry : Participants.List)
    {
        if (ParticipantEntry.ParticipantComponent)
            ParticipantEntry.ParticipantComponent->ServerNotifyConversationStarted(this, ParticipantEntry.ParticipantID);
    }

    bConversationStarted = true;
}
