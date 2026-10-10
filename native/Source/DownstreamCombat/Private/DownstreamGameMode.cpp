#include "DownstreamGameMode.h"
#include "DownstreamPlayerCharacter.h"
#include "DownstreamPlayerController.h"
#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamPrototypeTuning.h"
#include "DownstreamSalvagePickup.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

ADownstreamGameMode::ADownstreamGameMode()
{
    DefaultPawnClass = ADownstreamPlayerCharacter::StaticClass();
    PlayerControllerClass = ADownstreamPlayerController::StaticClass();
}

void ADownstreamGameMode::RestartPlayer(AController* NewPlayer)
{
    if (!IsValid(NewPlayer)) return;
    const auto* Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this);
    const FVector Home = Expedition ? Expedition->GetHomeLocation() : UDownstreamExpeditionSubsystem::GetTuning(GetWorld())->HomeLocation;
    RestartPlayerAtTransform(NewPlayer, FTransform(FRotator::ZeroRotator, Home));
}

void ADownstreamGameMode::RequestPlayerRespawn(AController* PlayerController, APawn* DeadPawn)
{
    if (!IsValid(PlayerController)) return;
    const TWeakObjectPtr<AController> WeakController = PlayerController;
    const TWeakObjectPtr<APawn> WeakDeadPawn = DeadPawn;
    const TWeakObjectPtr<ADownstreamGameMode> WeakMode = this;
    const float Delay = FMath::Max(0.01f, UDownstreamExpeditionSubsystem::GetTuning(GetWorld())->RespawnDelay);
    GetWorldTimerManager().SetTimer(RespawnTimer, [WeakController, WeakDeadPawn, WeakMode]()
    {
        if (!WeakMode.IsValid() || !WeakController.IsValid()) return;
        AController* Controller = WeakController.Get();
        if (Controller->GetPawn() && Controller->GetPawn() != WeakDeadPawn.Get()) return;
        Controller->UnPossess();
        WeakMode->RestartPlayer(Controller);
        if (WeakDeadPawn.IsValid()) WeakDeadPawn->Destroy();
    }, Delay, false);
}

void ADownstreamGameMode::StartPlay()
{
    Super::StartPlay();
    bFieldRespawnEnabled = !FParse::Param(FCommandLine::Get(), TEXT("DownstreamNoFieldRespawn"));
    // Register after level actors have begun play, then check once a second.
    GetWorldTimerManager().SetTimer(FieldTimer, this, &ADownstreamGameMode::TickField, 1.0f, true, 1.0f);
}

void ADownstreamGameMode::RegisterField()
{
    bFieldRegistered = true;
    for (TActorIterator<ADownstreamEnemy> It(GetWorld()); It; ++It)
    {
        if (!IsValid(*It) || !It->IsNetStartupActor()) continue;
        FEnemySlot Slot;
        Slot.Enemy = *It;
        Slot.Spawn = It->GetActorTransform();
        Slot.Kind = It->Kind;
        EnemySlots.Add(Slot);
    }
    for (TActorIterator<ADownstreamSalvagePickup> It(GetWorld()); It; ++It)
    {
        if (!IsValid(*It) || !It->IsNetStartupActor() || It->bRecoveryBag) continue;
        FCacheSlot Slot;
        Slot.Pickup = *It;
        Slot.Location = It->GetActorLocation();
        Slot.Amount = It->Amount;
        CacheSlots.Add(Slot);
    }
}

bool ADownstreamGameMode::SpawnEnemy(FEnemySlot& Slot)
{
    auto* Enemy = GetWorld()->SpawnActorDeferred<ADownstreamEnemy>(ADownstreamEnemy::StaticClass(), Slot.Spawn, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
    if (!Enemy) return false;
    Enemy->Kind = Slot.Kind;
    UGameplayStatics::FinishSpawningActor(Enemy, Slot.Spawn);
    Slot.Enemy = Enemy;
    Slot.DownSince = -1.0;
    ++FieldRespawns;
    return true;
}

bool ADownstreamGameMode::SpawnCache(FCacheSlot& Slot)
{
    auto* Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this);
    auto* Pickup = Expedition ? Expedition->SpawnSalvage(Slot.Location, Slot.Amount, false) : nullptr;
    if (!Pickup) return false;
    Slot.Pickup = Pickup;
    Slot.GoneSince = -1.0;
    ++FieldRespawns;
    return true;
}

void ADownstreamGameMode::TickField()
{
    if (!GetWorld()) return;
    if (!bFieldRegistered) RegisterField();
    if (!bFieldRespawnEnabled) return;
    const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld());
    const double Now = GetWorld()->GetTimeSeconds();
    const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    const float SafeSq = FMath::Square(FMath::Max(0.0f, Tuning->FieldRespawnSafeRadius));
    const auto FarFromPlayer = [Player, SafeSq](const FVector& Location)
    {
        return !Player || FVector::DistSquared2D(Player->GetActorLocation(), Location) > SafeSq;
    };
    for (FEnemySlot& Slot : EnemySlots)
    {
        const bool bAlive = Slot.Enemy.IsValid() && !Slot.Enemy->IsDead();
        if (bAlive) { Slot.DownSince = -1.0; continue; }
        if (Slot.DownSince < 0.0) { Slot.DownSince = Now; continue; }
        if (Now - Slot.DownSince >= Tuning->EnemyRespawnSeconds && FarFromPlayer(Slot.Spawn.GetLocation())) SpawnEnemy(Slot);
    }
    for (FCacheSlot& Slot : CacheSlots)
    {
        if (Slot.Pickup.IsValid()) { Slot.GoneSince = -1.0; continue; }
        if (Slot.GoneSince < 0.0) { Slot.GoneSince = Now; continue; }
        if (Now - Slot.GoneSince >= Tuning->CacheRespawnSeconds && FarFromPlayer(Slot.Location)) SpawnCache(Slot);
    }
}

int32 ADownstreamGameMode::RespawnFieldNow()
{
    if (!bFieldRegistered) RegisterField();
    int32 Count = 0;
    for (FEnemySlot& Slot : EnemySlots)
        if (!(Slot.Enemy.IsValid() && !Slot.Enemy->IsDead()) && SpawnEnemy(Slot)) ++Count;
    for (FCacheSlot& Slot : CacheSlots)
        if (!Slot.Pickup.IsValid() && SpawnCache(Slot)) ++Count;
    return Count;
}

void ADownstreamGameMode::RegisterFieldEnemy(ADownstreamEnemy* Enemy)
{
    if (!IsValid(Enemy)) return;
    if (!bFieldRegistered) RegisterField();
    FEnemySlot Slot;
    Slot.Enemy = Enemy;
    Slot.Spawn = Enemy->GetActorTransform();
    Slot.Kind = Enemy->Kind;
    EnemySlots.Add(Slot);
}

void ADownstreamGameMode::RegisterFieldCache(ADownstreamSalvagePickup* Pickup)
{
    if (!IsValid(Pickup)) return;
    if (!bFieldRegistered) RegisterField();
    FCacheSlot Slot;
    Slot.Pickup = Pickup;
    Slot.Location = Pickup->GetActorLocation();
    Slot.Amount = Pickup->Amount;
    CacheSlots.Add(Slot);
}
