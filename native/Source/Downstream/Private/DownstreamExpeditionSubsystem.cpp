#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamPrototypeTuning.h"
#include "DownstreamSalvagePickup.h"
#include "DownstreamSessionSubsystem.h"
#include "DownstreamWaterSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundBase.h"

#include <algorithm>

DEFINE_LOG_CATEGORY_STATIC(LogDownstreamExpedition, Log, All);

namespace
{
bool GWorldSavePath = false;
FString SavePath()
{
    return GWorldSavePath ? UDownstreamSessionSubsystem::WorldSavePath() : UDownstreamSessionSubsystem::ClassicSavePath();
}

bool IsWorldMap(const UWorld* World)
{
    return World && World->GetMapName().Contains(TEXT("M2_World"));
}

downstream::SettlementRules SettlementRules(const UDownstreamPrototypeTuning* Tuning)
{
    downstream::SettlementRules Rules;
    Rules.StorageCost = Tuning->StorageCost;
    Rules.WorkshopCost = Tuning->WorkshopCost;
    Rules.ChargerCost = Tuning->ChargerCost;
    Rules.HaulerCost = Tuning->HaulerCost;
    Rules.StorageCapacityBonus = Tuning->StorageCapacityBonus;
    Rules.HaulerCargoCapacity = Tuning->HaulerCargoCapacity;
    Rules.MinX = Tuning->BuildGridMinX;
    Rules.MaxX = Tuning->BuildGridMaxX;
    Rules.MinY = Tuning->BuildGridMinY;
    Rules.MaxY = Tuning->BuildGridMaxY;
    return Rules;
}

downstream::StructureType PureType(EDownstreamStructureType Type)
{
    switch (Type)
    {
    case EDownstreamStructureType::Storage: return downstream::StructureType::Storage;
    case EDownstreamStructureType::Workshop: return downstream::StructureType::Workshop;
    case EDownstreamStructureType::Charger: return downstream::StructureType::Charger;
    default: return static_cast<downstream::StructureType>(-1);
    }
}

const TCHAR* StructureName(downstream::StructureType Type)
{
    switch (Type)
    {
    case downstream::StructureType::Storage: return TEXT("storage");
    case downstream::StructureType::Workshop: return TEXT("workshop");
    case downstream::StructureType::Charger: return TEXT("charger");
    default: return TEXT("");
    }
}

bool TryInteger(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int32& Result)
{
    double Number = 0.0;
    if (!Object->HasTypedField<EJson::Number>(Field) || !Object->TryGetNumberField(Field, Number) || !FMath::IsFinite(Number)
        || Number < static_cast<double>(MIN_int32) || Number > static_cast<double>(MAX_int32)
        || FMath::FloorToDouble(Number) != Number) return false;
    Result = static_cast<int32>(Number);
    return true;
}

bool ParseEarthworks(const TSharedPtr<FJsonObject>& Object, downstream::EarthworksState& Out)
{
    const TSharedPtr<FJsonObject>* Earth = nullptr;
    if (!Object->TryGetObjectField(TEXT("earthworks"), Earth) || !Earth || !Earth->IsValid()) return false;
    bool bDigger = false, bVault = false;
    const TArray<TSharedPtr<FJsonValue>>* Cells = nullptr;
    if (!(*Earth)->HasTypedField<EJson::Boolean>(TEXT("digger_built")) || !(*Earth)->TryGetBoolField(TEXT("digger_built"), bDigger)
        || !(*Earth)->HasTypedField<EJson::Boolean>(TEXT("vault_opened")) || !(*Earth)->TryGetBoolField(TEXT("vault_opened"), bVault)
        || !(*Earth)->TryGetArrayField(TEXT("terrain_mm"), Cells)) return false;
    std::vector<downstream::TerrainDelta> Deltas;
    for (const auto& Value : *Cells)
    {
        const TArray<TSharedPtr<FJsonValue>>* Triple = nullptr;
        if (!Value.IsValid() || !Value->TryGetArray(Triple) || !Triple || Triple->Num() != 3) return false;
        int32 Parts[3] = {0, 0, 0};
        for (int32 Index = 0; Index < 3; ++Index)
        {
            double Number = 0.0;
            const auto& Part = (*Triple)[Index];
            if (!Part.IsValid() || Part->Type != EJson::Number || !Part->TryGetNumber(Number) || !FMath::IsFinite(Number)
                || FMath::FloorToDouble(Number) != Number || FMath::Abs(Number) > 1.0e6) return false;
            Parts[Index] = static_cast<int32>(Number);
        }
        Deltas.push_back({Parts[0], Parts[1], Parts[2]});
    }
    if (!Out.LoadPersisted(bDigger, bVault, Deltas)) return false;
    // Schema 4 world fields (optional): seed, every opened vault and outpost destruction masks.
    double SeedNumber = 0.0;
    const bool bHasSeed = (*Earth)->HasTypedField<EJson::Number>(TEXT("seed")) && (*Earth)->TryGetNumberField(TEXT("seed"), SeedNumber);
    const TArray<TSharedPtr<FJsonValue>>* VaultIds = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Masks = nullptr;
    (*Earth)->TryGetArrayField(TEXT("vaults_opened"), VaultIds);
    (*Earth)->TryGetArrayField(TEXT("outposts"), Masks);
    if (!bHasSeed && !VaultIds && !Masks) return true;
    if (bHasSeed && (!FMath::IsFinite(SeedNumber) || SeedNumber < 0.0 || SeedNumber > 4294967295.0 || FMath::FloorToDouble(SeedNumber) != SeedNumber)) return false;
    std::vector<int> Opened;
    if (VaultIds)
        for (const auto& Value : *VaultIds)
        {
            double Number = 0.0;
            if (!Value.IsValid() || Value->Type != EJson::Number || !Value->TryGetNumber(Number) || FMath::FloorToDouble(Number) != Number) return false;
            Opened.push_back(static_cast<int>(Number));
        }
    if (bVault && std::find(Opened.begin(), Opened.end(), 0) == Opened.end()) Opened.push_back(0);
    std::vector<std::pair<int, int>> MaskPairs;
    if (Masks)
        for (const auto& Value : *Masks)
        {
            const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
            double A = 0.0, B = 0.0;
            if (!Value.IsValid() || !Value->TryGetArray(Pair) || !Pair || Pair->Num() != 2
                || !(*Pair)[0]->TryGetNumber(A) || !(*Pair)[1]->TryGetNumber(B)
                || FMath::FloorToDouble(A) != A || FMath::FloorToDouble(B) != B) return false;
            MaskPairs.push_back({static_cast<int>(A), static_cast<int>(B)});
        }
    return Out.LoadWorld(static_cast<std::uint32_t>(SeedNumber), Opened, MaskPairs);
}

bool ParseSave(const FString& Json, downstream::ExpeditionState& Candidate,
    downstream::SettlementState& CandidateSettlement, downstream::EarthworksState& CandidateEarthworks,
    const UDownstreamPrototypeTuning* Tuning)
{
    TSharedPtr<FJsonObject> Object;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) || !Object.IsValid()) return false;
    int32 Version = 0;
    int32 Banked = 0;
    FString Skill;
    if (!TryInteger(Object, TEXT("version"), Version) || (Version < 1 || Version > 4)
        || !TryInteger(Object, TEXT("banked_salvage"), Banked) || Banked < 0
        || !Object->HasTypedField<EJson::String>(TEXT("selected_skill"))
        || !Object->TryGetStringField(TEXT("selected_skill"), Skill)) return false;
    auto NewState = Candidate;
    downstream::SettlementState NewSettlement(SettlementRules(Tuning));
    if (!NewState.LoadPersisted(1, Banked, TCHAR_TO_UTF8(*Skill))) return false;
    downstream::EarthworksState NewEarthworks;
    NewEarthworks.SetDiggerCost(Tuning->DiggerCost);
    if (Version >= 3 && !ParseEarthworks(Object, NewEarthworks)) return false;
    if (Version >= 2)
    {
        const TArray<TSharedPtr<FJsonValue>>* Structures = nullptr;
        const TSharedPtr<FJsonObject>* Hauler = nullptr;
        if (!Object->TryGetArrayField(TEXT("structures"), Structures)
            || !Object->TryGetObjectField(TEXT("hauler"), Hauler) || !Hauler || !Hauler->IsValid()) return false;
        std::vector<downstream::StructureRecord> Records;
        for (const auto& Value : *Structures)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object) return false;
            const auto Record = Value->AsObject();
            FString Type;
            int32 X = 0, Y = 0;
            if (!Record.IsValid() || !Record->HasTypedField<EJson::String>(TEXT("type"))
                || !Record->TryGetStringField(TEXT("type"), Type)
                || !TryInteger(Record, TEXT("cell_x"), X) || !TryInteger(Record, TEXT("cell_y"), Y)) return false;
            downstream::StructureType ParsedType;
            if (Type == TEXT("storage")) ParsedType = downstream::StructureType::Storage;
            else if (Type == TEXT("workshop")) ParsedType = downstream::StructureType::Workshop;
            else if (Type == TEXT("charger")) ParsedType = downstream::StructureType::Charger;
            else return false;
            Records.push_back({ParsedType, {X, Y}});
        }
        bool bBuilt = false, bEnabled = false;
        double Battery = 0.0;
        int32 Cargo = 0;
        if (!(*Hauler)->HasTypedField<EJson::Boolean>(TEXT("built"))
            || !(*Hauler)->TryGetBoolField(TEXT("built"), bBuilt)
            || !(*Hauler)->HasTypedField<EJson::Number>(TEXT("battery"))
            || !(*Hauler)->TryGetNumberField(TEXT("battery"), Battery)
            || !TryInteger(*Hauler, TEXT("cargo"), Cargo)
            || !(*Hauler)->HasTypedField<EJson::Boolean>(TEXT("hauling_enabled"))
            || !(*Hauler)->TryGetBoolField(TEXT("hauling_enabled"), bEnabled)
            || !NewSettlement.LoadPersisted(Records, bBuilt, Battery, Cargo, bEnabled)) return false;
    }
    if (!NewState.SetSettlementCapacityBonus(NewSettlement.HasStructure(downstream::StructureType::Storage)
        ? Tuning->StorageCapacityBonus : 0)) return false;
    // A digger needs the workshop that assembled it.
    if (NewEarthworks.IsDiggerBuilt() && !NewSettlement.HasStructure(downstream::StructureType::Workshop)) return false;
    Candidate = NewState;
    CandidateSettlement = NewSettlement;
    CandidateEarthworks = NewEarthworks;
    return true;
}
}

void UDownstreamExpeditionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    const auto* Tuning = GetTuning(GetWorld());
    State = downstream::ExpeditionState(Tuning->InventoryCapacity);
    Settlement = downstream::SettlementState(SettlementRules(Tuning));
    Earthworks = downstream::EarthworksState();
    Earthworks.SetDiggerCost(Tuning->DiggerCost);
    bWorldMode = IsWorldMap(GetWorld());
    GWorldSavePath = bWorldMode;
    if (!bWorldMode)
    {
        LoadSave();
        return;
    }
    // World map: the menu (or -DownstreamWorldSeed=N) decides between a new world and the saved one.
    uint32 NewSeed = 0;
    EDownstreamStartMode Mode = EDownstreamStartMode::ContinueWorld;
    if (auto* Session = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<UDownstreamSessionSubsystem>() : nullptr)
    {
        const EDownstreamStartMode Pending = Session->ConsumeStartMode(NewSeed);
        if (Pending != EDownstreamStartMode::None) Mode = Pending;
    }
    int64 CommandSeed = 0;
    if (FParse::Value(FCommandLine::Get(), TEXT("DownstreamWorldSeed="), CommandSeed) && CommandSeed > 0)
    {
        Mode = EDownstreamStartMode::NewWorld;
        NewSeed = static_cast<uint32>(CommandSeed);
    }
    if (Mode != EDownstreamStartMode::NewWorld)
    {
        LoadSave();
        if (!bSaveBlocked && Earthworks.GetSeed() != 0)
        {
            SetStatus(FString::Printf(TEXT("Frontier %u loaded. Your dug terrain, buildings and razed outposts persist."), Earthworks.GetSeed()));
            return;
        }
        if (bSaveBlocked) return;
        NewSeed = static_cast<uint32>(FMath::RandRange(1, 999999));
    }
    // A fresh frontier: everything starts empty except the seed, which is saved at once.
    State = downstream::ExpeditionState(Tuning->InventoryCapacity);
    Settlement = downstream::SettlementState(SettlementRules(Tuning));
    Earthworks = downstream::EarthworksState();
    Earthworks.SetDiggerCost(Tuning->DiggerCost);
    Earthworks.SetSeed(NewSeed == 0 ? 1 : NewSeed);
    bSaveBlocked = false;
    WriteSave(State, Settlement, Earthworks);
    SetStatus(FString::Printf(TEXT("New frontier, seed %u. Loot caches, bank at home, build up. Dam Cult outposts sit below the rivers..."), Earthworks.GetSeed()));
}

