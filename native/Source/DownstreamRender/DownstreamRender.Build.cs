using UnrealBuildTool;

public class DownstreamRender : ModuleRules
{
    public DownstreamRender(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "Downstream", "DownstreamSim", "DownstreamColony", "DownstreamCombat", "ProceduralMeshComponent"
        });
        PrivateDependencyModuleNames.AddRange(new string[] { "InputCore", "Slate", "SlateCore" });
    }
}
