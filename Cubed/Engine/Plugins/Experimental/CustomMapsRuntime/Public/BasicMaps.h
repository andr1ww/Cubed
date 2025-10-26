#pragma once

#include "pch.h"

inline xmap<xstring, bool> CustomMaps =
{
    {"Test1v1", true},
    {"MartozMap", false},
};

class CustomMapsRuntime
{
public:
    static bool IsPluginEnabled();
    static xstring GetEnabledMap();
};