FVector UDownstreamExpeditionSubsystem::GetHomeLocation() const
{
    if (bLayoutOverride) return HomeOverride;
    if (bWorldMode)
        if (auto* Water = UDownstreamWaterSubsystem::GetForWorld(const_cast<UDownstreamExpeditionSubsystem*>(this)))
            if (Water->EnsureWorldActive() && bLayoutOverride) return HomeOverride;
    return GetTuning(GetWorld())->HomeLocation;
}

float UDownstreamExpeditionSubsystem::GetHomeRadius() const
{
    return bLayoutOverride ? HomeRadiusOverride : GetTuning(GetWorld())->HomeRadius;
}

FVector UDownstreamExpeditionSubsystem::GetBuildGridCenter() const
{
    if (!bLayoutOverride && bWorldMode) GetHomeLocation();
    return bLayoutOverride ? GridCenterOverride : GetTuning(GetWorld())->BuildGridCenter;
}

void UDownstreamExpeditionSubsystem::SetLayoutOverride(const FVector& Home, float Radius, const FVector& GridCenter)
{
    bLayoutOverride = !Home.ContainsNaN() && !GridCenter.ContainsNaN() && FMath::IsFinite(Radius) && Radius > 0.0f;
    HomeOverride = Home;
    HomeRadiusOverride = Radius;
    GridCenterOverride = GridCenter;
}

bool UDownstreamExpeditionSubsystem::TrySpendBanked(int32 Amount, const FString& Reason)
{
    if (Amount <= 0) return true;
    auto Candidate = State;
    if (!Candidate.SpendBanked(Amount))
    {
        SetStatus(FString::Printf(TEXT("%s needs %d banked salvage."), *Reason, Amount));
        return false;
    }
    if (!WriteSave(Candidate, Settlement, Earthworks)) return false;
    State = Candidate;
    SetStatus(FString::Printf(TEXT("%s: spent %d salvage."), *Reason, Amount));
    return true;
}

bool UDownstreamExpeditionSubsystem::DevGrantSalvage(int32 Amount)
{
    auto Candidate = State;
    if (Amount <= 0 || !Candidate.AddBanked(Amount) || !WriteSave(Candidate, Settlement, Earthworks)) return false;
    State = Candidate;
    SetStatus(FString::Printf(TEXT("Developer: +%d banked salvage (now %d)."), Amount, State.GetBankedSalvage()));
    return true;
}

UDownstreamExpeditionSubsystem* UDownstreamExpeditionSubsystem::GetForWorld(UObject* ContextObject)
{
    UWorld* World = ContextObject ? ContextObject->GetWorld() : nullptr;
    return World ? World->GetSubsystem<UDownstreamExpeditionSubsystem>() : nullptr;
}

bool UDownstreamExpeditionSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

const UDownstreamPrototypeTuning* UDownstreamExpeditionSubsystem::GetTuning(UWorld* World)
{
    if (World)
    {
        if (auto* Expedition = World->GetSubsystem<UDownstreamExpeditionSubsystem>())
        {
            if (!Expedition->bTriedTuningLoad)
            {
                Expedition->bTriedTuningLoad = true;
                if (FPackageName::DoesPackageExist(TEXT("/Game/Downstream/Data/DA_PrototypeTuning")))
                {
                    Expedition->LoadedTuning = LoadObject<UDownstreamPrototypeTuning>(nullptr,
                        TEXT("/Game/Downstream/Data/DA_PrototypeTuning.DA_PrototypeTuning"));
                }
            }
            if (Expedition->LoadedTuning) return Expedition->LoadedTuning;
        }
    }
    return GetDefault<UDownstreamPrototypeTuning>();
}

