// Provider: Google Generative Language API (generativelanguage.googleapis.com)
// Endpoint: POST {base}/models/{modelId}:generateContent?key=...
// All image inputs/outputs are base64 inline_data inside JSON.
#pragma once

#include "CoreMinimal.h"
#include "../IImageGenProvider.h"
#include "Interfaces/IHttpRequest.h"

class FGoogleGeminiProvider : public IImageGenProvider
{
public:
    virtual void Submit(const FNanoBananaRequest& Request, const FProviderCallbacks& Callbacks) override;
    virtual void Cancel() override;

    // ---- Static helpers (testable without HTTP) ----

    /** Map model enum to canonical Google model id. Honors Custom + CustomModelId. */
    static FString ResolveModelId(ENanoBananaModel Model, const FString& CustomModelId);

    /** Build the full URL including ?key=... query param. */
    static FString BuildEndpointUrl(const FString& BaseUrlOverride, const FString& ModelId, const FString& ApiKey);

    /** Build the JSON request body. EncodedReferences are raw image bytes (PNG/JPEG/WebP). */
    static FString BuildRequestJson(const FNanoBananaRequest& Request, const TArray<TArray<uint8>>& EncodedReferences);

private:
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> InFlight;
    bool bCanceled = false;
};
