#include "pch.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationLibrary.h"

#include <SDK/CommonConversationRuntime_parameters.hpp>

#include "Globals.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Kismet/FortKismetLibrary.h"

FConversationTaskResult ConversationLibrary::ExecuteTaskNode(UConversationTaskNode* TaskNode, FConversationContext& InContext)
{
	Params::ConversationTaskNode_ExecuteTaskNode ExecuteTaskNode_Params{};
	ExecuteTaskNode_Params.Context = std::move(InContext);

	TaskNode->ProcessEvent(TaskNode->Class->FindFunction("ExecuteTaskNode"), &ExecuteTaskNode_Params);

	FConversationTaskResult Result = ExecuteTaskNode_Params.ReturnValue;

	if (auto SpeechNode = Cast<UFortConversationTaskNode_Speech>(TaskNode))
	{
		auto ParticipantComponent = (UFortNonPlayerConversationParticipantComponent*)InContext.ActiveConversation->Participants.List[0].InternalGetParticipantComponent();

		if (ParticipantComponent)
		{
			FGameplayTag& ParticipantID = InContext.ActiveConversation->Participants.List[1].ParticipantID; // maybe List[1] ???

			vector<FContextualMessageCandidate> Candidates;

			for (FContextualMessageCandidate& ContextualMessage : SpeechNode->GeneralConfig.ContextualMessages)
			{
				if (ContextualMessage.ContextRequirements.Num() == 0)
				{
					Candidates.push_back(ContextualMessage);
				}
				else if (ContextualMessage.RequirementMatchPolicy == EContextRequirementMatchPolicy::RequireAny)
				{
					for (FFortConversationContextRequirement& ContextRequirement : ContextualMessage.ContextRequirements)
					{
						if (ContextRequirement.ParticipantID == ParticipantID)
						{
							Candidates.push_back(ContextualMessage);
							break;
						}
					}
				}
				else if (ContextualMessage.RequirementMatchPolicy == EContextRequirementMatchPolicy::RequireAll)
				{
					for (FFortConversationContextRequirement& ContextRequirement : ContextualMessage.ContextRequirements)
					{
						if (ContextRequirement.ParticipantID != ParticipantID)
						{
							break;
						}

						Candidates.push_back(ContextualMessage);
					}
				}
			}

			FContextualMessageConfig* ConfigSearched = SpeechNode->SpeakerEntryTagToConfig.Search([&](FGameplayTag& Tag, FContextualMessageConfig&)
				{ return Tag == ParticipantComponent->ServiceProviderIDTag; } // shouldn't we be using SpeakerParticipantTag
			);

			if (!ConfigSearched)
			{
				UContextualMessageConfigCollection* ExternalContextualMessageCollection = SpeechNode->ExternalContextualMessageCollection;

				if (ExternalContextualMessageCollection)
				{
					ConfigSearched = ExternalContextualMessageCollection->SpeakerEntryTagToConfig.Search([&](FGameplayTag& Tag, FContextualMessageConfig&)
						{ return Tag == ParticipantComponent->ServiceProviderIDTag; } // shouldn't we be using SpeakerParticipantTag
					);
				}
			}

			if (ConfigSearched)
			{
				for (FContextualMessageCandidate& ContextualMessage : ConfigSearched->ContextualMessages)
				{
					if (ContextualMessage.ContextRequirements.Num() == 0)
					{
						Candidates.push_back(ContextualMessage);
					}
					else if (ContextualMessage.RequirementMatchPolicy == EContextRequirementMatchPolicy::RequireAny)
					{
						for (FFortConversationContextRequirement& ContextRequirement : ContextualMessage.ContextRequirements)
						{
							if (ContextRequirement.ParticipantID == ParticipantID)
							{
								Candidates.push_back(ContextualMessage);
								break;
							}
						}
					}
					else if (ContextualMessage.RequirementMatchPolicy == EContextRequirementMatchPolicy::RequireAll)
					{
						for (FFortConversationContextRequirement& ContextRequirement : ContextualMessage.ContextRequirements)
						{
							if (ContextRequirement.ParticipantID != ParticipantID)
							{
								break;
							}

							Candidates.push_back(ContextualMessage);
						}
					}
				}
			}

			if (Candidates.size() > 0)
			{
				FContextualMessageCandidate Candidate = PickWeighted(Candidates, [](float Total) { return ((float)rand() / 32767.f) * Total; });

				Result.Message.Text = Candidate.Message;
				Result.Message.MetadataParameters = Candidate.MetadataParameters;

				return Result;
			}
		}
		
		TArray<FConversationNodeParameterPair> Pairs;
		FConversationNodeParameterPair Pair;
		Pair.Name = L"Style";
		Pair.Value = L"Shock";
		Pairs.Add(Pair);
		Result.Message.Text = UKismetTextLibrary::Conv_StringToText(L"Failed to find the correct text to use??");
		Result.Message.MetadataParameters = Pairs;
	}
	else if (auto UpgradeItem = Cast<UFortConversationTaskNode_UpgradeItem>(TaskNode))
	{
		auto PlayerController = (AFortPlayerController*)InContext.ActiveConversation->Participants.List[1].Actor;
		auto ItemDef = PlayerController->MyFortPawn->CurrentWeapon->WeaponData;

		auto UpgradedWeaponDef = UFortKismetLibrary::GetUpgradedWeaponItemVerticalToRarity(ItemDef, EFortRarity(uint8(ItemDef->Rarity) + 1) /* crazy ass cast */);

		if (UpgradedWeaponDef && UpgradedWeaponDef != ItemDef)
		{
			auto ItemEntry = PlayerController->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& Entry)
				{ return Entry.ItemGuid == PlayerController->MyFortPawn->CurrentWeapon->ItemEntryGuid; });
			auto NewItemEntry = FortKismetLibrary::ConstructItemEntry(UpgradedWeaponDef, 1, 0);
			NewItemEntry.LoadedAmmo = ItemEntry->LoadedAmmo;
			PlayerController->WorldInventory->Remove(ItemEntry->ItemGuid);
			PlayerController->WorldInventory->AddItem(NewItemEntry.ItemDefinition, NewItemEntry.Count, NewItemEntry.LoadedAmmo);
		}
	}
	else if (auto DataDrivenService = Cast<UFortConversationTaskNode_DataDrivenService>(TaskNode))
	{
		auto PlayerController = (AFortPlayerController*)InContext.ActiveConversation->Participants.List[1].Actor;
		auto OtherActor = InContext.ActiveConversation->Participants.List[0].Actor;

		for (const auto& EffectRecipientConfig : DataDrivenService->EffectRecipientConfigs)
		{
			if (EffectRecipientConfig.Recipient != EDataDrivenEffectRecipient::Player)
				continue;

			FEffectRequestContext EffectRequestContext{};
			EffectRequestContext.EffectReceivingController = PlayerController;
			EffectRequestContext.EffectReceivingActor = PlayerController->Pawn;
			EffectRequestContext.InstigatingActor = OtherActor;

			for (const auto& Effect : EffectRecipientConfig.Effects)
			{
				if (!Effect)
					continue;

				Effect->ApplyEffect(EffectRequestContext);
			}
		}
	}

	return Result;
}

