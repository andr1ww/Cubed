#include "pch.h"
#include "Engine/Source/Runtime/Engine/Classes/Engine/World.h"

#include "Logging.h"
#include "memcury.h"
#include "Offsets.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"

bool World::Listen(UWorld* InWorld, FURL)
{
    UE_LOG(LogTemp, Log, "World::Listen called!");
    
    static FWorldContext* (*GetWorldContextFromObject)(UEngine*, UWorld*) = decltype(GetWorldContextFromObject)(ImageBase + 0xD2A1E8);
    FWorldContext* WorldContext = GetWorldContextFromObject(UFortEngine::GetEngine(), GetWorld());
    static UNetDriver* (*CreateNetDriver_Local)(UEngine*, FWorldContext*, FName) = decltype(CreateNetDriver_Local)(ImageBase + 0x1794a5c);
    FName GameNetDriver = UKismetStringLibrary::Conv_StringToName(L"GameNetDriver");
    auto NetDriver = CreateNetDriver_Local(UFortEngine::GetEngine(), WorldContext, GameNetDriver);
    if (!NetDriver) return false;
    GetWorld()->NetDriver = NetDriver;
    GetWorld()->NetDriver->World = GetWorld();
    GetWorld()->NetDriver->NetDriverName = GameNetDriver;

    FString Err;    
    FURL URL;
    URL.Port = 7777;

    Runtime::Funcs::SetWorld(NetDriver, GetWorld());
    if (Runtime::Funcs::InitListen(NetDriver, GetWorld(), URL, false, Err))
    {
        Runtime::Funcs::SetWorld(NetDriver, GetWorld());
        for (auto& Level : GetWorld()->LevelCollections) Level.NetDriver = NetDriver;
        SetConsoleTitleA("Cubed | Ready on Port 7777 | Joinable = false");
        return true;
    } 
    
    return false;
}

enum ESpawnActorNameMode : uint8
{
    Required_Fatal,
    Required_ErrorAndReturnNull,
    Required_ReturnNull,
    Requested
};

struct FActorSpawnParameters
{
public:
    FName Name;

    AActor* Template;
    AActor* Owner;
    APawn* Instigator;
    ULevel* OverrideLevel;
    UChildActorComponent* OverrideParentComponent;
    ESpawnActorCollisionHandlingMethod SpawnCollisionHandlingOverride;

private:
    uint8	bRemoteOwned : 1;
public:
    uint8	bNoFail : 1;
    uint8	bDeferConstruction : 1;
    uint8	bAllowDuringConstructionScript : 1;
    ESpawnActorNameMode NameMode;
    EObjectFlags ObjectFlags;
};

AActor* UWorld::SpawnActor(UClass* Class, FVector Loc, FRotator Rot, AActor* Owner)
{
    FActorSpawnParameters addr{};

    addr.Owner = Owner;
    addr.bDeferConstruction = false;
    addr.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    addr.NameMode = ESpawnActorNameMode::Requested;
    
    return ((AActor * (*)(UWorld *, UClass *, FVector const*, FRotator const*, FActorSpawnParameters*))(ImageBase + 0x1647B38))(this, Class, &Loc, &Rot, &addr);
}

void World::Setup()
{
    UHook* Hook = new UHook();
    
    Hook->Address = ImageBase + 0x1797470;
    Hook->Detour = Listen;
    UKismetHookingLibrary::Hook(Hook, EHook::Address);

    free(Hook);
}