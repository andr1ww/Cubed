#include "pch.h"
#include "Engine/Source/Runtime/GameplayAbilities/Public/AbilitySystemComponent.h"

#include "Offsets.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/CoreUObject/Public/Templates/Casts.h"

void AbilitySystemComponent::InternalServerTryActivateAbility(UAbilitySystemComponent* Component, FGameplayAbilitySpecHandle Handle, bool, FPredictionKey& PredictionKey, FGameplayEventData* TriggerEventData)
{
    auto Spec = Component->ActivatableAbilities.Items.Search([Handle](FGameplayAbilitySpec &item)
                                                               { return item.Handle.Handle == Handle.Handle; });
    if (!Spec)
        return Component->ClientActivateAbilityFailed(Handle, PredictionKey.Current);
    Spec->InputPressed = true;

    UGameplayAbility *AbilityOut = nullptr;
    if (!((bool (*)(UAbilitySystemComponent *AbilitySystemComp, FGameplayAbilitySpecHandle, FPredictionKey, UGameplayAbility **, void *, const FGameplayEventData *))(ImageBase + Runtime::Offsets::InternalTryActivateAbility))(Component, Handle, PredictionKey, &AbilityOut, nullptr, TriggerEventData))
    {
        Component->ClientActivateAbilityFailed(Handle, PredictionKey.Current);
        Spec->InputPressed = false;
    }
    Component->ActivatableAbilities.MarkItemDirty(*Spec);    
}

void AbilitySystemComponent::Setup()
{
    UHook* Hook = new UHook();

    Hook->Address = Runtime::Vfts::InternalServerTryActivateAbility;
    Hook->Class = UAbilitySystemComponent::StaticClass();
    Hook->Detour = InternalServerTryActivateAbility;
    UKismetHookingLibrary::Hook(Hook, EHook::EveryVFT);
}