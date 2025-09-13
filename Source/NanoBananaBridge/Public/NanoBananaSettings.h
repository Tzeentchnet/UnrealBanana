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
    // Full endpoint URL. For Gemini 2.5 Flash generateContent (image-to-image), set to:
    // https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent
    UPROPERTY(EditAnywhere, Config, Category="API")
    FString ApiBaseUrl = TEXT("https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent");

    // API Key: For Google, pass your Gemini API key (used as query param)
    UPROPERTY(EditAnywhere, Config, Category="API")
    FString ApiKey;

    // Output directory for saved images (absolute or relative to project)
    UPROPERTY(EditAnywhere, Config, Category="Output")
    FString OutputDirectory = TEXT("Saved/NanoBanana");

    // Optional model name for providers that require it (ignored by custom endpoints)
    UPROPERTY(EditAnywhere, Config, Category="API")
    FString Model = TEXT("imagegeneration");

    // When true and using Google, constructs requests for generateContent with the image_generation tool.
    UPROPERTY(EditAnywhere, Config, Category="API")
    bool bUseGenerateContent = true;

    // Default number of images to request (when supported)
    UPROPERTY(EditAnywhere, Config, Category="Generation")
    int32 DefaultNumImages = 1;

    // Default output dimensions (when supported)
    UPROPERTY(EditAnywhere, Config, Category="Generation")
    int32 DefaultWidth = 1024;

    UPROPERTY(EditAnywhere, Config, Category="Generation")
    int32 DefaultHeight = 1024;

    // Optional negative prompt text
    UPROPERTY(EditAnywhere, Config, Category="Generation")
    FString DefaultNegativePrompt;

    // Optional raw JSON fragment merged into the root of the request for advanced control
    // Example: { "toolConfig": { "imageGeneration": { "style": "photorealistic" } } }
    UPROPERTY(EditAnywhere, Config, Category="Advanced")
    FString AdvancedRequestJSON;
};
