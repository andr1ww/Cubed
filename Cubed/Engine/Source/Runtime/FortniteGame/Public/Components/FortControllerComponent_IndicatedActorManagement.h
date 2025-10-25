#pragma once
#include "pch.h"

#include "Engine/Source/Runtime/CoreUObject/Public/UObject/Stack.h"

namespace FortControllerComponent_IndicatedActorManagement
{
    namespace Library
    {
        static void AddActorsToIndicatedList(UObject*, FFrame& Stack);
        static void AddActorsToStenciledList(UObject*, FFrame& Stack);
    }
    
    void Setup();
}