int32 UDownstreamExpeditionSubsystem::GetCarriedSalvage() const { return State.GetCarriedSalvage(); }
int32 UDownstreamExpeditionSubsystem::GetBankedSalvage() const { return State.GetBankedSalvage(); }
int32 UDownstreamExpeditionSubsystem::GetInventoryCapacity() const { return State.GetInventoryCapacity(); }

TArray<FDownstreamStructureRecord> UDownstreamExpeditionSubsystem::GetBuiltStructures() const
{
    TArray<FDownstreamStructureRecord> Records;
    for (const auto& Structure : Settlement.GetStructures())
    {
        FDownstreamStructureRecord Record;
        Record.Type = static_cast<EDownstreamStructureType>(Structure.Type);
        Record.GridCell = FIntPoint(Structure.Cell.X, Structure.Cell.Y);
        Record.WorldLocation = GetBuildCellWorldLocation(Record.GridCell);
        Records.Add(Record);
    }
    return Records;
}

bool UDownstreamExpeditionSubsystem::HasStructure(EDownstreamStructureType Type) const
{
    return Settlement.HasStructure(PureType(Type));
}

int32 UDownstreamExpeditionSubsystem::GetStructureCost(EDownstreamStructureType Type) const
{
    return Settlement.GetStructureCost(PureType(Type));
}

bool UDownstreamExpeditionSubsystem::TryGetBuildCell(FVector Location, FIntPoint& Cell) const
{
    const auto* Tuning = GetTuning(GetWorld());
    const FVector Center = GetBuildGridCenter();
    if (Location.ContainsNaN() || Center.ContainsNaN()
        || !FMath::IsFinite(Tuning->BuildGridCellSize) || Tuning->BuildGridCellSize <= 0.0f) return false;
    const double X = FMath::FloorToDouble((Location.X - Center.X) / Tuning->BuildGridCellSize + 0.5);
    const double Y = FMath::FloorToDouble((Location.Y - Center.Y) / Tuning->BuildGridCellSize + 0.5);
    if (!FMath::IsFinite(X) || !FMath::IsFinite(Y) || X < MIN_int32 || X > MAX_int32 || Y < MIN_int32 || Y > MAX_int32) return false;
    Cell = FIntPoint(static_cast<int32>(X), static_cast<int32>(Y));
    return true;
}

FVector UDownstreamExpeditionSubsystem::GetBuildCellWorldLocation(FIntPoint Cell) const
{
    const auto* Tuning = GetTuning(GetWorld());
    return GetBuildGridCenter() + FVector(Cell.X * static_cast<double>(Tuning->BuildGridCellSize),
        Cell.Y * static_cast<double>(Tuning->BuildGridCellSize), 0.0);
}

FVector UDownstreamExpeditionSubsystem::GetSnappedBuildLocation(FVector Location) const
{
    FIntPoint Cell;
    return TryGetBuildCell(Location, Cell) ? GetBuildCellWorldLocation(Cell) : GetBuildGridCenter();
}

bool UDownstreamExpeditionSubsystem::CanPlaceStructure(EDownstreamStructureType Type, FVector Location) const
{
    FIntPoint Cell;
    if (bSaveBlocked || !GetWorld() || !TryGetBuildCell(Location, Cell)
        || !Settlement.CanPlaceStructure(State, PureType(Type), {Cell.X, Cell.Y})) return false;
    // Bottom is above the ground and home pad; real blocking props and pawn capsules remain in the query.
    const FVector Center = GetBuildCellWorldLocation(Cell) + FVector(0.0, 0.0, 100.0);
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(DownstreamBuildPlacement), false);
    GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity,
        FCollisionObjectQueryParams::AllObjects, FCollisionShape::MakeBox(FVector(90.0, 90.0, 95.0)), Query);
    for (const auto& Overlap : Overlaps)
    {
        const UPrimitiveComponent* Component = Overlap.GetComponent();
        if (IsValid(Component) && Component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block) return false;
    }
    return true;
}

bool UDownstreamExpeditionSubsystem::TryPlaceStructure(EDownstreamStructureType Type, FVector Location, AActor* Player)
{
    if (!IsValid(Player) || Player->GetWorld() != GetWorld()) return false;
    const auto* Tuning = GetTuning(GetWorld());
    auto Reject = [this, Player, Tuning](const TCHAR* Message)
    {
        SetStatus(Message);
        if (Tuning->ErrorSound) UGameplayStatics::PlaySoundAtLocation(Player, Tuning->ErrorSound, Player->GetActorLocation());
        return false;
    };
    if (!IsAtHome(Player->GetActorLocation())) return Reject(TEXT("Return inside the protected home circle to build."));
    if (bSaveBlocked) return Reject(TEXT("Save needs attention. Original file preserved; building disabled."));
    if (HasStructure(Type)) return Reject(TEXT("One of each structure is allowed."));
    if (GetStructureCost(Type) > State.GetBankedSalvage()) return Reject(TEXT("Bank more salvage to afford this structure."));
    FIntPoint Cell;
    if (!TryGetBuildCell(Location, Cell) || !CanPlaceStructure(Type, Location))
        return Reject(TEXT("Choose an empty cell in the home clearing, clear of props and people."));
    auto Candidate = State;
    auto CandidateSettlement = Settlement;
    if (!CandidateSettlement.PlaceStructure(Candidate, PureType(Type), {Cell.X, Cell.Y})
        || !WriteSave(Candidate, CandidateSettlement)) return false;
    State = Candidate;
    Settlement = CandidateSettlement;
    OnSettlementChanged.Broadcast();
    if (Tuning->BuildSound) UGameplayStatics::PlaySoundAtLocation(Player, Tuning->BuildSound, GetBuildCellWorldLocation(Cell));
    SetStatus(FString::Printf(TEXT("Built %s. Bank: %d."), StructureName(PureType(Type)), State.GetBankedSalvage()));
    return true;
}

