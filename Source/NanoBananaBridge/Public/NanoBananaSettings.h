// Project Settings > Plugins > Nano Banana / Gemini Images
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NanoBananaTypes.h"
#include "NanoBananaSettings.generated.h"

USTRUCT(BlueprintType)
struct NANOBANANABRIDGE_API FGoogleVendorConfig
{
    GENERATED_BODY()

    /** Gemini API key. Falls back to env var GEMINI_API_KEY (then GOOGLE_API_KEY) when blank. */
    UPROPERTY(EditAnywhere, Config, Category="Google")
    FString ApiKey;

    /** Override base URL. Default: https://generativelanguage.googleapis.com/v1beta */
    UPROPERTY(EditAnywhere, Config, Category="Google", AdvancedDisplay)
    FString BaseUrlOverride;
};

USTRUCT(BlueprintType)
struct NANOBANANABRIDGE_API FFalVendorConfig
{
    GENERATED_BODY()

    /** FAL key. Falls back to env var FAL_KEY when blank. */
    UPROPERTY(EditAnywhere, Config, Category="FAL")
    FString ApiKey;

    /** When true, always submit via queue endpoint (queue.fal.run) instead of trying sync first. */
    UPROPERTY(EditAnywhere, Config, Category="FAL")
    bool bAlwaysUseQueue = false;

    /** Sync base URL. Default: https://fal.run */
    UPROPERTY(EditAnywhere, Config, Category="FAL", AdvancedDisplay)
    FString SyncBaseUrlOverride;

    /** Queue base URL. Default: https://queue.fal.run */
    UPROPERTY(EditAnywhere, Config, Category="FAL", AdvancedDisplay)
    FString QueueBaseUrlOverride;
};

USTRUCT(BlueprintType)
struct NANOBANANABRIDGE_API FReplicateVendorConfig
{
    GENERATED_BODY()

    /** Replicate API token. Falls back to env var REPLICATE_API_TOKEN when blank. */
    UPROPERTY(EditAnywhere, Config, Category="Replicate")
    FString ApiKey;

    /** Send "Prefer: wait" header to attempt sync (up to 60s) before falling back to polling. */
    UPROPERTY(EditAnywhere, Config, Category="Replicate")
    bool bPreferSyncWait = true;

    /** Optional: pin a specific version hash for the NanoBanana slug (use when the bare model id is rejected). */
    UPROPERTY(EditAnywhere, Config, Category="Replicate", AdvancedDisplay)
    FString NanoBananaVersionHash;

    /** Optional: pin a specific version hash for the NanoBananaPro slug. */
    UPROPERTY(EditAnywhere, Config, Category="Replicate", AdvancedDisplay)
    FString NanoBananaProVersionHash;

    /** Base URL override. Default: https://api.replicate.com/v1 */
    UPROPERTY(EditAnywhere, Config, Category="Replicate", AdvancedDisplay)
    FString BaseUrlOverride;
};

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Nano Banana / Gemini Images"))
class NANOBANANABRIDGE_API UNanoBananaSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    // ---------------- Vendor selection ----------------

    /** Default vendor used when a request leaves Vendor unspecified. */
    UPROPERTY(EditAnywhere, Config, Category="Defaults")
    ENanoBananaVendor DefaultVendor = ENanoBananaVendor::Fal;

    /** Default model tier used when a request leaves Model unspecified. */
    UPROPERTY(EditAnywhere, Config, Category="Defaults")
    ENanoBananaModel DefaultModel = ENanoBananaModel::NanoBanana2;

    UPROPERTY(EditAnywhere, Config, Category="Defaults")
    ENanoBananaAspect DefaultAspect = ENanoBananaAspect::Auto;

    UPROPERTY(EditAnywhere, Config, Category="Defaults")
    ENanoBananaResolution DefaultResolution = ENanoBananaResolution::Res1K;

    UPROPERTY(EditAnywhere, Config, Category="Defaults")
    ENanoBananaOutputFormat DefaultOutputFormat = ENanoBananaOutputFormat::PNG;

    UPROPERTY(EditAnywhere, Config, Category="Defaults", meta=(ClampMin="1", ClampMax="8"))
    int32 DefaultNumImages = 1;

    UPROPERTY(EditAnywhere, Config, Category="Defaults")
    FString DefaultNegativePrompt;

    // ---------------- Per-vendor configuration ----------------

    UPROPERTY(EditAnywhere, Config, Category="Vendors", meta=(ShowOnlyInnerProperties))
    FGoogleVendorConfig Google;

    UPROPERTY(EditAnywhere, Config, Category="Vendors", meta=(ShowOnlyInnerProperties))
    FFalVendorConfig Fal;

    UPROPERTY(EditAnywhere, Config, Category="Vendors", meta=(ShowOnlyInnerProperties))
    FReplicateVendorConfig Replicate;

    // ---------------- IO + behavior ----------------

    /** Output directory for saved images (absolute or project-relative). */
    UPROPERTY(EditAnywhere, Config, Category="Output")
    FString OutputDirectory = TEXT("Saved/NanoBanana");

    /** When true, dumps each request/response JSON to OutputDirectory/Debug/. */
    UPROPERTY(EditAnywhere, Config, Category="Output")
    bool bSaveDebugRequestResponse = false;

    /** Soft timeout for the initial sync attempt before switching to queue/poll mode (FAL/Replicate). */
    UPROPERTY(EditAnywhere, Config, Category="Behavior", meta=(ClampMin="5", ClampMax="600"))
    int32 RequestTimeoutSeconds = 60;

    /** Maximum total time (in seconds) spent polling a queued/async job before giving up. */
    UPROPERTY(EditAnywhere, Config, Category="Behavior", meta=(ClampMin="10", ClampMax="1800"))
    int32 MaxPollSeconds = 240;

    // ---------------- Helpers ----------------

    /** Returns the effective API key for the given vendor: configured value, or env-var fallback. */
    FString GetEffectiveApiKey(ENanoBananaVendor Vendor) const;

    /** Convenience accessor matching UDeveloperSettings idiom. */
    static const UNanoBananaSettings& Get();
};
