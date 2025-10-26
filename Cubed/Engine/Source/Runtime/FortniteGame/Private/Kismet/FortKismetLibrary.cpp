#include "pch.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Kismet/FortKismetLibrary.h"

#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Inventory/FortInventory.h"

TArray<UDataTable*> LootTierDatas;
TArray<UDataTable*> LootPackageDatas;

float WeightedRandom(float AllWeights)
{
	return (rand() * 0.000030518509f) * AllWeights;
}

int GetItemLevel(const FDataTableCategoryHandle& LootLevelData, int WorldLevel)
{
	auto DataTable = LootLevelData.DataTable;

	if (!DataTable)
		return 0;

	if (!LootLevelData.ColumnName.ComparisonIndex)
		return 0;

	if (!LootLevelData.RowContents.ComparisonIndex)
		return 0;

	std::vector<FFortLootLevelData*> OurLootLevelDatas;

	for (auto& LootLevelDataPair : LootLevelData.DataTable->RowMap)
	{
		if (((FFortLootLevelData*)(LootLevelDataPair.Second))->Category != LootLevelData.RowContents)
			continue;

		OurLootLevelDatas.push_back(((FFortLootLevelData*)(LootLevelDataPair.Second)));
	}

	if (OurLootLevelDatas.size() > 0)
	{
		int PickedIndex = -1;
		int PickedLootLevel = 0;

		for (int i = 0; i < OurLootLevelDatas.size(); i++)
		{
			auto CurrentLootLevelData = OurLootLevelDatas.at(i);

			if (CurrentLootLevelData->LootLevel <= WorldLevel && CurrentLootLevelData->LootLevel > PickedLootLevel)
			{
				PickedLootLevel = CurrentLootLevelData->LootLevel;
				PickedIndex = i;
			}
		}

		if (PickedIndex != -1)
		{
			auto PickedLootLevelData = OurLootLevelDatas.at(PickedIndex);

			const auto PickedMinItemLevel = PickedLootLevelData->MinItemLevel;
			const auto PickedMaxItemLevel = PickedLootLevelData->MaxItemLevel;
			auto v15 = PickedMaxItemLevel - PickedMinItemLevel;

			if (v15 + 1 <= 0)
			{
				v15 = 0;
			}
			else
			{
				auto v16 = (int)(float)((float)((float)rand() * 0.000030518509) * (float)(v15 + 1));
				if (v16 <= v15)
					v15 = v16;
			}

			return v15 + PickedMinItemLevel;
		}
	}

	return 0;
}

int PickLevel(UFortWorldItemDefinition* def, int PreferredLevel)
{

	const int MinLevel = def->MinLevel;
	const int MaxLevel = def->MaxLevel;

	int PickedLevel = 0;

	if (PreferredLevel >= MinLevel)
		PickedLevel = PreferredLevel;

	if (MaxLevel >= 0)
	{
		if (PickedLevel <= MaxLevel)
			return PickedLevel;
		return MaxLevel;
	}

	return PickedLevel;
}

float GetAmountOfLootPackagesToDrop(FFortLootTierData* LootTierData, int OriginalNumberLootDrops)
{
	if (LootTierData->LootPackageCategoryMinArray.Num() != LootTierData->LootPackageCategoryWeightArray.Num() || LootTierData->LootPackageCategoryMinArray.Num() != LootTierData->LootPackageCategoryWeightArray.Num())
		return 0;

	float MinimumLootDrops = 0;

	if (LootTierData->LootPackageCategoryMinArray.Num() > 0)
	{
		for (int i = 0; i < LootTierData->LootPackageCategoryMinArray.Num(); ++i)
		{
			MinimumLootDrops += LootTierData->LootPackageCategoryMinArray[i];
		}
	}

	int SumLootPackageCategoryWeightArray = 0;

	if (LootTierData->LootPackageCategoryWeightArray.Num() > 0)
	{
		for (int i = 0; i < LootTierData->LootPackageCategoryWeightArray.Num(); ++i)
		{
			if (LootTierData->LootPackageCategoryWeightArray[i] > 0)
			{
				auto LootPackageCategoryMaxArrayIt = LootTierData->LootPackageCategoryMaxArray[i];

				float IDK = 0;

				if (LootPackageCategoryMaxArrayIt < 0 || IDK < LootPackageCategoryMaxArrayIt)
				{
					SumLootPackageCategoryWeightArray += LootTierData->LootPackageCategoryWeightArray[i];
				}
			}
		}
	}

	while (SumLootPackageCategoryWeightArray > 0)
	{
		float v29 = (float)rand() * 0.000030518509f;

		float v35 = (int)(float)((float)((float)((float)SumLootPackageCategoryWeightArray * v29)
			+ (float)((float)SumLootPackageCategoryWeightArray * v29))
			+ 0.5f) >> 1;

		MinimumLootDrops++;

		if (MinimumLootDrops >= OriginalNumberLootDrops)
			return MinimumLootDrops;

		SumLootPackageCategoryWeightArray--;
	}

	return MinimumLootDrops;
}

