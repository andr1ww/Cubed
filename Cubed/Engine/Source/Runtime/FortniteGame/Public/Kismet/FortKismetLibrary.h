#pragma once
#include "pch.h"

static EFortQuickBars GetQuickbar(UFortItemDefinition *ItemDefinition)
{
	if (!ItemDefinition)
		return EFortQuickBars::Max_None;
	if (!ItemDefinition->IsA(UFortWeaponMeleeItemDefinition::StaticClass()) &&
		!ItemDefinition->IsA(UFortBuildingItemDefinition::StaticClass()) &&
		!ItemDefinition->IsA(UFortAmmoItemDefinition::StaticClass()) &&
		!ItemDefinition->IsA(UFortResourceItemDefinition::StaticClass()) &&
		!ItemDefinition->IsA(UFortTrapItemDefinition::StaticClass()))
	{
		return EFortQuickBars::Primary;
	}

	return EFortQuickBars::Secondary;
}

namespace FortKismetLibrary 
{
	FFortItemEntry ConstructItemEntry(UFortItemDefinition* ItemDefinition, int32 Count, int32 Level);
	
	TArray<FFortItemEntry> PickLootDrops(FName TierGroupName, int WorldLevel = 0);
	AFortPickupAthena* SpawnPickup(FVector Loc, FFortItemEntry Entry, EFortPickupSourceTypeFlag SourceTypeFlag, EFortPickupSpawnSource SpawnSource, class AFortPlayerPawn* Pawn, int OverrideCount, bool Toss, bool RandomRotation, bool bCombine);
}
