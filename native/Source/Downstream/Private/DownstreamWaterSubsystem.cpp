#include "DownstreamWaterSubsystem.h"

#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamFloodedVault.h"
#include "DownstreamPrototypeTuning.h"
#include "DownstreamSalvagePickup.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#include <exception>

DEFINE_LOG_CATEGORY_STATIC(LogDownstreamWater, Log, All);

namespace
{
constexpr float MetresToCm = 100.0f;
constexpr double TerrainSaveInterval = 10.0;
constexpr float VaultInteractRadius = 260.0f;
constexpr double WarmupSeconds = 90.0;
}

UDownstreamWaterSubsystem* UDownstreamWaterSubsystem::GetForWorld(UObject* ContextObject)
{
    UWorld* World = ContextObject ? ContextObject->GetWorld() : nullptr;
    return World ? World->GetSubsystem<UDownstreamWaterSubsystem>() : nullptr;
}

bool UDownstreamWaterSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UDownstreamWaterSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UDownstreamWaterSubsystem, STATGROUP_Tickables);
}

void UDownstreamWaterSubsystem::Deinitialize()
{
    // Best effort: keep canal progress made since the last periodic save.
    if (Grid && bTerrainDirty) SaveTerrainNow();
    Grid.reset();
    Jobs.clear();
    Super::Deinitialize();
}

bool UDownstreamWaterSubsystem::ActivateSite(FVector SiteOrigin)
{
    if (Grid) return true;
    if (SiteOrigin.ContainsNaN()) return false;
    try
    {
        auto NewGrid = std::make_unique<downstream::WaterGrid>(downstream::DrownedVaultConfig());
        downstream::BuildDrownedVaultSite(*NewGrid, true);
        if (const auto* Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this))
            Expedition->GetEarthworks().ApplyTerrain(*NewGrid);
        // Settle saved canals and the spring before the first rendered frame.
        const int32 Ticks = FMath::RoundToInt(WarmupSeconds / NewGrid->Config().TickSeconds);
        for (int32 Index = 0; Index < Ticks; ++Index) NewGrid->Tick();
        NewGrid->ConsumeDirtyGroundChunks();
        Grid = std::move(NewGrid);
    }
    catch (const std::exception& Error)
    {
        UE_LOG(LogDownstreamWater, Error, TEXT("Water site failed to initialize: %hs"), Error.what());
        return false;
    }
    Origin = SiteOrigin;
    SpawnVault();
    UE_LOG(LogDownstreamWater, Log, TEXT("Drowned Vault active at %s: %.1f m3 water, vault depth %.2f m"),
        *Origin.ToCompactString(), Grid->TotalWater(), GetVaultDepth());
    return true;
}

void UDownstreamWaterSubsystem::SpawnVault()
{
    if (!GetWorld() || IsValid(Vault)) return;
    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Vault = GetWorld()->SpawnActor<ADownstreamFloodedVault>(ADownstreamFloodedVault::StaticClass(), GetVaultLocation(), FRotator(0, 20, 0), Params);
}

void UDownstreamWaterSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!Grid || !GetWorld() || !FMath::IsFinite(DeltaTime) || DeltaTime < 0.0f) return;
    Grid->Advance(DeltaTime);
    const double Now = GetWorld()->GetTimeSeconds();
    if (bTerrainDirty && Now >= NextTerrainSave)
    {
        NextTerrainSave = Now + TerrainSaveInterval;
        SaveTerrainNow();
    }
}

void UDownstreamWaterSubsystem::SimulateSeconds(float Seconds)
{
    if (!Grid || !FMath::IsFinite(Seconds) || Seconds <= 0.0f) return;
    const int32 Ticks = FMath::Min(36000, FMath::RoundToInt(Seconds / Grid->Config().TickSeconds));
    for (int32 Index = 0; Index < Ticks; ++Index) Grid->Tick();
}

bool UDownstreamWaterSubsystem::SaveTerrainNow()
{
    if (!Grid) return false;
    auto* Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this);
    if (!Expedition || !Expedition->CommitEarthworksTerrain(*Grid)) return false;
    bTerrainDirty = false;
    return true;
}