void AddLootTierData(UDataTable* DataTable) 
{
	if (!DataTable || IsBadReadPtr(DataTable, 8) || !DataTable->Class)
		return;
	LootTierDatas.Add(DataTable);
	return;
	if (auto CompositeDataTable = Cast<UCompositeDataTable>(DataTable)) {
		for (auto Table : CompositeDataTable->ParentTables) {
			LootTierDatas.Add(Table);
		}
	}
	else {
		LootTierDatas.Add(DataTable);
	}
}

void AddLootPackageData(UDataTable* DataTable)
{
	if (!DataTable || IsBadReadPtr(DataTable, 8) || !DataTable->Class)
		return;
	LootPackageDatas.Add(DataTable);
	return;
	if (auto CompositeDataTable = Cast<UCompositeDataTable>(DataTable)) {
		for (auto Table : CompositeDataTable->ParentTables) {
			LootPackageDatas.Add(Table);
		}
	}
	else {
		LootPackageDatas.Add(DataTable);
	}
}

void SetupLTDS() 
{
	auto GameMode = GetGameMode();
	auto GameState = GetGameState();
	if (!GameMode || !GameState)
		return;

	auto Playlist = GameState->CurrentPlaylistInfo.BasePlaylist;
	if (!Playlist)
		return;

	AddLootTierData(Playlist->LootTierData.Get());
	AddLootPackageData(Playlist->LootPackages.Get());

	auto GameFeatures = GetObjectsOfClass<UFortGameFeatureData>();

	for (UFortGameFeatureData* GameFeature : GameFeatures) 
	{
		for (auto Tag : Playlist->GameplayTagContainer.GameplayTags) 
		{
			auto OvverideLootTableDataIt = GameFeature->PlaylistOverrideLootTableData.Find(Tag, [](const FGameplayTag& Left, const FGameplayTag& Wright) {return Left == Wright; });

			if (OvverideLootTableDataIt.IsValid()) 
			{	
				auto OvverideLootTableData = GameFeature->PlaylistOverrideLootTableData[OvverideLootTableDataIt.GetIndex()].Second;
				auto LootPackageData = OvverideLootTableData.LootPackageData.Get();
				auto LootTierData = OvverideLootTableData.LootTierData.Get();
				AddLootPackageData(OvverideLootTableData.LootPackageData.Get());
				AddLootTierData(LootTierData);
			}
		}
	}
}

template <typename T>
T* PickWeighted(TArray<T*> Array) {
	float TotalWeight = std::accumulate(Array.begin(), Array.end(), 0.0f, [&](float acc, T*& p) { return acc + p->Weight; });
	float Number = WeightedRandom(TotalWeight);

	for (auto Element : Array)
	{
		float Weight = Element->Weight;
		if (Weight == 0.0f)
			continue;

		if (Number <= Weight) 
			return Element;

		Number -= Weight;
	}

	return nullptr;
}
bool ValidateLootPackage(FFortLootPackageData* LootPackage, FName LootPackageName, int LootPackageCategory, int WorldLevel) {
	if (LootPackage->LootPackageID != LootPackageName)
	{
		return false;
	}

	if (LootPackageCategory != -1 && LootPackage->LootPackageCategory != LootPackageCategory)
	{
		return false;
	}

	if (WorldLevel >= 0)
	{
		if (LootPackage->MaxWorldLevel >= 0 && WorldLevel > LootPackage->MaxWorldLevel)
			return 0;

		if (LootPackage->MinWorldLevel >= 0 && WorldLevel < LootPackage->MinWorldLevel)
			return 0;
	}

	return true;
}

