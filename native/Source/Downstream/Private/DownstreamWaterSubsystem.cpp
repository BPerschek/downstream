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
constexpr double ClassicWarmupSeconds = 90.0;
constexpr double WorldWarmupSeconds = 120.0;
// Engineers sandbag from the bank beside a cut, which can be a cell and a half away.
constexpr float DamReach = 330.0f;

int32 StructureReward(downstream::OutpostStructureType Type)
{
    switch (Type)
    {
    case downstream::OutpostStructureType::Stash: return 12;
    case downstream::OutpostStructureType::Generator: return 6;
    default: return 3;
    }
}

const TCHAR* StructureName(downstream::OutpostStructureType Type)
{
    switch (Type)
    {
    case downstream::OutpostStructureType::Stash: return TEXT("stash");
    case downstream::OutpostStructureType::Generator: return TEXT("generator");
    default: return TEXT("hut");
    }
}
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
    if (Grid && bTerrainDirty && !bPreview) SaveTerrainNow();
    Grid.reset();
    Jobs.clear();
    Outposts.clear();
    Super::Deinitialize();
}

bool UDownstreamWaterSubsystem::FinishActivation(std::unique_ptr<downstream::WaterGrid> NewGrid, double WarmupSeconds)
{
    if (!bPreview)
        if (const auto* Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this))
            Expedition->GetEarthworks().ApplyTerrain(*NewGrid);
    // Settle saved canals, rivers and springs before the first rendered frame.
    const int32 Ticks = FMath::RoundToInt(WarmupSeconds / NewGrid->Config().TickSeconds);
    for (int32 Index = 0; Index < Ticks; ++Index) NewGrid->Tick();
    Grid = std::move(NewGrid);
    SpawnVaults();
    return true;
}

bool UDownstreamWaterSubsystem::ActivateSite(FVector SiteOrigin)
{
    if (Grid) return true;
    if (SiteOrigin.ContainsNaN()) return false;
    try
    {
        Layout = downstream::MakeDrownedVaultLayout();
        auto NewGrid = std::make_unique<downstream::WaterGrid>(downstream::DrownedVaultConfig());
        downstream::BuildDrownedVaultSite(*NewGrid, true);
        Origin = SiteOrigin;
        bWorldSite = false;
        FinishActivation(std::move(NewGrid), ClassicWarmupSeconds);
    }
    catch (const std::exception& Error)
    {
        UE_LOG(LogDownstreamWater, Error, TEXT("Water site failed to initialize: %hs"), Error.what());
        return false;
    }
    UE_LOG(LogDownstreamWater, Log, TEXT("Drowned Vault active at %s: %.1f m3 water, vault depth %.2f m"),
        *Origin.ToCompactString(), Grid->TotalWater(), GetVaultDepth());
    return true;
}

bool UDownstreamWaterSubsystem::EnsureWorldActive()
{
    if (Grid) return true;
    if (bActivating) return false;
    const auto* Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this);
    if (!Expedition || !Expedition->IsWorldMode()) return false;
    return ActivateWorld(0, false);
}

bool UDownstreamWaterSubsystem::ActivateWorld(int64 Seed, bool bPreviewWorld)
{
    if (Grid) return true;
    TGuardValue<bool> Guard(bActivating, true);
    auto* Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this);
    uint32 UseSeed = static_cast<uint32>(FMath::Clamp<int64>(Seed, 0, MAX_uint32));
    if (UseSeed == 0 && Expedition) UseSeed = static_cast<uint32>(Expedition->GetWorldSeed());
    if (UseSeed == 0) UseSeed = 1;
    const double Start = FPlatformTime::Seconds();
    try
    {
        Layout = downstream::GenerateWorld(UseSeed);
        auto NewGrid = std::make_unique<downstream::WaterGrid>(downstream::WorldWaterConfig(Layout));
        downstream::ApplyWorld(*NewGrid, Layout, true);
        bWorldSite = true;
        bPreview = bPreviewWorld;
        // The world is centred on the map origin.
        Origin = FVector(-Layout.Width * 0.5f * MetresToCm, -Layout.Height * 0.5f * MetresToCm, 0.0f);
        Outposts.clear();
        if (!bPreview)
            for (const auto& Plan : Layout.Outposts)
            {
                downstream::OutpostState State(Plan);
                if (Expedition) State.RestoreDestroyedMask(Expedition->GetEarthworks().GetOutpostMask(Plan.Id));
                Outposts.push_back(State);
            }
        FinishActivation(std::move(NewGrid), WorldWarmupSeconds);
    }
    catch (const std::exception& Error)
    {
        UE_LOG(LogDownstreamWater, Error, TEXT("World %u failed to generate: %hs"), UseSeed, Error.what());
        return false;
    }
    if (!bPreview && Expedition)
    {
        const FVector Home = CellToWorld(FIntPoint(Layout.Home.X, Layout.Home.Y)) + FVector(0, 0, 100.0f);
        Expedition->SetLayoutOverride(Home, UDownstreamExpeditionSubsystem::GetTuning(GetWorld())->HomeRadius, Home - FVector(0, 0, 90.0f));
    }
    UE_LOG(LogDownstreamWater, Log, TEXT("World %u generated in %.2f s: %dx%d, %d rivers, %d outposts, %d vaults, %d caches"),
        UseSeed, FPlatformTime::Seconds() - Start, Layout.Width, Layout.Height, static_cast<int32>(Layout.Rivers.size()),
        static_cast<int32>(Layout.Outposts.size()), static_cast<int32>(Layout.Vaults.size()), static_cast<int32>(Layout.Caches.size()));
    return true;
}