FConversationTaskResult ConversationLibrary::ExecuteTaskNodeWithSideEffects(UConversationTaskNode* Node, FConversationContext& InContext)
{
    TGuardValue<UObject*> Swapper(Node->EvalWorldContextObj, UWorld::GetWorld());

    auto Result = ExecuteTaskNode(Node, InContext);

    if (Result.Type != EConversationTaskResultType::Invalid && Result.Type != EConversationTaskResultType::AbortConversation)
    {
        for (UConversationNode* SubNode : Node->SubNodes)
        {
            if (UConversationSideEffectNode* SideEffectNode = Cast<UConversationSideEffectNode>(SubNode))
            {
                SideEffectNode->ServerCauseSideEffect(InContext);
            }
        }

        FConversationParticipants Participants = InContext.ActiveConversation->Participants;
        for (FConversationParticipantEntry& ParticipantEntry : Participants.List)
        {
        	if (ParticipantEntry.ParticipantComponent)
        	{
        		if (ParticipantEntry.InternalGetParticipantComponent()->GetOwner()->GetRemoteRole() == ENetRole::ROLE_AutonomousProxy)
        		{
        			if (ParticipantEntry.InternalGetParticipantComponent()->GetOwner()->GetLocalRole() == ENetRole::ROLE_Authority)
        				ParticipantEntry.InternalGetParticipantComponent()->ClientExecuteTaskAndSideEffects(InContext.ActiveConversation->CurrentBranchPoint.ClientChoice.ChoiceReference.NodeReference);
        		}
        	}
        }
    }

    return Result;
}


