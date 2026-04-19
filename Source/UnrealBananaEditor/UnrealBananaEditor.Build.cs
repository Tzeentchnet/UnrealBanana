using System.IO;
using UnrealBuildTool;

public class UnrealBananaEditor : ModuleRules
{
    public UnrealBananaEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateIncludePaths.AddRange(new string[]
        {
            Path.Combine(ModuleDirectory, "Private")
        });

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate",
            "SlateCore",
            "InputCore",
            "EditorFramework",
            "UnrealEd",
            "ToolMenus",
            "LevelEditor",
            "Projects",
            "NanoBananaBridge"
        });
    }
}