void UDownstreamWaterSubsystem::SpawnVaults()
{
    if (!GetWorld() || bPreview) return;
    for (int32 Index = 0; Index < GetVaultCount(); ++Index)
    {
        FActorSpawnParameters Params;
        Params.ObjectFlags |= RF_Transient;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Params.bDeferConstruction = true;
        auto* Vault = GetWorld()->SpawnActor<ADownstreamFloodedVault>(ADownstreamFloodedVault::StaticClass(), GetVaultLocationAt(Index), FRotator(0, 20, 0), Params);
        if (!Vault) continue;
        Vault->VaultIndex = Index;
        Vault->FinishSpawning(FTransform(FRotator(0, 20, 0), GetVaultLocationAt(Index)));
        Vaults.Add(Vault);
    }
}

void UDownstreamWaterSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!Grid || !GetWorld() || !FMath::IsFinite(DeltaTime) || DeltaTime < 0.0f) return;
    Grid->Advance(DeltaTime);
    OutpostAccumulator += DeltaTime;
    if (OutpostAccumulator >= 1.0f)
    {
        TickOutposts(FMath::Min(OutpostAccumulator, 2.0f));
        OutpostAccumulator = 0.0f;
    }
    const double Now = GetWorld()->GetTimeSeconds();
    if (bTerrainDirty && !bPreview && Now >= NextTerrainSave)
    {
        NextTerrainSave = Now + TerrainSaveInterval;
        SaveTerrainNow();
    }
}

void UDownstreamWaterSubsystem::TickOutposts(float Seconds)
{
    for (int32 Index = 0; Index < GetOutpostCount(); ++Index)
    {
        auto& Outpost = Outposts[static_cast<std::size_t>(Index)];
        Outpost.Pump(*Grid, Seconds);
        for (const int Destroyed : Outpost.ApplyFlood(*Grid, Seconds)) HandleStructureDestroyed(Index, Destroyed);
    }
}

void UDownstreamWaterSubsystem::HandleStructureDestroyed(int32 OutpostIndex, int32 Structure)
{
    auto& Outpost = Outposts[static_cast<std::size_t>(OutpostIndex)];
    const auto& Part = Outpost.Structures()[static_cast<std::size_t>(Structure)];
    const FVector Location = GetOutpostStructureLocation(OutpostIndex, Structure);
    auto* Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this);
    if (Expedition)
    {
        // The salvage spills out whether or not the save succeeds; the mask retries on the next change.
        Expedition->CommitOutpostMask(Outpost.Layout().Id, Outpost.DestroyedMask());
        const int32 Reward = StructureReward(Part.Type);
        for (int32 Drop = 0; Drop < (Reward + 3) / 4; ++Drop)
        {
            const float Angle = Drop * 2.1f + Structure;
            Expedition->SpawnSalvage(Location + FVector(FMath::Cos(Angle) * 110.0f, FMath::Sin(Angle) * 110.0f, 70.0f), FMath::Min(4, Reward - Drop * 4), false);
        }
        if (Part.Type == downstream::OutpostStructureType::Generator)
            Expedition->PostStatus(TEXT("Outpost generator destroyed: its pumps are dead and the Dam Cult stops reinforcing it."));
        else
            Expedition->PostStatus(FString::Printf(TEXT("Dam Cult %s collapsed. Salvage spilled out."), StructureName(Part.Type)));
    }
    if (const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld()); Tuning->ExtractionSound)
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), Tuning->ExtractionSound, Location, 0.8f, 0.6f);
    OnOutpostStructureDestroyed.Broadcast(OutpostIndex, Structure, Location);
}

