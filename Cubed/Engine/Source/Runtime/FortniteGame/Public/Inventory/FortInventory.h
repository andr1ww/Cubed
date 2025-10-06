#pragma once

#include "pch.h"

namespace FortInventory
{
    static class AFortPlayerController* GetPlayerControllerFromInventoryOwner(void* InventoryOwner)
    {
        if (!InventoryOwner) return nullptr;
        return (AFortPlayerController*)(__int64(InventoryOwner) - 0x700);
    }
    
    static bool ServerRemoveInventoryItem(AFortPlayerController* PlayerController, FGuid& ItemGuid, int Count, bool bForceRemoval,bool bForcePersistWhenEmpty);
    static bool RemoveInventoryItem(void* InventoryOwner, const FGuid& ItemGuid, int32 Count, bool bForceRemoveFromQuickBars, bool bForceRemoval);
    static void SetLoadedAmmo(UFortWorldItem* Item, int LoadedAmmo);
    static void SetPhantomReserveAmmo(UFortWorldItem* Item, int PhantomReserveAmmo);

    void Setup();
};