#pragma once

#include "pch.h"

inline xmap<xstring, bool> CustomMaps =
{
    {"Test1v1", true},
    {"MartozMap", false},
};

struct FActor
{
    xstring ClassName;
    FVector Location;
    FRotator Rotation;
};

struct FMap
{
    xstring Name;
    xvector<FActor> Actors; 
};

class CustomMapsRuntime
{
public:
    static bool IsPluginEnabled();
    static xstring GetEnabledMap();
    static bool SetupMap();
    static bool ParseMap(const xstring& JsonContent, FMap& OutMapData);
    static bool Read(const xstring& FilePath, xstring& OutContent);
};
