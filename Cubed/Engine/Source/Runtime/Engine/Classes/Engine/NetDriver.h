#pragma once
#include "pch.h"

namespace NetDriver
{
    DefineOriginal(void, TickFlush, UNetDriver* NetDriver,  float DeltaTime);
    static float GetMaxTickRate() { return 120.f; }
    
    void Setup();
}