bool UDownstreamExpeditionSubsystem::TryConstructDigger(AActor* Player)
{
    if (!IsValid(Player) || Player->GetWorld() != GetWorld()) return false;
    if (!IsAtHome(Player->GetActorLocation()))
    {
        SetStatus(TEXT("Return inside the protected home circle to assemble the digger."));
        return false;
    }
    if (Earthworks.IsDiggerBuilt()) { SetStatus(TEXT("Your digger is already assembled. G plans a canal.")); return false; }
    if (!Settlement.HasStructure(downstream::StructureType::Workshop))
    {
        SetStatus(TEXT("Build the workshop first. The digger is assembled there."));
        return false;
    }
    if (!Earthworks.CanBuildDigger(State.GetBankedSalvage()))
    {
        SetStatus(FString::Printf(TEXT("The digger needs %d banked salvage."), Earthworks.GetDiggerCost()));
        return false;
    }
    auto Candidate = State;
    auto CandidateEarthworks = Earthworks;
    if (!Candidate.SpendBanked(Earthworks.GetDiggerCost())) return false;
    CandidateEarthworks.MarkDiggerBuilt();
    if (!WriteSave(Candidate, Settlement, CandidateEarthworks)) return false;
    State = Candidate;
    Earthworks = CandidateEarthworks;
    OnSettlementChanged.Broadcast();
    const auto* Tuning = GetTuning(GetWorld());
    if (Tuning->BuildSound) UGameplayStatics::PlaySoundAtLocation(Player, Tuning->BuildSound, Player->GetActorLocation());
    SetStatus(TEXT("Digger assembled. Press G, then drag a canal line on the flooded site."));
    return true;
}

bool UDownstreamExpeditionSubsystem::CommitVaultOpened(int32 VaultId)
{
    auto CandidateEarthworks = Earthworks;
    if (!CandidateEarthworks.OpenVault(VaultId) || !WriteSave(State, Settlement, CandidateEarthworks)) return false;
    Earthworks = CandidateEarthworks;
    return true;
}

bool UDownstreamExpeditionSubsystem::CommitOutpostMask(int32 OutpostId, int32 Mask)
{
    if (Earthworks.GetOutpostMask(OutpostId) == Mask) return true;
    auto CandidateEarthworks = Earthworks;
    CandidateEarthworks.SetOutpostMask(OutpostId, Mask);
    if (!WriteSave(State, Settlement, CandidateEarthworks)) return false;
    Earthworks = CandidateEarthworks;
    return true;
}

bool UDownstreamExpeditionSubsystem::CommitEarthworksTerrain(const downstream::WaterGrid& Grid, const std::vector<float>& BaseGround)
{
    auto CandidateEarthworks = Earthworks;
    CandidateEarthworks.CaptureTerrain(Grid, BaseGround);
    if (CandidateEarthworks.GetTerrain().size() == Earthworks.GetTerrain().size()
        && std::equal(CandidateEarthworks.GetTerrain().begin(), CandidateEarthworks.GetTerrain().end(), Earthworks.GetTerrain().begin(),
            [](const downstream::TerrainDelta& A, const downstream::TerrainDelta& B) { return A.X == B.X && A.Y == B.Y && A.HeightMm == B.HeightMm; }))
        return true;
    if (!WriteSave(State, Settlement, CandidateEarthworks)) return false;
    Earthworks = CandidateEarthworks;
    return true;
}

bool UDownstreamExpeditionSubsystem::IsHaulerConstructed() const { return Settlement.IsHaulerConstructed(); }
int32 UDownstreamExpeditionSubsystem::GetHaulerCost() const { return Settlement.GetHaulerCost(); }
int32 UDownstreamExpeditionSubsystem::GetHaulerCargoCapacity() const { return Settlement.GetHaulerCargoCapacity(); }
bool UDownstreamExpeditionSubsystem::IsHaulingEnabled() const { return Settlement.IsHaulingEnabled(); }
float UDownstreamExpeditionSubsystem::GetHaulerBattery() const { return static_cast<float>(Settlement.GetHaulerBattery()); }
int32 UDownstreamExpeditionSubsystem::GetHaulerCargo() const { return Settlement.GetHaulerCargo(); }

bool UDownstreamExpeditionSubsystem::TryConstructHauler(AActor* Player)
{
    if (!IsValid(Player) || Player->GetWorld() != GetWorld()) return false;
    if (!IsAtHome(Player->GetActorLocation()))
    {
        SetStatus(TEXT("Return inside the protected home circle to construct your hauler."));
        return false;
    }
    auto Candidate = State;
    auto CandidateSettlement = Settlement;
    if (!CandidateSettlement.ConstructHauler(Candidate))
    {
        SetStatus(IsHaulerConstructed() ? TEXT("Your one hauler is already built.")
            : TEXT("Build storage, workshop and charger, then bank enough salvage for the hauler."));
        return false;
    }
    if (!WriteSave(Candidate, CandidateSettlement)) return false;
    State = Candidate;
    Settlement = CandidateSettlement;
    OnSettlementChanged.Broadcast();
    const auto* Tuning = GetTuning(GetWorld());
    if (Tuning->BuildSound) UGameplayStatics::PlaySoundAtLocation(Player, Tuning->BuildSound, Player->GetActorLocation());
    SetStatus(TEXT("Hauler built. Press J to enable its salvage job."));
    return true;
}

