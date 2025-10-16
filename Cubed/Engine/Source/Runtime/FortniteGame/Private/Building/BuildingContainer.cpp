#include "pch.h"

#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Building/BuildingContainer.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Kismet/FortKismetLibrary.h"

bool BuildingContainer::SpawnLoot(ABuildingContainer* Container) 
{
	for (auto Pair : GetGameMode()->RedirectAthenaLootTierGroups)
	{
		if (Pair.First == Container->SearchLootTierGroup)
		{
			Container->SearchLootTierGroup = Pair.Second;
			break;
		}
	}

	for (auto& Drop : FortKismetLibrary::PickLootDrops(Container->SearchLootTierGroup, GetGameState()->WorldLevel))
	{
		if (!Drop.ItemDefinition || Drop.Count <= 0)
			continue;
		FVector Loc = Container->K2_GetActorLocation() + (Container->GetActorForwardVector() * Container->LootSpawnLocation_Athena.X) + (Container->GetActorRightVector() * Container->LootSpawnLocation_Athena.Y) + (Container->GetActorUpVector() * Container->LootSpawnLocation_Athena.Z);
		FortKismetLibrary::SpawnPickup(
		Loc, 
		Drop, 
		EFortPickupSourceTypeFlag::Container, 
		EFortPickupSpawnSource::Unset, 
		nullptr, 
-1, true, true, true);
	}
	
	Container->bAlreadySearched = true;
	Container->OnRep_bAlreadySearched();
	Container->SearchBounceData.SearchAnimationCount++;
	Container->BounceContainer();
	
	return true;
}

void BuildingContainer::Setup() {
	UHook* Hook = new UHook();

	Hook->Address = ImageBase + 0x4C80DB4;
	Hook->Original = (void**)&SpawnLootOG;
	Hook->Detour = SpawnLoot;
	UKismetHookingLibrary::Hook(Hook, EHook::Address);

	free(Hook);
}