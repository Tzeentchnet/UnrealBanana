// Async action: vendor-agnostic image generation, optionally seeded with a viewport capture.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/Texture2D.h"
#include "Templates/SharedPointer.h"
#include "NanoBananaTypes.h"
#include "NanoBananaBridgeAsyncAction.generated.h"

class IImageGenProvider;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNanoBananaProgress, float, Percent, const FString&, Stage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNanoBananaCompleted, const TArray<FNanoBananaImageResult>&, Results, const FString&, CompositePath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNanoBananaFailed, const FString&, Error);

/**
 * Vendor-agnostic image generation async action. Supports Google Gemini, FAL.ai, and Replicate
 * via the Vendor field on FNanoBananaRequest.
 */
UCLASS()
class NANOBANANABRIDGE_API UNanoBananaBridgeAsyncAction : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable)
    FNanoBananaProgress OnProgress;

    UPROPERTY(BlueprintAssignable)
    FNanoBananaCompleted OnCompleted;

    UPROPERTY(BlueprintAssignable)
    FNanoBananaFailed OnFailed;

    /**
     * Generate one or more images directly from an FNanoBananaRequest.
     * @param Request           Vendor + model + prompt + reference images, etc.
     * @param bAlsoSaveComposite If true and there is at least one reference image, also save a side-by-side PNG.
     */
    UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly="true", WorldContext="WorldContextObject"), Category="Nano Banana")
    static UNanoBananaBridgeAsyncAction* GenerateImage(UObject* WorldContextObject, const FNanoBananaRequest& Request, bool bAlsoSaveComposite = true);

    /**
     * Convenience: capture the current game viewport, attach it as a reference image,
     * and submit a request using the project's default vendor/model (overridable here).
     */
    UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly="true", WorldContext="WorldContextObject"), Category="Nano Banana")
    static UNanoBananaBridgeAsyncAction* CaptureViewportAndGenerate(
        UObject* WorldContextObject,
        const FString& Prompt,
        ENanoBananaVendor Vendor = ENanoBananaVendor::Fal,
        ENanoBananaModel Model = ENanoBananaModel::NanoBanana2,
        bool bShowUI = true,
        bool bAlsoSaveComposite = true);

    /** Cancel an in-flight request. Broadcasts OnFailed("Canceled") and tears down. */
    UFUNCTION(BlueprintCallable, Category="Nano Banana")
    void Cancel();

    // Overrides for save paths. Leave empty to auto-name under settings OutputDirectory.
    UPROPERTY()
    FString InputSavePath;

    UPROPERTY()
    FString CompositeSavePath;

protected:
    virtual void Activate() override;
    virtual void BeginDestroy() override;

private:
    enum class EMode : uint8 { Direct, CaptureFirst };

    UPROPERTY()
    TObjectPtr<UObject> WorldContextObject;

    FNanoBananaRequest Request;
    EMode Mode = EMode::Direct;
    bool bShowUI = true;
    bool bAlsoSaveComposite = true;
    bool bFinished = false;

    TSharedPtr<IImageGenProvider, ESPMode::ThreadSafe> Provider;

    void RunProvider();
    void HandleCaptured(const struct FViewportCaptureResult& Capture, const FString& SavedPath);
    void HandleSuccess(TArray<TArray<uint8>> Images, const FString& RawResponse);
    void Fail(const FString& Error);

    FString MakeTimestampedPath(const FString& BaseDir, const FString& Suffix) const;
    void DumpDebug(const FString& Suffix, const FString& Body) const;
};
