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
		auto ParticipantComponent = (UFortNonPlayerConversationParticipantComponent*)InContext.ActiveConversation->Participants.List[0].ParticipantComponent;

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
            if (ParticipantEntry.ParticipantComponent->GetOwner()->GetRemoteRole() == ENetRole::ROLE_AutonomousProxy)
            {
                if (ParticipantEntry.ParticipantComponent->GetOwner()->GetLocalRole() == ENetRole::ROLE_Authority)
                    ParticipantEntry.ParticipantComponent->ClientExecuteTaskAndSideEffects(InContext.ActiveConversation->CurrentBranchPoint.ClientChoice.ChoiceReference.NodeReference);
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

void ConversationLibrary::Setup()
{
    UHook* Hook = new UHook();

    Hook->Path = "/Script/CommonConversationRuntime.ConversationLibrary.StartConversation";
    Hook->Detour = StartConversation;
    UKismetHookingLibrary::Hook(Hook, Exec);

    delete Hook;
}
