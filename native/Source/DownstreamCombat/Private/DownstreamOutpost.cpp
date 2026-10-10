#include "DownstreamOutpost.h"

#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamPrototypeTuning.h"
#include "DownstreamWaterSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "Sound/SoundBase.h"

namespace
{
UStaticMesh* LoadMesh(const TCHAR* Package)
{
    if (FPackageName::DoesPackageExist(Package))
        if (auto* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), Package, *FPackageName::GetShortName(Package)))) return Mesh;
    return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
}

const TCHAR* TypeLabel(int32 Type) { return Type == 1 ? TEXT("GENERATOR") : Type == 2 ? TEXT("STASH") : TEXT("HUT"); }
}

// ---- Structure ----------------------------------------------------------------

ADownstreamOutpostStructure::ADownstreamOutpostStructure()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.2f;
    Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
    SetRootComponent(Collision);
    Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Collision->SetCanEverAffectNavigation(true);
    Collision->bDynamicObstacle = true;
    // Expensive but passable: the digger may still need to reach a canal cell beside a building.
    Collision->SetAreaClassOverride(UNavArea_Obstacle::StaticClass());
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(Collision);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCanEverAffectNavigation(false);
    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
    Label->SetupAttachment(Collision);
    Label->SetUsingAbsoluteRotation(true);
    Label->SetWorldRotation(FRotator(55, -135, 0));
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetWorldSize(20.0f);
    Tags.AddUnique(TEXT("DownstreamHostileStructure"));
}

void ADownstreamOutpostStructure::Initialize(int32 InOutpost, int32 InStructure)
{
    OutpostIndex = InOutpost;
    StructureIndex = InStructure;
    auto* Water = UDownstreamWaterSubsystem::GetForWorld(this);
    if (!Water) return;
    const int32 Type = Water->GetOutpostStructureType(OutpostIndex, StructureIndex);
    // CC0 Kenney models: corrugated hut, factory machine generator, shipping-container stash.
    const TCHAR* Package = Type == 1 ? TEXT("/Game/Downstream/ThirdParty/Kenney/Factory/machine/SM_K_machine")
        : Type == 2 ? TEXT("/Game/Downstream/ThirdParty/Kenney/Industrial/shipping_container_b/SM_K_shipping_container_b")
        : TEXT("/Game/Downstream/ThirdParty/Kenney/Survival/structure_metal_roof/SM_K_structure_metal_roof");
    UStaticMesh* Model = LoadMesh(Package);
    Mesh->SetStaticMesh(Model);
    const FVector Target = Type == 1 ? FVector(170, 170, 160) : Type == 2 ? FVector(180, 330, 160) : FVector(190, 190, 175);
    const FBoxSphereBounds Bounds = Model->GetBounds();
    const FVector Size = Bounds.BoxExtent * 2.0f;
    const float Scale = FMath::Min3(Target.X / FMath::Max(1.0, Size.X), Target.Y / FMath::Max(1.0, Size.Y), Target.Z / FMath::Max(1.0, Size.Z));
    Mesh->SetRelativeScale3D(FVector(Scale));
    MeshBaseZ = -(Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale - Size.Z * Scale * 0.5f;
    Mesh->SetRelativeLocation(FVector(-Bounds.Origin.X * Scale, -Bounds.Origin.Y * Scale, MeshBaseZ));
    // Collision a bit smaller than the model so raiders can slip between buildings.
    Collision->SetBoxExtent(Size * Scale * 0.5f * FVector(0.72f, 0.72f, 1.0f));
    BaseLocation = Water->GetOutpostStructureLocation(OutpostIndex, StructureIndex) + FVector(0, 0, Size.Z * Scale * 0.5f);
    SetActorLocation(BaseLocation);
    Label->SetRelativeLocation(FVector(0, 0, Size.Z * Scale * 0.5f + 70.0f));
    if (Type == 1) Label->SetTextRenderColor(FColor(255, 200, 80));
    Refresh();
    bInitialized = true;
}

float ADownstreamOutpostStructure::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    auto* Water = UDownstreamWaterSubsystem::GetForWorld(this);
    if (!Water || OutpostIndex < 0 || DamageAmount <= 0.0f) return 0.0f;
    const float Scaled = DamageAmount * UDownstreamExpeditionSubsystem::GetTuning(GetWorld())->StructureGunfireScale;
    Water->DamageOutpostStructure(OutpostIndex, StructureIndex, Scaled);
    Refresh();
    return Scaled;
}

void ADownstreamOutpostStructure::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Refresh();
}