UConversationInstance* ConversationLibrary::StartConversation(UObject*, FFrame& Stack, UConversationInstance** Ret)
{
    FGameplayTag ConversationEntryTag;
    AActor* Instigator;
    FGameplayTag InstigatorTag;
    AActor* Target;
    FGameplayTag TargetTag;
    Stack.StepCompiledIn(&ConversationEntryTag);
    Stack.StepCompiledIn(&Instigator);
    Stack.StepCompiledIn(&InstigatorTag);
    Stack.StepCompiledIn(&Target);
    Stack.StepCompiledIn(&TargetTag);
    Stack.IncrementCode();

    if (!Instigator || !Target)
        return *Ret = nullptr;
    
    UE_LOG(LogServer, Log, "Starting conversation %s between %s and %s", 
        ConversationEntryTag.TagName.ToString().c_str(), 
        Instigator->GetName().c_str(), 
        Target->GetName().c_str());
    
    UClass* Class = UConversationSettings::GetDefaultObj()->ConversationInstanceClass;
    if (Class == nullptr)
        Class = UConversationInstance::StaticClass();

    UConversationInstance* Instance = (UConversationInstance*)UGameplayStatics::SpawnObject(Class, GetWorld());
    if (Instance)
    {
        FConversationContext ConversationContext = Instance->ConstructServerContext(nullptr);
        UConversationContextHelpers::MakeConversationParticipant(ConversationContext, Target, TargetTag);
        UConversationContextHelpers::MakeConversationParticipant(ConversationContext, Instigator, InstigatorTag);
        Instance->ServerStartConversation(ConversationEntryTag);
    }

    return *Ret = Instance;
}

static void ServerAdvanceConversationHook(UObject* Context, FFrame& Stack)
{
	FAdvanceConversationRequest InChoicePicked;
	Stack.StepCompiledIn(&InChoicePicked);
	Stack.IncrementCode();
	((UConversationParticipantComponent*)Context)->Auth_CurrentConversation->ServerAdvanceConversation(InChoicePicked);
}

static EConversationRequirementResult IsRequirementSatisfied(UObject* Context, FFrame& Stack, EConversationRequirementResult* Ret)
{
	auto& ConversationContext = Stack.StepCompiledInRef<FConversationContext>();
	Stack.IncrementCode();
	
	if (Context->IsA(UConversationTaskNode::StaticClass()))
		return EConversationRequirementResult::Passed;

	auto ParticipantComponent =
		(UFortNonPlayerConversationParticipantComponent*)
		ConversationContext.ActiveConversation->Participants.List[0].Actor->GetComponentByClass(UFortNonPlayerConversationParticipantComponent::StaticClass());

	if (!ParticipantComponent)
		return *Ret = EConversationRequirementResult::FailedAndHidden;

	if (auto HasService = Cast<UFortConversationRequirement_HasService>(Context))
	{
		for (auto& GameplayTag : ParticipantComponent->SupportedServices.GameplayTags)
		{
			if (GameplayTag.TagName == HasService->ServiceTag.TagName)
				return *Ret = EConversationRequirementResult::Passed;
		}
	}

	return *Ret = EConversationRequirementResult::FailedAndHidden;
}

namespace ConversationLibrary
{
	xmap<uint32, AActor*> ConversationParticipantEntry;
}

