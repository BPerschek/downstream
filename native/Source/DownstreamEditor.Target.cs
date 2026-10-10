using UnrealBuildTool;
using System.Collections.Generic;

public class DownstreamEditorTarget : TargetRules
{
    public DownstreamEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.AddRange(new string[] { "Downstream", "DownstreamSim", "DownstreamColony", "DownstreamCombat", "DownstreamRender" });
    }
}