void ADownstreamOutpostStructure::Refresh()
{
    auto* Water = UDownstreamWaterSubsystem::GetForWorld(this);
    if (!Water || OutpostIndex < 0) return;
    const int32 Type = Water->GetOutpostStructureType(OutpostIndex, StructureIndex);
    if (Water->IsOutpostStructureDestroyed(OutpostIndex, StructureIndex))
    {
        if (bShownDestroyed) return;
        bShownDestroyed = true;
        if (bInitialized)
            if (const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld()); Tuning && Tuning->CollapseSound)
                UGameplayStatics::PlaySoundAtLocation(this, Tuning->CollapseSound, GetActorLocation(), 1.0f, FMath::FRandRange(0.85f, 1.1f));
        // Collapse: slump, tilt and sink into the mud; nothing left to shoot or block.
        Mesh->SetRelativeRotation(FRotator(18.0f, 0.0f, 24.0f));
        Mesh->SetRelativeLocation(Mesh->GetRelativeLocation() - FVector(0, 0, 70.0f));
        Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Tags.Remove(TEXT("DownstreamHostileStructure"));
        Label->SetText(FText::FromString(FString::Printf(TEXT("%s / collapsed"), TypeLabel(Type))));
        Label->SetTextRenderColor(FColor(140, 140, 140));
        for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
            if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(Slot))
                Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.15f, 0.13f, 0.11f));
        return;
    }
    const float Fraction = FMath::Clamp(Water->GetOutpostStructureIntegrity(OutpostIndex, StructureIndex)
        / FMath::Max(1.0f, Water->GetOutpostStructureMaxIntegrity(OutpostIndex, StructureIndex)), 0.0f, 1.0f);
    const float Wet = Water->GetOutpostStructureWater(OutpostIndex, StructureIndex);
    // Weakening shows as a lean and a slow sink.
    Mesh->SetRelativeRotation(FRotator(0.0f, 0.0f, (1.0f - Fraction) * 9.0f));
    FString Text = FString::Printf(TEXT("%s %d%%"), TypeLabel(Type), FMath::RoundToInt(Fraction * 100.0f));
    if (Wet > 0.25f) Text += FString::Printf(TEXT("\nFLOODING %.1f m"), Wet);
    Label->SetText(FText::FromString(Text));
    if (Wet > 0.25f) Label->SetTextRenderColor(FColor(120, 200, 255));
    else Label->SetTextRenderColor(Type == 1 ? FColor(255, 200, 80) : FColor(230, 170, 140));
}

// ---- Outpost ------------------------------------------------------------------

ADownstreamOutpost::ADownstreamOutpost()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.25f;
    auto* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
    Banner = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Banner"));
    Banner->SetupAttachment(Root);
    Banner->SetRelativeLocation(FVector(0, 0, 520));
    Banner->SetUsingAbsoluteRotation(true);
    Banner->SetWorldRotation(FRotator(55, -135, 0));
    Banner->SetHorizontalAlignment(EHTA_Center);
    Banner->SetWorldSize(34.0f);
    Banner->SetTextRenderColor(FColor(255, 120, 70));
    AlarmLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("AlarmLight"));
    AlarmLight->SetupAttachment(Root);
    AlarmLight->SetRelativeLocation(FVector(0, 0, 350));
    AlarmLight->SetAttenuationRadius(1600.0f);
    AlarmLight->SetIntensity(0.0f);
    AlarmLight->SetLightColor(FLinearColor(1.0f, 0.15f, 0.05f));
    AlarmLight->SetCastShadows(false);
}

