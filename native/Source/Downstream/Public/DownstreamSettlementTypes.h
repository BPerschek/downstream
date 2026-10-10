#pragma once

#include "CoreMinimal.h"
#include "DownstreamSettlementTypes.generated.h"

UENUM(BlueprintType)
enum class EDownstreamStructureType : uint8
{
    Storage,
    Workshop,
    Charger
};

USTRUCT(BlueprintType)
struct DOWNSTREAM_API FDownstreamStructureRecord
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="Downstream|Settlement") EDownstreamStructureType Type = EDownstreamStructureType::Storage;
    UPROPERTY(BlueprintReadOnly, Category="Downstream|Settlement") FIntPoint GridCell = FIntPoint::ZeroValue;
    UPROPERTY(BlueprintReadOnly, Category="Downstream|Settlement") FVector WorldLocation = FVector::ZeroVector;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDownstreamSettlementChanged);
