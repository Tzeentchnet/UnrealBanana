// Tests UNanoBananaSettings::GetEffectiveApiKey env-var fallback for each vendor.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "HAL/PlatformMisc.h"
#include "NanoBananaSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    struct FScopedEnv
    {
        const TCHAR* Name;
        FString Original;
        FScopedEnv(const TCHAR* InName, const FString& Value)
            : Name(InName)
        {
            Original = FPlatformMisc::GetEnvironmentVariable(Name);
            FPlatformMisc::SetEnvironmentVar(Name, *Value);
        }
        ~FScopedEnv()
        {
            FPlatformMisc::SetEnvironmentVar(Name, *Original);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNanoBananaSettings_EnvFallback_Test,
    "UnrealBanana.Settings.EnvVarFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FNanoBananaSettings_EnvFallback_Test::RunTest(const FString&)
{
    UNanoBananaSettings* S = GetMutableDefault<UNanoBananaSettings>();
    if (!S) { AddError(TEXT("Settings CDO null")); return false; }

    // Save & clear configured keys for the duration of the test.
    const FString GoogleSaved = S->Google.ApiKey;
    const FString FalSaved = S->Fal.ApiKey;
    const FString RepSaved = S->Replicate.ApiKey;
    S->Google.ApiKey = TEXT("");
    S->Fal.ApiKey = TEXT("");
    S->Replicate.ApiKey = TEXT("");

    {
        FScopedEnv G(TEXT("GEMINI_API_KEY"), TEXT("ENV_GEMINI"));
        FScopedEnv F(TEXT("FAL_KEY"), TEXT("ENV_FAL"));
        FScopedEnv R(TEXT("REPLICATE_API_TOKEN"), TEXT("ENV_REP"));

        TestEqual(TEXT("Gemini env fallback"),    S->GetEffectiveApiKey(ENanoBananaVendor::Google),    FString(TEXT("ENV_GEMINI")));
        TestEqual(TEXT("FAL env fallback"),       S->GetEffectiveApiKey(ENanoBananaVendor::Fal),       FString(TEXT("ENV_FAL")));
        TestEqual(TEXT("Replicate env fallback"), S->GetEffectiveApiKey(ENanoBananaVendor::Replicate), FString(TEXT("ENV_REP")));

        // Configured key wins over env var.
        S->Google.ApiKey = TEXT("CFG_GEMINI");
        TestEqual(TEXT("Configured beats env"), S->GetEffectiveApiKey(ENanoBananaVendor::Google), FString(TEXT("CFG_GEMINI")));
    }

    // Restore.
    S->Google.ApiKey = GoogleSaved;
    S->Fal.ApiKey = FalSaved;
    S->Replicate.ApiKey = RepSaved;
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