bool UDownstreamExpeditionSubsystem::SetHaulingEnabled(bool bEnabled, AActor* Player)
{
    if (!IsValid(Player) || Player->GetWorld() != GetWorld()) return false;
    if (!IsHaulerConstructed()) { SetStatus(TEXT("Build a hauler at home before enabling hauling.")); return false; }
    if (IsHaulingEnabled() == bEnabled) return true;
    auto CandidateSettlement = Settlement;
    if (!CandidateSettlement.SetHaulingEnabled(bEnabled) || !WriteSave(State, CandidateSettlement)) return false;
    Settlement = CandidateSettlement;
    SetStatus(bEnabled ? TEXT("Hauling enabled. The hauler collects salvage away from living hostiles.")
        : TEXT("Hauling disabled. The hauler returns cargo and recharges at home."));
    return true;
}

bool UDownstreamExpeditionSubsystem::TryLoadHaulerCargo(ADownstreamSalvagePickup* Pickup, AActor* Hauler)
{
    if (!IsHaulerConstructed() || !IsValid(Hauler) || Hauler->GetWorld() != GetWorld()
        || !Hauler->ActorHasTag(FName(TEXT("DownstreamHauler"))) || !IsValid(Pickup)
        || Pickup->GetWorld() != GetWorld() || Pickup->IsClaimed()
        || Hauler->GetActorLocation().ContainsNaN() || Pickup->GetActorLocation().ContainsNaN()
        || FVector::DistSquared(Hauler->GetActorLocation(), Pickup->GetActorLocation()) > FMath::Square(150.0f)) return false;
    auto CandidateSettlement = Settlement;
    if (!CandidateSettlement.LoadHaulerCargo(Pickup->Amount) || !Pickup->TryClaim()) return false;
    if (!WriteSave(State, CandidateSettlement)) { Pickup->ReleaseClaim(); return false; }
    Settlement = CandidateSettlement;
    const auto* Tuning = GetTuning(GetWorld());
    if (Tuning->PickupSound) UGameplayStatics::PlaySoundAtLocation(Hauler, Tuning->PickupSound, Pickup->GetActorLocation());
    Pickup->Destroy();
    SetStatus(FString::Printf(TEXT("Hauler collected salvage. Cargo: %d/%d."), GetHaulerCargo(), GetHaulerCargoCapacity()));
    return true;
}

bool UDownstreamExpeditionSubsystem::TryUnloadHaulerCargo(AActor* Hauler)
{
    if (!IsHaulerConstructed() || !IsValid(Hauler) || Hauler->GetWorld() != GetWorld()
        || !Hauler->ActorHasTag(FName(TEXT("DownstreamHauler"))) || GetHaulerCargo() <= 0) return false;
    FVector StorageLocation = FVector::ZeroVector;
    bool bAtStorage = false;
    for (const auto& Record : GetBuiltStructures())
    {
        if (Record.Type == EDownstreamStructureType::Storage
            && FVector::DistSquared(Hauler->GetActorLocation(), Record.WorldLocation) <= FMath::Square(220.0f))
        {
            StorageLocation = Record.WorldLocation;
            bAtStorage = true;
            break;
        }
    }
    if (!bAtStorage) return false;
    auto Candidate = State;
    auto CandidateSettlement = Settlement;
    const int32 Amount = GetHaulerCargo();
    if (!CandidateSettlement.UnloadHaulerCargo(Candidate)) { SetStatus(TEXT("Stash is full. Hauler cargo kept.")); return false; }
    if (!WriteSave(Candidate, CandidateSettlement)) return false;
    State = Candidate;
    Settlement = CandidateSettlement;
    const auto* Tuning = GetTuning(GetWorld());
    if (Tuning->DockSound) UGameplayStatics::PlaySoundAtLocation(Hauler, Tuning->DockSound, StorageLocation);
    SetStatus(FString::Printf(TEXT("Hauler delivered %d salvage. Bank saved: %d."), Amount, GetBankedSalvage()));
    return true;
}

void UDownstreamExpeditionSubsystem::UpdateHaulerBattery(float SignedDelta)
{
    const double Previous = Settlement.GetHaulerBattery();
    Settlement.UpdateHaulerBattery(SignedDelta);
    bHaulerBatteryDirty |= Settlement.GetHaulerBattery() != Previous;
}

bool UDownstreamExpeditionSubsystem::SaveHaulerCheckpoint()
{
    return !bHaulerBatteryDirty || WriteSave(State, Settlement);
}

float UDownstreamExpeditionSubsystem::GetRangedDamageMultiplier() const { return static_cast<float>(State.GetRangedDamageMultiplier()); }
FName UDownstreamExpeditionSubsystem::GetSelectedSkillId() const
{
    const auto& Skill = State.GetSelectedSkillId();
    return Skill.empty() ? NAME_None : FName(UTF8_TO_TCHAR(Skill.c_str()));
}

bool UDownstreamExpeditionSubsystem::ChooseStarterSkill(FName SkillId)
{
    if (SkillId.IsNone()) return false;
    if (GetSelectedSkillId() == SkillId) return true;
    auto Candidate = State;
    if (!Candidate.ChooseStarterSkill(TCHAR_TO_UTF8(*SkillId.ToString())))
    {
        SetStatus(GetSelectedSkillId().IsNone() ? TEXT("Choose Steady Aim or Salvage Handling for your free starter skill.")
            : TEXT("One free starter skill. Your selected node is already active."));
        return false;
    }
    if (!WriteSave(Candidate, Settlement)) return false;
    State = Candidate;
    SetStatus(SkillId == FName(TEXT("steady_aim")) ? TEXT("Steady Aim selected: ranged damage +15%.")
        : TEXT("Salvage Handling selected: carrying capacity +4."));
    return true;
}

