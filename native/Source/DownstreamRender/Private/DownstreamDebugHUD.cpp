#include "DownstreamDebugHUD.h"

#include "DownstreamDigger.h"
#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamMenu.h"
#include "DownstreamPlayerCharacter.h"
#include "DownstreamSalvagePickup.h"
#include "DownstreamSessionSubsystem.h"
#include "DownstreamSettlementManager.h"
#include "DownstreamSimulationSubsystem.h"
#include "DownstreamTerrainChunk.h"
#include "DownstreamWaterSubsystem.h"
#include "Components/InputComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

void ADownstreamDebugHUD::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Session = UDownstreamSessionSubsystem::Get(this)) Session->ApplyRuntimeOptions(this);
    BindPauseKey();
}

void ADownstreamDebugHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (PauseMenu.IsValid() && GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(PauseMenu.ToSharedRef());
    PauseMenu.Reset();
    Super::EndPlay(EndPlayReason);
}

void ADownstreamDebugHUD::BindPauseKey()
{
    APlayerController* Controller = GetOwningPlayerController();
    if (bPauseBound || !Controller || !Controller->IsLocalController()) return;
    EnableInput(Controller);
    if (!InputComponent) return;
    FInputKeyBinding& Binding = InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ADownstreamDebugHUD::TogglePauseMenu);
    Binding.bExecuteWhenPaused = true;
    bPauseBound = true;
}

void ADownstreamDebugHUD::TogglePauseMenu()
{
    if (PauseMenu.IsValid()) { ClosePauseMenu(); return; }
    APlayerController* Controller = GetOwningPlayerController();
    if (!Controller || !GEngine || !GEngine->GameViewport) return;
    SAssignNew(PauseMenu, SDownstreamMenu).bPauseMenu(true).World(GetWorld())
        .OnResume(FSimpleDelegate::CreateUObject(this, &ADownstreamDebugHUD::ClosePauseMenu));
    GEngine->GameViewport->AddViewportWidgetContent(PauseMenu.ToSharedRef(), 20);
    UGameplayStatics::SetGamePaused(this, true);
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(PauseMenu);
    Controller->SetInputMode(Mode);
    Controller->bShowMouseCursor = true;
}

