#pragma once

#include "pch.h"

inline xmap<xstring, bool> CustomMaps =
{
    {"Test1v1", false},
    {"MartozMap", false},
};

class CustomMapsRuntime
{
public:
    static bool IsPluginEnabled();
    static xstring GetEnabledMap();
};