float UDownstreamWaterSubsystem::GetCellSizeCm() const
{
    return (Grid ? Grid->Config().CellSize : downstream::DrownedVaultSite::CellSize) * MetresToCm;
}

FIntPoint UDownstreamWaterSubsystem::GetGridSize() const
{
    return Grid ? FIntPoint(Grid->Width(), Grid->Height()) : FIntPoint(downstream::DrownedVaultSite::Width, downstream::DrownedVaultSite::Height);
}

bool UDownstreamWaterSubsystem::IsCellValid(FIntPoint Cell) const
{
    return Grid && Grid->InBounds(Cell.X, Cell.Y);
}

bool UDownstreamWaterSubsystem::WorldToCell(FVector World, FIntPoint& Cell) const
{
    if (!Grid || World.ContainsNaN()) return false;
    const float Size = GetCellSizeCm();
    Cell = FIntPoint(FMath::FloorToInt((World.X - Origin.X) / Size), FMath::FloorToInt((World.Y - Origin.Y) / Size));
    return Grid->InBounds(Cell.X, Cell.Y);
}

FVector UDownstreamWaterSubsystem::CellToWorld(FIntPoint Cell) const
{
    const float Size = GetCellSizeCm();
    const float Ground = IsCellValid(Cell) ? Grid->Ground(Cell.X, Cell.Y) : 0.0f;
    return Origin + FVector((Cell.X + 0.5f) * Size, (Cell.Y + 0.5f) * Size, Ground * MetresToCm);
}

float UDownstreamWaterSubsystem::GetCellGround(FIntPoint Cell) const { return IsCellValid(Cell) ? Grid->Ground(Cell.X, Cell.Y) : 0.0f; }
float UDownstreamWaterSubsystem::GetCellWater(FIntPoint Cell) const { return IsCellValid(Cell) ? Grid->Water(Cell.X, Cell.Y) : 0.0f; }

float UDownstreamWaterSubsystem::GetWaterDepthAt(FVector World) const
{
    FIntPoint Cell;
    return WorldToCell(World, Cell) ? Grid->Water(Cell.X, Cell.Y) : 0.0f;
}

float UDownstreamWaterSubsystem::GetTotalWater() const { return Grid ? static_cast<float>(Grid->TotalWater()) : 0.0f; }
float UDownstreamWaterSubsystem::GetDrainedVolume() const { return Grid ? static_cast<float>(Grid->SunkVolume()) : 0.0f; }
int64 UDownstreamWaterSubsystem::GetWaterTick() const { return Grid ? static_cast<int64>(Grid->TickCount()) : 0; }

FVector UDownstreamWaterSubsystem::GetVaultLocation() const
{
    return CellToWorld(FIntPoint(downstream::DrownedVaultSite::Vault.X, downstream::DrownedVaultSite::Vault.Y));
}

float UDownstreamWaterSubsystem::GetVaultDepth() const
{
    return GetCellWater(FIntPoint(downstream::DrownedVaultSite::Vault.X, downstream::DrownedVaultSite::Vault.Y));
}

bool UDownstreamWaterSubsystem::IsVaultAccessible() const
{
    return Grid && GetVaultDepth() <= downstream::DrownedVaultSite::VaultAccessDepth;
}

bool UDownstreamWaterSubsystem::IsPlayerNearVault(AActor* Player) const
{
    return Grid && IsValid(Player) && FVector::DistSquared2D(Player->GetActorLocation(), GetVaultLocation()) <= FMath::Square(VaultInteractRadius);
}

void UDownstreamWaterSubsystem::PlayFeedback(bool bSuccess, const FVector& Location) const
{
    const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld());
    if (USoundBase* Sound = bSuccess ? Tuning->BuildSound.Get() : Tuning->ErrorSound.Get())
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), Sound, Location, 0.6f);
}

