#pragma once
#include "pch.h"

namespace AbilitySystemComponent
{
    void InternalServerTryActivateAbility(UAbilitySystemComponent* Component, FGameplayAbilitySpecHandle, bool, FPredictionKey&, FGameplayEventData*);
    
    void Setup();
}