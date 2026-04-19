// Public types for the Nano Banana bridge: enums and request/result structs.
#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "NanoBananaTypes.generated.h"

UENUM(BlueprintType)
enum class ENanoBananaVendor : uint8
{
    // Google Generative Language API (generativelanguage.googleapis.com)
    Google      UMETA(DisplayName="Google Gemini (Direct)"),
    // FAL.ai (fal.run / queue.fal.run)
    Fal         UMETA(DisplayName="FAL.ai"),
    // Replicate (api.replicate.com/v1/predictions)
    Replicate   UMETA(DisplayName="Replicate"),
};

UENUM(BlueprintType)
enum class ENanoBananaModel : uint8
{
    // Nano Banana — fast tier (Gemini 2.5 Flash Image / fal-ai/nano-banana / google/nano-banana)
    NanoBanana      UMETA(DisplayName="Nano Banana"),
    // Nano Banana 2 — balanced tier (Gemini 3.1 Flash Image / fal-ai/nano-banana-2)
    NanoBanana2     UMETA(DisplayName="Nano Banana 2"),
    // Nano Banana Pro — pro tier (Gemini 3 Pro Image / fal-ai/nano-banana-pro / google/nano-banana-pro)
    NanoBananaPro   UMETA(DisplayName="Nano Banana Pro"),
    // Use Request.CustomModelId verbatim (lets users target preview slugs without a code change)
    Custom          UMETA(DisplayName="Custom (use CustomModelId)"),
};

UENUM(BlueprintType)
enum class ENanoBananaAspect : uint8
{
    Auto    UMETA(DisplayName="Auto"),
    R1x1    UMETA(DisplayName="1:1"),
    R16x9   UMETA(DisplayName="16:9"),
    R9x16   UMETA(DisplayName="9:16"),
    R4x3    UMETA(DisplayName="4:3"),
    R3x4    UMETA(DisplayName="3:4"),
    R3x2    UMETA(DisplayName="3:2"),
    R2x3    UMETA(DisplayName="2:3"),
    R21x9   UMETA(DisplayName="21:9"),
};

UENUM(BlueprintType)
enum class ENanoBananaResolution : uint8
{
    Auto    UMETA(DisplayName="Auto"),
    Res512  UMETA(DisplayName="0.5K (512)"),
    Res1K   UMETA(DisplayName="1K (1024)"),
    Res2K   UMETA(DisplayName="2K (2048)"),
    Res4K   UMETA(DisplayName="4K (4096)"),
};

UENUM(BlueprintType)
enum class ENanoBananaOutputFormat : uint8
{
    PNG     UMETA(DisplayName="PNG"),
    JPEG    UMETA(DisplayName="JPEG"),
    WebP    UMETA(DisplayName="WebP"),
};

/**
 * One reference image input. Populate ONE of: Texture, RenderTarget, FilePath.
 * Pre-encoded PngBytes is used internally by the bridge after conversion.
 */
USTRUCT(BlueprintType)
struct NANOBANANABRIDGE_API FNanoBananaReferenceImage
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category="Nano Banana")
    TObjectPtr<UTexture2D> Texture = nullptr;

    UPROPERTY(BlueprintReadWrite, Category="Nano Banana")
    TObjectPtr<UTextureRenderTarget2D> RenderTarget = nullptr;

    /** Absolute path to a PNG/JPEG/WebP file on disk. */
    UPROPERTY(BlueprintReadWrite, Category="Nano Banana")
    FString FilePath;
};

/**
 * Single image generation request. Vendor + Model select the backend; everything
 * else is a vendor-agnostic description of what to generate.
 */
USTRUCT(BlueprintType)
struct NANOBANANABRIDGE_API FNanoBananaRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category="Nano Banana")
    FString Prompt;

    UPROPERTY(BlueprintReadWrite, Category="Nano Banana")
    ENanoBananaVendor Vendor = ENanoBananaVendor::Fal;

    UPROPERTY(BlueprintReadWrite, Category="Nano Banana")
    ENanoBananaModel Model = ENanoBananaModel::NanoBanana2;

    /** Free-form vendor-specific model id (used when Model == Custom). */
    UPROPERTY(BlueprintReadWrite, Category="Nano Banana")
    FString CustomModelId;

    UPROPERTY(BlueprintReadWrite, Category="Nano Banana")
    ENanoBananaAspect Aspect = ENanoBananaAspect::Auto;

    UPROPERTY(BlueprintReadWrite, Category="Nano Banana")
    ENanoBananaResolution Resolution = ENanoBananaResolution::Res1K;

    UPROPERTY(BlueprintReadWrite, Category="Nano Banana", meta=(ClampMin="1", ClampMax="8"))
    int32 NumImages = 1;

    UPROPERTY(BlueprintReadWrite, Category="Nano Banana")
    FString NegativePrompt;

    /** 0 means "vendor default / random". */
    UPROPERTY(BlueprintReadWrite, Category="Nano Banana")
    int32 Seed = 0;

    UPROPERTY(BlueprintReadWrite, Category="Nano Banana")
    ENanoBananaOutputFormat OutputFormat = ENanoBananaOutputFormat::PNG;

    UPROPERTY(BlueprintReadWrite, Category="Nano Banana")
    TArray<FNanoBananaReferenceImage> ReferenceImages;

    /** Optional inpaint/edit mask (alpha = where to edit). */
    UPROPERTY(BlueprintReadWrite, Category="Nano Banana")
    TObjectPtr<UTextureRenderTarget2D> OptionalMask = nullptr;
};

/**
 * One generated image in the result set.
 */
USTRUCT(BlueprintType)
struct NANOBANANABRIDGE_API FNanoBananaImageResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Nano Banana")
    TObjectPtr<UTexture2D> Texture = nullptr;

    UPROPERTY(BlueprintReadOnly, Category="Nano Banana")
    TArray<uint8> PngBytes;

    UPROPERTY(BlueprintReadOnly, Category="Nano Banana")
    FString SavedPath;
};

/** Helpers used by providers and tests. */
struct NANOBANANABRIDGE_API FNanoBananaTypeUtils
{
    static FString AspectToString(ENanoBananaAspect Aspect);
    static FString ResolutionToString(ENanoBananaResolution Res);
    static int32 ResolutionToPixels(ENanoBananaResolution Res); // 0 = auto
    static FString OutputFormatToMime(ENanoBananaOutputFormat Fmt);
    static FString OutputFormatToExt(ENanoBananaOutputFormat Fmt); // ".png" etc.
    static FString VendorToString(ENanoBananaVendor V);
    static FString ModelToDisplayString(ENanoBananaModel M);
};
