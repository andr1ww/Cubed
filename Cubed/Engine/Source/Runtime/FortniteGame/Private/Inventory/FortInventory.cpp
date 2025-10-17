#include "pch.h"

#include "Engine/Source/Runtime/FortniteGame/Public/Inventory/FortInventory.h"

#include "Globals.h"
#include "memcury.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"

FFortRangedWeaponStats* UFortWeaponItemDefinition::GetStats()
{
    if (!WeaponStatHandle.DataTable)
        return nullptr;

    auto Stats = WeaponStatHandle.DataTable->Search([this](FName &Key, uint8_t *Value)
                                                           { return this->WeaponStatHandle.RowName == Key && Value; });

    return Stats ? *(FFortRangedWeaponStats **)Stats : nullptr;
}

static void Update(AFortInventory* Inventory, FFortItemEntry *Entry)
{
    Inventory->bRequiresLocalUpdate = true;
    Inventory->HandleInventoryLocalUpdate();

    return Entry ? Inventory->Inventory.MarkItemDirty(*Entry) : Inventory->Inventory.MarkArrayDirty();
}

class UObject* AFortInventory::AddItem(UFortItemDefinition* Def, int32 Count, int LoadedAmmo, int32 Level)
{
    UFortWorldItem *Item = (UFortWorldItem *)Def->CreateTemporaryItemInstanceBP(Count, Level);
    if (!Item) return nullptr;
    auto Controller = Cast<AFortPlayerController>(GetOwner());
    
    Item->ItemEntry.LoadedAmmo = LoadedAmmo;
    Item->SetOwningControllerForTemporaryItem(Controller);
    
    if (auto Weapon = Cast<UFortWeaponItemDefinition>(Def))
    {
        auto Stats = Weapon->GetStats();
        if (Weapon->bUsesPhantomReserveAmmo)
            Item->ItemEntry.PhantomReserveAmmo = Stats->InitialClips - Stats->ClipSize;
    }

    if (Def->IsA(UFortAmmoItemDefinition::StaticClass()) || Def->IsA(UFortResourceItemDefinition::StaticClass()))
    {
        FFortItemEntryStateValue Value{};
        Value.IntValue = true;
        Value.StateType = EFortItemEntryState::ShouldShowItemToast;
        Item->ItemEntry.StateValues.Add(Value);
    }
    
    Inventory.ReplicatedEntries.Add(Item->ItemEntry);
    Inventory.ItemInstances.Add(Item);
    Update(this, &Item->ItemEntry);

    return Item;
}

bool AFortInventory::ReplaceEntry(FFortItemEntry& Entry)
{
    bool bSuccess = false;
    if (!this) 
        return bSuccess;

    if (Entry.Count <= 0)
    {
        Remove(Entry.ItemGuid);
        return bSuccess = true;
    }
    
    for (int i = 0; i < Inventory.ItemInstances.Num(); i++)
    {
        if (Inventory.ItemInstances[i]->ItemEntry.ItemGuid == Entry.ItemGuid)
        {
            Inventory.ItemInstances[i]->ItemEntry = Entry;
            
            bSuccess = true;
            break;
        }
    }

    for (int i = 0; i < Inventory.ReplicatedEntries.Num(); i++)
    {
        if (Inventory.ReplicatedEntries[i].ItemGuid == Entry.ItemGuid)
        {
            Inventory.ReplicatedEntries[i] = Entry;
            Inventory.MarkItemDirty(Entry);
            bSuccess = true;
            break;
        }
    }

    if (bSuccess)
        Update(this, &Entry);
    
    return bSuccess;
}

void AFortInventory::Remove(FGuid ItemGuid)
{
    bool bSuccess = false;
    if (!this) 
        return;
    
    for (int i = 0; i < Inventory.ItemInstances.Num(); i++)
    {
        if (Inventory.ItemInstances[i]->ItemEntry.ItemGuid == ItemGuid)
        {
            Inventory.ItemInstances.Remove(i);
            bSuccess = true;
            break;
        }
    }

    for (int i = 0; i < Inventory.ReplicatedEntries.Num(); i++)
    {
        if (Inventory.ReplicatedEntries[i].ItemGuid == ItemGuid)
        {
            Inventory.ReplicatedEntries.Remove(i);
            bSuccess = true;
            break;
        }
    }

    if (bSuccess)
        Update(this, nullptr);
}

bool FortInventory::ServerRemoveInventoryItem(AFortPlayerController* PlayerController, FGuid& ItemGuid, int Count, bool bForceRemoval, bool bForcePersistWhenEmpty)
{
    auto ItemEntry = PlayerController->WorldInventory->Inventory.ReplicatedEntries.Search([ItemGuid](FFortItemEntry &entry)
                                                                           { return entry.ItemGuid == ItemGuid; });
    if (!ItemEntry) return false;

    ItemEntry->Count -= Count;
    PlayerController->WorldInventory->ReplaceEntry(*ItemEntry);
    
    return true;
}

bool FortInventory::RemoveInventoryItem(void* InventoryOwner, const FGuid& ItemGuid, int32 Count, bool bForceRemoveFromQuickBars, bool bForceRemoval)
{
    AFortPlayerController* PlayerController = GetPlayerControllerFromInventoryOwner(InventoryOwner);
    if (!PlayerController) return false;

    FGuid Ok = ItemGuid;
    ServerRemoveInventoryItem(PlayerController, Ok, Count, bForceRemoveFromQuickBars, bForceRemoval);
    
    return true;
}

void FortInventory::SetLoadedAmmo(UFortWorldItem* Item, int LoadedAmmo)
{
    Item->ItemEntry.LoadedAmmo = LoadedAmmo;

    auto Inventory = Item->OwnerInventory;
    Inventory->ReplaceEntry(Item->ItemEntry);   
}

void FortInventory::SetPhantomReserveAmmo(UFortWorldItem* Item, int PhantomReserveAmmo)
{
    Item->ItemEntry.PhantomReserveAmmo = PhantomReserveAmmo;

    auto Inventory = Item->OwnerInventory;
    Inventory->ReplaceEntry(Item->ItemEntry);   
}

void FortInventory::Setup()
{
    UHook* Hook = new UHook();
    Hook->Address = ImageBase + 0x52368b0;
    Hook->Detour = RemoveInventoryItem;
    UKismetHookingLibrary::Hook(Hook, EHook::Address);

    auto SetOwningInventory = Memcury::Scanner::FindPattern("48 85 D2 74 ? 80 BA ? ? ? ? ? 75 ? 48 89 91").Get(); // credits to ploosh

    if (SetOwningInventory)
    {
        auto WorldItemVft = UFortWorldItem::GetDefaultObj()->VTable;
        int SetOwningInventoryIdx = 0;

        for (int i = 0; i < 500; i++)
        {
            if (WorldItemVft[i] == (void*)SetOwningInventory)
            {
                SetOwningInventoryIdx = i;
                break;
            }
        }

        if (SetOwningInventoryIdx)
        {
            Hook->Address = SetOwningInventoryIdx - 3;
            Hook->Class = UFortWorldItem::StaticClass();
            Hook->Detour = SetLoadedAmmo;
            UKismetHookingLibrary::Hook(Hook, EHook::EveryVFT);
            
            Hook->Address = SetOwningInventoryIdx - 2;
            Hook->Class = UFortWorldItem::StaticClass();
            Hook->Detour = SetPhantomReserveAmmo;
            UKismetHookingLibrary::Hook(Hook, EHook::EveryVFT);
        }
    }

    free(Hook);
}