FFortItemEntry FortKismetLibrary::ConstructItemEntry(UFortItemDefinition* ItemDefinition, int32 Count, int32 Level) {
	FFortItemEntry ItemEntry{};

	ItemEntry.MostRecentArrayReplicationKey = -1;
	ItemEntry.ReplicationID = -1;
	ItemEntry.ReplicationKey = -1;

	ItemEntry.ItemDefinition = ItemDefinition;
	ItemEntry.Count = Count;
	ItemEntry.Durability = 1.f;
	ItemEntry.GameplayAbilitySpecHandle = FGameplayAbilitySpecHandle(-1);
	ItemEntry.ParentInventory.ObjectIndex = -1;
	ItemEntry.Level = Level;
    
	return ItemEntry;
}

void PickLootFromLP(TArray<FFortItemEntry>& LootDrops, FName LootPackageName, int LootPackageCategory, int WorldLevel) {
	TArray<FFortLootPackageData*> LootPackages;
	for (auto Data : LootPackageDatas)
	{
		for (auto Pair : Data->RowMap)
		{
			auto LootPackageData = reinterpret_cast<FFortLootPackageData*>(Pair.Second);
			if (LootPackageData) {
				if (ValidateLootPackage(LootPackageData, LootPackageName, LootPackageCategory, WorldLevel))
					LootPackages.Add(LootPackageData);
					
			}
		}
	}
	if (LootPackages.Num() == 0)
		return;

	FName PickedPackageRowName;

	auto LootPackage = PickWeighted<FFortLootPackageData>(LootPackages);
	if (!LootPackage)
		return;

	if (LootPackage->LootPackageCall.Num() > 1)
	{
		for (auto i=0; i< LootPackage->Count;i++)
			return PickLootFromLP(LootDrops, LootPackage->LootPackageCall.IsValid() ? UKismetStringLibrary::Conv_StringToName(LootPackage->LootPackageCall) : FName(0), 0, WorldLevel);
	}

	auto ItemDefinition = LootPackage->ItemDefinition.Get();;
	if (!ItemDefinition)
		return;
	auto WeaponItemDefinition = Cast<UFortWeaponItemDefinition>(ItemDefinition);
	int LoadedAmmo = WeaponItemDefinition ? WeaponItemDefinition->GetStats()->ClipSize : 0;

	auto WorldItemDefinition = Cast<UFortWorldItemDefinition>(ItemDefinition);

	if (!WorldItemDefinition)
		return;

	int ItemLevel = GetItemLevel(WorldItemDefinition->LootLevelData, WorldLevel);

	int Count = LootPackage->Count;

	if (Count > 0)
	{
		int FinalItemLevel = 0;

		if (ItemLevel >= 0)
			FinalItemLevel = ItemLevel;

		while (Count > 0)
		{
			int MaxStackSize = ItemDefinition->MaxStackSize.Evaluate();

			int CurrentCountForEntry = MaxStackSize;

			if (Count <= MaxStackSize)
				CurrentCountForEntry = Count;

			if (CurrentCountForEntry <= 0)
				CurrentCountForEntry = 0;

			auto ActualItemLevel = PickLevel(WorldItemDefinition, FinalItemLevel);
			LootDrops.Add(FortKismetLibrary::ConstructItemEntry(ItemDefinition, CurrentCountForEntry, ActualItemLevel));
			bool IsWeapon = LootPackage->LootPackageID.ToString().contains(".Weapon.") && WeaponItemDefinition;

			if (IsWeapon && WeaponItemDefinition->GetAmmoWorldItemDefinition_BP() && WeaponItemDefinition->GetAmmoWorldItemDefinition_BP() != WeaponItemDefinition)
			{
				LootDrops.Add(FortKismetLibrary::ConstructItemEntry(WeaponItemDefinition->GetAmmoWorldItemDefinition_BP(), ((UFortAmmoItemDefinition*)WeaponItemDefinition->GetAmmoWorldItemDefinition_BP())->DropCount, 0));
			}

			Count -= CurrentCountForEntry;
		}
	}
}

