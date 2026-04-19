// Provider: Replicate
// POST /v1/predictions with `Prefer: wait` for sync attempt; if response is non-terminal,
// poll urls.get until succeeded/failed/canceled, then HTTP-fetch output URLs to bytes.
#pragma once

#include "CoreMinimal.h"
#include "../IImageGenProvider.h"
#include "Interfaces/IHttpRequest.h"

namespace NanoBanana::Http { class FPollLoop; }

class FReplicateProvider : public IImageGenProvider
{
public:
    virtual void Submit(const FNanoBananaRequest& Request, const FProviderCallbacks& Callbacks) override;
    virtual void Cancel() override;

    // ---- Static helpers (testable) ----

    /** Returns "owner/model" slug for built-in tiers, or CustomModelId verbatim for Custom. */
    static FString ResolveModelSlug(ENanoBananaModel Model, const FString& CustomModelId);

    /** Returns version hash override per model (or empty). */
    static FString ResolveVersionOverride(ENanoBananaModel Model);

    static FString BuildPredictionsUrl(const FString& BaseUrlOverride);
    static FString BuildRequestJson(const FNanoBananaRequest& Request, const TArray<TArray<uint8>>& EncodedReferences);

private:
    void HandleInitialResponse(const FString& Body, const FString& ApiKey, const FProviderCallbacks& Callbacks);
    void PollPrediction(const FString& GetUrl, const FString& ApiKey, const FProviderCallbacks& Callbacks);
    void HandleTerminalPrediction(const FString& Body, const FProviderCallbacks& Callbacks);
    void FetchImageUrls(const TArray<FString>& Urls, const FProviderCallbacks& Callbacks, const FString& RawResponse);

    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> InFlight;
    TSharedPtr<NanoBanana::Http::FPollLoop, ESPMode::ThreadSafe> Poll;
    bool bCanceled = false;
};