void ADownstreamDebugHUD::ClosePauseMenu()
{
    if (PauseMenu.IsValid() && GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(PauseMenu.ToSharedRef());
    PauseMenu.Reset();
    UGameplayStatics::SetGamePaused(this, false);
    if (APlayerController* Controller = GetOwningPlayerController())
    {
        FInputModeGameOnly Mode;
        Mode.SetConsumeCaptureMouseDown(false);
        Controller->SetInputMode(Mode);
        Controller->bShowMouseCursor = true;
    }
    if (auto* Session = UDownstreamSessionSubsystem::Get(this)) Session->SaveOptions();
}

void ADownstreamDebugHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!bPauseBound) BindPauseKey();
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
    const FLinearColor Panel(0.02f, 0.03f, 0.025f, 0.82f);
    const FLinearColor Ink(0.94f, 0.94f, 0.89f, 1.0f);
    const FLinearColor Muted(0.70f, 0.76f, 0.73f, 1.0f);
    const FLinearColor Salvage(0.98f, 0.73f, 0.32f, 1.0f);
    const FLinearColor Healthy(0.40f, 0.72f, 0.57f, 1.0f);
    const FLinearColor Wounded(0.90f, 0.24f, 0.20f, 1.0f);
    const FLinearColor WaterBlue(0.45f, 0.75f, 1.0f, 1.0f);
    const FLinearColor Cult(1.0f, 0.45f, 0.30f, 1.0f);

    const auto Box = [&](float X, float Y, float W, float H, const FLinearColor& Color)
    {
        DrawRect(Color, X * UIScale, Y * UIScale, W * UIScale, H * UIScale);
    };
    const auto Label = [&](const FString& Text, float X, float Y, float Pixels, const FLinearColor& Color)
    {
        DrawText(Text, Color, X * UIScale, Y * UIScale, Font, Pixels * UIScale / FontReferenceHeight);
    };
    const auto Bar = [&](float X, float Y, float W, float Fraction, const FLinearColor& Color)
    {
        Box(X, Y, W, 5.0f, FLinearColor(0.16f, 0.20f, 0.17f, 1.0f));
        Box(X, Y, W * FMath::Clamp(Fraction, 0.0f, 1.0f), 5.0f, Color);
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
    const UDownstreamWaterSubsystem* Water = GetWorld()->GetSubsystem<UDownstreamWaterSubsystem>();
    const auto* Session = UDownstreamSessionSubsystem::Get(this);
    const bool bHints = !Session || Session->GetShowHints();
    const float MaxHealth = Player ? FMath::Max(Player->GetMaxHealth(), 1.0f) : 100.0f;
    const float Health = Player ? FMath::Clamp(Player->GetHealth(), 0.0f, MaxHealth) : MaxHealth;
    const bool bDead = Player && Player->IsDead();
    const int32 Carried = Expedition ? Expedition->GetCarriedSalvage() : 0;
    const int32 Capacity = Expedition ? Expedition->GetInventoryCapacity() : 12;
    const int32 Banked = Expedition ? Expedition->GetBankedSalvage() : 0;
    const FName Skill = Expedition ? Expedition->GetSelectedSkillId() : NAME_None;
    const bool bBuilder = Player && Player->IsBuildMenuOpen();
    const bool bPlanner = Player && Player->IsDigPlannerOpen();
    const bool bAtHome = Pawn && Expedition && Expedition->IsAtHome(Pawn->GetActorLocation());
    const auto* Manager = ADownstreamSettlementManager::GetForWorld(GetWorld());

    // ---- Top-left: vitals (always), compact.
    {
        const float PH = Skill.IsNone() ? 150.0f : 104.0f;
        Box(14.0f, 14.0f, 330.0f, PH, Panel);
        const FString Mode = Expedition && Expedition->IsWorldMode() ? FString::Printf(TEXT("FRONTIER #%lld"), Expedition->GetWorldSeed()) : FString(TEXT("TRAINING GROUNDS"));
        Label(Mode, 26.0f, 22.0f, 13.0f, Muted);
        Label(FString::Printf(TEXT("Health %.0f"), Health), 26.0f, 40.0f, 17.0f, Health <= MaxHealth * 0.3f ? Wounded : Ink);
        Bar(130.0f, 49.0f, 200.0f, Health / MaxHealth, Health <= MaxHealth * 0.3f ? Wounded : Healthy);
        Label(FString::Printf(TEXT("Carried %d/%d"), Carried, Capacity), 26.0f, 64.0f, 17.0f, Salvage);
        Label(FString::Printf(TEXT("Banked %d"), Banked), 200.0f, 64.0f, 17.0f, Ink);
        if (Skill.IsNone())
        {
            Label(TEXT("Pick a free starter node:"), 26.0f, 92.0f, 14.0f, Ink);
            Label(TEXT("[1] +15% ranged    [2] +4 pack slots"), 26.0f, 114.0f, 14.0f, Salvage);
        }
        else
        {
            Label(Skill == FName(TEXT("steady_aim")) ? TEXT("Steady aim (+15% ranged)") : TEXT("Salvage handling (+4 slots)"), 26.0f, 90.0f, 13.0f, Muted);
        }
    }

    // ---- Left: builder (only when open or at home).
    if (Player && Expedition && (bBuilder || bAtHome))
    {
        const float Y0 = Skill.IsNone() ? 176.0f : 130.0f;
        Box(14.0f, Y0, 330.0f, 172.0f, Panel);
        Label(bBuilder ? TEXT("HOME BUILDER") : TEXT("HOME  /  B to build"), 26.0f, Y0 + 8.0f, 15.0f, Ink);
        const auto StructureRow = [&](EDownstreamStructureType Type, const TCHAR* Name, int32 Key, float Y)
        {
            const bool bBuilt = Expedition->HasStructure(Type);
            const int32 Cost = Expedition->GetStructureCost(Type);
            Label(bBuilt ? FString::Printf(TEXT("[%d] %s  built"), Key, Name) : FString::Printf(TEXT("[%d] %s  %d"), Key, Name, Cost),
                26.0f, Y, 14.0f, bBuilt ? Healthy : (Banked >= Cost ? Salvage : Muted));
        };
        StructureRow(EDownstreamStructureType::Storage, TEXT("Storage +8 pack"), 1, Y0 + 32.0f);
        StructureRow(EDownstreamStructureType::Workshop, TEXT("Workshop"), 2, Y0 + 52.0f);
        StructureRow(EDownstreamStructureType::Charger, TEXT("Charger"), 3, Y0 + 72.0f);
        Label(Expedition->IsHaulerConstructed() ? TEXT("[4] Hauler  assembled") : FString::Printf(TEXT("[4] Hauler  %d (needs all 3)"), Expedition->GetHaulerCost()),
            26.0f, Y0 + 96.0f, 14.0f, Expedition->IsHaulerConstructed() ? Healthy : Salvage);
        Label(Expedition->IsDiggerBuilt() ? TEXT("[5] Digger  assembled") : FString::Printf(TEXT("[5] Digger  %d (needs workshop)"), Expedition->GetDiggerCost()),
            26.0f, Y0 + 116.0f, 14.0f, Expedition->IsDiggerBuilt() ? Healthy
            : (Banked >= Expedition->GetDiggerCost() && Expedition->HasStructure(EDownstreamStructureType::Workshop) ? Salvage : Muted));
        Label(bBuilder ? TEXT("LMB place | RMB cancel | B close") : TEXT("E banks carried salvage and heals"), 26.0f, Y0 + 144.0f, 13.0f, Muted);
    }

    // ---- Top-right: robots.
    float RightY = 14.0f;
    const float RightX = Width - 314.0f;
    if (Expedition && (Expedition->IsHaulerConstructed() || Expedition->IsDiggerBuilt()))
    {
        const bool bBoth = Expedition->IsHaulerConstructed() && Expedition->IsDiggerBuilt();
        const float PH = bBoth ? 104.0f : 60.0f;
        Box(RightX, RightY, 300.0f, PH, Panel);
        float Y = RightY + 8.0f;
        if (Expedition->IsHaulerConstructed())
        {
            Label(FString::Printf(TEXT("HAULER  %s"), Manager ? *Manager->GetHaulerStatus().ToString() : TEXT("...")), RightX + 12.0f, Y, 14.0f, Salvage);
            Label(FString::Printf(TEXT("Battery %.0f | Cargo %d/%d | J %s"), Expedition->GetHaulerBattery(), Expedition->GetHaulerCargo(), Expedition->GetHaulerCargoCapacity(),
                Expedition->IsHaulingEnabled() ? TEXT("pauses") : TEXT("enables")), RightX + 12.0f, Y + 20.0f, 13.0f, Muted);
            Y += 46.0f;
        }
        if (Expedition->IsDiggerBuilt())
        {
            const ADownstreamDigger* Digger = Manager ? Manager->GetDigger() : nullptr;
            const bool bWrecked = Digger && Digger->IsWrecked();
            Label(FString::Printf(TEXT("DIGGER  %s"), Manager ? *Manager->GetDiggerStatus().ToString() : TEXT("...")), RightX + 12.0f, Y, 14.0f, bWrecked ? Wounded : Salvage);
            if (Digger) Bar(RightX + 12.0f, Y + 24.0f, 276.0f, Digger->GetHealth() / FMath::Max(1.0f, Digger->GetMaxHealth()), bWrecked ? Wounded : Healthy);
        }
        RightY += PH + 10.0f;
    }

    // ---- Right: context (nearby outpost, else earthworks).
    int32 NearOutpost = -1;
    if (Water && Pawn) NearOutpost = Water->GetOutpostAt(Pawn->GetActorLocation(), 2500.0f);
    if (NearOutpost >= 0)
    {
        Box(RightX, RightY, 300.0f, 116.0f, Panel);
        const bool bRazed = Water->IsOutpostRazed(NearOutpost);
        Label(TEXT("DAM CULT OUTPOST"), RightX + 12.0f, RightY + 8.0f, 16.0f, bRazed ? Muted : Cult);
        const float Flood = Water->GetOutpostFloodDepth(NearOutpost);
        FIntPoint Breach;
        const bool bBreach = Water->GetOutpostBreach(NearOutpost, Breach);
        Label(bRazed ? TEXT("Razed. No more reinforcements.") : Flood > 0.25f ? FString::Printf(TEXT("FLOODED %.2f m: buildings failing"), Flood)
            : bBreach ? TEXT("Levee breached! Engineers are sandbagging") : TEXT("Levee holding"), RightX + 12.0f, RightY + 32.0f, 14.0f,
            bRazed ? Muted : Flood > 0.25f || bBreach ? WaterBlue : Ink);
        float Generator = 0.0f;
        for (int32 Index = 0; Index < Water->GetOutpostStructureCount(NearOutpost); ++Index)
            if (Water->GetOutpostStructureType(NearOutpost, Index) == 1)
                Generator = Water->GetOutpostStructureIntegrity(NearOutpost, Index) / FMath::Max(1.0f, Water->GetOutpostStructureMaxIntegrity(NearOutpost, Index));
        Label(FString::Printf(TEXT("Generator %d%% | %d/%d buildings standing"), FMath::RoundToInt(Generator * 100.0f),
            Water->GetOutpostStandingCount(NearOutpost), Water->GetOutpostStructureCount(NearOutpost)), RightX + 12.0f, RightY + 54.0f, 13.0f, Muted);
        Bar(RightX + 12.0f, RightY + 74.0f, 276.0f, Generator, bRazed ? Muted : Cult);
        if (bHints) Label(TEXT("Cut a canal from the river through the levee"), RightX + 12.0f, RightY + 90.0f, 12.0f, Muted);
        RightY += 126.0f;
    }
    else if (Water && Water->IsSiteActive() && Expedition && (bPlanner || Expedition->IsDiggerBuilt() || (Pawn && Water->IsPlayerNearVault(const_cast<APawn*>(Pawn))) || !Expedition->IsWorldMode()))
    {
        Box(RightX, RightY, 300.0f, 104.0f, Panel);
        const int32 Vault = Pawn ? Water->GetVaultNear(const_cast<APawn*>(Pawn)) : -1;
        const int32 ShowVault = Vault >= 0 ? Vault : 0;
        Label(Expedition->IsWorldMode() ? TEXT("EARTHWORKS") : TEXT("DROWNED VAULT"), RightX + 12.0f, RightY + 8.0f, 15.0f, Ink);
        if (Water->GetVaultCount() > 0 && (Vault >= 0 || !Expedition->IsWorldMode()))
        {
            if (Water->IsVaultOpenedAt(ShowVault)) Label(TEXT("Vault opened"), RightX + 12.0f, RightY + 30.0f, 14.0f, Healthy);
            else if (Water->IsVaultAccessibleAt(ShowVault)) Label(TEXT("Drained! Open the vault [E]"), RightX + 12.0f, RightY + 30.0f, 14.0f, Salvage);
            else Label(FString::Printf(TEXT("Vault under %.2f m of water"), Water->GetVaultDepthAt(ShowVault)), RightX + 12.0f, RightY + 30.0f, 14.0f, WaterBlue);
        }
        else Label(FString::Printf(TEXT("%d vault(s) somewhere on this frontier"), Water->GetVaultCount()), RightX + 12.0f, RightY + 30.0f, 14.0f, Muted);
        Label(FString::Printf(TEXT("Canals queued %d | done %d"), Water->GetQueuedCanalCount(), Water->GetCompletedCanalCount()), RightX + 12.0f, RightY + 52.0f, 13.0f, Muted);
        Label(bPlanner ? FString::Printf(TEXT("PLANNER: depth %.2f m (Shift+wheel)"), Water->GetCanalDepth())
            : FString::Printf(TEXT("G dig planner | depth %.2f m | F9 flow"), Water->GetCanalDepth()), RightX + 12.0f, RightY + 76.0f, 13.0f, bPlanner ? Salvage : Muted);
        RightY += 114.0f;
    }
    if (Player && Player->GetWaterDepth() > 0.15f)
    {
        Box((Width - 260.0f) * 0.5f, 14.0f, 260.0f, 30.0f, Panel);
        CenteredLabel(FString::Printf(TEXT("Wading %.1f m - slowed"), Player->GetWaterDepth()), 20.0f, 15.0f, WaterBlue);
    }

    // ---- Bottom: interaction prompt, mode hint, status, control footer.
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
        const int32 Vault = Water ? Water->GetVaultNear(const_cast<APawn*>(Pawn)) : -1;
        if (Vault >= 0 && !Water->IsVaultOpenedAt(Vault))
            Prompt = Water->IsVaultAccessibleAt(Vault) ? TEXT("E  Open the vault") : FString::Printf(TEXT("Vault hatch is %.1f m under water"), Water->GetVaultDepthAt(Vault));
        else if (Nearest)
            Prompt = Nearest->bRecoveryBag ? FString::Printf(TEXT("E  Recover %d salvage"), Nearest->Amount)
                                          : FString::Printf(TEXT("E  Collect %d salvage"), Nearest->Amount);
        else if (bAtHome)
            Prompt = Carried > 0 ? FString::Printf(TEXT("E  Extract %d salvage to your stash"), Carried)
                                 : TEXT("Home - bring salvage here to extract");
        else if (bNearbyOverCapacity || Carried >= Capacity)
            Prompt = TEXT("Not enough pack space - return home to extract");
        else if (Player && Player->GetTrenchDepth() > 0.5f && bHints)
            Prompt = TEXT("In a trench: Space scrambles up the bank");
    }
    if (bPlanner)
    {
        Prompt = FString::Printf(TEXT("Drag from the water to where it should drain | depth %.2f m (Shift+wheel)"), Water ? Water->GetCanalDepth() : 0.0f);
        FIntPoint Cursor;
        float StartBed = 0.0f, EndBed = 0.0f;
        if (Player->IsDraggingCanal() && Water && Player->GetCanalCursorCell(Cursor)
            && Water->GetCanalBedRange(Player->GetCanalDragStart(), Cursor, StartBed, EndBed))
            Prompt = FString::Printf(TEXT("Release to queue | bed %.2f m -> %.2f m | water drains toward the lower end"), StartBed, EndBed);
    }
    if (bBuilder)
        Prompt = Player->IsPlacementSelected() ? TEXT("Green footprint = valid | LMB place | RMB cancel")
            : TEXT("1 Storage | 2 Workshop | 3 Charger | 4 Hauler | 5 Digger | B close");
    if (!Prompt.IsEmpty())
    {
        Box((Width - 660.0f) * 0.5f, Height - (bHints ? 96.0f : 64.0f), 660.0f, 32.0f, Panel);
        CenteredLabel(Prompt, Height - (bHints ? 89.0f : 57.0f), 16.0f, bDead ? Wounded : Salvage);
    }

    const FString Status = Expedition ? Expedition->GetStatusMessage().ToString() : FString();
    const float Now = GetWorld()->GetTimeSeconds();
    if (Status != LastStatusMessage)
    {
        LastStatusMessage = Status;
        StatusMessageExpiresAt = Now + 6.0f;
    }
    if (!LastStatusMessage.IsEmpty() && Now < StatusMessageExpiresAt)
    {
        const float Y = Height - (bHints ? 132.0f : 100.0f);
        Box(140.0f, Y, Width - 280.0f, 28.0f, Panel);
        CenteredLabel(LastStatusMessage, Y + 6.0f, 14.0f, Ink);
    }
    if (bHints)
    {
        Box(14.0f, Height - 52.0f, Width - 28.0f, 36.0f, Panel);
        CenteredLabel(bBuilder ? TEXT("WASD move | 1-3 select | 4 hauler | 5 digger | LMB place | RMB cancel | B close | Esc menu")
                      : bPlanner ? TEXT("WASD move | LMB drag canal | Shift+wheel depth | RMB cancel | C clear queue | G close | F9 flow")
                      : TEXT("WASD move | LMB shoot | RMB melee | Space dodge | E interact | B build | J hauling | G dig | Esc menu"),
                      Height - 41.0f, 13.0f, Muted);
    }

    // ---- M0 terrain station (technical map only).
    if (!ObservedChunk.IsValid())
    {
        for (TActorIterator<ADownstreamTerrainChunk> It(GetWorld()); It; ++It)
        {
            ObservedChunk = *It;
            break;
        }
    }
    const ADownstreamTerrainChunk* Chunk = ObservedChunk.Get();
    if (!Chunk) return;
    const UDownstreamSimulationSubsystem* Subsystem = GetWorld()->GetSubsystem<UDownstreamSimulationSubsystem>();
    const downstream::Simulation* Simulation = Subsystem ? Subsystem->GetSimulation() : nullptr;
    const float DebugX = Width - 268.0f;
    const float DebugY = FMath::Max(16.0f, RightY);
    Box(DebugX, DebugY, 252.0f, 112.0f, Panel);
    Label(TEXT("Terrain station"), DebugX + 12.0f, DebugY + 12.0f, 14.0f, Muted);
    Label(FString::Printf(TEXT("Tick %lld | Sim %.1f s"),
          static_cast<long long>(Subsystem ? Subsystem->GetSimulationTick() : 0), Simulation ? Simulation->ElapsedSeconds() : 0.0),
          DebugX + 12.0f, DebugY + 36.0f, 14.0f, Muted);
    Label(FString::Printf(TEXT("Rebuilds %d | Height %.2f"), Chunk->GetRebuildCount(), Chunk->GetCellHeight()),
          DebugX + 12.0f, DebugY + 60.0f, 14.0f, Muted);
    Label(TEXT("F6 view  F7 raise  F8 lower"), DebugX + 12.0f, DebugY + 86.0f, 13.0f, Muted);
}
