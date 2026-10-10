#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "DownstreamTerrainMeshComponent.generated.h"

// Procedural terrain that hands its own triangles to navigation. Recast does not
// pick up this build's procedural collision on its own, so the water site exports
// the same triangles it renders and collides with.
UCLASS(ClassGroup=(Downstream), meta=(BlueprintSpawnableComponent))
class DOWNSTREAMRENDER_API UDownstreamTerrainMeshComponent : public UProceduralMeshComponent
{
    GENERATED_BODY()
public:
    UDownstreamTerrainMeshComponent(const FObjectInitializer& ObjectInitializer);
    virtual bool DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const override;
    void SetNavigationGeometry(TArray<FVector> InVertices, TArray<int32> InIndices);
    int32 GetNavigationTriangleCount() const { return NavIndices.Num() / 3; }

private:
    TArray<FVector> NavVertices;
    TArray<int32> NavIndices;
};
