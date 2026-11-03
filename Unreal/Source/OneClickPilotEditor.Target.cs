using UnrealBuildTool;
using System.Collections.Generic;
public class OneClickPilotEditorTarget : TargetRules {
    public OneClickPilotEditorTarget(TargetInfo Target) : base(Target) {
        Type=TargetType.Editor;
        DefaultBuildSettings=BuildSettingsVersion.V6;
        IncludeOrderVersion=EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("OneClickPilot");
    }
}
