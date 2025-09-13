// Async action to capture viewport, send to Nano Banana, and save results
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/Texture2D.h"
#include "NanoBananaBridgeAsyncAction.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNanoBananaProgress, float, Percent, const FString&, Stage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FNanoBananaCompleted, UTexture2D*, ResultTexture, const FString&, SavedResultPath, const FString&, SavedCompositePath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNanoBananaFailed, const FString&, Error);

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

    UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly="true", WorldContext="WorldContextObject"), Category="Nano Banana")
    static UNanoBananaBridgeAsyncAction* CaptureAndGenerate(UObject* WorldContextObject, const FString& Prompt, bool bShowUI = true);

    // Advanced: supply optional mask, size, and candidate count overrides
    UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly="true", WorldContext="WorldContextObject"), Category="Nano Banana")
    static UNanoBananaBridgeAsyncAction* CaptureAndGenerateAdvanced(UObject* WorldContextObject, const FString& Prompt, class UTextureRenderTarget2D* OptionalMask, int32 NumImages = 1, int32 OutWidth = 1024, int32 OutHeight = 1024, const FString& NegativePrompt = TEXT(""), bool bShowUI = true);

    // Optional path to save input PNG; if empty, auto path under settings
    UPROPERTY()
    FString InputSavePath;

    // Optional path to save result PNG; if empty, auto path under settings
    UPROPERTY()
    FString ResultSavePath;

    // Optional path to save composite PNG; if empty, auto path under settings
    UPROPERTY()
    FString CompositeSavePath;

protected:
    virtual void Activate() override;

private:
    UPROPERTY()
    TObjectPtr<UObject> WorldContextObject;

    FString Prompt;
    bool bShowUI = true;

    // Advanced optional mask
    UPROPERTY()
    TObjectPtr<class UTextureRenderTarget2D> OptionalMask;

    int32 NumImages = 1;
    int32 OutWidth = 1024;
    int32 OutHeight = 1024;
    FString NegativePrompt;

    void HandleCaptured(const struct FViewportCaptureResult& Capture, const FString& SavedPath);
    void SendToService(const TArray<uint8>& InputPNG);
    void HandleServiceResponse(TSharedPtr<class IHttpRequest>, TSharedPtr<class IHttpResponse>, bool bSucceeded);
    FString MakeTimestampedPath(const FString& BaseDir, const FString& Suffix) const;
};