bool UDownstreamExpeditionSubsystem::IsAtHome(FVector Location) const
{
    const auto* Tuning = GetTuning(GetWorld());
    return !Location.ContainsNaN() && FVector::DistSquared2D(Location, GetHomeLocation())
        <= FMath::Square(FMath::Max(0.0f, GetHomeRadius()));
}

bool UDownstreamExpeditionSubsystem::TryInteract(AActor* Player)
{
    if (!IsValid(Player) || Player->GetWorld() != GetWorld()) return false;
    ADownstreamSalvagePickup* Nearest = nullptr;
    float NearestDistance = FMath::Square(220.0f);
    for (TActorIterator<ADownstreamSalvagePickup> It(GetWorld()); It; ++It)
    {
        if (!IsValid(*It) || It->IsClaimed() || !State.CanCollect(It->Amount)) continue;
        const float Distance = FVector::DistSquared(Player->GetActorLocation(), It->GetActorLocation());
        if (Distance <= NearestDistance)
        {
            Nearest = *It;
            NearestDistance = Distance;
        }
    }
    if (Nearest && Nearest->TryClaim())
    {
        const int32 Amount = Nearest->Amount;
        if (!State.Collect(Amount))
        {
            Nearest->ReleaseClaim();
            return false;
        }
        const auto* Tuning = GetTuning(GetWorld());
        if (Tuning->PickupSound) UGameplayStatics::PlaySoundAtLocation(Player, Tuning->PickupSound, Nearest->GetActorLocation());
        Nearest->Destroy();
        SetStatus(FString::Printf(TEXT("Collected %d salvage. Return home and press E to bank."), Amount));
        return true;
    }
    if (IsAtHome(Player->GetActorLocation()))
    {
        const int32 Amount = State.GetCarriedSalvage();
        if (Amount == 0)
        {
            SetStatus(bSaveBlocked ? TEXT("Save needs attention. Original file preserved; banking disabled.") : TEXT("Home base. Explore, collect salvage, and return here to bank."));
            return true;
        }
        auto Candidate = State;
        if (!Candidate.BankCarried())
        {
            SetStatus(TEXT("Stash is full. Carried salvage kept."));
            return false;
        }
        if (!WriteSave(Candidate, Settlement)) return false;
        State = Candidate;
        const auto* Tuning = GetTuning(GetWorld());
        if (Tuning->ExtractionSound) UGameplayStatics::PlaySoundAtLocation(Player, Tuning->ExtractionSound, GetHomeLocation());
        SetStatus(FString::Printf(TEXT("Extracted %d salvage. Stash saved: %d."), Amount, State.GetBankedSalvage()));
        return true;
    }
    SetStatus(State.GetCarriedSalvage() >= State.GetInventoryCapacity()
        ? TEXT("Inventory full. Return to the home marker and press E.") : TEXT("Move closer to salvage, then press E. Bank at the home marker."));
    return false;
}

ADownstreamSalvagePickup* UDownstreamExpeditionSubsystem::SpawnSalvage(FVector Location, int32 Amount, bool bRecoveryBag)
{
    if (!GetWorld() || Amount <= 0 || Location.ContainsNaN()) return nullptr;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Pickup = GetWorld()->SpawnActor<ADownstreamSalvagePickup>(Location, FRotator::ZeroRotator, Params);
    if (Pickup)
    {
        Pickup->Amount = Amount;
        Pickup->bRecoveryBag = bRecoveryBag;
        Pickup->RefreshAppearance();
    }
    return Pickup;
}

void UDownstreamExpeditionSubsystem::HandlePlayerDeath(FVector DeathLocation)
{
    const int32 Amount = State.GetCarriedSalvage();
    if (Amount <= 0)
    {
        SetStatus(TEXT("Returned home. Your banked stash is safe."));
        return;
    }
    // Spawn before clearing carry so an allocation/spawn failure cannot erase inventory.
    if (!SpawnSalvage(DeathLocation, Amount, true))
    {
        SetStatus(TEXT("Recovery bag could not spawn. Carried salvage kept."));
        return;
    }
    State.DropCarriedOnDeath();
    SetStatus(FString::Printf(TEXT("Dropped %d salvage. Recover the gold bag; stash is safe."), Amount));
}

void UDownstreamExpeditionSubsystem::SetStatus(const FString& Message)
{
    StatusMessage = FText::FromString(Message);
}

void UDownstreamExpeditionSubsystem::LoadSave()
{
    const FString Path = SavePath();
    FString LoadPath = Path;
    const bool bRecoverBackup = !IFileManager::Get().FileExists(*Path) && IFileManager::Get().FileExists(*(Path + TEXT(".bak")));
    if (bRecoverBackup) LoadPath += TEXT(".bak");
    if (!IFileManager::Get().FileExists(*LoadPath))
    {
        SetStatus(TEXT("Fresh run. Pick one starter skill, then collect salvage."));
        return;
    }
    FString Json;
    auto Candidate = State;
    auto CandidateSettlement = Settlement;
    auto CandidateEarthworks = Earthworks;
    if (!FFileHelper::LoadFileToString(Json, *LoadPath)
        || !ParseSave(Json, Candidate, CandidateSettlement, CandidateEarthworks, GetTuning(GetWorld())))
    {
        bSaveBlocked = true;
        SetStatus(TEXT("Save unreadable or unsupported. Safe defaults; original file preserved. Progress writes disabled."));
        UE_LOG(LogDownstreamExpedition, Warning, TEXT("Invalid prototype save preserved at %s. Progress writes disabled until the file is repaired or moved aside."), *LoadPath);
        return;
    }
    State = Candidate;
    Settlement = CandidateSettlement;
    Earthworks = CandidateEarthworks;
    SetStatus(bRecoverBackup ? TEXT("Recovered stash from save backup. Field pickups reset for this run.")
        : TEXT("Stash, starter skill and settlement loaded. Field pickups reset for this run."));
    if (bRecoverBackup) UE_LOG(LogDownstreamExpedition, Warning, TEXT("Recovered prototype stash from %s."), *LoadPath);
}

