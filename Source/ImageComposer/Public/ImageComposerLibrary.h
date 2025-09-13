// Utilities to compose images and save to disk
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ImageComposerLibrary.generated.h"

UCLASS()
class IMAGECOMPOSER_API UImageComposerLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Compose two PNG images side-by-side with optional padding (in pixels).
    UFUNCTION(BlueprintCallable, Category = "Image Composer")
    static bool ComposeSideBySidePNGs(const TArray<uint8>& LeftPNG, const TArray<uint8>& RightPNG, TArray<uint8>& OutCompositePNG, int32 Padding = 8);
};

