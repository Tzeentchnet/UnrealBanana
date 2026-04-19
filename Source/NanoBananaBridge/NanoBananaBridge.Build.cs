using System.IO;
using UnrealBuildTool;

public class NanoBananaBridge : ModuleRules
{
    public NanoBananaBridge(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        // Allow nested Private/ subfolders (Providers/, Http/, Tests/) to use
        // path-relative includes without long ../../ chains.
        PrivateIncludePaths.AddRange(new string[]
        {
            Path.Combine(ModuleDirectory, "Private")
        });

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "HTTP",
            "Json",
            "JsonUtilities",
            "ImageWrapper",
            "DeveloperSettings"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "ViewportCapture",
            "ImageComposer",
            "Projects"
        });
    }
}

