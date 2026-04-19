#include "ProviderFactory.h"
#include "IImageGenProvider.h"
#include "Google/GoogleGeminiProvider.h"
#include "Fal/FalAiProvider.h"
#include "Replicate/ReplicateProvider.h"

TSharedPtr<IImageGenProvider, ESPMode::ThreadSafe> FProviderFactory::Make(ENanoBananaVendor Vendor)
{
    switch (Vendor)
    {
    case ENanoBananaVendor::Google:
        return MakeShared<FGoogleGeminiProvider, ESPMode::ThreadSafe>();
    case ENanoBananaVendor::Fal:
        return MakeShared<FFalAiProvider, ESPMode::ThreadSafe>();
    case ENanoBananaVendor::Replicate:
        return MakeShared<FReplicateProvider, ESPMode::ThreadSafe>();
    default:
        return nullptr;
    }
}
