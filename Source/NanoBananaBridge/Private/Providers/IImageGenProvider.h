// Internal interface for vendor-specific image generation backends.
// Not exposed to Blueprint; the public Blueprint surface is UNanoBananaBridgeAsyncAction.
#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "Templates/Function.h"
#include "NanoBananaTypes.h"

/** Callbacks fired by a provider over the lifetime of a single Submit() call. */
struct FProviderCallbacks
{
    /** Percent in [0, 1], plus a human-readable stage label. */
    TFunction<void(float /*Percent*/, const FString& /*Stage*/)> OnProgress;

    /** One or more decoded image byte buffers (already in their native format — usually PNG).
     *  RawResponse is the full response body as text (or summary) for debug capture. */
    TFunction<void(TArray<TArray<uint8>> /*Images*/, const FString& /*RawResponse*/)> OnSuccess;

    TFunction<void(const FString& /*Error*/)> OnFailure;

    /** Optional: invoked once with the serialized request body, before HTTP send (for debug dump). */
    TFunction<void(const FString& /*RequestJson*/)> OnRequestBuilt;
};

/**
 * Vendor-agnostic image generation provider.
 * Lifetime is managed via TSharedPtr held by the async action. Each instance handles
 * exactly one Submit() call; create a fresh provider per request.
 */
class IImageGenProvider : public TSharedFromThis<IImageGenProvider, ESPMode::ThreadSafe>
{
public:
    virtual ~IImageGenProvider() = default;

    /** Begin processing the request. Must invoke exactly one of OnSuccess/OnFailure. */
    virtual void Submit(const FNanoBananaRequest& Request, const FProviderCallbacks& Callbacks) = 0;

    /** Best-effort cancel of any in-flight HTTP request or poll loop. */
    virtual void Cancel() = 0;
};
