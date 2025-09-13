using UnrealBuildTool;

public class ImageComposer : ModuleRules
{
    public ImageComposer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "ImageWrapper"
        });
    }
}

