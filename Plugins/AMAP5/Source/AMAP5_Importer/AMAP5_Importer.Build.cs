using UnrealBuildTool;
using System.IO;

public class AMAP5_Importer : ModuleRules
{
    public AMAP5_Importer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.Add(ModuleDirectory);

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings",
            "InterchangeCore",
            "InterchangeFactoryNodes",
            "InterchangeCommonParser",
            "PhysicsCore",
            "MeshDescription",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd",
            "RenderCore",
            "InterchangeNodes",
            "InterchangeEngine",
            "InterchangeEditor",
            "InterchangeImport",
            "InterchangePipelines",
            "StaticMeshDescription",
            "SkeletalMeshDescription",
            "AnimationCore",
            "AssetRegistry",
            "ImageCore",
            "ImageWrapper",
            "KismetCompiler",
            "BlueprintGraph",
            "SkeletalMeshModifiers",
            "Slate",
            "SlateCore",
            "PropertyEditor",
        });

        PublicIncludePaths.AddRange(new string[]
        {
            Path.Combine(EngineDirectory, "Plugins/Interchange/Runtime/Source/FactoryNodes/Public"),
            Path.Combine(EngineDirectory, "Plugins/Interchange/Runtime/Source/Nodes/Public"),
            Path.Combine(EngineDirectory, "Plugins/Interchange/Runtime/Source/Import/Public"),
            Path.Combine(EngineDirectory, "Plugins/Interchange/Runtime/Source/Import/Public/Mesh"),
            Path.Combine(EngineDirectory, "Plugins/Interchange/Runtime/Source/Pipelines/Public")
        });
    }
}