void UDownstreamWaterSubsystem::SimulateSeconds(float Seconds)
{
    if (!Grid || !FMath::IsFinite(Seconds) || Seconds <= 0.0f) return;
    const int32 Ticks = FMath::Min(36000, FMath::RoundToInt(Seconds / Grid->Config().TickSeconds));
    for (int32 Index = 0; Index < Ticks; ++Index)
    {
        Grid->Tick();
        if (Index % 10 == 9) TickOutposts(1.0f);
    }
}

bool UDownstreamWaterSubsystem::SaveTerrainNow()
{
    if (!Grid || bPreview) return false;
    auto* Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this);
    if (!Expedition || !Expedition->CommitEarthworksTerrain(*Grid, Layout.Ground)) return false;
    bTerrainDirty = false;
    return true;
}

float UDownstreamWaterSubsystem::GetCellSizeCm() const
{
    return (Grid ? Grid->Config().CellSize : 1.0f) * MetresToCm;
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

FVector UDownstreamWaterSubsystem::CellCentre(int32 X, int32 Y) const
{
    return CellToWorld(FIntPoint(X, Y));
}

FVector UDownstreamWaterSubsystem::CellToWorld(FIntPoint Cell) const
{
    const float Size = GetCellSizeCm();
    const float Ground = IsCellValid(Cell) ? Grid->Ground(Cell.X, Cell.Y) : 0.0f;
    return Origin + FVector((Cell.X + 0.5f) * Size, (Cell.Y + 0.5f) * Size, Ground * MetresToCm);
}

float UDownstreamWaterSubsystem::GetCellGround(FIntPoint Cell) const { return IsCellValid(Cell) ? Grid->Ground(Cell.X, Cell.Y) : 0.0f; }
float UDownstreamWaterSubsystem::GetCellWater(FIntPoint Cell) const { return IsCellValid(Cell) ? Grid->Water(Cell.X, Cell.Y) : 0.0f; }
float UDownstreamWaterSubsystem::GetCellBaseGround(FIntPoint Cell) const
{
    return IsCellValid(Cell) && !Layout.Ground.empty() ? Layout.GroundAt(Cell.X, Cell.Y) : 0.0f;
}

float UDownstreamWaterSubsystem::GetWaterDepthAt(FVector World) const
{
    FIntPoint Cell;
    return WorldToCell(World, Cell) ? Grid->Water(Cell.X, Cell.Y) : 0.0f;
}

bool UDownstreamWaterSubsystem::GetGroundZAt(FVector World, float& Z) const
{
    FIntPoint Cell;
    if (!WorldToCell(World, Cell)) return false;
    Z = Origin.Z + Grid->Ground(Cell.X, Cell.Y) * MetresToCm;
    return true;
}

float UDownstreamWaterSubsystem::GetTotalWater() const { return Grid ? static_cast<float>(Grid->TotalWater()) : 0.0f; }
float UDownstreamWaterSubsystem::GetDrainedVolume() const { return Grid ? static_cast<float>(Grid->SunkVolume()) : 0.0f; }
int64 UDownstreamWaterSubsystem::GetWaterTick() const { return Grid ? static_cast<int64>(Grid->TickCount()) : 0; }

// ---- Vaults -----------------------------------------------------------------

FVector UDownstreamWaterSubsystem::GetVaultLocationAt(int32 Index) const
{
    if (Index < 0 || Index >= GetVaultCount()) return Origin;
    const auto& V = Layout.Vaults[static_cast<std::size_t>(Index)];
    return CellCentre(V.Cell.X, V.Cell.Y);
}

float UDownstreamWaterSubsystem::GetVaultDepthAt(int32 Index) const
{
    if (Index < 0 || Index >= GetVaultCount()) return 0.0f;
    const auto& V = Layout.Vaults[static_cast<std::size_t>(Index)];
    return GetCellWater(FIntPoint(V.Cell.X, V.Cell.Y));
}

bool UDownstreamWaterSubsystem::IsVaultAccessibleAt(int32 Index) const
{
    return Grid && Index >= 0 && Index < GetVaultCount() && GetVaultDepthAt(Index) <= downstream::DrownedVaultSite::VaultAccessDepth;
}

int32 UDownstreamWaterSubsystem::GetVaultId(int32 Index) const
{
    return Index >= 0 && Index < GetVaultCount() ? Layout.Vaults[static_cast<std::size_t>(Index)].Id : -1;
}

bool UDownstreamWaterSubsystem::IsVaultOpenedAt(int32 Index) const
{
    const auto* Expedition = UDownstreamExpeditionSubsystem::GetForWorld(const_cast<UDownstreamWaterSubsystem*>(this));
    return Expedition && Expedition->IsVaultIdOpened(GetVaultId(Index));
}

FIntPoint UDownstreamWaterSubsystem::GetVaultRiverCell(int32 Index) const
{
    if (Index < 0 || Index >= GetVaultCount()) return FIntPoint::ZeroValue;
    const auto& V = Layout.Vaults[static_cast<std::size_t>(Index)];
    return FIntPoint(V.RiverCell.X, V.RiverCell.Y);
}

int32 UDownstreamWaterSubsystem::GetVaultNear(AActor* Player) const
{
    if (!Grid || !IsValid(Player) || bPreview) return -1;
    int32 Best = -1;
    float BestDistance = FMath::Square(VaultInteractRadius);
    for (int32 Index = 0; Index < GetVaultCount(); ++Index)
    {
        const float Distance = FVector::DistSquared2D(Player->GetActorLocation(), GetVaultLocationAt(Index));
        if (Distance <= BestDistance) { BestDistance = Distance; Best = Index; }
    }
    return Best;
}

void UDownstreamWaterSubsystem::PlayFeedback(bool bSuccess, const FVector& Location) const
{
    const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld());
    if (USoundBase* Sound = bSuccess ? Tuning->BuildSound.Get() : Tuning->ErrorSound.Get())
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), Sound, Location, 0.6f);
}

