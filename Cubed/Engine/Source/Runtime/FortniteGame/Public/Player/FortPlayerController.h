#pragma once
#include "pch.h"

namespace FortPlayerController
{
    DefineOriginal(void, ServerAcknowledgePossession, AFortPlayerControllerAthena* Controller, APawn* Pawn);
    DefineOriginal(void, ServerExecuteInventoryItem, AFortPlayerController* Controller, FGuid ItemGuid);
    void ServerCreateBuildingActor(AFortPlayerControllerAthena *Controller, FCreateBuildingActorData &CreateBuildingData);
    void ServerBeginEditingBuildingActor(AFortPlayerController* PlayerController, ABuildingSMActor* BuildingActorToEdit);
    void ServerEditBuildingActor(AFortPlayerController* PlayerController, ABuildingSMActor* Building, TSubclassOf<ABuildingSMActor> BuildingClass, uint8 Rot, bool Mirrored);
    void ServerEndEditingBuildingActor(AFortPlayerController* PlayerController, ABuildingSMActor* BuildingActorToEdit);
    void ServerAttemptInventoryDrop(AFortPlayerController* Controller, const struct FGuid& ItemGuid, int32 Count, bool bTrash);
    DefineOriginal(void, GetPlayerViewPoint, AFortPlayerControllerAthena* Controller, FVector&, FRotator&);
    DefineOriginal(void, ServerLoadingScreenDropped, AFortPlayerControllerAthena* PC);
    
    void Setup();
}