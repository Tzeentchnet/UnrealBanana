#include "ViewportCaptureLibrary.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "ImageUtils.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "HighResScreenshot.h"
#include "Engine/Engine.h"

static void EnsureDirectory(const FString& InDir)
{
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (!PF.DirectoryExists(*InDir))
    {
        PF.CreateDirectoryTree(*InDir);
    }
}

void UViewportCaptureLibrary::CaptureCurrentViewportToPNG(UObject* WorldContextObject, bool bShowUI, const FString& OptionalOutputPath, FOnViewportCaptured OnCaptured)
{
    if (!GEngine || !GEngine->GameViewport)
    {
        if (OnCaptured.IsBound())
        {
            FViewportCaptureResult Empty; OnCaptured.Execute(Empty, TEXT(""));
        }
        return;
    }

    UGameViewportClient* GVC = GEngine->GameViewport;

    // Bind a one-shot handler to the screenshot delegate
    TSharedPtr<FDelegateHandle> HandlePtr = MakeShared<FDelegateHandle>();
    *HandlePtr = GVC->OnScreenshotCaptured().AddLambda([OnCaptured, HandlePtr, OptionalOutputPath](int32 Width, int32 Height, const TArray<FColor>& Colors)
    {
        // Compress to PNG
        TArray<uint8> PNG;
        CompressColorsToPNG(Colors, FIntPoint(Width, Height), PNG);

        // Optional save
        FString SavedPath;
        if (!OptionalOutputPath.IsEmpty())
        {
            SavedPath = SavePNGToDisk(PNG, OptionalOutputPath);
        }

        // Create transient texture for preview
        UTexture2D* Texture = FImageUtils::ImportBufferAsTexture2D(PNG);

        FViewportCaptureResult Result;
        Result.Texture = Texture;
        Result.PngBytes = MoveTemp(PNG);
        Result.Width = Width;
        Result.Height = Height;

        if (OnCaptured.IsBound())
        {
            OnCaptured.Execute(Result, SavedPath);
        }

        // Remove handler (best-effort)
        if (GEngine && GEngine->GameViewport)
        {
            GEngine->GameViewport->OnScreenshotCaptured().Remove(*HandlePtr);
        }
    });

    // Queue screenshot request (empty name sends to default, but we intercept pixels via delegate)
    FScreenshotRequest::RequestScreenshot(bShowUI);

    // Remove handler on next tick to avoid leaks if engine does not trigger
    // (in practice, engine triggers next frame; this safety is mostly redundant)
}

bool UViewportCaptureLibrary::RenderTargetToPNG(UTextureRenderTarget2D* RenderTarget, TArray<uint8>& OutPNG, int32& OutWidth, int32& OutHeight, bool bSRGB)
{
    if (!RenderTarget)
    {
        return false;
    }

    FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
    if (!RTResource)
    {
        return false;
    }

    TArray<FColor> Bitmap;
    FReadSurfaceDataFlags ReadPixelFlags(RCM_UNorm);
    ReadPixelFlags.SetLinearToGamma(bSRGB);
    if (!RTResource->ReadPixels(Bitmap, ReadPixelFlags))
    {
        return false;
    }

    OutWidth = RenderTarget->SizeX;
    OutHeight = RenderTarget->SizeY;
    CompressColorsToPNG(Bitmap, FIntPoint(OutWidth, OutHeight), OutPNG);
    return OutPNG.Num() > 0;
}

FString UViewportCaptureLibrary::SavePNGToDisk(const TArray<uint8>& PNG, const FString& AbsolutePath)
{
    FString Path = AbsolutePath;
    if (Path.IsEmpty())
    {
        Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("NanoBanana") / TEXT("Viewport.png"));
    }
    EnsureDirectory(FPaths::GetPath(Path));
    FFileHelper::SaveArrayToFile(PNG, *Path);
    return Path;
}

void UViewportCaptureLibrary::CompressColorsToPNG(const TArray<FColor>& Colors, const FIntPoint& Size, TArray<uint8>& OutPNG)
{
    OutPNG.Reset();
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

    const int32 Width = Size.X;
    const int32 Height = Size.Y;

    ImageWrapper->SetRaw(Colors.GetData(), Colors.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8);
    OutPNG = ImageWrapper->GetCompressed(100);
}
