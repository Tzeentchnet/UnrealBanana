// Settings for Nano Banana API
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NanoBananaSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Nano Banana / Gemini Images"))
class NANOBANANABRIDGE_API UNanoBananaSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    // Full endpoint URL. For Google Gemini Images, set to:
    // https://generativelanguage.googleapis.com/v1beta/models/imagegeneration:generate
    UPROPERTY(EditAnywhere, Config, Category="API")
    FString ApiBaseUrl = TEXT("https://generativelanguage.googleapis.com/v1beta/models/imagegeneration:generate");

    // API Key: For Google, pass your Gemini API key (used as query param)
    UPROPERTY(EditAnywhere, Config, Category="API")
    FString ApiKey;

    // Output directory for saved images (absolute or relative to project)
    UPROPERTY(EditAnywhere, Config, Category="Output")
    FString OutputDirectory = TEXT("Saved/NanoBanana");

    // Optional model name for providers that require it (ignored by custom endpoints)
    UPROPERTY(EditAnywhere, Config, Category="API")
    FString Model = TEXT("imagegeneration");
};