bool UDownstreamWaterSubsystem::TryInteractVault(AActor* Player)
{
    const int32 Index = GetVaultNear(Player);
    if (Index < 0) return false;
    auto* Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this);
    if (!Expedition) return false;
    const FVector Location = GetVaultLocationAt(Index);
    if (IsVaultOpenedAt(Index))
    {
        Expedition->PostStatus(TEXT("The vault is empty. Its salvage is already out."));
        return true;
    }
    if (!IsVaultAccessibleAt(Index))
    {
        Expedition->PostStatus(FString::Printf(TEXT("Vault hatch is under %.1f m of water. Cut a canal from the vault to lower ground to drain it."), GetVaultDepthAt(Index)));
        PlayFeedback(false, Location);
        return true;
    }
    // Commit first: a failed save leaves the vault sealed and spawns nothing.
    if (!Expedition->CommitVaultOpened(GetVaultId(Index))) return true;
    const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld());
    int32 Remaining = FMath::Max(1, Tuning->VaultReward);
    int32 Drop = 0;
    while (Remaining > 0)
    {
        const int32 Amount = FMath::Min(4, Remaining);
        const float Angle = Drop * 2.39996f;
        Expedition->SpawnSalvage(Location + FVector(FMath::Cos(Angle) * 120.0f, FMath::Sin(Angle) * 120.0f, 60.0f), Amount, false);
        Remaining -= Amount;
        ++Drop;
    }
    if (Tuning->ExtractionSound) UGameplayStatics::PlaySoundAtLocation(GetWorld(), Tuning->ExtractionSound, Location);
    Expedition->PostStatus(FString::Printf(TEXT("Vault cracked open: %d salvage spilled out. Carry it home."), FMath::Max(1, Tuning->VaultReward)));
    return true;
}

// ---- Outposts ---------------------------------------------------------------

FIntPoint UDownstreamWaterSubsystem::GetOutpostCenterCell(int32 Outpost) const
{
    if (Outpost < 0 || Outpost >= GetOutpostCount()) return FIntPoint::ZeroValue;
    const auto& C = Outposts[static_cast<std::size_t>(Outpost)].Layout().Center;
    return FIntPoint(C.X, C.Y);
}

FVector UDownstreamWaterSubsystem::GetOutpostCenter(int32 Outpost) const
{
    return CellToWorld(GetOutpostCenterCell(Outpost));
}

FIntPoint UDownstreamWaterSubsystem::GetOutpostRiverCell(int32 Outpost) const
{
    if (Outpost < 0 || Outpost >= GetOutpostCount()) return FIntPoint::ZeroValue;
    const auto& C = Outposts[static_cast<std::size_t>(Outpost)].Layout().RiverCell;
    return FIntPoint(C.X, C.Y);
}

