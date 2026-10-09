#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamPrototypeTuning.h"
#include "DownstreamSalvagePickup.h"

#include "Dom/JsonObject.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogDownstreamExpedition, Log, All);

namespace
{
FString SavePath()
{
    FString OverrideDirectory;
    if (FParse::Value(FCommandLine::Get(), TEXT("DownstreamSaveDir="), OverrideDirectory)
        && !OverrideDirectory.IsEmpty() && !FPaths::IsRelative(OverrideDirectory))
    {
        return FPaths::Combine(OverrideDirectory, TEXT("prototype-save.json"));
    }
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Downstream/prototype-save.json"));
}

bool ParseSave(const FString& Json, downstream::ExpeditionState& Candidate)
{
    TSharedPtr<FJsonObject> Object;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) || !Object.IsValid()) return false;
    double Version = 0.0;
    double Banked = 0.0;
    FString Skill;
    if (!Object->TryGetNumberField(TEXT("version"), Version) || !Object->TryGetNumberField(TEXT("banked_salvage"), Banked)
        || !Object->TryGetStringField(TEXT("selected_skill"), Skill)) return false;
    if (!FMath::IsFinite(Version) || Version != 1.0 || !FMath::IsFinite(Banked) || Banked < 0.0
        || Banked > static_cast<double>(MAX_int32) || FMath::FloorToDouble(Banked) != Banked) return false;
    return Candidate.LoadPersisted(1, static_cast<int64>(Banked), TCHAR_TO_UTF8(*Skill));
}
}

void UDownstreamExpeditionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    State = downstream::ExpeditionState(GetTuning(GetWorld())->InventoryCapacity);
    LoadSave();
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
    if (!WriteSave(Candidate)) return false;
    State = Candidate;
    SetStatus(SkillId == FName(TEXT("steady_aim")) ? TEXT("Steady Aim selected: ranged damage +15%.")
        : TEXT("Salvage Handling selected: carrying capacity +4."));
    return true;
}

bool UDownstreamExpeditionSubsystem::IsAtHome(FVector Location) const
{
    const auto* Tuning = GetTuning(GetWorld());
    return !Location.ContainsNaN() && FVector::DistSquared2D(Location, Tuning->HomeLocation)
        <= FMath::Square(FMath::Max(0.0f, Tuning->HomeRadius));
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
        if (!WriteSave(Candidate)) return false;
        State = Candidate;
        const auto* Tuning = GetTuning(GetWorld());
        if (Tuning->ExtractionSound) UGameplayStatics::PlaySoundAtLocation(Player, Tuning->ExtractionSound, Tuning->HomeLocation);
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
    if (!FFileHelper::LoadFileToString(Json, *LoadPath) || !ParseSave(Json, Candidate))
    {
        bSaveBlocked = true;
        SetStatus(TEXT("Save unreadable or unsupported. Safe defaults; original file preserved. Banking disabled."));
        UE_LOG(LogDownstreamExpedition, Warning, TEXT("Invalid prototype save preserved at %s. Banking disabled until the file is repaired or moved aside."), *LoadPath);
        return;
    }
    State = Candidate;
    SetStatus(bRecoverBackup ? TEXT("Recovered stash from save backup. Field pickups reset for this run.")
        : TEXT("Stash and starter skill loaded. Field pickups reset for this run."));
    if (bRecoverBackup) UE_LOG(LogDownstreamExpedition, Warning, TEXT("Recovered prototype stash from %s."), *LoadPath);
}

bool UDownstreamExpeditionSubsystem::WriteSave(const downstream::ExpeditionState& Candidate)
{
    if (bSaveBlocked)
    {
        SetStatus(TEXT("Save needs attention. Original file preserved; carried salvage kept."));
        return false;
    }
    const FString Path = SavePath();
    const FString TempPath = Path + TEXT(".tmp");
    const FString BackupPath = Path + TEXT(".bak");
    auto& Files = IFileManager::Get();
    auto Fail = [this, &Path]()
    {
        SetStatus(TEXT("Save write failed. Action not committed; carried salvage kept."));
        UE_LOG(LogDownstreamExpedition, Warning, TEXT("Prototype save commit failed at %s; inventory/stash state was not changed."), *Path);
        return false;
    };
    if (!Files.MakeDirectory(*FPaths::GetPath(Path), true)) return Fail();
    TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("version"), 1);
    Object->SetNumberField(TEXT("banked_salvage"), Candidate.GetBankedSalvage());
    Object->SetStringField(TEXT("selected_skill"), UTF8_TO_TCHAR(Candidate.GetSelectedSkillId().c_str()));
    FString Json;
    if (!FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Json))
        || !FFileHelper::SaveStringToFile(Json, *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) return Fail();
    if (Files.FileExists(*Path) && Files.Copy(*BackupPath, *Path, true, true) != COPY_OK) return Fail();
    if (!Files.Move(*Path, *TempPath, true, true, false, true))
    {
        if (!Files.FileExists(*Path) && Files.FileExists(*BackupPath)) Files.Copy(*Path, *BackupPath, true, true);
        return Fail();
    }
    return true;
}
