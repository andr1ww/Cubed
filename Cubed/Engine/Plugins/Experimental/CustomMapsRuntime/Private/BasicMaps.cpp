#include "pch.h"

#include "Engine/Plugins/Experimental/CustomMapsRuntime/Public/BasicMaps.h"

#include "json.hpp"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"
#include <fstream>

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

bool CustomMapsRuntime::SetupMap() 
{
    if (IsPluginEnabled())
    {
        auto EnabledMap = GetEnabledMap();
        auto World = GetWorld();

        std::filesystem::path Path = std::filesystem::current_path() / "Maps" / "CustomMapsRuntime" / (EnabledMap + ".json");
        
        xstring Content;
        if (!Read(xstring(Path.string()), Content))
            return false;

        FMap MapData;
        if (!ParseMap(Content, MapData))
            return false;

        for (const auto& Actor : MapData.Actors)
        {
            UClass* ActorClass = StaticLoadObject<UClass>(Actor.ClassName.c_str());
            
            if (!ActorClass)
                continue;

            World->SpawnActor(ActorClass, Actor.Location, Actor.Rotation);
        }
        
        return true;
    }
    
    return false;
}

bool CustomMapsRuntime::Read(const xstring& FilePath, xstring& OutContent)
{
    std::ifstream File(FilePath.c_str());
    if (!File.is_open())
        return false;
    
    std::stringstream Buffer;
    Buffer << File.rdbuf();
    OutContent = Buffer.str();
    
    return true;
}

bool CustomMapsRuntime::ParseMap(const xstring& JsonContent, FMap& OutMapData)
{
    auto JsonObj = nlohmann::json::parse(JsonContent.c_str());
    
    OutMapData.Name = JsonObj["Name"].get<std::string>();
    
    for (const auto& ActorJson : JsonObj["Actors"])
    {
        FActor Actor;
        Actor.ClassName = ActorJson["ClassName"].get<std::string>();
        
        Actor.Location.X = ActorJson["Location"]["X"].get<float>();
        Actor.Location.Y = ActorJson["Location"]["Y"].get<float>();
        Actor.Location.Z = ActorJson["Location"]["Z"].get<float>();
        
        Actor.Rotation.Pitch = ActorJson["Rotation"]["Pitch"].get<float>();
        Actor.Rotation.Yaw = ActorJson["Rotation"]["Yaw"].get<float>();
        Actor.Rotation.Roll = ActorJson["Rotation"]["Roll"].get<float>();
        
        OutMapData.Actors.push_back(Actor);
    }
    
    return true;
}
