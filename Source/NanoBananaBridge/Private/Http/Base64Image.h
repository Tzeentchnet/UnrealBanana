// Helpers for converting reference images / render targets / textures to PNG bytes
// and base64 / data-URI strings used in vendor request payloads.
#pragma once

#include "CoreMinimal.h"
#include "NanoBananaTypes.h"

namespace NanoBanana::Image
{
    /** Encode any UTexture2D (CPU-readable or GPU) to PNG bytes. Returns true on success. */
    bool TextureToPng(const UTexture2D* Texture, TArray<uint8>& OutPng);

    /** Read a render target on the game thread and PNG-encode. */
    bool RenderTargetToPng(UTextureRenderTarget2D* RT, TArray<uint8>& OutPng, bool bSRGB = true);

    /** Resolve a single FNanoBananaReferenceImage to PNG bytes (texture > render target > file path). */
    bool ResolveReferenceToPng(const FNanoBananaReferenceImage& Ref, TArray<uint8>& OutPng);

    /** Resolve every reference in a request to PNG bytes. Skips entries that fail. */
    void ResolveAllReferences(const FNanoBananaRequest& Req, TArray<TArray<uint8>>& OutPngs);

    /** Build a "data:image/png;base64,..." URI from raw PNG bytes. */
    FString PngBytesToDataUri(const TArray<uint8>& Png, const FString& MimeType = TEXT("image/png"));

    /** Sniff bytes (PNG/JPEG/WebP magic) and return MIME type, defaulting to image/png. */
    FString SniffImageMimeType(const TArray<uint8>& Bytes);
}
