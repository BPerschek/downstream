#include "DownstreamDebugHUD.h"

#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamPlayerCharacter.h"
#include "DownstreamSalvagePickup.h"
#include "DownstreamSimulationSubsystem.h"
#include "DownstreamTerrainChunk.h"
#include "DownstreamSettlementManager.h"
#include "DownstreamWaterSubsystem.h"
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
    Label(TEXT("Bank at home [E], then build [B]."), 30.0f, 177.0f, 17.0f, Muted);

    if (Skill.IsNone())
    {
        Box(16.0f, 232.0f, 440.0f, 112.0f, Panel);
        Label(Player && Player->IsBuildMenuOpen() ? TEXT("Starter skills: close builder to choose") : TEXT("Choose one free starter node"), 30.0f, 245.0f, 18.0f, Ink);
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

    const bool bBuilder = Player && Player->IsBuildMenuOpen();
    if (Player && Expedition)
    {
        Box(16.0f, 356.0f, 440.0f, 196.0f, Panel);
        Label(bBuilder ? TEXT("HOME BUILDER") : TEXT("Settlement / B to build at home"), 30.0f, 368.0f, 19.0f, Ink);
        const auto StructureRow = [&](EDownstreamStructureType Type, const TCHAR* Name, int32 Key, float Y)
        {
            const bool bBuilt = Expedition->HasStructure(Type);
            const int32 Cost = Expedition->GetStructureCost(Type);
            const FString Text = bBuilt ? FString::Printf(TEXT("[%d] %s  /  built"), Key, Name)
                : FString::Printf(TEXT("[%d] %s  /  %d salvage"), Key, Name, Cost);
            Label(Text, 30.0f, Y, 17.0f, bBuilt ? Healthy : (Banked >= Cost ? Salvage : Muted));
        };
        StructureRow(EDownstreamStructureType::Storage, TEXT("Storage (+8 pack)"), 1, 401.0f);
        StructureRow(EDownstreamStructureType::Workshop, TEXT("Workshop"), 2, 426.0f);
        StructureRow(EDownstreamStructureType::Charger, TEXT("Charger"), 3, 451.0f);
        Label(Expedition->IsHaulerConstructed() ? TEXT("[4] Hauler / assembled")
            : FString::Printf(TEXT("[4] Assemble hauler / %d salvage"), Expedition->GetHaulerCost()),
            30.0f, 476.0f, 17.0f, Expedition->IsHaulerConstructed() ? Healthy : Salvage);
        Label(Expedition->IsDiggerBuilt() ? TEXT("[5] Digger / assembled")
            : FString::Printf(TEXT("[5] Assemble digger / %d salvage (needs workshop)"), Expedition->GetDiggerCost()),
            30.0f, 501.0f, 17.0f, Expedition->IsDiggerBuilt() ? Healthy : (Banked >= Expedition->GetDiggerCost() && Expedition->HasStructure(EDownstreamStructureType::Workshop) ? Salvage : Muted));
        Label(bBuilder ? TEXT("1-3 select | LMB place | RMB cancel | 4-5 robots") : TEXT("Build, then assemble a hauler and a digger."),
              30.0f, 528.0f, 14.0f, Muted);

        const float BotX = Width - 308.0f;
        Box(BotX, 144.0f, 292.0f, 140.0f, Panel);
        Label(TEXT("Salvage hauler"), BotX + 12.0f, 157.0f, 18.0f, Ink);
        if (Expedition->IsHaulerConstructed())
        {
            const auto* Manager = ADownstreamSettlementManager::GetForWorld(GetWorld());
            Label(Manager ? Manager->GetHaulerStatus().ToString() : TEXT("Starting..."), BotX + 12.0f, 189.0f, 16.0f, Salvage);
            Label(FString::Printf(TEXT("Battery %.0f | Cargo %d / %d"), Expedition->GetHaulerBattery(),
                Expedition->GetHaulerCargo(), Expedition->GetHaulerCargoCapacity()), BotX + 12.0f, 216.0f, 15.0f, Ink);
            Label(Expedition->IsHaulingEnabled() ? TEXT("J pauses hauling") : TEXT("J enables hauling"), BotX + 12.0f, 250.0f, 15.0f, Muted);
        }
        else
        {
            Label(TEXT("Assemble at the home workshop."), BotX + 12.0f, 194.0f, 15.0f, Muted);
            Label(TEXT("Hauls salvage from cleared areas."), BotX + 12.0f, 224.0f, 15.0f, Muted);
        }
    }
    const UDownstreamWaterSubsystem* Water = GetWorld()->GetSubsystem<UDownstreamWaterSubsystem>();
    const bool bPlanner = Player && Player->IsDigPlannerOpen();
    if (Water && Water->IsSiteActive() && Expedition)
    {
        const float SiteX = Width - 308.0f;
        Box(SiteX, 296.0f, 292.0f, 176.0f, Panel);
        Label(TEXT("Drowned Vault"), SiteX + 12.0f, 309.0f, 18.0f, Ink);
        const FLinearColor WaterBlue(0.45f, 0.75f, 1.0f, 1.0f);
        if (Expedition->IsVaultOpened())
            Label(TEXT("Vault opened"), SiteX + 12.0f, 338.0f, 16.0f, Healthy);
        else if (Water->IsVaultAccessible())
            Label(TEXT("Drained! Open the vault [E]"), SiteX + 12.0f, 338.0f, 16.0f, Salvage);
        else
            Label(FString::Printf(TEXT("Vault under %.2f m of water"), Water->GetVaultDepth()), SiteX + 12.0f, 338.0f, 16.0f, WaterBlue);
        const auto* Manager = ADownstreamSettlementManager::GetForWorld(GetWorld());
        Label(Expedition->IsDiggerBuilt() ? FString::Printf(TEXT("Digger: %s"), Manager ? *Manager->GetDiggerStatus().ToString() : TEXT("..."))
            : TEXT("Digger: not assembled (B, 5)"), SiteX + 12.0f, 366.0f, 14.0f, Expedition->IsDiggerBuilt() ? Ink : Muted);
        Label(FString::Printf(TEXT("Canals queued %d | done %d"), Water->GetQueuedCanalCount(), Water->GetCompletedCanalCount()),
            SiteX + 12.0f, 392.0f, 14.0f, Muted);
        Label(FString::Printf(TEXT("Water %.0f m3 | drained %.0f m3"), Water->GetTotalWater(), Water->GetDrainedVolume()),
            SiteX + 12.0f, 416.0f, 14.0f, Muted);
        Label(bPlanner ? TEXT("PLANNER: drag LMB | RMB cancel | G close") : TEXT("G dig planner | C clear queue | F9 flow"),
            SiteX + 12.0f, 444.0f, 14.0f, bPlanner ? Salvage : Muted);
        if (Player && Player->GetWaterDepth() > 0.15f)
        {
            Box((Width - 260.0f) * 0.5f, 16.0f, 260.0f, 34.0f, Panel);
            CenteredLabel(FString::Printf(TEXT("Wading %.1f m - slowed"), Player->GetWaterDepth()), 24.0f, 16.0f, WaterBlue);
        }
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
        if (Water && Water->IsPlayerNearVault(const_cast<APawn*>(Pawn)) && !Expedition->IsVaultOpened())
            Prompt = Water->IsVaultAccessible() ? TEXT("E  Open the vault") : FString::Printf(TEXT("Vault hatch is %.1f m under water"), Water->GetVaultDepth());
        else if (Nearest)
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
    if (bPlanner)
    {
        Box((Width - 640.0f) * 0.5f, Height - 110.0f, 640.0f, 38.0f, Panel);
        CenteredLabel(Player->IsDraggingCanal() ? TEXT("Release to queue. Canal is graded from start height to end height.")
            : TEXT("Drag from the water to where it should drain (try the old culvert)"), Height - 100.0f, 17.0f, Salvage);
    }
    if (bBuilder)
    {
        Box((Width - 640.0f) * 0.5f, Height - 110.0f, 640.0f, 38.0f, Panel);
        CenteredLabel(Player->IsPlacementSelected() ? TEXT("Green footprint = valid | LMB place | RMB cancel")
            : TEXT("1 Storage | 2 Workshop | 3 Charger | 4 Hauler | 5 Digger | B close"), Height - 100.0f, 17.0f, Salvage);
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
    CenteredLabel(bBuilder ? TEXT("WASD move | 1-3 select | 4 hauler | 5 digger | LMB place | RMB cancel | B close")
                  : bPlanner ? TEXT("WASD move | LMB drag canal | RMB cancel | C clear queue | G close planner | F9 water flow")
                  : TEXT("WASD move | Mouse aim | LMB shoot | RMB melee | Space dodge | E interact | B build | J hauling | G dig"),
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
    if (!Chunk) return;
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