TArray<FFortItemEntry> FortKismetLibrary::PickLootDrops(FName TierGroupName, int WorldLevel)
{
	if (LootTierDatas.Num() == 0 || LootPackageDatas.Num() == 0)
		SetupLTDS();
	TArray<FFortItemEntry> Result;
	TArray<FFortLootTierData*> LootTiers;
	for (auto Data : LootTierDatas) 
	{
		for (auto Pair : Data->RowMap) 
		{
			auto LootTierData = reinterpret_cast<FFortLootTierData*>(Pair.Second);
			if (LootTierData) {
				if (LootTierData->TierGroup == TierGroupName)
					LootTiers.Add(LootTierData);
			}
		}
	}
	auto LootTierData = PickWeighted<FFortLootTierData>(LootTiers);
	if (!LootTierData)
		return Result;

	float NumLootPackageDrops = LootTierData->NumLootPackageDrops;
	float NumberLootDrops = 0;

	if (NumLootPackageDrops > 0)
	{
		if (NumLootPackageDrops < 1)
		{
			NumberLootDrops = 1;
		}
		else
		{
			NumberLootDrops = (int)(float)((float)(NumLootPackageDrops + NumLootPackageDrops) - 0.5f) >> 1;
			float v20 = NumLootPackageDrops - NumberLootDrops;
			if (v20 > 0.0000099999997f)
			{
				NumberLootDrops += v20 >= (rand() * 0.000030518509f);
			}
		}
	}
	float AmountOfLootPackageDrops = GetAmountOfLootPackagesToDrop(LootTierData, NumberLootDrops);

	if (AmountOfLootPackageDrops > 0)
	{
		for (int i = 0; i < AmountOfLootPackageDrops; ++i)
		{
			if (i >= LootTierData->LootPackageCategoryMinArray.Num())
				break;

			for (int j = 0; j < LootTierData->LootPackageCategoryMinArray[i]; ++j)
			{
				if (LootTierData->LootPackageCategoryMinArray[i] < 1)
					break;

				PickLootFromLP(Result, LootTierData->LootPackage, i, WorldLevel);
			}
		}
	}
	return Result;
}

bool FortKismetLibrary::PickLootDropsHook(UObject* Object, FFrame& Stack, bool* Ret)
{
	UObject* WorldContextObject;
	FName TierGroupName;
	int32 WorldLevel;
	int32 ForcedLootTier;
	FGameplayTagContainer OptionalLootTags;

	Stack.StepCompiledIn(&WorldContextObject);
	auto& OutLootToDrop = Stack.StepCompiledInRef<TArray<FFortItemEntry>>();
	Stack.StepCompiledIn(&TierGroupName);
	Stack.StepCompiledIn(&WorldLevel);
	Stack.StepCompiledIn(&ForcedLootTier);
	Stack.StepCompiledIn(&OptionalLootTags);
	Stack.IncrementCode();
	
	auto LootDrops = PickLootDrops(TierGroupName, WorldLevel);

	for (auto& LootDrop : LootDrops)
		OutLootToDrop.Add(LootDrop);

	return *Ret = true;
}

AFortPickupAthena* FortKismetLibrary::SpawnPickup(FVector Loc, FFortItemEntry Entry, EFortPickupSourceTypeFlag SourceTypeFlag, EFortPickupSpawnSource SpawnSource, AFortPlayerPawn* Pawn, int OverrideCount, bool Toss, bool RandomRotation, bool bCombine)
{
	static auto PickupClass = AFortPickupAthena::StaticClass();
	AFortPickupAthena* NewPickup = UWorld::GetWorld()->SpawnActor<AFortPickupAthena>(PickupClass, Loc);
    
	NewPickup->bRandomRotation = RandomRotation;
	NewPickup->PrimaryPickupItemEntry.ItemDefinition = Entry.ItemDefinition;
	NewPickup->PrimaryPickupItemEntry.LoadedAmmo = Entry.LoadedAmmo;
	NewPickup->PrimaryPickupItemEntry.Count = OverrideCount != -1 ? OverrideCount : Entry.Count;
	NewPickup->PawnWhoDroppedPickup = Pawn;
	NewPickup->OnRep_PrimaryPickupItemEntry();

	if (Toss) NewPickup->TossPickup(Loc, Pawn, -1, Toss, true, SourceTypeFlag, SpawnSource);
	NewPickup->MovementComponent = (UProjectileMovementComponent*)UGameplayStatics::SpawnObject(UProjectileMovementComponent::StaticClass(), NewPickup);
	NewPickup->SetReplicateMovement(true);
	NewPickup->OnRep_ReplicateMovement();
	
	return NewPickup;
}

void FortKismetLibrary::Setup()
{
	UHook* Hook = new UHook();

	Hook->Path = "/Script/FortniteGame.FortKismetLibrary.PickLootDrops";
	Hook->Detour = PickLootDropsHook;
	UKismetHookingLibrary::Hook(Hook, Exec);

	delete Hook;
}
