#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DownstreamRaider.h"
#include "DownstreamOutpost.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UBoxComponent;
class UPointLightComponent;

// One Dam Cult structure. Integrity lives in the water subsystem (flooding and gunfire);
// this actor shows it: tilt and sink as it weakens, a FLOODING warning, collapse.
UCLASS()
class DOWNSTREAMCOMBAT_API ADownstreamOutpostStructure : public AActor
{
    GENERATED_BODY()
public:
    ADownstreamOutpostStructure();
    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
    void Initialize(int32 InOutpost, int32 InStructure);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Outpost") TObjectPtr<UBoxComponent> Collision;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Outpost") TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Outpost") TObjectPtr<UTextRenderComponent> Label;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Outpost") int32 OutpostIndex = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Outpost") int32 StructureIndex = -1;

private:
    void Refresh();
    FVector BaseLocation = FVector::ZeroVector;
    float MeshBaseZ = 0.0f;
    bool bShownDestroyed = false;
    bool bInitialized = false;
};

// A Dam Cult outpost: spawns its structures and garrison, respawns raiders while the
// generator stands, and raises the alarm when water or the digger threatens the levee.
UCLASS()
class DOWNSTREAMCOMBAT_API ADownstreamOutpost : public AActor
{
    GENERATED_BODY()
public:
    ADownstreamOutpost();
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Outpost") int32 OutpostIndex = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Outpost") TObjectPtr<UTextRenderComponent> Banner;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Outpost") TObjectPtr<UPointLightComponent> AlarmLight;
    UFUNCTION(BlueprintPure, Category="Outpost") bool IsAlerted() const { return bAlerted; }
    UFUNCTION(BlueprintPure, Category="Outpost") int32 GetLivingRaiders() const;
    UFUNCTION(BlueprintPure, Category="Outpost") int32 GetLivingEngineers() const;
    UFUNCTION(BlueprintPure, Category="Outpost") TArray<ADownstreamRaider*> GetRaiders() const;
    UFUNCTION(BlueprintCallable, Category="Outpost") int32 RespawnGarrisonNow();
    UFUNCTION(BlueprintPure, Category="Outpost") FText GetStatusText() const;

protected:
    virtual void BeginPlay() override;

private:
    struct FSlot
    {
        FVector Location = FVector::ZeroVector;
        EDownstreamRaiderRole Role = EDownstreamRaiderRole::Guard;
        TWeakObjectPtr<ADownstreamRaider> Raider;
        double DownSince = -1.0;
    };
    bool SpawnRaider(FSlot& Slot);
    void UpdateBanner();
    TArray<FSlot> Slots;
    UPROPERTY(Transient) TArray<TObjectPtr<ADownstreamOutpostStructure>> Structures;
    bool bAlerted = false;
    bool bWasBreached = false;
    double NextThink = 0.0;
    double NextAlarm = 0.0;
};
