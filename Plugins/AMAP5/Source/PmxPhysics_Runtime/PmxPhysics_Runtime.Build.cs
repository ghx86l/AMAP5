using UnrealBuildTool;
using System.IO;

public class PmxPhysics_Runtime : ModuleRules
{
    public PmxPhysics_Runtime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(new string[]
        {
            ModuleDirectory,
        });

        PrivateIncludePaths.AddRange(new string[]
        {
            ModuleDirectory,
        });

        string BulletSrc = Path.Combine(ModuleDirectory, "../ThirdParty/BulletPhysicsEngineLibrary/src");
        string BulletLib = Path.Combine(ModuleDirectory, "../ThirdParty/BulletPhysicsEngineLibrary/lib/Release");

        PublicIncludePaths.Add(BulletSrc);
        PublicAdditionalLibraries.Add(Path.Combine(BulletLib, "BulletDynamics.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(BulletLib, "BulletCollision.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(BulletLib, "LinearMath.lib"));

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "AnimGraphRuntime",
            "Json",
            "JsonUtilities",
            "MovieScene",
            "MovieSceneTracks",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AnimationCore",
        });
    }
}
