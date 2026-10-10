#include "DownstreamWorldDirector.h"

#include "DownstreamEnemy.h"
#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamExtractionPoint.h"
#include "DownstreamGameMode.h"
#include "DownstreamOutpost.h"
#include "DownstreamSalvagePickup.h"
#include "DownstreamSettlementManager.h"
#include "DownstreamWaterSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogDownstreamWorld, Log, All);

namespace
{
UStaticMesh* LoadKenney(const TCHAR* Kit, const TCHAR* Model)
{
    const FString Package = FString::Printf(TEXT("/Game/Downstream/ThirdParty/Kenney/%s/%s/SM_K_%s"), Kit, Model, Model);
    if (!FPackageName::DoesPackageExist(Package)) return nullptr;
    return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.SM_K_%s"), *Package, Model));
}
}

ADownstreamWorldDirector::ADownstreamWorldDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.25f;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ADownstreamWorldDirector::BeginPlay()
{
    Super::BeginPlay();
    Populate();
}

void ADownstreamWorldDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bPopulated) Populate();
}

TArray<ADownstreamOutpost*> ADownstreamWorldDirector::GetOutposts() const
{
    TArray<ADownstreamOutpost*> Result;
    for (const auto& Outpost : Outposts) if (IsValid(Outpost)) Result.Add(Outpost);
    return Result;
}

void ADownstreamWorldDirector::Populate()
{
    auto* Water = UDownstreamWaterSubsystem::GetForWorld(this);
    auto* Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this);
    if (bPopulated || !Water || !Expedition || !Water->EnsureWorldActive() || !Water->IsWorldSite()) return;
    bPopulated = true;
    const auto& Layout = Water->GetLayout();
    auto* GameMode = GetWorld()->GetAuthGameMode<ADownstreamGameMode>();
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // Home: extraction marker and the settlement manager on the flat clearing.
    GetWorld()->SpawnActor<ADownstreamExtractionPoint>(ADownstreamExtractionPoint::StaticClass(), Expedition->GetHomeLocation(), FRotator::ZeroRotator, Params);
    if (!ADownstreamSettlementManager::GetForWorld(this))
        GetWorld()->SpawnActor<ADownstreamSettlementManager>(ADownstreamSettlementManager::StaticClass(), Expedition->GetBuildGridCenter(), FRotator::ZeroRotator, Params);
    // If the player spawned before the layout existed, bring them home.
    if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
        if (!Expedition->IsAtHome(Pawn->GetActorLocation()))
            Pawn->TeleportTo(Expedition->GetHomeLocation(), Pawn->GetActorRotation());

    for (int32 Index = 0; Index < Water->GetOutpostCount(); ++Index)
    {
        FActorSpawnParameters OutpostParams = Params;
        OutpostParams.bDeferConstruction = true;
        auto* Outpost = GetWorld()->SpawnActor<ADownstreamOutpost>(ADownstreamOutpost::StaticClass(), Water->GetOutpostCenter(Index), FRotator::ZeroRotator, OutpostParams);
        if (!Outpost) continue;
        Outpost->OutpostIndex = Index;
        Outpost->FinishSpawning(FTransform(Water->GetOutpostCenter(Index)));
        Outposts.Add(Outpost);
    }

    // Feral salvage bots wander the ruins and respawn like M1's field enemies.
    for (const auto& Patrol : Layout.Patrols)
    {
        const FVector Location = Water->CellToWorld(FIntPoint(Patrol.Cell.X, Patrol.Cell.Y)) + FVector(0, 0, 100);
        FActorSpawnParameters EnemyParams;
        EnemyParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        EnemyParams.bDeferConstruction = true;
        auto* Enemy = GetWorld()->SpawnActor<ADownstreamEnemy>(ADownstreamEnemy::StaticClass(), Location, FRotator::ZeroRotator, EnemyParams);
        if (!Enemy) continue;
        Enemy->Kind = Patrol.Shooter ? EDownstreamEnemyKind::Shooter : EDownstreamEnemyKind::Chaser;
        Enemy->FinishSpawning(FTransform(Location));
        if (GameMode) GameMode->RegisterFieldEnemy(Enemy);
    }

    for (const auto& Cache : Layout.Caches)
    {
        auto* Pickup = Expedition->SpawnSalvage(Water->CellToWorld(FIntPoint(Cache.Cell.X, Cache.Cell.Y)) + FVector(0, 0, 60), Cache.Amount, false);
        if (Pickup && GameMode) GameMode->RegisterFieldCache(Pickup);
    }

    for (const auto& Prop : Layout.Props)
        SpawnProp(FIntPoint(Prop.Cell.X, Prop.Cell.Y), static_cast<int32>(Prop.Type), Prop.Yaw);
    UE_LOG(LogDownstreamWorld, Log, TEXT("World %lld populated: %d outposts, %d patrols, %d caches, %d props"), Expedition->GetWorldSeed(),
        Outposts.Num(), static_cast<int32>(Layout.Patrols.size()), static_cast<int32>(Layout.Caches.size()), SpawnedProps);
}

void ADownstreamWorldDirector::SpawnProp(const FIntPoint& Cell, int32 Type, float Yaw)
{
    auto* Water = UDownstreamWaterSubsystem::GetForWorld(this);
    // WorldPropType: Barrel, Crate, Container, Tank, Rock, Chimney.
    UStaticMesh* Mesh = nullptr;
    float Scale = 2.6f;
    switch (Type)
    {
    case 0: Mesh = LoadKenney(TEXT("Survival"), TEXT("barrel")); Scale = 2.6f; break;
    case 1: Mesh = LoadKenney(TEXT("Survival"), TEXT("box_large")); Scale = 2.6f; break;
    case 2: Mesh = LoadKenney(TEXT("Industrial"), TEXT("shipping_container_a")); Scale = 5.0f; break;
    case 3: Mesh = LoadKenney(TEXT("Industrial"), TEXT("detail_tank")); Scale = 2.4f; break;
    case 4:
    {
        static const TCHAR* Rocks[] = {TEXT("rock_sand_a"), TEXT("rock_sand_b"), TEXT("rock_sand_c")};
        Mesh = LoadKenney(TEXT("Survival"), Rocks[(Cell.X + Cell.Y) % 3]);
        Scale = 2.5f + static_cast<float>((Cell.X * 7 + Cell.Y * 3) % 10) * 0.15f;
        break;
    }
    default: Mesh = LoadKenney(TEXT("Industrial"), TEXT("chimney_medium")); Scale = 2.6f; break;
    }
    if (!Mesh || !Water) return;
    FVector Location = Water->CellToWorld(Cell) - FVector(0, 0, 8.0f);
    auto* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, FRotator(0, Yaw, 0));
    if (!Actor) return;
    Actor->SetMobility(EComponentMobility::Movable);
    Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
    Actor->SetActorScale3D(FVector(Scale));
    Actor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
    ++SpawnedProps;
}