bool UDownstreamWaterSubsystem::TryInteractVault(AActor* Player)
{
    if (!IsPlayerNearVault(Player)) return false;
    auto* Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this);
    if (!Expedition) return false;
    if (Expedition->IsVaultOpened())
    {
        Expedition->PostStatus(TEXT("The vault is empty. Its salvage is already out."));
        return true;
    }
    if (!IsVaultAccessible())
    {
        Expedition->PostStatus(FString::Printf(TEXT("Vault hatch is under %.1f m of water. Cut a canal to the old culvert to drain it."), GetVaultDepth()));
        PlayFeedback(false, GetVaultLocation());
        return true;
    }
    // Commit first: a failed save leaves the vault sealed and spawns nothing.
    if (!Expedition->CommitVaultOpened()) return true;
    const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld());
    int32 Remaining = FMath::Max(1, Tuning->VaultReward);
    int32 Index = 0;
    while (Remaining > 0)
    {
        const int32 Amount = FMath::Min(4, Remaining);
        const float Angle = Index * 2.39996f;
        const FVector Offset(FMath::Cos(Angle) * 120.0f, FMath::Sin(Angle) * 120.0f, 60.0f);
        Expedition->SpawnSalvage(GetVaultLocation() + Offset, Amount, false);
        Remaining -= Amount;
        ++Index;
    }
    if (Tuning->ExtractionSound) UGameplayStatics::PlaySoundAtLocation(GetWorld(), Tuning->ExtractionSound, GetVaultLocation());
    Expedition->PostStatus(FString::Printf(TEXT("Vault cracked open: %d salvage spilled out. Carry it home."), FMath::Max(1, Tuning->VaultReward)));
    return true;
}

TArray<FIntPoint> UDownstreamWaterSubsystem::PreviewCanal(FIntPoint From, FIntPoint To) const
{
    TArray<FIntPoint> Result;
    if (!IsCellValid(From) || !IsCellValid(To)) return Result;
    for (const auto& Cell : downstream::RasterizeCanal({From.X, From.Y}, {To.X, To.Y})) Result.Add(FIntPoint(Cell.X, Cell.Y));
    return Result;
}

bool UDownstreamWaterSubsystem::IsCanalValid(FIntPoint From, FIntPoint To) const
{
    const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld());
    const int32 Length = PreviewCanal(From, To).Num();
    return Length > 0 && Length <= 48 && static_cast<int32>(Jobs.size()) < FMath::Clamp(Tuning->MaxQueuedCanals, 1, 3);
}

bool UDownstreamWaterSubsystem::DesignateCanal(FIntPoint From, FIntPoint To, AActor* Player)
{
    auto* Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this);
    if (!Grid || !Expedition) return false;
    const FVector Feedback = IsValid(Player) ? Player->GetActorLocation() : Origin;
    if (!Expedition->IsDiggerBuilt())
    {
        Expedition->PostStatus(TEXT("Assemble a digger at the home workshop first (B, then 5)."));
        PlayFeedback(false, Feedback);
        return false;
    }
    const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld());
    if (static_cast<int32>(Jobs.size()) >= FMath::Clamp(Tuning->MaxQueuedCanals, 1, 3))
    {
        Expedition->PostStatus(TEXT("Dig queue is full. Let the digger finish, or press C to clear it."));
        PlayFeedback(false, Feedback);
        return false;
    }
    const TArray<FIntPoint> Cells = PreviewCanal(From, To);
    if (Cells.Num() < 2 || Cells.Num() > 48)
    {
        Expedition->PostStatus(TEXT("Drag a canal of 2 to 48 cells inside the flooded site."));
        PlayFeedback(false, Feedback);
        return false;
    }
    std::vector<downstream::WaterCell> Path;
    for (const FIntPoint& Cell : Cells) Path.push_back({Cell.X, Cell.Y});
    try
    {
        downstream::DigRules Rules;
        Rules.StepDepth = FMath::Max(0.01f, Tuning->DigStepDepth);
        downstream::DigJob Job(*Grid, std::move(Path), Rules);
        if (Job.IsComplete(*Grid))
        {
            Expedition->PostStatus(TEXT("Nothing to dig: that line is already at grade. Start where the water is, end where it should go."));
            PlayFeedback(false, Feedback);
            return false;
        }
        Jobs.push_back(std::move(Job));
    }
    catch (const std::exception& Error)
    {
        UE_LOG(LogDownstreamWater, Warning, TEXT("Rejected canal: %hs"), Error.what());
        return false;
    }
    PlayFeedback(true, Feedback);
    Expedition->PostStatus(FString::Printf(TEXT("Canal queued: %d cells, graded from start to end. The digger is on its way."), Cells.Num()));
    return true;
}