void ADownstreamOutpost::BeginPlay()
{
    Super::BeginPlay();
    auto* Water = UDownstreamWaterSubsystem::GetForWorld(this);
    if (!Water || OutpostIndex < 0 || OutpostIndex >= Water->GetOutpostCount()) return;
    SetActorLocation(Water->GetOutpostCenter(OutpostIndex));
    for (int32 Index = 0; Index < Water->GetOutpostStructureCount(OutpostIndex); ++Index)
    {
        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.ObjectFlags |= RF_Transient;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (auto* Structure = GetWorld()->SpawnActor<ADownstreamOutpostStructure>(ADownstreamOutpostStructure::StaticClass(), GetActorLocation(), FRotator(0, 15.0f * Index, 0), Params))
        {
            Structure->Initialize(OutpostIndex, Index);
            Structures.Add(Structure);
        }
    }
    // Garrison: two guards, two riflemen around the pad, two engineers on the levee.
    const auto& Plan = Water->GetLayout().Outposts[static_cast<std::size_t>(OutpostIndex)];
    for (int32 Index = 0; Index < static_cast<int32>(Plan.GuardSpawns.size()); ++Index)
    {
        FSlot Slot;
        Slot.Location = Water->CellToWorld(FIntPoint(Plan.GuardSpawns[Index].X, Plan.GuardSpawns[Index].Y)) + FVector(0, 0, 100);
        Slot.Location.Z = Water->GetOutpostCenter(OutpostIndex).Z + 100.0f;
        Slot.Role = Index % 2 == 0 ? EDownstreamRaiderRole::Guard : EDownstreamRaiderRole::Rifleman;
        Slots.Add(Slot);
    }
    // Engineers man the levee on the river side, outside the ring of buildings.
    const FVector Center = Water->GetOutpostCenter(OutpostIndex);
    FVector RiverSide = (Water->CellToWorld(Water->GetOutpostRiverCell(OutpostIndex)) - Center).GetSafeNormal2D();
    if (RiverSide.IsNearlyZero()) RiverSide = FVector(1, 0, 0);
    const FVector Across(-RiverSide.Y, RiverSide.X, 0.0f);
    for (int32 Index = 0; Index < static_cast<int32>(Plan.EngineerSpawns.size()); ++Index)
    {
        FSlot Slot;
        Slot.Location = Center + RiverSide * 390.0f + Across * (Index % 2 == 0 ? 240.0f : -240.0f);
        Slot.Location.Z = Center.Z + 100.0f;
        Slot.Role = EDownstreamRaiderRole::Engineer;
        Slots.Add(Slot);
    }
    if (!Water->IsOutpostRazed(OutpostIndex))
        for (FSlot& Slot : Slots) SpawnRaider(Slot);
    UpdateBanner();
}

bool ADownstreamOutpost::SpawnRaider(FSlot& Slot)
{
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    Params.bDeferConstruction = true;
    const FTransform Transform(FRotator(0, FMath::FRandRange(0.0f, 360.0f), 0), Slot.Location);
    auto* Raider = GetWorld()->SpawnActor<ADownstreamRaider>(ADownstreamRaider::StaticClass(), Transform, Params);
    if (!Raider) return false;
    Raider->RaiderRole = Slot.Role;
    Raider->OutpostIndex = OutpostIndex;
    Raider->FinishSpawning(Transform);
    // Never leave a raider embedded in a hut or the generator: find the nearest clear spot.
    FVector Clear = Slot.Location;
    if (GetWorld()->FindTeleportSpot(Raider, Clear, Transform.Rotator()) && !Clear.Equals(Raider->GetActorLocation(), 1.0f))
        Raider->SetActorLocation(Clear, false, nullptr, ETeleportType::TeleportPhysics);
    Raider->SetHomePost(Raider->GetActorLocation());
    Slot.Raider = Raider;
    Slot.DownSince = -1.0;
    return true;
}

int32 ADownstreamOutpost::RespawnGarrisonNow()
{
    int32 Count = 0;
    for (FSlot& Slot : Slots)
        if (!(Slot.Raider.IsValid() && !Slot.Raider->IsDead()) && SpawnRaider(Slot)) ++Count;
    return Count;
}

int32 ADownstreamOutpost::GetLivingRaiders() const
{
    int32 Count = 0;
    for (const FSlot& Slot : Slots) if (Slot.Raider.IsValid() && !Slot.Raider->IsDead()) ++Count;
    return Count;
}

int32 ADownstreamOutpost::GetLivingEngineers() const
{
    int32 Count = 0;
    for (const FSlot& Slot : Slots)
        if (Slot.Role == EDownstreamRaiderRole::Engineer && Slot.Raider.IsValid() && !Slot.Raider->IsDead()) ++Count;
    return Count;
}

TArray<ADownstreamRaider*> ADownstreamOutpost::GetRaiders() const
{
    TArray<ADownstreamRaider*> Result;
    for (const FSlot& Slot : Slots) if (Slot.Raider.IsValid()) Result.Add(Slot.Raider.Get());
    return Result;
}

