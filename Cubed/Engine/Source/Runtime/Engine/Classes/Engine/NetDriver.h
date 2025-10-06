#pragma once
#include "pch.h"

namespace NetDriver
{
    DefineOriginal(void, TickFlush, UNetDriver* NetDriver,  float DeltaTime);
    
    void Setup();
}