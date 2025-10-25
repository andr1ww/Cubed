#include "pch.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Components/FortAthenaAISpawnerDataComponents.h"

#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"
#include "Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h"

void FortAthenaAISpawnerDataComponents::OnPawnAISpawned(AFortAthenaAIBotController* C, AFortPlayerPawn* Pawn)
{
    auto Controller = Cast<AFortAthenaAIBotController>(Pawn->GetController());
    *reinterpret_cast<bool*>(__int64(Controller->CachedAIServicePlayerBots) + 0x688) = true; //bCanActivateBrain
    OnPawnAISpawnedOG(C, Pawn);

    static xvector<UAthenaCharacterItemDefinition*> Characters;
    static xvector<UAthenaDanceItemDefinition*> Dances;
    if (Characters.size() == 0)
    {
        Characters = GetObjectsOfClass<UAthenaCharacterItemDefinition>();
        Dances = GetObjectsOfClass<UAthenaDanceItemDefinition>();
    }

    for (auto& Dance : Dances) {
        if (IsValidPointer(Dance)) 
            if (!Dance->IsA(UAthenaSprayItemDefinition::StaticClass())) Controller->CosmeticLoadoutBC.Dances.Add(Dance);
    }
    
    auto PlayerState = Cast<AFortPlayerStateAthena>(Controller->PlayerState);
    if (Characters.size() != 0) {
        UAthenaCharacterItemDefinition* CID = nullptr;
        while (!IsValidPointer(CID)) {
            auto ok = Characters[UKismetMathLibrary::RandomIntegerInRange(0, Characters.size() - 1)];
            if (IsValidPointer(ok))
                if (ok->BaseCharacterParts.Num() >= 1) CID = ok;
        }
        
        if (CID)
        {
            for (size_t i = 0; i < CID->BaseCharacterParts.Num(); i++)
            {
                UCustomCharacterPart* Part = CID->BaseCharacterParts[i].Get();
                if (Part)
                {
                    PlayerState->CharacterData.Parts[(uintptr_t)Part->CharacterPartType] = Part;
                    Pawn->CharacterParts[(uintptr_t)Part->CharacterPartType] = Part;
                    Pawn->ServerChoosePart(Part->CharacterPartType, Part);
                } 
            }

            PlayerState->OnRep_CharacterData();
        }
    }
}

void FortAthenaAISpawnerDataComponents::Setup()
{
    UHook* Hook = new UHook();

    Hook->Address = ImageBase + 0x47E939C; 
    Hook->Detour = OnPawnAISpawned;
    Hook->Original = (void**)&OnPawnAISpawnedOG;
    UKismetHookingLibrary::Hook(Hook, Address);
    
    Hook->Address = ImageBase + 0x129ED4C;
    UKismetHookingLibrary::Hook(Hook, RTrue);

    delete Hook;
}