void ADownstreamOutpost::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    auto* Water = UDownstreamWaterSubsystem::GetForWorld(this);
    if (!Water || OutpostIndex < 0) return;
    const double Now = GetWorld()->GetTimeSeconds();
    FIntPoint Breach;
    const bool bBreach = Water->GetOutpostBreach(OutpostIndex, Breach);
    const bool bFlooded = Water->GetOutpostFloodDepth(OutpostIndex) > 0.25f;
    bool bDiggerNear = false;
    TArray<AActor*> Diggers;
    UGameplayStatics::GetAllActorsWithTag(this, TEXT("DownstreamDigger"), Diggers);
    for (AActor* Digger : Diggers)
        if (FVector::DistSquared2D(Digger->GetActorLocation(), GetActorLocation())
            < FMath::Square(Water->GetOutpostRadiusCm(OutpostIndex) + UDownstreamExpeditionSubsystem::GetTuning(GetWorld())->DiggerThreatRadius))
            bDiggerNear = true;
    const bool bRazed = Water->IsOutpostRazed(OutpostIndex);
    const bool bWasAlerted = bAlerted;
    bAlerted = !bRazed && (bBreach || bFlooded || bDiggerNear);
    if (const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld()))
    {
        // Klaxon on alert and every few seconds while it lasts; a gush when the levee first opens.
        if (bAlerted && Tuning->AlarmSound && (!bWasAlerted || Now >= NextAlarm))
        {
            NextAlarm = Now + 6.0;
            UGameplayStatics::PlaySoundAtLocation(this, Tuning->AlarmSound, GetActorLocation() + FVector(0, 0, 300), 0.6f);
        }
        if (bBreach && !bWasBreached && Tuning->BreachSound)
            UGameplayStatics::PlaySoundAtLocation(this, Tuning->BreachSound, Water->CellToWorld(Breach), 1.0f);
    }
    bWasBreached = bBreach;
    AlarmLight->SetIntensity(bAlerted ? 2500.0f + 2000.0f * FMath::Abs(FMath::Sin(Now * 4.0)) : 0.0f);
    if (Now >= NextThink)
    {
        NextThink = Now + 1.0;
        if (!bRazed)
        {
            const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld());
            const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
            for (FSlot& Slot : Slots)
            {
                const bool bAlive = Slot.Raider.IsValid() && !Slot.Raider->IsDead();
                if (bAlive) { Slot.DownSince = -1.0; continue; }
                if (Slot.DownSince < 0.0) { Slot.DownSince = Now; continue; }
                // Engineers are replaced faster while the levee is breached.
                const float Delay = Tuning->OutpostRespawnSeconds * (Slot.Role == EDownstreamRaiderRole::Engineer && bBreach ? 0.5f : 1.0f);
                const bool bPlayerFar = !Player || FVector::DistSquared2D(Player->GetActorLocation(), Slot.Location) > FMath::Square(1500.0f);
                if (Now - Slot.DownSince >= Delay && bPlayerFar) SpawnRaider(Slot);
            }
        }
        UpdateBanner();
    }
}

FText ADownstreamOutpost::GetStatusText() const
{
    auto* Water = UDownstreamWaterSubsystem::GetForWorld(const_cast<ADownstreamOutpost*>(this));
    if (!Water || OutpostIndex < 0) return FText::GetEmpty();
    if (Water->IsOutpostRazed(OutpostIndex)) return FText::FromString(TEXT("RAZED"));
    const float Flood = Water->GetOutpostFloodDepth(OutpostIndex);
    FIntPoint Breach;
    if (Flood > 0.25f) return FText::FromString(FString::Printf(TEXT("FLOODED %.1f m"), Flood));
    if (Water->GetOutpostBreach(OutpostIndex, Breach)) return FText::FromString(TEXT("LEVEE BREACHED"));
    return FText::FromString(bAlerted ? TEXT("ALERT") : TEXT("holding"));
}

void ADownstreamOutpost::UpdateBanner()
{
    auto* Water = UDownstreamWaterSubsystem::GetForWorld(this);
    if (!Water || OutpostIndex < 0) return;
    float Generator = 0.0f;
    for (int32 Index = 0; Index < Water->GetOutpostStructureCount(OutpostIndex); ++Index)
        if (Water->GetOutpostStructureType(OutpostIndex, Index) == 1)
            Generator = Water->GetOutpostStructureIntegrity(OutpostIndex, Index) / FMath::Max(1.0f, Water->GetOutpostStructureMaxIntegrity(OutpostIndex, Index));
    Banner->SetText(FText::FromString(FString::Printf(TEXT("DAM CULT OUTPOST\n%s / generator %d%% / %d raiders"),
        *GetStatusText().ToString(), FMath::RoundToInt(Generator * 100.0f), GetLivingRaiders())));
    Banner->SetTextRenderColor(Water->IsOutpostRazed(OutpostIndex) ? FColor(150, 150, 150) : bAlerted ? FColor(255, 70, 50) : FColor(255, 150, 90));
}
