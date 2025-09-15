#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImageComposer_SmokeTest,
    "UnrealBanana.ImageComposer.Smoke",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FImageComposer_SmokeTest::RunTest(const FString& Parameters)
{
    const FName ModuleName(TEXT("ImageComposer"));

    if (!FModuleManager::Get().IsModuleLoaded(ModuleName))
    {
        FModuleManager::Get().LoadModule(ModuleName);
    }

    TestTrue(TEXT("ImageComposer module is loaded"), FModuleManager::Get().IsModuleLoaded(ModuleName));
    TestEqual(TEXT("Basic arithmetic"), 2 + 2, 4);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

