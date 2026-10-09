using UnrealBuildTool;

public class Downstream : ModuleRules
{
    public Downstream(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bEnableExceptions = true;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DownstreamSim" });
        PrivateDependencyModuleNames.Add("Json");
    }
}
