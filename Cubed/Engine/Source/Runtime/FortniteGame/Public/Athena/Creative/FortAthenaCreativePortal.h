#pragma once
#include "pch.h"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/Stack.h"

namespace FortAthenaCreativePortal
{
	void TeleportPlayerToLinkedVolumeHook(UObject* Context, FFrame& Stack, void* Ret);
	
	void Setup();
}