using UnrealBuildTool;

public class DownstreamColony : ModuleRules
{
    public DownstreamColony(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "Downstream", "DownstreamSim" });
        PrivateDependencyModuleNames.AddRange(new[] { "AIModule", "NavigationSystem" });
    }
}
