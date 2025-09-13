using UnrealBuildTool;

public class ViewportCapture : ModuleRules
{
    public ViewportCapture(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "ImageWrapper",
            "RenderCore",
            "RHI",
            "SlateCore",
            "Slate"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Projects"
        });
    }
}

