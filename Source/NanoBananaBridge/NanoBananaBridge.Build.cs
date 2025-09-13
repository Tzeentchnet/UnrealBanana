using UnrealBuildTool;

public class NanoBananaBridge : ModuleRules
{
    public NanoBananaBridge(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "HTTP",
            "Json",
            "JsonUtilities",
            "ImageWrapper"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "ViewportCapture",
            "ImageComposer",
            "Projects"
        });
    }
}

