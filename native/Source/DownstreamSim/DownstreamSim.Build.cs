using UnrealBuildTool;

public class DownstreamSim : ModuleRules
{
    public DownstreamSim(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        bEnableExceptions = true;
        // Core is needed only by the module registration wrapper, not simulation.
        PrivateDependencyModuleNames.Add("Core");
    }
}
