using UnrealBuildTool;
using System;
using System.IO;
public class OneClickPilot : ModuleRules {
    public OneClickPilot(ReadOnlyTargetRules Target) : base(Target) {
        PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard=CppStandardVersion.Cpp20;
        bEnableExceptions=true;
        FPSemantics=FPSemanticsMode.Precise;
        bUseUnity=false;
        PublicDependencyModuleNames.AddRange(new [] {"Core","CoreUObject","Engine"});
        PrivateDependencyModuleNames.Add("ProceduralMeshComponent");
        string Root=Path.GetFullPath(Path.Combine(ModuleDirectory,"../.."));
        PrivateIncludePaths.Add(Path.Combine(Root,"FlightDynamics"));
        if(Target.Platform==UnrealTargetPlatform.IOS) {
            string Slice=Target.Architecture==UnrealArch.IOSSimulator?"Simulator":"IOS";
            string Archive=Path.Combine(Root,"ThirdParty",Slice,"OneClickFlightDynamics.embeddedframework.zip");
            if(!File.Exists(Archive)) throw new BuildException("Run build_unreal_jsbsim.py before building OneClickPilot.");
            PublicAdditionalFrameworks.Add(new Framework("OneClickFlightDynamics",Archive,null,true));
        } else if(Target.Platform!=UnrealTargetPlatform.Mac) {
            throw new BuildException("OneClickPilot targets iOS. Mac is used only for Unreal Editor development.");
        }
    }
}