void ConversationLibrary::MakeConversationParticipant(UObject *, FFrame& Stack)
{
	auto& ConversationContext = Stack.StepCompiledInRef<FConversationContext>();
	AActor* ParticipantActor;
	FGameplayTag ParticipantID; 
	Stack.StepCompiledIn(&ParticipantActor);
	Stack.StepCompiledIn(&ParticipantID);
	Stack.IncrementCode();
	if (!IsValidPointer(ParticipantActor) || ParticipantID.TagName.ComparisonIndex <= 0) 
		return;

	if (UConversationInstance* Instance = ConversationContext.ActiveConversation)
	{
		if (ParticipantID.TagName.ComparisonIndex <= 0 || !Instance)
			return;

		Instance->ServerRemoveParticipant(ParticipantID);

		FConversationParticipantEntry NewEntry = FConversationParticipantEntry();
		NewEntry.ParticipantID = ParticipantID;
	//	NewEntry.Actor = ParticipantActor;
		Instance->Participants.List.Add(NewEntry);
		ConversationParticipantEntry[ParticipantID.TagName.Number] = ParticipantActor; // for some reason when i set the one above it closed so Proper!

		if (Instance->bConversationStarted)
		{
			if (UConversationParticipantComponent* ParticipantComponent = NewEntry.ParticipantComponent)
				ParticipantComponent->ServerNotifyConversationStarted(Instance, ParticipantID);
		}
	}
}

void ConversationLibrary::AdvanceConversation(UObject*, FFrame& Stack, FConversationTaskResult* Ret)
{
	auto& ConversationContext = Stack.StepCompiledInRef<FConversationContext>();
	Stack.IncrementCode();

	FConversationTaskResult OutResult;
	OutResult.Type = EConversationTaskResultType::AdvanceConversation;
	OutResult.AdvanceToChoice = FAdvanceConversationRequest();
	OutResult.Message = FClientConversationMessage();
	*Ret = OutResult;
}

void ConversationLibrary::AdvanceConversationWithChoice(UObject*, FFrame& Stack, FConversationTaskResult* Ret)
{
	auto& ConversationContext = Stack.StepCompiledInRef<FConversationContext>();
	auto& Choice = Stack.StepCompiledInRef<FAdvanceConversationRequest>();
	Stack.IncrementCode();

	FConversationTaskResult OutResult;
	OutResult.Type = EConversationTaskResultType::AdvanceConversationWithChoice;
	OutResult.AdvanceToChoice = Choice;
	OutResult.Message = FClientConversationMessage();
	*Ret = OutResult;
}

void ConversationLibrary::PauseConversationAndSendClientChoices(UObject*, FFrame& Stack, FConversationTaskResult* Ret)
{
	auto& ConversationContext = Stack.StepCompiledInRef<FConversationContext>();
	auto& Message = Stack.StepCompiledInRef<FClientConversationMessage>();
	Stack.IncrementCode();

	FConversationTaskResult OutResult;
	OutResult.Type = EConversationTaskResultType::PauseConversationAndSendClientChoices;
	OutResult.AdvanceToChoice = FAdvanceConversationRequest();
	OutResult.Message = Message;
	*Ret = OutResult;
}

void ConversationLibrary::ReturnToLastClientChoice(UObject*, FFrame& Stack, FConversationTaskResult* Ret)
{
	auto& ConversationContext = Stack.StepCompiledInRef<FConversationContext>();
	Stack.IncrementCode();

	FConversationTaskResult OutResult;
	OutResult.Type = EConversationTaskResultType::ReturnToLastClientChoice;
	OutResult.AdvanceToChoice = FAdvanceConversationRequest();
	OutResult.Message = FClientConversationMessage();
	*Ret = OutResult;
}

void ConversationLibrary::ReturnToCurrentClientChoice(UObject*, FFrame& Stack, FConversationTaskResult* Ret)
{
	auto& ConversationContext = Stack.StepCompiledInRef<FConversationContext>();
	Stack.IncrementCode();

	FConversationTaskResult OutResult;
	OutResult.Type = EConversationTaskResultType::ReturnToCurrentClientChoice;
	OutResult.AdvanceToChoice = FAdvanceConversationRequest();
	OutResult.Message = FClientConversationMessage();
	*Ret = OutResult;
}

