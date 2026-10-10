#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DownstreamWorldDirector.generated.h"

class ADownstreamOutpost;

// Placed once in M2_World. When play starts it asks the water subsystem for the seeded
// world and spawns everything the layout describes: home and settlement, Dam Cult
// outposts, feral patrols, salvage caches and ruin props. Nothing here is saved; the
// layout is reproduced from the seed and the save holds only what the player changed.
UCLASS()
class DOWNSTREAMCOMBAT_API ADownstreamWorldDirector : public AActor
{
    GENERATED_BODY()
public:
    ADownstreamWorldDirector();
    UFUNCTION(BlueprintPure, Category="Downstream|World") bool IsPopulated() const { return bPopulated; }
    UFUNCTION(BlueprintPure, Category="Downstream|World") TArray<ADownstreamOutpost*> GetOutposts() const;
    UFUNCTION(BlueprintPure, Category="Downstream|World") int32 GetSpawnedProps() const { return SpawnedProps; }

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    void Populate();
    void SpawnProp(const FIntPoint& Cell, int32 Type, float Yaw);
    UPROPERTY(Transient) TArray<TObjectPtr<ADownstreamOutpost>> Outposts;
    bool bPopulated = false;
    int32 SpawnedProps = 0;
};
