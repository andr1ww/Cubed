// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

#include <memcury.h>

#include "Offsets.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"
#include "Engine/Source/Runtime/Engine/Classes/Engine/NetDriver.h"
#include "Engine/Source/Runtime/Engine/Classes/Engine/World.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Athena/FortGameModeAthena.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Building/BuildingContainer.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Athena/FortPoiVolume.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Athena/AI/Systems/AthenaAIServicePlayerBots.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Athena/Player/FortPlayerControllerAthena.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Building/BuildingSMActor.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Inventory/FortInventory.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Player/FortPlayerController.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Player/FortPlayerPawn.h"
#include "Engine/Source/Runtime/GameplayAbilities/Public/AbilitySystemComponent.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Athena/Creative/FortAthenaCreativePortal.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Athena/Creative/AFortMinigameSettingsBuilding.h"

static AFortPoiVolume* (*FindNextBestPOIOG)(UFortAthenaAIBotEvaluator_SelectNextPOI*);
AFortPoiVolume* FindNextBestPOI(UFortAthenaAIBotEvaluator_SelectNextPOI* This)
{
    auto Poi = FindNextBestPOIOG(This);
    if (!Poi)
    {
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(UWorld::GetWorld(), AFortPoiVolume::StaticClass(), &AllActors);
        if (AllActors.Num() > 0)
            Poi = (AFortPoiVolume*)AllActors[std::rand() % AllActors.Num()];
    }
    UE_LOG(LogTemp, Log, Poi->GetFullName().c_str());
    return Poi;
}

// move these funcs later
static void (*CreateAndConfigureNavigationSystemOG)(UAthenaNavSystemConfig* This, UWorld* World);
void CreateAndConfigureNavigationSystem(UAthenaNavSystemConfig* This, UWorld* World)
{
    UE_LOG(LogTemp, Log, __FUNCTION__);
    This->bPrioritizeNavigationAroundSpawners = true;
    This->bAutoSpawnMissingNavData = true;
    This->bAllowAutoRebuild = true;
    This->bSupportRuntimeNavmeshDisabling = false; // main fixes for nav
    This->bUsesStreamedInNavLevel = true;
    
    return CreateAndConfigureNavigationSystemOG(This, World);
}

static void (*SendRequestNowOG)(void* Arg1, void* MCPData, int);
static void SendRequestNow(void* Arg1, void* MCPData, int)
{
    return SendRequestNowOG(Arg1, MCPData, 3);
}

namespace SDK
{
    UObject *InternalObjectGet(FSoftObjectPtr *Ptr, UClass *Class)
    {
        if (!Ptr)
            return nullptr;
        auto Ret = Ptr->Get();
        if (!Ret && Ptr->ObjectID.AssetPathName.ComparisonIndex > 0) [[unlikely]]
        {
            Ret = StaticLoadObjectNoType(Ptr->ObjectID.AssetPathName.GetRawString().c_str(), Class);
            if (Ret)
            {
                Ptr->WeakPtr.ObjectIndex = Ret->Index;
                Ptr->WeakPtr.ObjectSerialNumber = UObject::GObjects->GetItemByIndex(Ret->Index)->SerialNumber;
            }
        }
        return Ret;
    }
}

uint32 RetNegativeOne() { return -1; }

