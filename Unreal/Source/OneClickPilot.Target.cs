using UnrealBuildTool;
using System.Collections.Generic;
public class OneClickPilotTarget : TargetRules {
    public OneClickPilotTarget(TargetInfo Target) : base(Target) {
        Type=TargetType.Game;
        DefaultBuildSettings=BuildSettingsVersion.V6;
        IncludeOrderVersion=EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("OneClickPilot");
    }
}
