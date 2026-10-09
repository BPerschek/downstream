#include "DownstreamDebugHUD.h"

#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamPlayerCharacter.h"
#include "DownstreamSalvagePickup.h"
#include "DownstreamSimulationSubsystem.h"
#include "DownstreamTerrainChunk.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

void ADownstreamDebugHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas || !GEngine || !GetWorld() || Canvas->SizeX <= 0 || Canvas->SizeY <= 0)
        return;

    UFont* Font = GEngine->GetSmallFont();
    if (!Font)
        return;
    float FontWidth = 0.0f, FontHeight = 0.0f;
    GetTextSize(TEXT("Hg"), FontWidth, FontHeight, Font, 1.0f);
    const float FontReferenceHeight = FMath::Max(FontHeight, 1.0f);
    const float UIScale = FMath::Clamp(FMath::Min(Canvas->SizeX / 1280.0f, Canvas->SizeY / 720.0f), 0.6f, 2.0f);
    const float Width = Canvas->SizeX / UIScale;
    const float Height = Canvas->SizeY / UIScale;
    const FLinearColor Panel(0.02f, 0.03f, 0.025f, 0.92f);
    const FLinearColor Ink(0.94f, 0.94f, 0.89f, 1.0f);
    const FLinearColor Muted(0.70f, 0.76f, 0.73f, 1.0f);
    const FLinearColor Salvage(0.98f, 0.73f, 0.32f, 1.0f);
    const FLinearColor Healthy(0.40f, 0.72f, 0.57f, 1.0f);
    const FLinearColor Wounded(0.90f, 0.24f, 0.20f, 1.0f);

    const auto Box = [&](float X, float Y, float W, float H, const FLinearColor& Color)
    {
        DrawRect(Color, X * UIScale, Y * UIScale, W * UIScale, H * UIScale);
    };
    const auto Label = [&](const FString& Text, float X, float Y, float Pixels, const FLinearColor& Color)
    {
        DrawText(Text, Color, X * UIScale, Y * UIScale, Font, Pixels * UIScale / FontReferenceHeight);
    };
    const auto CenteredLabel = [&](const FString& Text, float Y, float Pixels, const FLinearColor& Color)
    {
        float TextWidth = 0.0f, TextHeight = 0.0f;
        float TextScale = Pixels * UIScale / FontReferenceHeight;
        GetTextSize(Text, TextWidth, TextHeight, Font, TextScale);
        if (TextWidth > (Width - 64.0f) * UIScale)
        {
            TextScale *= (Width - 64.0f) * UIScale / TextWidth;
            GetTextSize(Text, TextWidth, TextHeight, Font, TextScale);
        }
        DrawText(Text, Color, (Canvas->SizeX - TextWidth) * 0.5f, Y * UIScale, Font, TextScale);
    };

    const APlayerController* Controller = GetOwningPlayerController();
    const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    const ADownstreamPlayerCharacter* Player = Cast<ADownstreamPlayerCharacter>(Pawn);
    const UDownstreamExpeditionSubsystem* Expedition = GetWorld()->GetSubsystem<UDownstreamExpeditionSubsystem>();
    const float MaxHealth = Player ? FMath::Max(Player->GetMaxHealth(), 1.0f) : 100.0f;
    const float Health = Player ? FMath::Clamp(Player->GetHealth(), 0.0f, MaxHealth) : MaxHealth;
    const bool bDead = Player && Player->IsDead();
    const int32 Carried = Expedition ? Expedition->GetCarriedSalvage() : 0;
    const int32 Capacity = Expedition ? Expedition->GetInventoryCapacity() : 12;
    const int32 Banked = Expedition ? Expedition->GetBankedSalvage() : 0;
    const FName Skill = Expedition ? Expedition->GetSelectedSkillId() : NAME_None;

    Box(16.0f, 16.0f, 440.0f, 204.0f, Panel);
    Label(TEXT("DOWNSTREAM / Expedition"), 30.0f, 28.0f, 24.0f, Ink);
    Label(FString::Printf(TEXT("Health %.0f / %.0f"), Health, MaxHealth), 30.0f, 66.0f, 20.0f,
          Health <= MaxHealth * 0.3f ? Wounded : Ink);
    Box(30.0f, 95.0f, 412.0f, 6.0f, FLinearColor(0.16f, 0.20f, 0.17f, 1.0f));
    Box(30.0f, 95.0f, 412.0f * Health / MaxHealth, 6.0f, Health <= MaxHealth * 0.3f ? Wounded : Healthy);
    Label(FString::Printf(TEXT("Carried %d / %d"), Carried, Capacity), 30.0f, 112.0f, 20.0f, Salvage);
    Label(FString::Printf(TEXT("Banked %d"), Banked), 256.0f, 112.0f, 20.0f, Ink);
    Label(TEXT("Explore the ruin, collect salvage,"), 30.0f, 152.0f, 17.0f, Muted);
    Label(TEXT("return home and press E."), 30.0f, 177.0f, 17.0f, Muted);

    if (Skill.IsNone())
    {
        Box(16.0f, 232.0f, 440.0f, 112.0f, Panel);
        Label(TEXT("Choose one free starter node"), 30.0f, 245.0f, 18.0f, Ink);
        Label(TEXT("[1] Steady aim (+15% ranged)"), 30.0f, 278.0f, 18.0f, Salvage);
        Label(TEXT("[2] Salvage handling (+4 slots)"), 30.0f, 310.0f, 18.0f, Salvage);
    }
    else
    {
        const bool bSteadyAim = Skill == FName(TEXT("steady_aim"));
        Box(16.0f, 232.0f, 440.0f, 68.0f, Panel);
        Label(TEXT("Starter node"), 30.0f, 244.0f, 14.0f, Muted);
        Label(bSteadyAim ? TEXT("Steady aim (+15% ranged)") : TEXT("Salvage handling (+4 slots)"),
              30.0f, 268.0f, 18.0f, Salvage);
    }

    // Keep the prompt consistent with TryInteract: nearest collectable pickup first,
    // then home extraction. Inventory and pickup ownership stay in the subsystem.
    FString Prompt;
    if (bDead)
    {
        Prompt = TEXT("Returning home...");
    }
    else if (Pawn && Expedition)
    {
        const ADownstreamSalvagePickup* Nearest = nullptr;
        float NearestDistance = FMath::Square(220.0f);
        bool bNearbyOverCapacity = false;
        for (TActorIterator<ADownstreamSalvagePickup> It(GetWorld()); It; ++It)
        {
            if (!IsValid(*It) || It->IsClaimed() || It->Amount <= 0)
                continue;
            const float Distance = FVector::DistSquared(Pawn->GetActorLocation(), It->GetActorLocation());
            if (Distance > FMath::Square(220.0f))
                continue;
            if (It->Amount > Capacity - Carried)
            {
                bNearbyOverCapacity = true;
                continue;
            }
            if (Distance <= NearestDistance)
            {
                Nearest = *It;
                NearestDistance = Distance;
            }
        }
        if (Nearest)
            Prompt = Nearest->bRecoveryBag ? FString::Printf(TEXT("E  Recover %d salvage"), Nearest->Amount)
                                          : FString::Printf(TEXT("E  Collect %d salvage"), Nearest->Amount);
        else if (Expedition->IsAtHome(Pawn->GetActorLocation()))
            Prompt = Carried > 0 ? FString::Printf(TEXT("E  Extract %d salvage to your stash"), Carried)
                                 : TEXT("Home - bring salvage here to extract");
        else if (bNearbyOverCapacity || Carried >= Capacity)
            Prompt = TEXT("Not enough pack space - return home to extract");
    }
    if (!Prompt.IsEmpty())
    {
        Box((Width - 640.0f) * 0.5f, Height - 110.0f, 640.0f, 38.0f, Panel);
        CenteredLabel(Prompt, Height - 100.0f, 18.0f, bDead ? Wounded : Salvage);
    }

    const FString Status = Expedition ? Expedition->GetStatusMessage().ToString() : FString();
    const float Now = GetWorld()->GetTimeSeconds();
    if (Status != LastStatusMessage)
    {
        LastStatusMessage = Status;
        StatusMessageExpiresAt = Now + 5.0f;
    }
    if (!LastStatusMessage.IsEmpty() && Now < StatusMessageExpiresAt)
    {
        Box(16.0f, Height - 150.0f, Width - 32.0f, 32.0f, Panel);
        CenteredLabel(LastStatusMessage, Height - 142.0f, 16.0f, Ink);
    }
    Box(16.0f, Height - 56.0f, Width - 32.0f, 40.0f, Panel);
    CenteredLabel(TEXT("WASD move | Mouse aim | LMB shoot | RMB melee | Space dodge | E collect/extract | F6 terrain view"),
                  Height - 44.0f, 14.0f, Muted);

    if (!ObservedChunk.IsValid())
    {
        for (TActorIterator<ADownstreamTerrainChunk> It(GetWorld()); It; ++It)
        {
            ObservedChunk = *It;
            break;
        }
    }
    const UDownstreamSimulationSubsystem* Subsystem = GetWorld()->GetSubsystem<UDownstreamSimulationSubsystem>();
    const downstream::Simulation* Simulation = Subsystem ? Subsystem->GetSimulation() : nullptr;
    const ADownstreamTerrainChunk* Chunk = ObservedChunk.Get();
    const float DebugX = Width - 268.0f;
    Box(DebugX, 16.0f, 252.0f, 112.0f, Panel);
    Label(TEXT("Terrain station"), DebugX + 12.0f, 28.0f, 14.0f, Muted);
    Label(FString::Printf(TEXT("Tick %lld | Sim %.1f s"),
          static_cast<long long>(Subsystem ? Subsystem->GetSimulationTick() : 0), Simulation ? Simulation->ElapsedSeconds() : 0.0),
          DebugX + 12.0f, 52.0f, 14.0f, Muted);
    Label(FString::Printf(TEXT("Rebuilds %d | Height %.2f"),
          Chunk ? Chunk->GetRebuildCount() : 0, Chunk ? Chunk->GetCellHeight() : 0.0f),
          DebugX + 12.0f, 76.0f, 14.0f, Muted);
    Label(TEXT("F6 view  F7 raise  F8 lower"), DebugX + 12.0f, 102.0f, 13.0f, Muted);
}