DWORD WINAPI Startup(LPVOID)
{
    AllocConsole();
    FILE* OutPtr = nullptr;
    freopen_s(&OutPtr, "CONIN$", "r", stdin);
    freopen_s(&OutPtr, "CONOUT$", "w", stdout);
    freopen_s(&OutPtr, "CONOUT$", "w", stderr);
    SetConsoleTitleA("Cubed | Loading...");

    std::srand(std::time(nullptr));

    MH_Initialize();
    Sleep(5500);
    
    FortGameModeAthena::Setup();
    World::Setup();
    NetDriver::Setup();
    FortInventory::Setup();
    FortPlayerController::Setup();
    AbilitySystemComponent::Setup();
    FortPlayerControllerAthena::Setup();
    FortPoiVolume::Setup();
    FortPlayerPawn::Setup();
    BuildingSMActor::Setup();
//    AthenaAIServicePlayerBots::Setup();
    BuildingContainer::Setup();
    FortAthenaCreativePortal::Setup();
    FortMinigameSettingsBuilding::Setup();
    
    UHook* Hook = new UHook();
    
    auto RequestExit = Memcury::Scanner::FindPattern("40 53 48 83 EC ? 80 3D ? ? ? ? ? 0F B6 D9 72 ? 48 8D 05 ? ? ? ? 89 5C 24 ? 41 B9 ? ? ? ? 48 89 44 24 ? 4C 8D 05 ? ? ? ? 33 D2 33 C9 E8 ? ? ? ? 48 8D 0D").Get();
    if (!RequestExit)
        RequestExit = Memcury::Scanner::FindPattern("88 4C 24 ? 53 48 83 EC ? 80 3D ? ? ? ? ? 8A D9").Get();
    if (!RequestExit)
        RequestExit = Memcury::Scanner::FindPattern("40 53 48 83 EC ? 41 B9 ? ? ? ? 0F B6 D9").Get();

    if (RequestExit)
    {
        Hook->Address = RequestExit;
        Hook->Byte = 0xC3;
        UKismetHookingLibrary::Hook(Hook, Byte);
    }
    
    for (auto& Addr : Runtime::Offsets::NullFuncs)
    {
        Hook->Address = ImageBase + Addr;
        Hook->Byte = 0xC3;
        UKismetHookingLibrary::Hook(Hook, Byte);
    }
    
    for (auto& Addr : Runtime::Offsets::RetTrueFuncs)
    {
        Hook->Address = ImageBase + Addr;
        UKismetHookingLibrary::Hook(Hook, RTrue);
    }

    Hook->Address = ImageBase + 0x3079B00;
    Hook->Detour = RetNegativeOne;
    UKismetHookingLibrary::Hook(Hook, Address);

    /*Hook->Address = ImageBase + 0x47E286C;
    Hook->Original = (void**)&FindNextBestPOIOG;
    Hook->Detour = FindNextBestPOI;
    UKismetHookingLibrary::Hook(Hook, Address);
*/
    Hook->Address = ImageBase + 0x1023D3C;
    Hook->Original = (void**)&SendRequestNowOG;
    Hook->Detour = SendRequestNow;
    UKismetHookingLibrary::Hook(Hook, Address);

    Hook->Address = 0x1B;
    Hook->Class = AActor::StaticClass();
    Hook->Detour = RetTrue;
    UKismetHookingLibrary::Hook(Hook, EveryVFT);

    Hook->Address = ImageBase + 0x11B6DAC;
    Hook->Original = (void**)&CreateAndConfigureNavigationSystemOG;
    Hook->Detour = CreateAndConfigureNavigationSystem;
    UKismetHookingLibrary::Hook(Hook, Address);

    free(Hook);
    
    *(bool *)(ImageBase + Runtime::Offsets::GIsClient) = false;
    *(bool *)(ImageBase + Runtime::Offsets::GIsClient + 1) = true;

    MH_EnableHook(MH_ALL_HOOKS);
    UWorld::GetWorld()->OwningGameInstance->LocalPlayers.Remove(0);
    UGameplayStatics::OpenLevel(UWorld::GetWorld(), bCreative ? UKismetStringLibrary::Conv_StringToName(L"Creative_NoApollo_Terrain") : UKismetStringLibrary::Conv_StringToName(L"Apollo_Terrain"), true, FString());

    std::vector<std::wstring> Logs = {
        L"LogFortUIDirector",
        L"LogQos",
        L"LogFortInventory",
        L"LogFortWorld",
        L"LogFortMutatorInventoryOverride",
        L"LogFort",
        L"LogFortInvServiceComp",
        L"LogFortPlayerRegistration",
        L"LogOnlineGame",
        L"LogAthenaAIServiceBots",
        L"LogStats",
        L"LogHotfixManager",
        L"LogAISpawnerData",
        L"LogOnlineSession",
        L"LogMatchmakingServiceDedicatedServer",
        L"LogGameplayTags",
        L"LogJson",
        L"LogNavigationBuild",
        L"LogFortDayNight",
        L"LogLivingWorldManager",
        L"LogFortAI",
        L"LogFortAbility",
        //L"LogAthenaBots",
        L"LogCharacter",
        L"LogBehaviorTree",
        L"LogPathFollowing",
        L"LogPawnAction",
        L"LogFortAIDirector",
        L"LogFortQuest",
        L"LogOnline",
        L"LogAthenaBots",
        L"LogEQS",
        L"LogBrushComponent",
        L"LogPlayerQuestProgress",
        L"LogFortQuestObjective",
        L"LogAISpawner"
    };

    for (const std::wstring& Log : Logs)
    {
        auto LogCMD = std::format(L"log {} all", Log);
        // UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(std::format(L"log {} VeryVerbose", Log).c_str()), nullptr);
    }
    
    return 0;
}

bool DllMain(HMODULE hModule, int rsn, void*)
{
    if (rsn == DLL_PROCESS_ATTACH)
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Startup, 0, 0, 0);
    
    return true;
}