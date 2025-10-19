#include "pch.h"

#include "Globals.h"
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
			UE_LOG(LogServer, Log, ("AddChoice: Invalid GUID, using TaskBeingConsidered GUID"));
			BranchPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID = InContext.TaskBeingConsidered->Compiled_NodeGUID;
		}
		BranchPoint.ReturnScopeStack = InContext.ReturnScopeStack;

		BranchPoints.Add(BranchPoint);
		UE_LOG(LogServer, Log, ("AddChoice: Added branch point, total count: %d"), BranchPoints.Num());
	}
};

UConversationParticipantComponent* FConversationParticipantEntry::InternalGetParticipantComponent()
{
	auto A = ConversationLibrary::ConversationParticipantEntry[ParticipantID.TagName.Number];
    if (A)
	{
    	UE_LOG(LogServer, Log, ("InternalGetParticipantComponent: Found actor for participant ID: %s"), ParticipantID.TagName.ToString().c_str());
        return (UConversationParticipantComponent*)A->GetComponentByClass(UConversationParticipantComponent::StaticClass());
	}

    return nullptr;
}

FConversationContext UConversationInstance::ConstructServerContext(UConversationTaskNode* InTaskBeingConsidered)
{
	UE_LOG(LogServer, Log, ("ConstructServerContext: Building context for task: %s"), 
		InTaskBeingConsidered ? InTaskBeingConsidered->GetName().c_str() : ("None"));
	
    FConversationContext Context;
    Context.ActiveConversation = this;
    Context.TaskBeingConsidered = InTaskBeingConsidered;
    Context.bServer = true;
    Context.ConversationRegistry = ConversationRegistryFromWorld(GetWorld());

	UE_LOG(LogServer, Log, ("ConstructServerContext: Context created, Registry: %s"), 
		Context.ConversationRegistry ? ("Valid") : ("Invalid"));

    return Context;
}

void UConversationInstance::ServerRemoveParticipant(FGameplayTag ParticipantID)
{
	UE_LOG(LogServer, Log, ("ServerRemoveParticipant: Removing participant: %s"), ParticipantID.TagName.ToString().c_str());
	
    for (int i = 0; i < Participants.List.Num(); i++)
    {
        auto& Participant = Participants.List[i];
        if (Participant.ParticipantID.TagName == ParticipantID.TagName)
        {
			UE_LOG(LogServer, Log, ("ServerRemoveParticipant: Found participant at index %d"), i);
			
            if (bConversationStarted)
			{
				UE_LOG(LogServer, Log, ("ServerRemoveParticipant: Notifying component of conversation end"));
                Participant.InternalGetParticipantComponent()->ServerNotifyConversationEnded(this);
			}

            Participants.List.Remove(i);
			UE_LOG(LogServer, Log, ("ServerRemoveParticipant: Participant removed, remaining count: %d"), Participants.List.Num());
            break;
        }
    }
}

void UConversationInstance::ServerAbortConversation()
{
	UE_LOG(LogServer, Warning, ("ServerAbortConversation: Aborting conversation"));
	
    if (!this)
		return;
    	
    if (bConversationStarted)
    {
		UE_LOG(LogServer, Log, ("ServerAbortConversation: Conversation was started, removing %d participants"), Participants.List.Num());
		
        TArray<FConversationParticipantEntry> ListCopy = Participants.List;
        for (const FConversationParticipantEntry& ParticipantEntry : ListCopy)
		{
			UE_LOG(LogServer, Log, ("ServerAbortConversation: Removing participant: %s"), ParticipantEntry.ParticipantID.TagName.ToString().c_str());
            ServerRemoveParticipant(ParticipantEntry.ParticipantID);
		}
    }

    StartingEntryGameplayTag = FGameplayTag();
    StartingBranchPoint = FConversationBranchPoint();
    CurrentBranchPoint = FConversationBranchPoint();
    CurrentBranchPoints.ResetNum();
    ClientBranchPoints.ResetNum();
    bConversationStarted = false;
	
	UE_LOG(LogServer, Log, ("ServerAbortConversation: Conversation state reset"));
}

