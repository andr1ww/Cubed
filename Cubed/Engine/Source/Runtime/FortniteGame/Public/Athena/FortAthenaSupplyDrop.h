#pragma once
#include "pch.h"

#include "Engine/Source/Runtime/CoreUObject/Public/UObject/Stack.h"

namespace FortAthenaSupplyDrop
{
    static AFortPickup* SpawnPickup(AFortAthenaSupplyDrop*, FFrame&, AFortPickup**);

    void Setup();
}
