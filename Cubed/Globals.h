#pragma once
#include "pch.h"

DEFINE_LOG_CATEGORY(LogServer);
DEFINE_LOG_CATEGORY(LogAthena);
DEFINE_LOG_CATEGORY(LogMatch);
DEFINE_LOG_CATEGORY(LogTeams);
DEFINE_LOG_CATEGORY(LogStorm);
DEFINE_LOG_CATEGORY(LogInventory);
DEFINE_LOG_CATEGORY(LogWeapons);
DEFINE_LOG_CATEGORY(LogAbilities);
DEFINE_LOG_CATEGORY(LogItems);
DEFINE_LOG_CATEGORY(LogVehicles);
DEFINE_LOG_CATEGORY(LogBuilding);
DEFINE_LOG_CATEGORY(LogAI);
DEFINE_LOG_CATEGORY(LogPlayer);
DEFINE_LOG_CATEGORY(LogPawn);
DEFINE_LOG_CATEGORY(LogCreative);
DEFINE_LOG_CATEGORY(LogQuests);
DEFINE_LOG_CATEGORY(LogTemp);
DEFINE_LOG_CATEGORY(LogCheats);
DEFINE_LOG_CATEGORY(LogKismet);
DEFINE_LOG_CATEGORY(LogGameMode);
DEFINE_LOG_CATEGORY(LogGameState);
DEFINE_LOG_CATEGORY(LogNet);

inline bool bCreative = false;
constexpr bool bGameSessions = false;
inline bool bImposters = false;
inline bool bPlayEvent = false;
namespace Creative
{
    extern xmap<xstring, UFortCreativeRealEstatePlotItemDefinition*> PlotDefinitionsByMcpId;
}
