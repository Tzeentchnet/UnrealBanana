#include "NanoBananaBridgeAsyncAction.h"
#include "NanoBananaSettings.h"
#include "ViewportCaptureLibrary.h"
#include "ImageComposerLibrary.h"
#include "Providers/IImageGenProvider.h"
#include "Providers/ProviderFactory.h"
#include "ImageUtils.h"

#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "Async/Async.h"

UNanoBananaBridgeAsyncAction* UNanoBananaBridgeAsyncAction::GenerateImage(UObject* InWorldContextObject, const FNanoBananaRequest& InRequest, bool bInAlsoSaveComposite)
{
    UNanoBananaBridgeAsyncAction* Action = NewObject<UNanoBananaBridgeAsyncAction>();
    Action->WorldContextObject = InWorldContextObject;
    Action->Request = InRequest;
    Action->Mode = EMode::Direct;
    Action->bAlsoSaveComposite = bInAlsoSaveComposite;
    Action->RegisterWithGameInstance(InWorldContextObject);
    return Action;
}

UNanoBananaBridgeAsyncAction* UNanoBananaBridgeAsyncAction::CaptureViewportAndGenerate(
    UObject* InWorldContextObject,
    const FString& InPrompt,
    ENanoBananaVendor InVendor,
    ENanoBananaModel InModel,
    bool bInShowUI,
    bool bInAlsoSaveComposite)
{
    UNanoBananaBridgeAsyncAction* Action = NewObject<UNanoBananaBridgeAsyncAction>();
    Action->WorldContextObject = InWorldContextObject;
    Action->Mode = EMode::CaptureFirst;
    Action->bShowUI = bInShowUI;
    Action->bAlsoSaveComposite = bInAlsoSaveComposite;

    const UNanoBananaSettings& S = UNanoBananaSettings::Get();
    Action->Request.Prompt = InPrompt;
    Action->Request.Vendor = InVendor;
    Action->Request.Model = InModel;
    Action->Request.Aspect = S.DefaultAspect;
    Action->Request.Resolution = S.DefaultResolution;
    Action->Request.OutputFormat = S.DefaultOutputFormat;
    Action->Request.NumImages = FMath::Max(1, S.DefaultNumImages);
    Action->Request.NegativePrompt = S.DefaultNegativePrompt;

    Action->RegisterWithGameInstance(InWorldContextObject);
    return Action;
}

void UNanoBananaBridgeAsyncAction::Cancel()
{
    if (bFinished) return;
    if (Provider.IsValid())
    {
        Provider->Cancel();
    }
    Fail(TEXT("Canceled"));
}

void UNanoBananaBridgeAsyncAction::BeginDestroy()
{
    if (Provider.IsValid())
    {
        Provider->Cancel();
        Provider.Reset();
    }
    Super::BeginDestroy();
}

void UNanoBananaBridgeAsyncAction::Activate()
{
    if (Mode == EMode::CaptureFirst)
    {
        OnProgress.Broadcast(0.05f, TEXT("Capturing viewport"));

        const UNanoBananaSettings& S = UNanoBananaSettings::Get();
        const FString AbsBaseDir = FPaths::ConvertRelativePathToFull(S.OutputDirectory);
        InputSavePath = MakeTimestampedPath(AbsBaseDir, TEXT("_Input.png"));

        UViewportCaptureLibrary::CaptureCurrentViewportToPNG(WorldContextObject.Get(), bShowUI, InputSavePath,
            FOnViewportCaptured::CreateUObject(this, &UNanoBananaBridgeAsyncAction::HandleCaptured));
    }
    else
    {
        RunProvider();
    }
}

void UNanoBananaBridgeAsyncAction::HandleCaptured(const FViewportCaptureResult& Capture, const FString& SavedPath)
{
    if (Capture.PngBytes.Num() == 0)
    {
        Fail(TEXT("Failed to capture viewport."));
        return;
    }

    // Attach captured PNG as a reference image (file path so providers can re-load it).
    FNanoBananaReferenceImage Ref;
    Ref.FilePath = SavedPath.IsEmpty() ? InputSavePath : SavedPath;
    Request.ReferenceImages.Insert(Ref, 0);

    OnProgress.Broadcast(0.15f, TEXT("Submitting request"));
    RunProvider();
}

void UNanoBananaBridgeAsyncAction::RunProvider()
{
    Provider = FProviderFactory::Make(Request.Vendor);
    if (!Provider.IsValid())
    {
        Fail(FString::Printf(TEXT("Unsupported vendor: %s"), *FNanoBananaTypeUtils::VendorToString(Request.Vendor)));
        return;
    }

    FProviderCallbacks Cb;
    TWeakObjectPtr<UNanoBananaBridgeAsyncAction> Weak(this);

    Cb.OnProgress = [Weak](float Pct, const FString& Stage)
    {
        AsyncTask(ENamedThreads::GameThread, [Weak, Pct, Stage]()
        {
            if (UNanoBananaBridgeAsyncAction* This = Weak.Get())
            {
                if (!This->bFinished) This->OnProgress.Broadcast(Pct, Stage);
            }
        });
    };
    Cb.OnSuccess = [Weak](TArray<TArray<uint8>> Images, const FString& RawResponse)
    {
        AsyncTask(ENamedThreads::GameThread, [Weak, Images = MoveTemp(Images), RawResponse]() mutable
        {
            if (UNanoBananaBridgeAsyncAction* This = Weak.Get())
            {
                This->HandleSuccess(MoveTemp(Images), RawResponse);
            }
        });
    };
    Cb.OnFailure = [Weak](const FString& Err)
    {
        AsyncTask(ENamedThreads::GameThread, [Weak, Err]()
        {
            if (UNanoBananaBridgeAsyncAction* This = Weak.Get())
            {
                This->Fail(Err);
            }
        });
    };
    Cb.OnRequestBuilt = [Weak](const FString& Body)
    {
        AsyncTask(ENamedThreads::GameThread, [Weak, Body]()
        {
            if (UNanoBananaBridgeAsyncAction* This = Weak.Get())
            {
                This->DumpDebug(TEXT("_request.json"), Body);
            }
        });
    };

    Provider->Submit(Request, Cb);
}

