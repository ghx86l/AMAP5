using UnrealBuildTool;

public class PmxPhysics_Editor : ModuleRules
{
    public PmxPhysics_Editor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "PmxPhysics_Runtime",
            "AnimGraph",
            "AnimGraphRuntime",
            "AnimationEditMode",
            "BlueprintGraph",
            "EditorFramework",
            "UnrealEd",
            "Slate",
            "SlateCore",
            "PropertyEditor",
            "EditorStyle",
            "AnimationBlueprintLibrary",
            "Kismet",
            "KismetCompiler",
            "ToolMenus",
            "AssetTools",
            "RenderCore",
            "RHI",
            "InterchangeCore"
        });
    }
}
