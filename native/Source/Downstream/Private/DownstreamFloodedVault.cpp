#include "DownstreamFloodedVault.h"

#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamWaterSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"

ADownstreamFloodedVault::ADownstreamFloodedVault()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.2f;
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    SetRootComponent(Body);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetCanEverAffectNavigation(false);
    Hatch = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hatch"));
    Hatch->SetupAttachment(Body);
    Hatch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Hatch->SetCanEverAffectNavigation(false);
    Beacon = CreateDefaultSubobject<UPointLightComponent>(TEXT("Beacon"));
    Beacon->SetupAttachment(Body);
    Beacon->SetRelativeLocation(FVector(0, 0, 160));
    Beacon->SetAttenuationRadius(520.0f);
    Beacon->SetIntensity(900.0f);
    Beacon->SetCastShadows(false);
    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
    Label->SetupAttachment(Body);
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetWorldSize(30.0f);
    Label->SetUsingAbsoluteRotation(true);
    Label->SetWorldRotation(FRotator(55, -135, 0));
}

void ADownstreamFloodedVault::BeginPlay()
{
    Super::BeginPlay();
    auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    Body->SetStaticMesh(Cube);
    Body->SetWorldScale3D(FVector(1.3f, 0.9f, 0.7f));
    Hatch->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
    Hatch->SetRelativeLocation(FVector(0, 0, 55));
    Hatch->SetRelativeScale3D(FVector(0.45f, 0.65f, 0.12f));
    if (FPackageName::DoesPackageExist(TEXT("/Game/Downstream/Materials/M_PrototypeRecovery")))
        if (auto* Gold = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Downstream/Materials/M_PrototypeRecovery.M_PrototypeRecovery")))
            Hatch->SetMaterial(0, Gold);
    // Sit the box on the basin floor.
    AddActorWorldOffset(FVector(0, 0, 35));
    Label->SetWorldLocation(GetActorLocation() + FVector(0, 0, 260));
}

void ADownstreamFloodedVault::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const auto* Water = UDownstreamWaterSubsystem::GetForWorld(this);
    const auto* Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this);
    if (!Water || !Expedition) return;
    Label->SetWorldLocation(GetActorLocation() + FVector(0, 0, 260));
    if (Water->IsVaultOpenedAt(VaultIndex))
    {
        Label->SetText(FText::FromString(TEXT("VAULT / empty")));
        Label->SetTextRenderColor(FColor(150, 160, 155));
        Beacon->SetLightColor(FLinearColor(0.3f, 0.35f, 0.33f));
        Beacon->SetIntensity(150.0f);
        Hatch->SetRelativeRotation(FRotator(0, 0, 70));
        return;
    }
    const float Depth = Water->GetVaultDepthAt(VaultIndex);
    if (Water->IsVaultAccessibleAt(VaultIndex))
    {
        Label->SetText(FText::FromString(TEXT("VAULT / [E] open")));
        Label->SetTextRenderColor(FColor(255, 196, 82));
        Beacon->SetLightColor(FLinearColor(1.0f, 0.65f, 0.15f));
        Beacon->SetIntensity(1400.0f);
    }
    else
    {
        Label->SetText(FText::FromString(FString::Printf(TEXT("SEALED VAULT / %.1f m under"), Depth)));
        Label->SetTextRenderColor(FColor(120, 200, 255));
        Beacon->SetLightColor(FLinearColor(0.2f, 0.55f, 1.0f));
        Beacon->SetIntensity(700.0f);
    }
}
