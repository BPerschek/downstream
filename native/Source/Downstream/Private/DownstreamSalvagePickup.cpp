#include "DownstreamSalvagePickup.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"

ADownstreamSalvagePickup::ADownstreamSalvagePickup()
{
    PrimaryActorTick.bCanEverTick = true;
    auto* Root = CreateDefaultSubobject<USceneComponent>(TEXT("PickupRoot"));
    SetRootComponent(Root);
    SalvageMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SalvageMesh"));
    SalvageMesh->SetupAttachment(Root);
    SalvageMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SalvageMesh->SetRelativeLocation(RestMeshLocation);
    SalvageMesh->SetMobility(EComponentMobility::Movable);
    SalvageGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("SalvageGlow"));
    SalvageGlow->SetupAttachment(Root);
    SalvageGlow->SetRelativeLocation(FVector(0.0f, 0.0f, 45.0f));
    SalvageGlow->SetIntensity(280.0f);
    SalvageGlow->SetAttenuationRadius(180.0f);
    SalvageGlow->SetCastShadows(false);
    SalvageLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SalvageLabel"));
    SalvageLabel->SetupAttachment(Root);
    SalvageLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
    SalvageLabel->SetRelativeRotation(FRotator(55.0f, -135.0f, 0.0f));
    SalvageLabel->SetHorizontalAlignment(EHTA_Center);
    SalvageLabel->SetWorldSize(21.0f);
    SetActorEnableCollision(false);
}

void ADownstreamSalvagePickup::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshAppearance();
}

void ADownstreamSalvagePickup::BeginPlay()
{
    Super::BeginPlay();
    RefreshAppearance();
}

void ADownstreamSalvagePickup::RefreshAppearance()
{
    const TCHAR* Package = bRecoveryBag ? TEXT("/Game/Downstream/Art/SM_ScrapStack") : TEXT("/Game/Downstream/Art/SM_Crate");
    UStaticMesh* Mesh = nullptr;
    if (FPackageName::DoesPackageExist(Package)) Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), Package, *FPackageName::GetShortName(Package)));
    if (!Mesh) Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    SalvageMesh->SetStaticMesh(Mesh);
    SalvageMesh->SetRelativeScale3D(FVector(Mesh && FPackageName::DoesPackageExist(Package) ? 0.65f : 0.45f));
    const FColor Color = bRecoveryBag ? FColor(255, 193, 54) : FColor(97, 224, 187);
    SalvageGlow->SetLightColor(Color);
    SalvageLabel->SetTextRenderColor(Color);
    SalvageLabel->SetText(FText::FromString(FString::Printf(TEXT("%s +%d"), bRecoveryBag ? TEXT("RECOVERY") : TEXT("SALVAGE"), FMath::Max(0, Amount))));
    const TCHAR* MaterialPackage = bRecoveryBag ? TEXT("/Game/Downstream/Materials/M_PrototypeRecovery") : TEXT("/Game/Downstream/Materials/M_PrototypeSalvage");
    if (FPackageName::DoesPackageExist(MaterialPackage))
    {
        if (auto* Material = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("%s.%s"), MaterialPackage, *FPackageName::GetShortName(MaterialPackage))))
        {
            for (int32 Index = 0; Index < SalvageMesh->GetNumMaterials(); ++Index) SalvageMesh->SetMaterial(Index, Material);
        }
    }
}

void ADownstreamSalvagePickup::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.0f || bClaimed) return;
    BobSeconds = FMath::Fmod(BobSeconds + DeltaSeconds, 100.0f);
    SalvageMesh->SetRelativeLocation(RestMeshLocation + FVector(0.0f, 0.0f, FMath::Sin(BobSeconds * 2.0f) * 7.0f));
    SalvageMesh->AddLocalRotation(FRotator(0.0f, DeltaSeconds * 23.0f, 0.0f));
}

bool ADownstreamSalvagePickup::TryClaim()
{
    if (bClaimed || IsActorBeingDestroyed() || Amount <= 0) return false;
    bClaimed = true;
    return true;
}

void ADownstreamSalvagePickup::ReleaseClaim()
{
    if (!IsActorBeingDestroyed()) bClaimed = false;
}