int32 UDownstreamWaterSubsystem::GetOutpostId(int32 Outpost) const
{
    return Outpost >= 0 && Outpost < GetOutpostCount() ? Outposts[static_cast<std::size_t>(Outpost)].Layout().Id : -1;
}

float UDownstreamWaterSubsystem::GetOutpostRadiusCm(int32 Outpost) const
{
    return Outpost >= 0 && Outpost < GetOutpostCount() ? (Outposts[static_cast<std::size_t>(Outpost)].Layout().RimRadius + 1.5f) * GetCellSizeCm() : 0.0f;
}

int32 UDownstreamWaterSubsystem::GetOutpostStructureCount(int32 Outpost) const
{
    return Outpost >= 0 && Outpost < GetOutpostCount() ? static_cast<int32>(Outposts[static_cast<std::size_t>(Outpost)].Structures().size()) : 0;
}

FVector UDownstreamWaterSubsystem::GetOutpostStructureLocation(int32 Outpost, int32 Structure) const
{
    if (Structure < 0 || Structure >= GetOutpostStructureCount(Outpost)) return Origin;
    const auto& C = Outposts[static_cast<std::size_t>(Outpost)].Structures()[static_cast<std::size_t>(Structure)].Cell;
    // Structures stand on the designed pad, not on any trench dug through it.
    FVector Location = CellCentre(C.X, C.Y);
    Location.Z = Origin.Z + Outposts[static_cast<std::size_t>(Outpost)].Layout().PadHeight * MetresToCm;
    return Location;
}

int32 UDownstreamWaterSubsystem::GetOutpostStructureType(int32 Outpost, int32 Structure) const
{
    if (Structure < 0 || Structure >= GetOutpostStructureCount(Outpost)) return 0;
    switch (Outposts[static_cast<std::size_t>(Outpost)].Structures()[static_cast<std::size_t>(Structure)].Type)
    {
    case downstream::OutpostStructureType::Generator: return 1;
    case downstream::OutpostStructureType::Stash: return 2;
    default: return 0;
    }
}

float UDownstreamWaterSubsystem::GetOutpostStructureIntegrity(int32 Outpost, int32 Structure) const
{
    return Structure >= 0 && Structure < GetOutpostStructureCount(Outpost) ? Outposts[static_cast<std::size_t>(Outpost)].Structures()[static_cast<std::size_t>(Structure)].Integrity : 0.0f;
}

float UDownstreamWaterSubsystem::GetOutpostStructureMaxIntegrity(int32 Outpost, int32 Structure) const
{
    return Structure >= 0 && Structure < GetOutpostStructureCount(Outpost) ? Outposts[static_cast<std::size_t>(Outpost)].Structures()[static_cast<std::size_t>(Structure)].MaxIntegrity : 1.0f;
}

bool UDownstreamWaterSubsystem::IsOutpostStructureDestroyed(int32 Outpost, int32 Structure) const
{
    return Structure < 0 || Structure >= GetOutpostStructureCount(Outpost) || Outposts[static_cast<std::size_t>(Outpost)].Structures()[static_cast<std::size_t>(Structure)].Destroyed;
}

float UDownstreamWaterSubsystem::GetOutpostStructureWater(int32 Outpost, int32 Structure) const
{
    if (Structure < 0 || Structure >= GetOutpostStructureCount(Outpost)) return 0.0f;
    const auto& C = Outposts[static_cast<std::size_t>(Outpost)].Structures()[static_cast<std::size_t>(Structure)].Cell;
    return GetCellWater(FIntPoint(C.X, C.Y));
}

bool UDownstreamWaterSubsystem::IsOutpostRazed(int32 Outpost) const
{
    return Outpost >= 0 && Outpost < GetOutpostCount() && Outposts[static_cast<std::size_t>(Outpost)].IsRazed();
}

int32 UDownstreamWaterSubsystem::GetOutpostStandingCount(int32 Outpost) const
{
    int32 Count = 0;
    for (int32 Index = 0; Index < GetOutpostStructureCount(Outpost); ++Index)
        if (!IsOutpostStructureDestroyed(Outpost, Index)) ++Count;
    return Count;
}

float UDownstreamWaterSubsystem::GetOutpostFloodDepth(int32 Outpost) const
{
    return Grid && Outpost >= 0 && Outpost < GetOutpostCount() ? Outposts[static_cast<std::size_t>(Outpost)].FloodDepth(*Grid) : 0.0f;
}

