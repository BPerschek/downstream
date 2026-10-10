using UnrealBuildTool;

public class DownstreamCombat : ModuleRules
{
    public DownstreamCombat(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "Downstream", "DownstreamSim", "DownstreamColony",
            "GameplayAbilities", "GameplayTags", "GameplayTasks"
        });
        PrivateDependencyModuleNames.AddRange(new string[] { "InputCore", "AIModule", "NavigationSystem" });
    }
}
