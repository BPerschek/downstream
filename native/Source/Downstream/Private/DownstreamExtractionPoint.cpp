#include "DownstreamExtractionPoint.h"
#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamPrototypeTuning.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"

ADownstreamExtractionPoint::ADownstreamExtractionPoint()
{
    auto* Root = CreateDefaultSubobject<USceneComponent>(TEXT("HomeRoot"));
    SetRootComponent(Root);
    HomeMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HomeMarker"));
    HomeMarker->SetupAttachment(Root);
    HomeMarker->SetRelativeLocation(FVector(0.0f, 0.0f, -80.0f));
    HomeMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HomeGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("HomeGlow"));
    HomeGlow->SetupAttachment(Root);
    HomeGlow->SetLightColor(FLinearColor(0.15f, 0.9f, 0.8f));
    HomeGlow->SetIntensity(900.0f);
    HomeGlow->SetAttenuationRadius(700.0f);
    HomeGlow->SetCastShadows(false);
    HomeLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HomeLabel"));
    HomeLabel->SetupAttachment(Root);
    HomeLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 95.0f));
    HomeLabel->SetRelativeRotation(FRotator(55.0f, -135.0f, 0.0f));
    HomeLabel->SetHorizontalAlignment(EHTA_Center);
    HomeLabel->SetWorldSize(36.0f);
    HomeLabel->SetTextRenderColor(FColor(97, 224, 187));
    HomeLabel->SetText(FText::FromString(TEXT("HOME\nE TO EXTRACT")));
    SetActorEnableCollision(false);
}

void ADownstreamExtractionPoint::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshAppearance();
}

void ADownstreamExtractionPoint::BeginPlay()
{
    Super::BeginPlay();
    RefreshAppearance();
    SetActorLocation(UDownstreamExpeditionSubsystem::GetTuning(GetWorld())->HomeLocation);
}

void ADownstreamExtractionPoint::RefreshAppearance()
{
    HomeMarker->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
    const float Radius = UDownstreamExpeditionSubsystem::GetTuning(GetWorld())->HomeRadius;
    HomeMarker->SetRelativeScale3D(FVector(FMath::Max(1.0f, Radius) / 50.0f, FMath::Max(1.0f, Radius) / 50.0f, 0.08f));
    const TCHAR* Package = TEXT("/Game/Downstream/Materials/M_PrototypeHome");
    if (FPackageName::DoesPackageExist(Package))
    {
        if (auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Downstream/Materials/M_PrototypeHome.M_PrototypeHome"))) HomeMarker->SetMaterial(0, Material);
    }
}