bool UDownstreamWaterSubsystem::GetOutpostBreach(int32 Outpost, FIntPoint& Cell) const
{
    if (!Grid || Outpost < 0 || Outpost >= GetOutpostCount()) return false;
    downstream::WaterCell Breach;
    if (!Outposts[static_cast<std::size_t>(Outpost)].FindBreach(*Grid, Breach)) return false;
    Cell = FIntPoint(Breach.X, Breach.Y);
    return true;
}

int32 UDownstreamWaterSubsystem::GetOutpostAt(FVector World, float ExtraRadiusCm) const
{
    for (int32 Index = 0; Index < GetOutpostCount(); ++Index)
        if (FVector::DistSquared2D(World, GetOutpostCenter(Index)) <= FMath::Square(GetOutpostRadiusCm(Index) + ExtraRadiusCm)) return Index;
    return -1;
}

bool UDownstreamWaterSubsystem::PerformDamAction(AActor* Engineer, int32 Outpost)
{
    FIntPoint Cell;
    if (!IsValid(Engineer) || !GetOutpostBreach(Outpost, Cell)) return false;
    if (FVector::DistSquared2D(Engineer->GetActorLocation(), CellToWorld(Cell)) > FMath::Square(DamReach)) return false;
    const double Placed = Outposts[static_cast<std::size_t>(Outpost)].DamBreach(*Grid, {Cell.X, Cell.Y});
    if (Placed <= 0.0) return false;
    bTerrainDirty = true;
    return true;
}

bool UDownstreamWaterSubsystem::DamageOutpostStructure(int32 Outpost, int32 Structure, float Amount)
{
    if (Outpost < 0 || Outpost >= GetOutpostCount()) return false;
    if (!Outposts[static_cast<std::size_t>(Outpost)].Damage(Structure, Amount)) return false;
    HandleStructureDestroyed(Outpost, Structure);
    return true;
}

// ---- Canals -----------------------------------------------------------------

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
    return Length > 0 && Length <= 64 && static_cast<int32>(Jobs.size()) < FMath::Clamp(Tuning->MaxQueuedCanals, 1, 3);
}

bool UDownstreamWaterSubsystem::DesignateCanal(FIntPoint From, FIntPoint To, AActor* Player)
{
    auto* Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this);
    if (!Grid || !Expedition || bPreview) return false;
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
    if (Cells.Num() < 2 || Cells.Num() > 64)
    {
        Expedition->PostStatus(TEXT("Drag a canal of 2 to 64 cells."));
        PlayFeedback(false, Feedback);
        return false;
    }
    std::vector<downstream::WaterCell> Path;
    for (const FIntPoint& Cell : Cells) Path.push_back({Cell.X, Cell.Y});
    try
    {
        downstream::DigRules Rules;
        Rules.StepDepth = FMath::Max(0.01f, Tuning->DigStepDepth);
        Rules.MinimumBedDrop = GetCanalDepth();
        Rules.MaxCells = 64;
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
    Expedition->PostStatus(FString::Printf(TEXT("Canal queued: %d cells, %.2f m below grade. The digger is on its way."), Cells.Num(), GetCanalDepth()));
    return true;
}

float UDownstreamWaterSubsystem::GetCanalDepth() const
{
    const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld());
    const float Max = FMath::Max(0.5f, Tuning->MaxCanalDepth);
    return FMath::Clamp(CanalDepth < 0.0f ? Tuning->DefaultCanalDepth : CanalDepth, 0.0f, Max);
}

void UDownstreamWaterSubsystem::SetCanalDepth(float Metres)
{
    if (!FMath::IsFinite(Metres)) return;
    const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld());
    CanalDepth = FMath::Clamp(Metres, 0.0f, FMath::Max(0.5f, Tuning->MaxCanalDepth));
}

void UDownstreamWaterSubsystem::AdjustCanalDepth(int32 Steps)
{
    const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld());
    SetCanalDepth(GetCanalDepth() + Steps * FMath::Clamp(Tuning->CanalDepthStep, 0.05f, 1.0f));
}

bool UDownstreamWaterSubsystem::GetCanalBedRange(FIntPoint From, FIntPoint To, float& StartBed, float& EndBed) const
{
    if (!IsCellValid(From) || !IsCellValid(To)) return false;
    const float Floor = Grid->Config().MinimumGround;
    StartBed = FMath::Max(Floor, Grid->Ground(From.X, From.Y) - GetCanalDepth());
    EndBed = FMath::Max(Floor, Grid->Ground(To.X, To.Y) - GetCanalDepth());
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
