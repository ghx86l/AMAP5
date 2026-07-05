using UnrealBuildTool;

public class AMAP5_Runtime : ModuleRules
{
    public AMAP5_Runtime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.Add(ModuleDirectory);

        PublicDependencyModuleNames.AddRange(
            new string[] { "Core", "CoreUObject", "Engine", "AnimGraphRuntime", "InterchangeCore" }
        );

        PrivateDependencyModuleNames.AddRange(new string[] { });
    }
}
