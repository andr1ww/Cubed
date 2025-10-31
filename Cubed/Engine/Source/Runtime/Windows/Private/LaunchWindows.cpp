// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

#include <memcury.h>

#include "Offsets.h"
#include "Engine/Plugins/Experimental/CommonConversation/Source/CommonConversationRuntime/Public/ConversationLibrary.h"
#include "Engine/Plugins/Experimental/CustomMapsRuntime/Public/BasicMaps.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"
#include "Engine/Source/Runtime/Engine/Classes/Engine/NetDriver.h"
#include "Engine/Source/Runtime/Engine/Classes/Engine/World.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Athena/FortAthenaSupplyDrop.h"
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
#include "Engine/Source/Runtime/FortniteGame/Public/Components/FortAthenaAISpawnerDataComponents.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Components/FortControllerComponent_IndicatedActorManagement.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Kismet/FortKismetLibrary.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Online/FortGameSessionDedicated.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Quests/FortQuestManager.h"

inline const wchar_t* (*GetOG)();
const wchar_t* Get()
{
    static auto cmdLine = wstring(GetOG()) + L" -AllowAllPlaylistsInShipping -EnableMMS";
    return cmdLine.c_str(); // -EnableMMS may work for backfill?
}

struct FServicePermissionsMcp {
public:
    char Unk_0[0x10];
    class FString Id;
};

FWeakObjectPtr::FWeakObjectPtr(const UObject* Object)
{
    static int StartingSerial = 676767676; // scuffed

    if (Object)
    {
        ObjectIndex = Object->Index;
        auto Item = UObject::GObjects->GetItemByIndex(Object->Index);

        if (Item->SerialNumber == 0)
            Item->SerialNumber = StartingSerial++;

        ObjectSerialNumber = Item->SerialNumber;
    }
    else 
    {
        ObjectIndex = -1;
        ObjectSerialNumber = 0;
    }
}

FServicePermissionsMcp* MatchmakingServicePerms(int64, int64)
{
    static FServicePermissionsMcp* ServicePerms = nullptr;
    if (!ServicePerms)
        ServicePerms = new FServicePermissionsMcp{ .Id = L"ec684b8c687f479fadea3cb2ad83f5c6" };
    return ServicePerms;
}

