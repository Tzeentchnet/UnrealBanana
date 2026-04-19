// Recursive JSON scanner that extracts inline base64 image payloads from arbitrary
// vendor response shapes (Gemini "inline_data", OpenAI-style "b64_json", etc.).
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace NanoBanana::Json
{
    /** Walk an arbitrary JSON object tree, decoding any base64 image fields found. */
    void CollectInlineImages(const TSharedPtr<FJsonObject>& Root, TArray<TArray<uint8>>& OutImages);

    /** Convenience: parse a response body string and collect images in one call. */
    void CollectInlineImagesFromResponseBody(const FString& Body, TArray<TArray<uint8>>& OutImages);
}
