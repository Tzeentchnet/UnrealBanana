#include "Base64Image.h"
#include "ViewportCaptureLibrary.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "PixelFormat.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace NanoBanana::Image
{
    bool TextureToPng(const UTexture2D* Texture, TArray<uint8>& OutPng)
    {
        OutPng.Reset();
        if (!Texture)
        {
            return false;
        }

        // Try mip-0 readback for uncompressed textures (works for textures created via
        // FImageUtils::ImportBufferAsTexture2D, which is the common reference-image case).
        FTexturePlatformData* PD = const_cast<UTexture2D*>(Texture)->GetPlatformData();
        if (!PD || PD->Mips.Num() == 0)
        {
            return false;
        }
        const EPixelFormat Fmt = Texture->GetPixelFormat();
        if (Fmt != PF_B8G8R8A8 && Fmt != PF_R8G8B8A8)
        {
            return false; // Unsupported source format — caller should use RenderTarget or FilePath.
        }

        FTexture2DMipMap& Mip = PD->Mips[0];
        const int32 W = Mip.SizeX;
        const int32 H = Mip.SizeY;
        const void* Data = Mip.BulkData.LockReadOnly();
        if (!Data)
        {
            return false;
        }

        IImageWrapperModule& Mod = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
        TSharedPtr<IImageWrapper> Wrapper = Mod.CreateImageWrapper(EImageFormat::PNG);
        const ERGBFormat RGBFmt = (Fmt == PF_B8G8R8A8) ? ERGBFormat::BGRA : ERGBFormat::RGBA;
        Wrapper->SetRaw(Data, (int64)W * (int64)H * 4, W, H, RGBFmt, 8);
        OutPng = Wrapper->GetCompressed(100);

        Mip.BulkData.Unlock();
        return OutPng.Num() > 0;
    }

    bool RenderTargetToPng(UTextureRenderTarget2D* RT, TArray<uint8>& OutPng, bool bSRGB)
    {
        OutPng.Reset();
        if (!RT)
        {
            return false;
        }
        int32 W = 0, H = 0;
        return UViewportCaptureLibrary::RenderTargetToPNG(RT, OutPng, W, H, bSRGB);
    }

    bool ResolveReferenceToPng(const FNanoBananaReferenceImage& Ref, TArray<uint8>& OutPng)
    {
        if (Ref.Texture && TextureToPng(Ref.Texture, OutPng))
        {
            return true;
        }
        if (Ref.RenderTarget && RenderTargetToPng(Ref.RenderTarget, OutPng, /*bSRGB*/ true))
        {
            return true;
        }
        if (!Ref.FilePath.IsEmpty())
        {
            return FFileHelper::LoadFileToArray(OutPng, *Ref.FilePath);
        }
        return false;
    }

    void ResolveAllReferences(const FNanoBananaRequest& Req, TArray<TArray<uint8>>& OutPngs)
    {
        OutPngs.Reset();
        for (const FNanoBananaReferenceImage& Ref : Req.ReferenceImages)
        {
            TArray<uint8> Png;
            if (ResolveReferenceToPng(Ref, Png) && Png.Num() > 0)
            {
                OutPngs.Add(MoveTemp(Png));
            }
        }
    }

    FString PngBytesToDataUri(const TArray<uint8>& Png, const FString& MimeType)
    {
        if (Png.Num() == 0)
        {
            return FString();
        }
        const FString B64 = FBase64::Encode(Png);
        return FString::Printf(TEXT("data:%s;base64,%s"), *MimeType, *B64);
    }

    FString SniffImageMimeType(const TArray<uint8>& Bytes)
    {
        if (Bytes.Num() >= 8)
        {
            // PNG signature: 89 50 4E 47 0D 0A 1A 0A
            if (Bytes[0] == 0x89 && Bytes[1] == 0x50 && Bytes[2] == 0x4E && Bytes[3] == 0x47)
            {
                return TEXT("image/png");
            }
            // JPEG signature: FF D8 FF
            if (Bytes[0] == 0xFF && Bytes[1] == 0xD8 && Bytes[2] == 0xFF)
            {
                return TEXT("image/jpeg");
            }
            // WebP: "RIFF" .... "WEBP"
            if (Bytes.Num() >= 12 && Bytes[0] == 'R' && Bytes[1] == 'I' && Bytes[2] == 'F' && Bytes[3] == 'F'
                && Bytes[8] == 'W' && Bytes[9] == 'E' && Bytes[10] == 'B' && Bytes[11] == 'P')
            {
                return TEXT("image/webp");
            }
        }
        return TEXT("image/png");
    }
}