TArray<FGuid> UConversationInstance::ConstructBranches(const TArray<FGuid>& SourceList, EConversationRequirementResult MaximumRequirementResult)
{
	UE_LOG(LogServer, Log, ("ConstructBranches: Evaluating %d source nodes"), SourceList.Num());
	
    FConversationContext Context = ConstructServerContext(nullptr);

    TArray<FGuid> EnabledPaths;
    for (FGuid& TestGUID : SourceList)
    {
        UConversationNode* TestNode = GetRuntimeNodeFromGUID(ConversationRegistryFromWorld(GetWorld()), TestGUID);
        if (UConversationTaskNode* TaskNode = Cast<UConversationTaskNode>(TestNode))
        {
            const EConversationRequirementResult RequirementResult = CheckRequirements(TaskNode, Context);
			UE_LOG(LogServer, Log, ("ConstructBranches: Node %s requirement result: %d (max: %d)"), 
				TaskNode->GetName().c_str(), (int32)RequirementResult, (int32)MaximumRequirementResult);
			
            if ((int64)RequirementResult <= (int64)MaximumRequirementResult)
			{
                EnabledPaths.Add(TestGUID);
				UE_LOG(LogServer, Log, ("ConstructBranches: Node %s added to enabled paths"), TaskNode->GetName().c_str());
			}
        }
    }

	UE_LOG(LogServer, Log, ("ConstructBranches: Total enabled paths: %d"), EnabledPaths.Num());
    return EnabledPaths;
}

void UConversationInstance::ModifyCurrentConversationNode(FConversationBranchPoint& NewBranchPoint)
{
	UE_LOG(LogServer, Log, ("ModifyCurrentConversationNode: Modifying to new branch point"));
	
	CurrentBranchPoint = NewBranchPoint;

	for (auto& Handle : NewBranchPoint.ReturnScopeStack)
	{
		FConversationChoiceReference Ref;
		Ref.NodeReference = Handle;
		ScopeStack.Add(Ref);
		UE_LOG(LogServer, Log, ("ModifyCurrentConversationNode: Added to scope stack, size: %d"), ScopeStack.Num());
	}

	OnCurrentConversationNodeModified();
}

void UConversationInstance::ModifyCurrentConversationNode(const FConversationChoiceReference& NewChoice)
{
	UE_LOG(LogServer, Log, ("ModifyCurrentConversationNode: Modifying with choice reference"));
	
	FConversationBranchPoint BranchPoint;
	BranchPoint.ClientChoice.ChoiceReference = NewChoice;
	ModifyCurrentConversationNode(BranchPoint);
}

