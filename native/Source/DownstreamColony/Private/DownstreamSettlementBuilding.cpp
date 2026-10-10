#include "DownstreamSettlementBuilding.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "NavAreas/NavArea_Null.h"

namespace
{
UStaticMesh* LoadBuildingMesh(const TCHAR* Preferred, const TCHAR* Package)
{
    // CC0 Kenney model first (third_party/kenney-*), then the original blockout.
    if (FPackageName::DoesPackageExist(Preferred))
        if (auto* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), Preferred, *FPackageName::GetShortName(Preferred)))) return Mesh;
    if (FPackageName::DoesPackageExist(Package))
        if (auto* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), Package, *FPackageName::GetShortName(Package)))) return Mesh;
    return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
}
}

ADownstreamSettlementBuilding::ADownstreamSettlementBuilding()
{
    PrimaryActorTick.bCanEverTick = false;
    auto* Root = CreateDefaultSubobject<USceneComponent>(TEXT("BuildingRoot"));
    SetRootComponent(Root);
    BuildingCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BuildingCollision"));
    BuildingCollision->SetupAttachment(Root);
    BuildingCollision->SetBoxExtent(FVector(90.0f, 90.0f, 100.0f));
    BuildingCollision->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
    BuildingCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    BuildingCollision->SetGenerateOverlapEvents(false);
    BuildingCollision->SetCanEverAffectNavigation(true);
    BuildingCollision->bDynamicObstacle = true;
    BuildingCollision->SetAreaClassOverride(UNavArea_Null::StaticClass());
    BuildingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildingMesh"));
    BuildingMesh->SetupAttachment(Root);
    BuildingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BuildingMesh->SetCanEverAffectNavigation(false);
    FoundationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FoundationMesh"));
    FoundationMesh->SetupAttachment(Root);
    FoundationMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FoundationMesh->SetCanEverAffectNavigation(false);
    FoundationMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 5.0f));
    FoundationMesh->SetRelativeScale3D(FVector(1.6f, 1.6f, 0.1f));
    WorldLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("WorldLabel"));
    WorldLabel->SetupAttachment(Root);
    WorldLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 195.0f));
    WorldLabel->SetRelativeRotation(FRotator(55.0f, -135.0f, 0.0f));
    WorldLabel->SetHorizontalAlignment(EHTA_Center);
    WorldLabel->SetWorldSize(23.0f);
}

void ADownstreamSettlementBuilding::InitializeStructure(EDownstreamStructureType Type)
{
    StructureType = Type;
    RefreshAppearance();
}

void ADownstreamSettlementBuilding::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshAppearance();
}

void ADownstreamSettlementBuilding::BeginPlay()
{
    Super::BeginPlay();
    RefreshAppearance();
}

void ADownstreamSettlementBuilding::RefreshAppearance()
{
    const TCHAR* Package = TEXT("/Game/Downstream/Art/SM_Crate");
    const TCHAR* Preferred = TEXT("/Game/Downstream/ThirdParty/Kenney/Industrial/shipping_container_a/SM_K_shipping_container_a");
    const TCHAR* Label = TEXT("STORAGE +8");
    FColor Color(245, 216, 145);
    if (StructureType == EDownstreamStructureType::Workshop)
    {
        Package = TEXT("/Game/Downstream/Art/SM_Shelter");
        Preferred = TEXT("/Game/Downstream/ThirdParty/Kenney/Survival/structure_metal/SM_K_structure_metal");
        Label = TEXT("WORKSHOP");
        Color = FColor(244, 178, 113);
    }
    else if (StructureType == EDownstreamStructureType::Charger)
    {
        Package = TEXT("/Game/Downstream/Art/SM_Drum");
        Preferred = TEXT("/Game/Downstream/ThirdParty/Kenney/Industrial/solar_panel_landscape_group/SM_K_solar_panel_landscape_group");
        Label = TEXT("CHARGER");
        Color = FColor(94, 236, 213);
    }
    UStaticMesh* Mesh = LoadBuildingMesh(Preferred, Package);
    BuildingMesh->SetStaticMesh(Mesh);
    if (Mesh)
    {
        // Use imported bounds, including an off-centre pivot. All art fits 160x160x180cm,
        // inside the authoritative 180x180x200cm collision used by placement and Recast.
        const FBoxSphereBounds Bounds = Mesh->GetBounds();
        const FVector Size = Bounds.BoxExtent * 2.0f;
        const float Scale = FMath::Min(160.0f / FMath::Max(1.0f, FMath::Max(Size.X, Size.Y)), 170.0f / FMath::Max(1.0f, Size.Z));
        BuildingMesh->SetRelativeScale3D(FVector(Scale));
        BuildingMesh->SetRelativeLocation(FVector(-Bounds.Origin.X * Scale, -Bounds.Origin.Y * Scale,
            10.0f - (Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale));
    }
    FoundationMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    if (FPackageName::DoesPackageExist(TEXT("/Game/Downstream/Materials/M_PrototypeHome")))
        if (auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Downstream/Materials/M_PrototypeHome.M_PrototypeHome")))
            FoundationMesh->SetMaterial(0, Material);
    WorldLabel->SetText(FText::FromString(Label));
    WorldLabel->SetTextRenderColor(Color);
}