bool UDownstreamExpeditionSubsystem::WriteSave(const downstream::ExpeditionState& Candidate,
    const downstream::SettlementState& CandidateSettlement)
{
    return WriteSave(Candidate, CandidateSettlement, Earthworks);
}

bool UDownstreamExpeditionSubsystem::WriteSave(const downstream::ExpeditionState& Candidate,
    const downstream::SettlementState& CandidateSettlement, const downstream::EarthworksState& CandidateEarthworks)
{
    if (bSaveBlocked)
    {
        SetStatus(TEXT("Save needs attention. Original file preserved; action not committed."));
        return false;
    }
    const FString Path = SavePath();
    const FString TempPath = Path + TEXT(".tmp");
    const FString BackupPath = Path + TEXT(".bak");
    auto& Files = IFileManager::Get();
    auto Fail = [this, &Path]()
    {
        SetStatus(TEXT("Save write failed. Action not committed; salvage kept."));
        UE_LOG(LogDownstreamExpedition, Warning, TEXT("Prototype save commit failed at %s; inventory/stash state was not changed."), *Path);
        return false;
    };
    if (!Files.MakeDirectory(*FPaths::GetPath(Path), true)) return Fail();
    TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("version"), 4);
    Object->SetNumberField(TEXT("banked_salvage"), Candidate.GetBankedSalvage());
    Object->SetStringField(TEXT("selected_skill"), UTF8_TO_TCHAR(Candidate.GetSelectedSkillId().c_str()));
    TArray<TSharedPtr<FJsonValue>> Structures;
    for (const auto& Structure : CandidateSettlement.GetStructures())
    {
        TSharedRef<FJsonObject> Record = MakeShared<FJsonObject>();
        Record->SetStringField(TEXT("type"), StructureName(Structure.Type));
        Record->SetNumberField(TEXT("cell_x"), Structure.Cell.X);
        Record->SetNumberField(TEXT("cell_y"), Structure.Cell.Y);
        Structures.Add(MakeShared<FJsonValueObject>(Record));
    }
    Object->SetArrayField(TEXT("structures"), Structures);
    TSharedRef<FJsonObject> Hauler = MakeShared<FJsonObject>();
    Hauler->SetBoolField(TEXT("built"), CandidateSettlement.IsHaulerConstructed());
    Hauler->SetNumberField(TEXT("battery"), CandidateSettlement.GetHaulerBattery());
    Hauler->SetNumberField(TEXT("cargo"), CandidateSettlement.GetHaulerCargo());
    Hauler->SetBoolField(TEXT("hauling_enabled"), CandidateSettlement.IsHaulingEnabled());
    Object->SetObjectField(TEXT("hauler"), Hauler);
    TSharedRef<FJsonObject> Earth = MakeShared<FJsonObject>();
    Earth->SetBoolField(TEXT("digger_built"), CandidateEarthworks.IsDiggerBuilt());
    Earth->SetBoolField(TEXT("vault_opened"), CandidateEarthworks.IsVaultOpened());
    TArray<TSharedPtr<FJsonValue>> Cells;
    for (const auto& Delta : CandidateEarthworks.GetTerrain())
    {
        TArray<TSharedPtr<FJsonValue>> Triple;
        Triple.Add(MakeShared<FJsonValueNumber>(Delta.X));
        Triple.Add(MakeShared<FJsonValueNumber>(Delta.Y));
        Triple.Add(MakeShared<FJsonValueNumber>(Delta.HeightMm));
        Cells.Add(MakeShared<FJsonValueArray>(Triple));
    }
    Earth->SetArrayField(TEXT("terrain_mm"), Cells);
    Earth->SetNumberField(TEXT("seed"), static_cast<double>(CandidateEarthworks.GetSeed()));
    TArray<TSharedPtr<FJsonValue>> OpenedVaults;
    for (int Id : CandidateEarthworks.GetOpenedVaults()) OpenedVaults.Add(MakeShared<FJsonValueNumber>(Id));
    Earth->SetArrayField(TEXT("vaults_opened"), OpenedVaults);
    TArray<TSharedPtr<FJsonValue>> OutpostMasks;
    for (const auto& Mask : CandidateEarthworks.GetOutpostMasks())
    {
        TArray<TSharedPtr<FJsonValue>> Pair;
        Pair.Add(MakeShared<FJsonValueNumber>(Mask.first));
        Pair.Add(MakeShared<FJsonValueNumber>(Mask.second));
        OutpostMasks.Add(MakeShared<FJsonValueArray>(Pair));
    }
    Earth->SetArrayField(TEXT("outposts"), OutpostMasks);
    Object->SetObjectField(TEXT("earthworks"), Earth);
    FString Json;
    if (!FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Json))
        || !FFileHelper::SaveStringToFile(Json, *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) return Fail();
    if (Files.FileExists(*Path) && Files.Copy(*BackupPath, *Path, true, true) != COPY_OK) return Fail();
    if (!Files.Move(*Path, *TempPath, true, true, false, true))
    {
        if (!Files.FileExists(*Path) && Files.FileExists(*BackupPath)) Files.Copy(*Path, *BackupPath, true, true);
        return Fail();
    }
    bHaulerBatteryDirty = false;
    return true;
}
