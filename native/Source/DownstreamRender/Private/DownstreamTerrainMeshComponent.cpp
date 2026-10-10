#include "DownstreamTerrainMeshComponent.h"

#include "AI/NavigationSystemHelpers.h"

UDownstreamTerrainMeshComponent::UDownstreamTerrainMeshComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bHasCustomNavigableGeometry = EHasCustomNavigableGeometry::Yes;
    SetCanEverAffectNavigation(true);
}

void UDownstreamTerrainMeshComponent::SetNavigationGeometry(TArray<FVector> InVertices, TArray<int32> InIndices)
{
    NavVertices = MoveTemp(InVertices);
    NavIndices = MoveTemp(InIndices);
}

bool UDownstreamTerrainMeshComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
    if (NavVertices.Num() > 0 && NavIndices.Num() >= 3)
        GeomExport.ExportCustomMesh(NavVertices.GetData(), NavVertices.Num(), NavIndices.GetData(), NavIndices.Num(), GetComponentTransform());
    // false: do not additionally export the (unreliable) procedural collision.
    return false;
}
