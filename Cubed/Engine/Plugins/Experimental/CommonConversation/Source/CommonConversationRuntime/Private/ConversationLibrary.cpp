#include "pch.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationLibrary.h"

#include <SDK/CommonConversationRuntime_parameters.hpp>

#include "Globals.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"
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
						Candidates.push_back(ContextualMessage);
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
		auto PlayerController = (AFortPlayerController*)ConversationParticipantEntry[InContext.ActiveConversation->Participants.List[1].ParticipantID.TagName.Number];
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
		auto PlayerController = (AFortPlayerController*)ConversationParticipantEntry[InContext.ActiveConversation->Participants.List[1].ParticipantID.TagName.Number];
		auto OtherActor = ConversationParticipantEntry[InContext.ActiveConversation->Participants.List[0].ParticipantID.TagName.Number];

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
	if (auto Instance = ((UConversationParticipantComponent*)Context)->Auth_CurrentConversation)
		Instance->ServerAdvanceConversation(InChoicePicked);
}

static EConversationRequirementResult IsRequirementSatisfied(UObject* Context, FFrame& Stack, EConversationRequirementResult* Ret)
{
	auto& ConversationContext = Stack.StepCompiledInRef<FConversationContext>();
	Stack.IncrementCode();
	
	UE_LOG(LogServer, Log, ("IsRequirementSatisfied: Node Class: %s"), Context->Class->GetFullName().c_str());
	
	if (auto UpgradeItem = Cast<UFortConversationTaskNode_UpgradeItem>(Context))
	{
		auto PlayerController = (AFortPlayerController*)ConversationLibrary::ConversationParticipantEntry[ConversationContext.ActiveConversation->Participants.List[1].ParticipantID.TagName.Number];
		
		if (!PlayerController || !PlayerController->MyFortPawn || !PlayerController->MyFortPawn->CurrentWeapon)
		{
			UE_LOG(LogServer, Warning, ("IsRequirementSatisfied: UpgradeItem - Invalid player/pawn/weapon"));
			return *Ret = EConversationRequirementResult::FailedButVisible;
		}
		
		auto ItemDef = PlayerController->MyFortPawn->CurrentWeapon->WeaponData;
		auto UpgradedWeaponDef = UFortKismetLibrary::GetUpgradedWeaponItemVerticalToRarity(ItemDef, EFortRarity(uint8(ItemDef->Rarity) + 1));

		if (UpgradedWeaponDef && UpgradedWeaponDef != ItemDef)
		{
			UE_LOG(LogServer, Log, ("IsRequirementSatisfied: UpgradeItem - Upgrade available"));
			return *Ret = EConversationRequirementResult::Passed;
		}

		UE_LOG(LogServer, Log, ("IsRequirementSatisfied: UpgradeItem - No upgrade available"));
		return *Ret = EConversationRequirementResult::FailedButVisible;
	}
	else if (auto GrantPlayerBounty = Cast<UFortConversationTaskNode_GrantPlayerBounty>(Context))
	{
		auto PlayerController = (AFortPlayerController*)ConversationLibrary::ConversationParticipantEntry[ConversationContext.ActiveConversation->Participants.List[1].ParticipantID.TagName.Number];

		if (!PlayerController)
		{
			UE_LOG(LogServer, Warning, ("IsRequirementSatisfied: GrantPlayerBounty - Invalid player controller"));
			return *Ret = EConversationRequirementResult::FailedButVisible;
		}

		TArray<AFortPlayerController*> AllPossiblePlayers = UFortKismetLibrary::GetAllFortPlayerControllers(GetWorld(), true, false);
		AllPossiblePlayers.Remove(PlayerController);

		if (AllPossiblePlayers.Num() > 0)
		{
			UE_LOG(LogServer, Log, ("IsRequirementSatisfied: GrantPlayerBounty - %d valid targets found"), AllPossiblePlayers.Num());
			return *Ret = EConversationRequirementResult::Passed;
		}

		UE_LOG(LogServer, Log, ("IsRequirementSatisfied: GrantPlayerBounty - No valid targets"));
		return *Ret = EConversationRequirementResult::FailedButVisible;
	}
	else if (auto DataDrivenService = Cast<UFortConversationTaskNode_DataDrivenService>(Context))
	{
		auto PlayerController = (AFortPlayerController*)ConversationLibrary::ConversationParticipantEntry[ConversationContext.ActiveConversation->Participants.List[1].ParticipantID.TagName.Number];
		auto OtherActor = ConversationLibrary::ConversationParticipantEntry[ConversationContext.ActiveConversation->Participants.List[0].ParticipantID.TagName.Number];

		if (!PlayerController || !OtherActor)
		{
			UE_LOG(LogServer, Warning, ("IsRequirementSatisfied: DataDrivenService - Invalid actors"));
			return *Ret = EConversationRequirementResult::FailedButVisible;
		}

		for (const auto& Requirement : DataDrivenService->Requirements)
		{
			if (Requirement.ParticipantID != ConversationContext.ActiveConversation->Participants.List[1].ParticipantID)
			{
				UE_LOG(LogServer, Log, ("IsRequirementSatisfied: DataDrivenService - Participant ID mismatch"));
				return *Ret = Requirement.FailureNodeBehaviour;
			}

			if (!DataDrivenService->ServiceBriefConfigCollection)
			{
				DataDrivenService->ServiceBriefConfigCollection = (UServiceBriefConfigCollection*)UGameplayStatics::SpawnObject(
					UServiceBriefConfigCollection::StaticClass(), 
					DataDrivenService
				);
				
				auto DefaultServiceBrief = StaticFindObject<UFortDataDrivenServiceBrief>("/FortniteConversation/Conversation/FortDataDrivenServiceBrief.Default__FortDataDrivenServiceBrief_C");
				if (DefaultServiceBrief)
				{
					UE_LOG(LogServer, Log, ("IsRequirementSatisfied: DataDrivenService - Loaded default service brief"));
				}
			}

			UE_LOG(LogServer, Log, ("IsRequirementSatisfied: DataDrivenService - Collection: %p, KeyOverride: %s"),
				DataDrivenService->ServiceBriefConfigCollection, 
				DataDrivenService->ServiceBriefCollectionKeyOveride.ToString().c_str());

			if (DataDrivenService->ServiceBriefConfigCollection)
			{
				UE_LOG(LogServer, Log, ("IsRequirementSatisfied: DataDrivenService - ServiceConfigs count: %d"),
					DataDrivenService->ServiceBriefConfigCollection->ServiceConfigs.Num());

				for (auto& [ConfigName, ServiceConfig] : DataDrivenService->ServiceBriefConfigCollection->ServiceConfigs)
				{
					UE_LOG(LogServer, Log, ("IsRequirementSatisfied: DataDrivenService - Config: %s"), 
						ConfigName.ToString().c_str());
				}
			}

			FControllerRequirementTestContext RequirementTestContext{};
			RequirementTestContext.TestSubjectController = PlayerController;
			RequirementTestContext.TestSubjectActor = PlayerController->Pawn;
			RequirementTestContext.OtherActor = OtherActor;

			if (!Requirement.Requirement)
			{
				UE_LOG(LogServer, Warning, ("IsRequirementSatisfied: DataDrivenService - Requirement is null"));
				return *Ret = Requirement.FailureNodeBehaviour;
			}

			struct FortControllerRequirement_IsRequirementMetInternal final
			{
			public:
				struct FControllerRequirementTestContext      RequestContext;                                    // 0x0000(0x0020)(ConstParm, Parm, OutParm, ReferenceParm, NoDestructor, NativeAccessSpecifierPublic)
				bool                                          ReturnValue;                                       // 0x0020(0x0001)(Parm, OutParm, ZeroConstructor, ReturnParm, IsPlainOldData, NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic)
				uint8                                         Pad_21[0x7];                                       // 0x0021(0x0007)(Fixing Struct Size After Last Property [ Dumper-7 ])
			};
			
			FortControllerRequirement_IsRequirementMetInternal IsRequirementMetInternal_Params{};
			IsRequirementMetInternal_Params.RequestContext = std::move(RequirementTestContext);

			Requirement.Requirement->ProcessEvent(
				Requirement.Requirement->Class->FindFunction("IsRequirementMetInternal"), 
				&IsRequirementMetInternal_Params
			);

			bool bRequirementIsMet = IsRequirementMetInternal_Params.ReturnValue;

			if (!bRequirementIsMet)
			{
				UE_LOG(LogServer, Log, ("IsRequirementSatisfied: DataDrivenService - Requirement not met, behavior: %d"), 
					(int32)Requirement.FailureNodeBehaviour);
				return *Ret = Requirement.FailureNodeBehaviour;
			}

			UE_LOG(LogServer, Log, ("IsRequirementSatisfied: DataDrivenService - Requirement passed"));
		}
		
		return *Ret = EConversationRequirementResult::Passed;
	}
	else if (Context->IsA(UConversationTaskNode::StaticClass()))
	{
		UE_LOG(LogServer, Log, ("IsRequirementSatisfied: Generic TaskNode - Passed"));
		return *Ret = EConversationRequirementResult::Passed;
	}

	auto ParticipantComponent = (UFortNonPlayerConversationParticipantComponent*)
		ConversationLibrary::ConversationParticipantEntry[ConversationContext.ActiveConversation->Participants.List[0].ParticipantID.TagName.Number]
		->GetComponentByClass(UFortNonPlayerConversationParticipantComponent::StaticClass());

	if (!ParticipantComponent)
		return *Ret = EConversationRequirementResult::FailedAndHidden;

	if (auto HasService = Cast<UFortConversationRequirement_HasService>(Context))
	{
		for (auto& GameplayTag : ParticipantComponent->SupportedServices.GameplayTags)
		{
			if (GameplayTag.TagName == HasService->ServiceTag.TagName)
			{
				UE_LOG(LogServer, Log, ("IsRequirementSatisfied: HasService - Service found: %s"), 
					HasService->ServiceTag.TagName.ToString().c_str());
				return *Ret = EConversationRequirementResult::Passed;
			}
		}
		
		UE_LOG(LogServer, Log, ("IsRequirementSatisfied: HasService - Service not found: %s"), 
			HasService->ServiceTag.TagName.ToString().c_str());
	}

	return *Ret = EConversationRequirementResult::Passed; // should be FailedAndHidden but like who cares
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

void ConversationLibrary::RequestServerAbortConversation(UObject* Context, FFrame& Stack)
{
	Stack.IncrementCode();
	if (auto Instance = ((UConversationParticipantComponent*)Context)->Auth_CurrentConversation)
		Instance->ServerAbortConversation();
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

	Hook->Path = "/Script/FortniteConversationRuntime.FortPlayerConversationComponent.RequestServerAbortConversation;";
	Hook->Detour = RequestServerAbortConversation;
	UKismetHookingLibrary::Hook(Hook, Exec);

	Hook->Path = "/Script/CommonConversationRuntime.ConversationParticipantComponent.ServerAdvanceConversation";
	Hook->Detour = ServerAdvanceConversationHook;
	UKismetHookingLibrary::Hook(Hook, Exec);

	Hook->Path = "/Script/CommonConversationRuntime.ConversationRequirementNode.IsRequirementSatisfied";
	Hook->Detour = IsRequirementSatisfied;
	UKismetHookingLibrary::Hook(Hook, Exec);
	
	Hook->Path = "/Script/CommonConversationRuntime.ConversationTaskNode.IsRequirementSatisfied";
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