void ConversationLibrary::ReturnToConversationStart(UObject*, FFrame& Stack, FConversationTaskResult* Ret)
{
	auto& ConversationContext = Stack.StepCompiledInRef<FConversationContext>();
	Stack.IncrementCode();

	FConversationTaskResult OutResult;
	OutResult.Type = EConversationTaskResultType::ReturnToConversationStart;
	OutResult.AdvanceToChoice = FAdvanceConversationRequest();
	OutResult.Message = FClientConversationMessage();
	*Ret = OutResult;
}

void ConversationLibrary::AbortConversation(UObject*, FFrame& Stack, FConversationTaskResult* Ret)
{
	auto& ConversationContext = Stack.StepCompiledInRef<FConversationContext>();
	Stack.IncrementCode();

	FConversationTaskResult OutResult;
	OutResult.Type = EConversationTaskResultType::AbortConversation;
	OutResult.AdvanceToChoice = FAdvanceConversationRequest();
	OutResult.Message = FClientConversationMessage();
	*Ret = OutResult;
}

static FConversationTaskResult ExecuteTaskNodeWithSideEffectsHook(UConversationTaskNode* TaskNode, FConversationTaskResult* Ret, FConversationContext& InContext)
{
	if (Ret)
		return *Ret = ConversationLibrary::ExecuteTaskNodeWithSideEffects(TaskNode, InContext);

	return ConversationLibrary::ExecuteTaskNodeWithSideEffects(TaskNode, InContext);
}

void ConversationLibrary::Setup()
{
    UHook* Hook = new UHook();

    Hook->Path = "/Script/CommonConversationRuntime.ConversationLibrary.StartConversation";
    Hook->Detour = StartConversation;
    UKismetHookingLibrary::Hook(Hook, Exec);

	Hook->Address = ImageBase + 0x3DC8768;
	Hook->Detour = ExecuteTaskNodeWithSideEffectsHook;
	UKismetHookingLibrary::Hook(Hook, Address);

	Hook->Path = "/Script/CommonConversationRuntime.ConversationParticipantComponent.ServerAdvanceConversation";
	Hook->Detour = ServerAdvanceConversationHook;
	UKismetHookingLibrary::Hook(Hook, Exec);

	Hook->Path = "/Script/CommonConversationRuntime.ConversationRequirementNode.IsRequirementSatisfied";
	Hook->Detour = IsRequirementSatisfied;
	UKismetHookingLibrary::Hook(Hook, Exec);

	Hook->Path = "/Script/CommonConversationRuntime.ConversationContextHelpers.MakeConversationParticipant";
	Hook->Detour = MakeConversationParticipant;
	UKismetHookingLibrary::Hook(Hook, Exec);

	Hook->Path = "/Script/CommonConversationRuntime.ConversationContextHelpers.AdvanceConversation";
	Hook->Detour = AdvanceConversation;
	UKismetHookingLibrary::Hook(Hook, Exec);

	Hook->Path = "/Script/CommonConversationRuntime.ConversationContextHelpers.AdvanceConversationWithChoice";
	Hook->Detour = AdvanceConversationWithChoice;
	UKismetHookingLibrary::Hook(Hook, Exec);

	Hook->Path = "/Script/CommonConversationRuntime.ConversationContextHelpers.PauseConversationAndSendClientChoices";
	Hook->Detour = PauseConversationAndSendClientChoices;
	UKismetHookingLibrary::Hook(Hook, Exec);

	Hook->Path = "/Script/CommonConversationRuntime.ConversationContextHelpers.ReturnToLastClientChoice";
	Hook->Detour = ReturnToLastClientChoice;
	UKismetHookingLibrary::Hook(Hook, Exec);

	Hook->Path = "/Script/CommonConversationRuntime.ConversationContextHelpers.ReturnToCurrentClientChoice";
	Hook->Detour = ReturnToCurrentClientChoice;
	UKismetHookingLibrary::Hook(Hook, Exec);

	Hook->Path = "/Script/CommonConversationRuntime.ConversationContextHelpers.ReturnToConversationStart";
	Hook->Detour = ReturnToConversationStart;
	UKismetHookingLibrary::Hook(Hook, Exec);

	Hook->Path = "/Script/CommonConversationRuntime.ConversationContextHelpers.AbortConversation";
	Hook->Detour = AbortConversation;
	UKismetHookingLibrary::Hook(Hook, Exec);

    delete Hook;
}
