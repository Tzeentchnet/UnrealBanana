// Provider: FAL.ai
// Tries sync POST https://fal.run/{slug} first (60s blocking), falls back to
// queue.fal.run/{slug} + status polling on timeout or when settings force it.
#pragma once

#include "CoreMinimal.h"
#include "../IImageGenProvider.h"
#include "Interfaces/IHttpRequest.h"

namespace NanoBanana::Http { class FPollLoop; }

class FFalAiProvider : public IImageGenProvider
{
public:
    virtual void Submit(const FNanoBananaRequest& Request, const FProviderCallbacks& Callbacks) override;
    virtual void Cancel() override;

    // ---- Static helpers (testable without HTTP) ----

    static FString ResolveModelSlug(ENanoBananaModel Model, const FString& CustomModelId);
    static FString BuildSyncUrl(const FString& SyncBaseUrlOverride, const FString& Slug);
    static FString BuildQueueSubmitUrl(const FString& QueueBaseUrlOverride, const FString& Slug);
    static FString BuildQueueStatusUrl(const FString& QueueBaseUrlOverride, const FString& Slug, const FString& RequestId);
    static FString BuildQueueResultUrl(const FString& QueueBaseUrlOverride, const FString& Slug, const FString& RequestId);
    static FString BuildRequestJson(const FNanoBananaRequest& Request, const TArray<TArray<uint8>>& EncodedReferences);

private:
    void SubmitSync(const FString& Url, const FString& Body, const FString& ApiKey, const FProviderCallbacks& Callbacks);
    void SubmitQueue(const FString& Slug, const FString& Body, const FString& ApiKey, const FProviderCallbacks& Callbacks);
    void HandleResultPayload(const FString& Body, const FProviderCallbacks& Callbacks);
    void FetchImageUrls(const TArray<FString>& Urls, const FProviderCallbacks& Callbacks, const FString& RawResponse);

    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> InFlight;
    TSharedPtr<NanoBanana::Http::FPollLoop, ESPMode::ThreadSafe> Poll;
    bool bCanceled = false;
};
