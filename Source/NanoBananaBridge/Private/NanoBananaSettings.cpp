#include "NanoBananaSettings.h"
#include "HAL/PlatformMisc.h"

const UNanoBananaSettings& UNanoBananaSettings::Get()
{
    return *GetDefault<UNanoBananaSettings>();
}

FString UNanoBananaSettings::GetEffectiveApiKey(ENanoBananaVendor Vendor) const
{
    auto FromEnv = [](const TCHAR* Var) -> FString
    {
        return FPlatformMisc::GetEnvironmentVariable(Var);
    };

    switch (Vendor)
    {
    case ENanoBananaVendor::Google:
    {
        if (!Google.ApiKey.IsEmpty()) return Google.ApiKey;
        FString K = FromEnv(TEXT("GEMINI_API_KEY"));
        if (K.IsEmpty()) K = FromEnv(TEXT("GOOGLE_API_KEY"));
        return K;
    }
    case ENanoBananaVendor::Fal:
    {
        if (!Fal.ApiKey.IsEmpty()) return Fal.ApiKey;
        return FromEnv(TEXT("FAL_KEY"));
    }
    case ENanoBananaVendor::Replicate:
    {
        if (!Replicate.ApiKey.IsEmpty()) return Replicate.ApiKey;
        return FromEnv(TEXT("REPLICATE_API_TOKEN"));
    }
    default:
        return FString();
    }
}
