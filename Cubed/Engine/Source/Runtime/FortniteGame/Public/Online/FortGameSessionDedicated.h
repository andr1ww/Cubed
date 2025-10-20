#pragma once
#include "pch.h"

class FMatchmakingDedicatedServerMatchAssignment
{
public:

    inline FName& InternalGetPlaylistName()
    {
        return *(FName*)(__int64(this) + 0x28);
    }

    inline void InternalSetPlaylistName(const FName& InPlaylistName)
    {
        *(FName*)(__int64(this) + 0x28) = InPlaylistName;
    }

    __declspec(property(get = InternalGetPlaylistName, put = InternalSetPlaylistName))
    FName PlaylistName;
};

namespace FortGameSessionDedicated
{
    DefineOriginal(bool, HandleMatchAssignmentV2, __int64, FMatchmakingDedicatedServerMatchAssignment* MatchAssignment);

    void Setup();
}