void UConversationInstance::ServerAdvanceConversation(FAdvanceConversationRequest InChoicePicked)
{
	UE_LOG(LogServer, Log, ("ServerAdvanceConversation: Advancing conversation"));
	UE_LOG(LogServer, Log, ("ServerAdvanceConversation: Conversation started: %s"), bConversationStarted ? ("Yes") : ("No"));
	
	if (bConversationStarted && UKismetGuidLibrary::IsValid_Guid(CurrentBranchPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID))
	{
		TArray<FConversationBranchPoint> CandidateDestinations;

		if (UKismetGuidLibrary::IsValid_Guid(InChoicePicked.Choice.NodeReference.NodeGUID))
		{
			UE_LOG(LogServer, Log, ("ServerAdvanceConversation: Valid choice GUID provided, searching branch points"));
			
			FConversationBranchPoint* BranchPoint = CurrentBranchPoints.Search([&](FConversationBranchPoint& Point) {
				return Point.ClientChoice.ChoiceType == EConversationChoiceType::ServerOnly ? false : Point.ClientChoice.ChoiceReference.NodeReference.NodeGUID == InChoicePicked.Choice.NodeReference.NodeGUID;
			});
			
			if (BranchPoint)
			{
				UE_LOG(LogServer, Log, ("ServerAdvanceConversation: Branch point found"));
				CandidateDestinations.ResetNum();
				CandidateDestinations.Add(*BranchPoint);
			}
			else
			{
				UE_LOG(LogServer, Warning, ("ServerAdvanceConversation: Branch point not found, aborting"));
				ServerAbortConversation();
				return;
			}
		}
		else
		{
			UE_LOG(LogServer, Log, ("ServerAdvanceConversation: No choice GUID, checking scope stack"));
			UE_LOG(LogServer, Log, ("ServerAdvanceConversation: Current branch points: %d, Scope stack: %d"), 
				CurrentBranchPoints.Num(), ScopeStack.Num());
			
			if (CurrentBranchPoints.Num() == 0 && ScopeStack.Num() > 0)
			{
				UE_LOG(LogServer, Log, ("ServerAdvanceConversation: Returning to scope"));
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
			UE_LOG(LogServer, Log, ("ServerAdvanceConversation: Candidate destinations: %d"), CandidateDestinations.Num());
		}

		TArray<FConversationBranchPoint> ValidDestinations;
		{
			FConversationContext Context = ConstructServerContext(nullptr);
			for (FConversationBranchPoint& BranchPoint : CandidateDestinations)
			{
				if (UConversationTaskNode* TaskNode = Cast<UConversationTaskNode>(GetRuntimeNodeFromGUID(Context.ConversationRegistry, BranchPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID)))
				{
					EConversationRequirementResult Result = TaskNode->bIgnoreRequirementsWhileAdvancingConversations ?  EConversationRequirementResult::Passed : CheckRequirements(TaskNode, Context);
					UE_LOG(LogServer, Log, ("ServerAdvanceConversation: Node %s requirement result: %d, IgnoreReqs: %s"), 
						TaskNode->GetName().c_str(), (int32)Result, TaskNode->bIgnoreRequirementsWhileAdvancingConversations ? ("Yes") : ("No"));
										
					if (Result == EConversationRequirementResult::Passed)
						ValidDestinations.Add(BranchPoint);
				}
				else
				{
					UE_LOG(LogServer, Log, ("ServerAdvanceConversation: Non-task node, adding to valid destinations"));
					ValidDestinations.Add(BranchPoint);
				}
			}
		}

		UE_LOG(LogServer, Log, ("ServerAdvanceConversation: Valid destinations: %d"), ValidDestinations.Num());

		if (ValidDestinations.Num() == 0)
		{
			UE_LOG(LogServer, Warning, ("ServerAdvanceConversation: No valid destinations, aborting"));
			ServerAbortConversation();
			return;
		}
		
		const int32 StartingIndex = UKismetMathLibrary::RandomIntegerInRange(0, ValidDestinations.Num() - 1);
		UE_LOG(LogServer, Log, ("ServerAdvanceConversation: Selected destination index: %d"), StartingIndex);
		
		FConversationBranchPoint& TargetChoice = ValidDestinations[StartingIndex];
		ModifyCurrentConversationNode(TargetChoice);
	}
	else
	{
		UE_LOG(LogServer, Warning, ("ServerAdvanceConversation: Conversation not started or invalid current branch point"));
	}
}

void GenerateChoicesForDestinations(FConversationBranchPointBuilder& BranchBuilder, FConversationContext& InContext, const TArray<FGuid>& CandidateDestinations)
{
	for (FGuid& DestinationGUID : CandidateDestinations)
	{
		if (UConversationTaskNode* DestinationTaskNode = Cast<UConversationTaskNode>(GetRuntimeNodeFromGUID(InContext.ConversationRegistry, DestinationGUID)))
		{
			TGuardValue<UObject*> Swapper(DestinationTaskNode->EvalWorldContextObj, UWorld::GetWorld());
			
			FConversationContext DestinationContext = InContext;
			DestinationContext.TaskBeingConsidered = DestinationTaskNode;

			const int32 StartingNumber = BranchBuilder.BranchPoints.Num();

			if (auto LinkNode = Cast<UConversationLinkNode>(DestinationTaskNode))
			{
				TArray<FGuid> PotentialStartingPoints = InContext.ConversationRegistry->GetEntryNodeIDs(LinkNode->RemoteEntryTag);
				TArray<FGuid> LegalStartingPoints = InContext.ActiveConversation->ConstructBranches(PotentialStartingPoints, EConversationRequirementResult::FailedButVisible);
				InContext.ReturnScopeStack.Add(FConversationNodeHandle(LinkNode->Compiled_NodeGUID));
				GenerateChoicesForDestinations(BranchBuilder, InContext, LegalStartingPoints);
			}
			
			static void (*GatherStaticChoicesOG)(UConversationTaskNode* Node, FConversationBranchPointBuilder&, FConversationContext& InContext) = decltype(GatherStaticChoicesOG)(DestinationTaskNode->VTable[0x56]);
			static void (*GatherDynamicChoicesOG)(UConversationTaskNode* Node, FConversationBranchPointBuilder&, FConversationContext& InContext) = decltype(GatherDynamicChoicesOG)(DestinationTaskNode->VTable[0x57]);
			GatherStaticChoicesOG(DestinationTaskNode, BranchBuilder, InContext);
			GatherDynamicChoicesOG(DestinationTaskNode, BranchBuilder, InContext);
			
		//	if (BranchBuilder.BranchPoints.Num() == StartingNumber)
			{
				const EConversationRequirementResult RequirementResult = /*CheckRequirements(DestinationTaskNode, InContext);*/EConversationRequirementResult::Passed;

				if (RequirementResult == EConversationRequirementResult::Passed)
				{
					FClientConversationOptionEntry DefaultChoice;
					DefaultChoice.ChoiceReference.NodeReference.NodeGUID = DestinationGUID;
					DefaultChoice.ChoiceType = EConversationChoiceType::ServerOnly;
					BranchBuilder.AddChoice(DestinationContext, DefaultChoice);
				}
			}
		}
	}
}

void UConversationInstance::UpdateNextChoices(FConversationContext& Context)
{
	UE_LOG(LogServer, Log, ("UpdateNextChoices: Updating available choices"));
	
	TArray<FConversationBranchPoint> AllChoices;

	if (UConversationTaskNode* TaskNode = Cast<UConversationTaskNode>(GetRuntimeNodeFromGUID(Context.ConversationRegistry, CurrentBranchPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID)))
	{
		UE_LOG(LogServer, Log, ("UpdateNextChoices: Current task node: %s"), TaskNode->GetName().c_str());
		
		FConversationContext ChoiceContext = Context;
		ChoiceContext.TaskBeingConsidered = TaskNode;

		TArray<FGuid> ArrayWithGUID;
		ArrayWithGUID.Add(CurrentBranchPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID);
		const TArray<FGuid> CandidateDestinations = ChoiceContext.ConversationRegistry->GetConnectedNodeIDs(ArrayWithGUID);
		
		UE_LOG(LogServer, Log, ("UpdateNextChoices: Found %d connected nodes"), CandidateDestinations.Num());

		FConversationBranchPointBuilder BranchBuilder;
		GenerateChoicesForDestinations(BranchBuilder, ChoiceContext, CandidateDestinations);
		AllChoices = BranchBuilder.BranchPoints;
		UE_LOG(LogServer, Log, ("UpdateNextChoices: Built %d branch points"), AllChoices.Num());
	}
	else
	{
		UE_LOG(LogServer, Warning, ("UpdateNextChoices: Current node is not a task node"));
	}

	CurrentUserChoices.ResetNum();
	CurrentBranchPoints = AllChoices;

	for (const FConversationBranchPoint& UserBranchPoint : CurrentBranchPoints)
	{
		if (UserBranchPoint.ClientChoice.ChoiceType != EConversationChoiceType::ServerOnly)
			CurrentUserChoices.Add(UserBranchPoint.ClientChoice);
	}

	if (CurrentBranchPoints.Num() > 0 || ScopeStack.Num() > 0)
	{
		if (CurrentUserChoices.Num() == 0)
		{
			FClientConversationOptionEntry DefaultChoice;
			DefaultChoice.ChoiceReference = FConversationChoiceReference();
			DefaultChoice.ChoiceText = UKismetTextLibrary::Conv_StringToText(L"Continue");
			DefaultChoice.ChoiceType = EConversationChoiceType::UserChoiceAvailable;
			CurrentUserChoices.Add(DefaultChoice);
		}
	}
}

void UConversationInstance::OnCurrentConversationNodeModified()
{
	UE_LOG(LogServer, Log, ("OnCurrentConversationNodeModified: Processing node modification"));
	
	FConversationContext AnonContext = ConstructServerContext(nullptr);
	UConversationNode* CurrentNode = GetRuntimeNodeFromGUID(AnonContext.ConversationRegistry, CurrentBranchPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID);
	
	if (UConversationTaskNode* TaskNode = Cast<UConversationTaskNode>(CurrentNode))
	{
		UE_LOG(LogServer, Log, ("OnCurrentConversationNodeModified: Executing task node: %s"), TaskNode->GetName().c_str());
		
		FConversationContext Context = AnonContext;
		Context.TaskBeingConsidered = TaskNode;

		FConversationTaskResult TaskResult = ConversationLibrary::ExecuteTaskNodeWithSideEffects(TaskNode, Context);
		UE_LOG(LogServer, Log, ("OnCurrentConversationNodeModified: Task result type: %d"), (int32)TaskResult.Type);

		if (ScopeStack.Num() > 0 && ScopeStack[ScopeStack.Num() - 1].NodeReference.NodeGUID == TaskNode->Compiled_NodeGUID)
		{
			UE_LOG(LogServer, Log, ("OnCurrentConversationNodeModified: Removing from scope stack"));
			ScopeStack.Remove(ScopeStack.Num() - 1);
		}
		
		UpdateNextChoices(Context);

		if (TaskResult.Type == EConversationTaskResultType::AbortConversation)
		{
			UE_LOG(LogServer, Warning, ("OnCurrentConversationNodeModified: Task requested abort"));
			ServerAbortConversation();
		}
		else if (TaskResult.Type == EConversationTaskResultType::AdvanceConversation)
		{
			UE_LOG(LogServer, Log, ("OnCurrentConversationNodeModified: Task requested advance"));
			ServerAdvanceConversation(FAdvanceConversationRequest());
		}
		else if (TaskResult.Type == EConversationTaskResultType::AdvanceConversationWithChoice)
		{
			UE_LOG(LogServer, Log, ("OnCurrentConversationNodeModified: Task requested advance with choice"));
			ModifyCurrentConversationNode(TaskResult.AdvanceToChoice.Choice);
		}
		else if (TaskResult.Type == EConversationTaskResultType::PauseConversationAndSendClientChoices)
		{
			UE_LOG(LogServer, Log, ("OnCurrentConversationNodeModified: Pausing and sending client choices"));
			UE_LOG(LogServer, Log, ("OnCurrentConversationNodeModified: Message: %s"), TaskResult.Message.Text.ToString().c_str());
			
			FClientConversationMessagePayload LastMessage = FClientConversationMessagePayload();
			LastMessage.Message = TaskResult.Message;
			LastMessage.CurrentNode = Context.ActiveConversation->CurrentBranchPoint.ClientChoice.ChoiceReference.NodeReference;
			LastMessage.Participants = Participants;
			LastMessage.Options = CurrentUserChoices;

			ClientBranchPoints.Add(FCheckpoint(CurrentBranchPoint, ScopeStack));
			UE_LOG(LogServer, Log, ("OnCurrentConversationNodeModified: Client branch points: %d"), ClientBranchPoints.Num());

			for (FConversationParticipantEntry& KVP : LastMessage.Participants.List)
			{
				if (UConversationParticipantComponent* ParticipantComponent = KVP.InternalGetParticipantComponent())
				{
					UE_LOG(LogServer, Log, ("OnCurrentConversationNodeModified: Updating participant: %s"), KVP.ParticipantID.TagName.ToString().c_str());
					ParticipantComponent->LastMessage = LastMessage;

					if (ParticipantComponent->GetOwner()->GetRemoteRole() == ENetRole::ROLE_AutonomousProxy)
					{
						UE_LOG(LogServer, Log, ("OnCurrentConversationNodeModified: Sending to client"));
						ParticipantComponent->ClientUpdateConversation(LastMessage);
					}
				}
			}
		}
		else if (TaskResult.Type == EConversationTaskResultType::ReturnToLastClientChoice)
		{
			UE_LOG(LogServer, Log, ("OnCurrentConversationNodeModified: Returning to last client choice"));
			
			if (ClientBranchPoints.Num() > 1)
			{
				ClientBranchPoints.Remove(ClientBranchPoints.Num() - 1);

				FCheckpoint& Checkpoint = ClientBranchPoints[ClientBranchPoints.Num() - 1];
				ScopeStack = Checkpoint.ScopeStack;
				UE_LOG(LogServer, Log, ("OnCurrentConversationNodeModified: Restored scope stack size: %d"), ScopeStack.Num());
				ModifyCurrentConversationNode(Checkpoint.ClientBranchPoint);
			}
			else
			{
				UE_LOG(LogServer, Warning, ("OnCurrentConversationNodeModified: Not enough client branch points to return"));
			}
		}
		else if (TaskResult.Type == EConversationTaskResultType::ReturnToCurrentClientChoice)
		{
			UE_LOG(LogServer, Log, ("OnCurrentConversationNodeModified: Returning to current client choice"));
			
			if (ClientBranchPoints.Num() > 0)
			{
				FCheckpoint Checkpoint = ClientBranchPoints[ClientBranchPoints.Num() - 1];
				ClientBranchPoints.Remove(ClientBranchPoints.Num() - 1);
				ScopeStack = Checkpoint.ScopeStack;
				UE_LOG(LogServer, Log, ("OnCurrentConversationNodeModified: Restored scope stack size: %d"), ScopeStack.Num());
				ModifyCurrentConversationNode(Checkpoint.ClientBranchPoint);
			}
			else
			{
				UE_LOG(LogServer, Warning, ("OnCurrentConversationNodeModified: No client branch points available"));
			}
		}
		else if (TaskResult.Type == EConversationTaskResultType::ReturnToConversationStart)
		{
			UE_LOG(LogServer, Log, ("OnCurrentConversationNodeModified: Returning to conversation start"));
			UE_LOG(LogServer, Log, ("OnCurrentConversationNodeModified: Starting entry tag: %s"), StartingEntryGameplayTag.TagName.ToString().c_str());
			
			FGameplayTag EntryStartPointGameplayTagCache = StartingEntryGameplayTag;
			FConversationBranchPoint StartingBranchPointCache = StartingBranchPoint;
			
			CurrentBranchPoint = FConversationBranchPoint();
			CurrentBranchPoints.ResetNum();
			ClientBranchPoints.ResetNum();

			StartingEntryGameplayTag = EntryStartPointGameplayTagCache;
			StartingBranchPoint = StartingBranchPointCache;

			ModifyCurrentConversationNode(StartingBranchPoint);
		}
	}
	else
	{
		UE_LOG(LogServer, Error, ("OnCurrentConversationNodeModified: Current node is not a task node, aborting"));
		ServerAbortConversation();
	}
}

void UConversationInstance::ServerStartConversation(FGameplayTag Entry)
{
	UE_LOG(LogServer, Log, ("ServerStartConversation: Starting conversation with entry: %s"), Entry.TagName.ToString().c_str());
	
    StartingEntryGameplayTag = FGameplayTag();
    StartingBranchPoint = FConversationBranchPoint();
    CurrentBranchPoint = FConversationBranchPoint();
    CurrentBranchPoints.ResetNum();
    ClientBranchPoints.ResetNum();
    StartingEntryGameplayTag = Entry;
    
    UConversationRegistry* ConversationRegistry = ConversationRegistryFromWorld(GetWorld());
	if (!ConversationRegistry)
	{
		UE_LOG(LogServer, Error, ("ServerStartConversation: Failed to get conversation registry"));
		ServerAbortConversation();
		return;
	}
	
    TArray<FGuid> PotentialStartingPoints = ConversationRegistry->GetConnectedNodeIDs(Entry);
	UE_LOG(LogServer, Log, ("ServerStartConversation: Found %d potential starting points"), PotentialStartingPoints.Num());
	
    if (PotentialStartingPoints.Num() == 0)
    {
		UE_LOG(LogServer, Warning, ("ServerStartConversation: No starting points found, aborting"));
        ServerAbortConversation();
        return;
    }

    TArray<FGuid> StartingPoints = ConstructBranches(PotentialStartingPoints, EConversationRequirementResult::FailedButVisible);
	UE_LOG(LogServer, Log, ("ServerStartConversation: Valid starting points after requirements: %d"), StartingPoints.Num());

    if (StartingPoints.Num() == 0)
    {
		UE_LOG(LogServer, Warning, ("ServerStartConversation: No valid starting points after requirements check, aborting"));
        ServerAbortConversation();
        return;
    }
    
    FConversationBranchPoint StartingPoint{};
	int32 SelectedIndex = UKismetMathLibrary::RandomIntegerInRange(0, StartingPoints.Num() - 1);
    StartingPoint.ClientChoice.ChoiceReference.NodeReference.NodeGUID = StartingPoints[SelectedIndex];
	UE_LOG(LogServer, Log, ("ServerStartConversation: Selected starting point index: %d"), SelectedIndex);

    StartingBranchPoint = StartingPoint;
    CurrentBranchPoint = StartingPoint;

	UE_LOG(LogServer, Log, ("ServerStartConversation: Notifying %d participants"), Participants.List.Num());
    for (FConversationParticipantEntry& ParticipantEntry : Participants.List)
    {
        if (ParticipantEntry.InternalGetParticipantComponent())
		{
			UE_LOG(LogServer, Log, ("ServerStartConversation: Notifying participant: %s"), ParticipantEntry.ParticipantID.TagName.ToString().c_str());
            ParticipantEntry.InternalGetParticipantComponent()->ServerNotifyConversationStarted(this, ParticipantEntry.ParticipantID);
		}
		else
		{
			UE_LOG(LogServer, Warning, ("ServerStartConversation: Participant component is null for: %s"), ParticipantEntry.ParticipantID.TagName.ToString().c_str());
		}
    }

    bConversationStarted = true;
	UE_LOG(LogServer, Log, ("ServerStartConversation: Conversation started successfully"));
	OnCurrentConversationNodeModified();
}