void UDownstreamWaterSubsystem::ClearDigQueue()
{
    if (Jobs.empty()) return;
    Jobs.clear();
    if (auto* Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this))
        Expedition->PostStatus(TEXT("Dig queue cleared. Dug ground stays as it is."));
}

TArray<FIntPoint> UDownstreamWaterSubsystem::GetQueuedCanalCells() const
{
    TArray<FIntPoint> Result;
    if (!Grid) return Result;
    for (const auto& Job : Jobs)
        for (std::size_t Index = 0; Index < Job.Cells().size(); ++Index)
        {
            const auto& Cell = Job.Cells()[Index];
            // Only cells still above their target bed need a marker.
            if (Grid->Ground(Cell.X, Cell.Y) > Job.TargetBed(Index) + 1e-3f) Result.AddUnique(FIntPoint(Cell.X, Cell.Y));
        }
    return Result;
}

float UDownstreamWaterSubsystem::GetActiveCanalProgress() const
{
    return Grid && !Jobs.empty() ? Jobs.front().Progress(*Grid) : 0.0f;
}

bool UDownstreamWaterSubsystem::GetNextDigCell(FIntPoint& Cell) const
{
    if (!Grid || Jobs.empty()) return false;
    const int32 Index = Jobs.front().NextCell(*Grid);
    if (Index < 0) return false;
    const auto& Next = Jobs.front().Cells()[static_cast<std::size_t>(Index)];
    Cell = FIntPoint(Next.X, Next.Y);
    return true;
}

bool UDownstreamWaterSubsystem::GetDigStandCell(FIntPoint& Cell) const
{
    if (!Grid || Jobs.empty()) return false;
    const int32 Index = Jobs.front().NextCell(*Grid);
    if (Index < 0) return false;
    const auto& Stand = Jobs.front().Cells()[static_cast<std::size_t>(FMath::Max(0, Index - 1))];
    Cell = FIntPoint(Stand.X, Stand.Y);
    return true;
}

bool UDownstreamWaterSubsystem::PerformDigAction(AActor* Digger)
{
    if (!Grid || !IsValid(Digger) || !GetWorld()) return false;
    // Drop finished or no-longer-needed jobs (e.g. an earlier canal already cut this one).
    while (!Jobs.empty() && Jobs.front().IsComplete(*Grid)) { Jobs.pop_front(); ++CompletedCanals; bTerrainDirty = true; NextTerrainSave = 0.0; }
    FIntPoint Cell;
    if (!GetNextDigCell(Cell)) return false;
    const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld());
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now < NextDigAction) return false;
    if (FVector::DistSquared2D(Digger->GetActorLocation(), CellToWorld(Cell)) > FMath::Square(FMath::Max(1.0f, Tuning->DigReach))) return false;
    NextDigAction = Now + FMath::Max(0.05f, Tuning->DigActionSeconds);
    if (!Jobs.front().WorkNext(*Grid)) return false;
    bTerrainDirty = true;
    if (Jobs.front().IsComplete(*Grid))
    {
        Jobs.pop_front();
        ++CompletedCanals;
        NextTerrainSave = 0.0;
        if (auto* Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this))
            Expedition->PostStatus(Jobs.empty() ? TEXT("Canal finished. Watch where the water goes.") : TEXT("Canal finished. Starting the next one."));
        PlayFeedback(true, Digger->GetActorLocation());
    }
    return true;
}
