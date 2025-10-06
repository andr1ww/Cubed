#include "pch.h"
#include "Engine/Source/Runtime/FortniteGame/Public/Athena/FortPoiVolume.h"

#include "Logging.h"
#include "Engine/Plugins/HookingLibrary/Public/HookingLibrary.h"

void FortPoiVolume::PostInitializeComponents(AFortPoiVolume* Volume)
{
    Volume->BrushComponent = (UBrushComponent*)UGameplayStatics::SpawnObject(UBrushComponent::StaticClass(), Volume);
    Volume->K2_AttachToComponent(Volume->RootComponent, FName(0), EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, false);
    
    static void (*SetCollisonProfileName)(UBrushComponent*, __int64*, __int64*) =
        decltype(SetCollisonProfileName)(Volume->BrushComponent->VTable[(0x620 / 8)]);
    
    ((void (*)(UObject* Component, UObject* World))(ImageBase + 0xED7958))(Volume->BrushComponent, UWorld::GetWorld());
    SetCollisonProfileName(Volume->BrushComponent, reinterpret_cast<__int64*>(ImageBase + 0x9D00AE8), (__int64*)1);
    Volume->BrushComponent->bGenerateOverlapEvents = true;
    Volume->BrushComponent->SetGenerateOverlapEvents(true);
    
    UBodySetup* BodySetup = (UBodySetup*)UGameplayStatics::SpawnObject(UBodySetup::StaticClass(), Volume->BrushComponent);
    BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;
    BodySetup->bGenerateMirroredCollision = false;
    BodySetup->bDoubleSidedGeometry = true;

    Volume->BrushComponent->Brush = Volume->Brush;
    Volume->BrushComponent->BrushBodySetup = BodySetup;
    
    if (Volume->Brush)
    {
        TArray<FVector>* Points = (TArray<FVector>*)(__int64(Volume->Brush) + 0x58);
        
        if (Points && Points->Num() > 0)
        {
            FVector MinPt = (*Points)[0];
            FVector MaxPt = (*Points)[0];
            
            for (int i = 1; i < Points->Num(); i++)
            {
                FVector& pt = (*Points)[i];
                MinPt.X = pt.X < MinPt.X ? pt.X : MinPt.X;
                MinPt.Y = pt.Y < MinPt.Y ? pt.Y : MinPt.Y;
                MinPt.Z = pt.Z < MinPt.Z ? pt.Z : MinPt.Z;
                MaxPt.X = pt.X > MaxPt.X ? pt.X : MaxPt.X;
                MaxPt.Y = pt.Y > MaxPt.Y ? pt.Y : MaxPt.Y;
                MaxPt.Z = pt.Z > MaxPt.Z ? pt.Z : MaxPt.Z;
            }
            
            FVector Center = (MinPt + MaxPt) * 0.5f;
            FVector Extent = (MaxPt - MinPt) * 0.5f;
            
            float* Bounds = (float*)(__int64(Volume->BrushComponent) + 0x100);
            Bounds[0] = Center.X;
            Bounds[1] = Center.Y;
            Bounds[2] = Center.Z;
            *(float*)(__int64(Volume->BrushComponent) + 0x10C) = Extent.X;
            *(float*)(__int64(Volume->BrushComponent) + 0x110) = Extent.Y;
        }
    }
    
    return PostInitializeComponentsOG(Volume);
}

void FortPoiVolume::Setup()
{
    UHook* Hook = new UHook();

    Hook->Address = ImageBase + 0x16EB308;
    Hook->Original = (void**)&PostInitializeComponentsOG;
    Hook->Detour = PostInitializeComponents;
    UKismetHookingLibrary::Hook(Hook, EHook::Address);
    
    UKismetHookingLibrary::PatchBytes(ImageBase + 0x20CA9E8, {
        0xB0, 0x01,  // mov al, 1
        0xC3         // ret
    });
}