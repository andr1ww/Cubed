#include "pch.h"

#include "Engine/Plugins/Experimental/CustomMapsRuntime/Public/BasicMaps.h"

xstring CustomMapsRuntime::GetEnabledMap()
{
    for (auto& Map : CustomMaps)
        if (Map.second == true) 
         return Map.first;

    return "None";
}

bool CustomMapsRuntime::IsPluginEnabled()
{
    return GetEnabledMap() != "None" ? true : false;
}

