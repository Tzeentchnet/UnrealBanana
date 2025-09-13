// Minimal public interface for viewport capture utilities
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "ViewportCaptureLibrary.generated.h"

USTRUCT(BlueprintType)
struct FViewportCaptureResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    UTexture2D* Texture = nullptr;

    UPROPERTY(BlueprintReadOnly)
    TArray<uint8> PngBytes;

    UPROPERTY(BlueprintReadOnly)
    int32 Width = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 Height = 0;
};

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnViewportCaptured, const FViewportCaptureResult&, Result, const FString&, SavedPath);

UCLASS()
class VIEWPORTCAPTURE_API UViewportCaptureLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Capture the current game viewport asynchronously using the engine screenshot pipeline.
    UFUNCTION(BlueprintCallable, Category = "Viewport Capture", meta=(WorldContext="WorldContextObject"))
    static void CaptureCurrentViewportToPNG(UObject* WorldContextObject, bool bShowUI, const FString& OptionalOutputPath, FOnViewportCaptured OnCaptured);

    // Read a render target and compress to PNG bytes.
    UFUNCTION(BlueprintCallable, Category = "Viewport Capture")
    static bool RenderTargetToPNG(UTextureRenderTarget2D* RenderTarget, TArray<uint8>& OutPNG, int32& OutWidth, int32& OutHeight, bool bSRGB = true);

    // Save PNG bytes to disk at path. Returns final absolute path.
    UFUNCTION(BlueprintCallable, Category = "Viewport Capture")
    static FString SavePNGToDisk(const TArray<uint8>& PNG, const FString& AbsolutePath);

private:
    static void CompressColorsToPNG(const TArray<FColor>& Colors, const FIntPoint& Size, TArray<uint8>& OutPNG);
};