static inline char (*GetStringOG)(__int64 a1, const wchar_t* Section, const wchar_t* Key, __int64* Value, __int64* Filename)  = nullptr;
char GetStringINI(__int64 a1, const wchar_t* Section, const wchar_t* Key, __int64* Value, __int64* Filename) {
    if (Section && Key) {
        if (wcscmp(Section, L"OnlineSubsystemMcp.McpProfile") == 0 && wcslen(Key) == 0) {
            Key = L"McpDedicatedServerCommandUrl";
        }
    }
    
    return GetStringOG(a1, Section, Key, Value, Filename);
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
static void SendRequestNow(void* Arg1, void* MCPData, int) { return SendRequestNowOG(Arg1, MCPData, 3); }

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
namespace Creative { xmap<xstring, UFortCreativeRealEstatePlotItemDefinition*> PlotDefinitionsByMcpId = {}; }

EMeshNetworkNodeType GetNodeType() { return EMeshNetworkNodeType::Edge; }

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
    FortQuestManager::Setup();
    BuildingSMActor::Setup();
  //  AthenaAIServicePlayerBots::Setup();
    BuildingContainer::Setup();
    FortAthenaCreativePortal::Setup();
    FortMinigameSettingsBuilding::Setup();
    FortAthenaSupplyDrop::Setup();
    FortKismetLibrary::Setup();
    ConversationLibrary::Setup();
    FortGameSessionDedicated::Setup();
    FortAthenaAISpawnerDataComponents::Setup();
    FortControllerComponent_IndicatedActorManagement::Setup();
    
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
    
    Runtime::Offsets::NullFuncs.push_back(0x29192EC);
    Runtime::Offsets::NullFuncs.push_back(0x509bf28);
    Runtime::Offsets::RetTrueFuncs.push_back(0x50af34c);
    Runtime::Offsets::RetTrueFuncs.push_back(0x509AFC0); // mms enable
    Runtime::Offsets::RetTrueFuncs.push_back(0x309EE30);
    Runtime::Offsets::NullFuncs.push_back(0x16CA9E0);
    // kickplayer ones ^ (dk why they wont work with gamesessions but WTV!)
    
    for (auto& Addr : Runtime::Offsets::NullFuncs)
    {
        Hook->Address = ImageBase + Addr;
        Hook->Byte = 0xC3;
        UKismetHookingLibrary::Hook(Hook, Byte);
    }

    UKismetHookingLibrary::PatchBytes(ImageBase + 0x12B7F2C, { 0x90, 0x90 });

    Hook->Address = ImageBase + Runtime::Offsets::EncryptionPatch;
    Hook->Byte = 0x74; 
    UKismetHookingLibrary::Hook(Hook, Byte);

    UKismetHookingLibrary::NullCall(0x50A8E27);
    UKismetHookingLibrary::PatchBytes(ImageBase + 0x5091E8F, { 0x75, 0x7F });

    Hook->Address = ImageBase + 0x1116A28;
    Hook->Original = (void**)&GetOG;
    Hook->Detour = Get;
    UKismetHookingLibrary::Hook(Hook, Address);

    Hook->Address = ImageBase + 0x172394C;
    Hook->Detour = MatchmakingServicePerms;
    UKismetHookingLibrary::Hook(Hook, Address);
    
    for (auto& Addr : Runtime::Offsets::RetTrueFuncs)
    {
        Hook->Address = ImageBase + Addr;
        UKismetHookingLibrary::Hook(Hook, RTrue);
    }

    Hook->Address = ImageBase + 0x3079B00;
    Hook->Detour = RetNegativeOne;
    UKismetHookingLibrary::Hook(Hook, Address);

    Hook->Address = ImageBase + 0x1075F30;
    Hook->Original = (void**)&GetStringOG;
    Hook->Detour = GetStringINI;
    UKismetHookingLibrary::Hook(Hook, Address);

    Hook->Address = ImageBase + 0x1023D3C;
    Hook->Original = (void**)&SendRequestNowOG;
    Hook->Detour = SendRequestNow;
    UKismetHookingLibrary::Hook(Hook, Address);

    Hook->Address = 0x1B;
    Hook->Class = AActor::StaticClass();
    Hook->Detour = RetTrue;
    if (!bCreative) UKismetHookingLibrary::Hook(Hook, EveryVFT);

    Hook->Address = ImageBase + 0x11B6DAC;
    Hook->Original = (void**)&CreateAndConfigureNavigationSystemOG;
    Hook->Detour = CreateAndConfigureNavigationSystem;
    if (!bCreative) UKismetHookingLibrary::Hook(Hook, Address);

    Hook->Address = ImageBase + 0x1FE669C;
    Hook->Detour = GetNodeType;
    UKismetHookingLibrary::Hook(Hook, Address);

    Hook->Address = Memcury::Scanner::FindPattern("75 02 33 F6 41 BE ? ? ? ? 48 85 F6 74 17 48 8D 93").Get();
    Hook->Byte = 0x74;
   // UKismetHookingLibrary::Hook(Hook, Byte);
    
    free(Hook);

    if (bGameSessions)
    {
        // backfill related
        UKismetHookingLibrary::PatchBytes(ImageBase + 0x42FC76B, { 0xEB, 0x3A });
        UKismetHookingLibrary::PatchBytes(ImageBase + 0x50944CD, { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 });
    }
    
    *(bool*)(ImageBase + Runtime::Offsets::GIsClient) = false;
    *(bool*)(ImageBase + Runtime::Offsets::GIsClient + 1) = true;
    
    MH_EnableHook(MH_ALL_HOOKS);
    if (!bGameSessions) GetWorld()->OwningGameInstance->LocalPlayers.Remove(0);
    UKismetSystemLibrary::ExecuteConsoleCommand(GetWorld(),
        bCreative || CustomMapsRuntime::IsPluginEnabled() ? L"open Creative_NoApollo_Terrain" : bImposters ? L"open MoleGame_Layout" : L"open Apollo_Terrain", nullptr);

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
        L"LogPlayerQuestProgress",
        L"LogFortQuestObjective",
        L"LogAISpawner",
    };

    for (const std::wstring& Log : Logs)
    {
        auto LogCMD = std::format(L"log {} all", Log);
         UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(std::format(L"log {} VeryVerbose", Log).c_str()), nullptr);
    }
    
    return 0;
}

bool DllMain(HMODULE hModule, int rsn, void*)
{
    if (rsn == DLL_PROCESS_ATTACH)
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Startup, 0, 0, 0);
    
    return true;
}