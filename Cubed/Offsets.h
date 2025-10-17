#pragma once
#include <stdint.h>
#include <array>
#include "pch.h"

namespace Runtime {
    namespace Offsets {
        inline uint64_t Step = 0xd6e9dc;
        inline uint64_t StepExplicitProperty = 0xfaeb20;
        inline uint64_t GetWorldContext = 0xc9f891;
        inline uint64_t CreateNetDriverWorldContext = 0x1794a5c;
        inline uint64_t InitHost = 0x3b26ed8;
        inline uint64_t PauseBeaconRequests = 0x50a55e0;
        inline uint64_t InitListen = 0x3b27284;
        inline uint64_t SetWorld = 0x11b826c;
        inline uint64_t GetNetMode = 0xd4e718;
        inline uint64_t TickFlush = 0xd122e0;
        inline uint64_t ServerReplicateActors = 0x3fd2b1c;
        inline uint64_t GetMaxTickRate = 0xe976cc;
        inline uint64_t SendRequestNow = 0x1023d3c;
        inline uint64_t Realloc = ImageBase + 0x197ebdc;
        inline uint64_t StaticFindObject = 0xd13c3c;
        inline uint64_t InternalGiveAbility = 0x3d53ee0;
        inline uint64_t StaticLoadObject = 0x14ad4a0;
        inline uint64_t GIsClient = 0x9c0af6b;
        inline uint64_t GameSessionPatch = 0x12a0044;
        inline uint64_t EncryptionPatch = 0x66e410e;
        inline uint64_t ApplyCharacterCustomization = 0x526b2e4;
        inline uint64_t InternalTryActivateAbility = 0x3d555ec;
        inline std::vector<uint64_t> NullFuncs = { 0x509bf28, 0x4A8F4F8 };
        inline std::vector<uint64_t> RetTrueFuncs = { 0x50af34c, 0xd4e718, /*0x47E06A4*/ 0x3D27AC8 };
    };

    namespace Vfts {
        inline uint32_t ReadyToStartMatch = 0x104;
        inline uint32_t SpawnDefaultPawnFor = 0xca;
        inline uint32_t ServerAcknowledgePossesion = 0x114;
        inline uint32_t ServerAttemptAircraftJump = 0x94;
        inline uint32_t ServerExecuteInventoryItem = 0x21d;
        inline uint32_t InternalServerTryActivateAbility = 0xfe;
        inline uint32_t HandleStartingNewPlayer = 0xd0;
    };

    namespace Funcs {
        inline auto InitHost = (bool (*)(SDK::AOnlineBeaconHost*)) (ImageBase + Offsets::InitHost);
        inline auto PauseBeaconRequests = (void (*)(SDK::AOnlineBeaconHost*, bool)) (ImageBase + Offsets::PauseBeaconRequests);
        inline auto InitListen = (bool (*)(SDK::UNetDriver*, SDK::UWorld*, SDK::FURL&, bool, UC::FString&)) (ImageBase + Offsets::InitListen);
        inline auto SetWorld = (void (*)(SDK::UNetDriver*, SDK::UWorld*)) (ImageBase + Offsets::SetWorld);
        inline auto ServerReplicateActors = (void (*)(SDK::UReplicationDriver*, float)) (ImageBase + Offsets::ServerReplicateActors);
        inline auto Realloc = (void* (*)(void*, __int64, unsigned int)) (ImageBase + Offsets::Realloc);
        inline auto StaticFindObject = (SDK::UObject* (*)(SDK::UClass*, SDK::UObject*, const wchar_t*, bool)) (ImageBase + Offsets::StaticFindObject);
        inline auto StaticLoadObject = (SDK::UObject* (*)(SDK::UClass*, SDK::UObject*, const wchar_t*, const wchar_t*, uint32_t, SDK::UObject*, bool)) (ImageBase + Offsets::StaticLoadObject);
    };
};

static inline __int64 (*LoadPlaysetOG)(UPlaysetLevelStreamComponent*) = decltype(LoadPlaysetOG)(__int64(InSDKUtils::GetImageBase() + 0x5279654));
static inline __int64 (*UnLoadPlaysetOG)(UPlaysetLevelStreamComponent*) = decltype(UnLoadPlaysetOG)(__int64(InSDKUtils::GetImageBase() + 0x528261C));