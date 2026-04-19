#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "NanoBananaTypes.h"

class IImageGenProvider;

/** Factory: returns a fresh provider instance for the given vendor, or nullptr if unsupported. */
class FProviderFactory
{
public:
    static TSharedPtr<IImageGenProvider, ESPMode::ThreadSafe> Make(ENanoBananaVendor Vendor);
};
