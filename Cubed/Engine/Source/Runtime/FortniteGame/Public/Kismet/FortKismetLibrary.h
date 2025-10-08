#pragma once
#include "pch.h"

namespace FortKismetLibrary 
{
	TArray<FFortItemEntry> PickLootDrops(FName TierGroupName, int WorldLevel = 0);
	AFortPickupAthena* SpawnPickup(FVector Loc, FFortItemEntry* Entry, EFortPickupSourceTypeFlag SourceTypeFlag, EFortPickupSpawnSource SpawnSource, class AFortPlayerPawn* Pawn, int OverrideCount, bool Toss, bool RandomRotation, bool bCombine);
}
