#include "DownstreamGameMode.h"
#include "DownstreamPlayerCharacter.h"
#include "DownstreamPlayerController.h"
#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamPrototypeTuning.h"
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
    const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld());
    RestartPlayerAtTransform(NewPlayer, FTransform(FRotator::ZeroRotator, Tuning->HomeLocation));
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