void UNanoBananaBridgeAsyncAction::HandleSuccess(TArray<TArray<uint8>> Images, const FString& RawResponse)
{
    if (bFinished) return;
    if (Images.Num() == 0)
    {
        Fail(TEXT("Empty result image."));
        return;
    }

    DumpDebug(TEXT("_response.json"), RawResponse);

    const UNanoBananaSettings& S = UNanoBananaSettings::Get();
    const FString AbsBaseDir = FPaths::ConvertRelativePathToFull(S.OutputDirectory);
    const FString Ext = FNanoBananaTypeUtils::OutputFormatToExt(Request.OutputFormat);

    OnProgress.Broadcast(0.9f, TEXT("Saving images"));

    // Save all results with consistent timestamped basename.
    const FString Stamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (!PF.DirectoryExists(*AbsBaseDir)) { PF.CreateDirectoryTree(*AbsBaseDir); }

    TArray<FNanoBananaImageResult> Results;
    Results.Reserve(Images.Num());
    for (int32 i = 0; i < Images.Num(); ++i)
    {
        FNanoBananaImageResult R;
        R.PngBytes = MoveTemp(Images[i]);
        const FString Suffix = (Images.Num() == 1)
            ? FString::Printf(TEXT("_Result%s"), *Ext)
            : FString::Printf(TEXT("_Result_%02d%s"), i + 1, *Ext);
        R.SavedPath = AbsBaseDir / FString::Printf(TEXT("NanoBanana_%s%s"), *Stamp, *Suffix);
        FFileHelper::SaveArrayToFile(R.PngBytes, *R.SavedPath);
        R.Texture = FImageUtils::ImportBufferAsTexture2D(R.PngBytes);
        Results.Add(MoveTemp(R));
    }

    // Optional side-by-side composite (only meaningful if there's an input).
    FString CompositePath;
    if (bAlsoSaveComposite && Results.Num() > 0)
    {
        TArray<uint8> InputPng;
        if (!InputSavePath.IsEmpty() && FFileHelper::LoadFileToArray(InputPng, *InputSavePath) && InputPng.Num() > 0)
        {
            TArray<uint8> CompositePng;
            if (UImageComposerLibrary::ComposeSideBySidePNGs(InputPng, Results[0].PngBytes, CompositePng, 8)
                && CompositePng.Num() > 0)
            {
                CompositePath = CompositeSavePath.IsEmpty()
                    ? AbsBaseDir / FString::Printf(TEXT("NanoBanana_%s_Composite.png"), *Stamp)
                    : CompositeSavePath;
                FFileHelper::SaveArrayToFile(CompositePng, *CompositePath);
            }
        }
    }

    bFinished = true;
    OnProgress.Broadcast(1.0f, TEXT("Completed"));
    OnCompleted.Broadcast(Results, CompositePath);
    Provider.Reset();
    SetReadyToDestroy();
}

void UNanoBananaBridgeAsyncAction::Fail(const FString& Error)
{
    if (bFinished) return;
    bFinished = true;
    OnFailed.Broadcast(Error);
    Provider.Reset();
    SetReadyToDestroy();
}

FString UNanoBananaBridgeAsyncAction::MakeTimestampedPath(const FString& BaseDir, const FString& Suffix) const
{
    const FString Stamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
    const FString Dir = FPaths::ConvertRelativePathToFull(BaseDir);
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (!PF.DirectoryExists(*Dir)) { PF.CreateDirectoryTree(*Dir); }
    return Dir / FString::Printf(TEXT("NanoBanana_%s%s"), *Stamp, *Suffix);
}

void UNanoBananaBridgeAsyncAction::DumpDebug(const FString& Suffix, const FString& Body) const
{
    const UNanoBananaSettings& S = UNanoBananaSettings::Get();
    if (!S.bSaveDebugRequestResponse || Body.IsEmpty()) return;
    const FString DebugDir = FPaths::ConvertRelativePathToFull(S.OutputDirectory) / TEXT("Debug");
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (!PF.DirectoryExists(*DebugDir)) { PF.CreateDirectoryTree(*DebugDir); }
    const FString Stamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
    const FString Path = DebugDir / FString::Printf(TEXT("%s%s"), *Stamp, *Suffix);
    FFileHelper::SaveStringToFile(Body, *Path);
}
