#include "NanoBananaBridgeAsyncAction.h"
#include "NanoBananaSettings.h"
#include "ViewportCaptureLibrary.h"
#include "ImageComposerLibrary.h"
#include "ImageUtils.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/BufferArchive.h"
#include "Misc/Base64.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Engine/Engine.h"
#include "GenericPlatform/GenericPlatformHttp.h"

// Helper: recursively collect image inline_data from arbitrary JSON shapes
static void CollectInlineImagesFromJsonObject(const TSharedPtr<FJsonObject>& Obj, TArray<TArray<uint8>>& OutPNGs)
{
    if (!Obj.IsValid()) return;

    // inline_data / inlineData
    auto ParseInlineData = [&OutPNGs](const TSharedPtr<FJsonObject>& Inline)
    {
        if (!Inline.IsValid()) return;
        FString Mime;
        Inline->TryGetStringField(TEXT("mime_type"), Mime);
        if (Mime.IsEmpty()) Inline->TryGetStringField(TEXT("mimeType"), Mime);
        FString DataB64;
        if (Inline->TryGetStringField(TEXT("data"), DataB64))
        {
            if (Mime.IsEmpty() || Mime.StartsWith(TEXT("image/")))
            {
                TArray<uint8> Img;
                if (FBase64::Decode(DataB64, Img) && Img.Num() > 0)
                {
                    OutPNGs.Add(MoveTemp(Img));
                }
            }
        }
    };

    // Direct inline_data object
    ParseInlineData(Obj->GetObjectField(TEXT("inline_data")));
    ParseInlineData(Obj->GetObjectField(TEXT("inlineData")));

    // Other common encodings
    FString B64;
    if (Obj->TryGetStringField(TEXT("b64_json"), B64) || Obj->TryGetStringField(TEXT("bytesBase64"), B64))
    {
        TArray<uint8> Img;
        if (FBase64::Decode(B64, Img) && Img.Num() > 0)
        {
            OutPNGs.Add(MoveTemp(Img));
        }
    }

    // Recurse into arrays and objects
    for (const auto& KVP : Obj->Values)
    {
        const TSharedPtr<FJsonValue>& Val = KVP.Value;
        if (!Val.IsValid()) continue;
        if (Val->Type == EJson::Object)
        {
            CollectInlineImagesFromJsonObject(Val->AsObject(), OutPNGs);
        }
        else if (Val->Type == EJson::Array)
        {
            const TArray<TSharedPtr<FJsonValue>> Arr = Val->AsArray();
            for (const TSharedPtr<FJsonValue>& Elem : Arr)
            {
                if (Elem.IsValid() && Elem->Type == EJson::Object)
                {
                    CollectInlineImagesFromJsonObject(Elem->AsObject(), OutPNGs);
                }
            }
        }
    }
}

UNanoBananaBridgeAsyncAction* UNanoBananaBridgeAsyncAction::CaptureAndGenerate(UObject* InWorldContextObject, const FString& InPrompt, bool bInShowUI)
{
    UNanoBananaBridgeAsyncAction* Action = NewObject<UNanoBananaBridgeAsyncAction>();
    Action->WorldContextObject = InWorldContextObject;
    Action->Prompt = InPrompt;
    Action->bShowUI = bInShowUI;
    // Pull defaults from settings
    if (UNanoBananaSettings* Settings = GetMutableDefault<UNanoBananaSettings>())
    {
        Action->NumImages = FMath::Max(1, Settings->DefaultNumImages);
        Action->OutWidth = FMath::Max(1, Settings->DefaultWidth);
        Action->OutHeight = FMath::Max(1, Settings->DefaultHeight);
        Action->NegativePrompt = Settings->DefaultNegativePrompt;
    }
    Action->RegisterWithGameInstance(InWorldContextObject);
    return Action;
}

UNanoBananaBridgeAsyncAction* UNanoBananaBridgeAsyncAction::CaptureAndGenerateAdvanced(UObject* InWorldContextObject, const FString& InPrompt, UTextureRenderTarget2D* InOptionalMask, int32 InNumImages, int32 InOutWidth, int32 InOutHeight, const FString& InNegativePrompt, bool bInShowUI)
{
    UNanoBananaBridgeAsyncAction* Action = NewObject<UNanoBananaBridgeAsyncAction>();
    Action->WorldContextObject = InWorldContextObject;
    Action->Prompt = InPrompt;
    Action->bShowUI = bInShowUI;
    Action->OptionalMask = InOptionalMask;
    Action->NumImages = FMath::Max(1, InNumImages);
    Action->OutWidth = FMath::Max(1, InOutWidth);
    Action->OutHeight = FMath::Max(1, InOutHeight);
    Action->NegativePrompt = InNegativePrompt;
    Action->RegisterWithGameInstance(InWorldContextObject);
    return Action;
}

void UNanoBananaBridgeAsyncAction::Activate()
{
    // Step 1: Capture viewport
    OnProgress.Broadcast(0.05f, TEXT("Capturing viewport"));

    UNanoBananaSettings* Settings = GetMutableDefault<UNanoBananaSettings>();
    FString BaseDir = Settings ? Settings->OutputDirectory : TEXT("Saved/NanoBanana");
    FString AbsBaseDir = FPaths::ConvertRelativePathToFull(BaseDir);

    InputSavePath = InputSavePath.IsEmpty() ? MakeTimestampedPath(AbsBaseDir, TEXT("_Input.png")) : InputSavePath;

    UViewportCaptureLibrary::CaptureCurrentViewportToPNG(WorldContextObject.Get(), bShowUI, InputSavePath,
        FOnViewportCaptured::CreateUObject(this, &UNanoBananaBridgeAsyncAction::HandleCaptured));
}

void UNanoBananaBridgeAsyncAction::HandleCaptured(const FViewportCaptureResult& Capture, const FString& SavedPath)
{
    if (Capture.PngBytes.Num() == 0)
    {
        OnFailed.Broadcast(TEXT("Failed to capture viewport."));
        SetReadyToDestroy();
        return;
    }
    OnProgress.Broadcast(0.2f, TEXT("Sending to Nano Banana"));
    SendToService(Capture.PngBytes);
}

void UNanoBananaBridgeAsyncAction::SendToService(const TArray<uint8>& InputPNG)
{
    UNanoBananaSettings* Settings = GetMutableDefault<UNanoBananaSettings>();
    if (!Settings || Settings->ApiBaseUrl.IsEmpty())
    {
        OnFailed.Broadcast(TEXT("Nano Banana settings missing ApiBaseUrl."));
        SetReadyToDestroy();
        return;
    }

    const bool bIsGoogle = Settings->ApiBaseUrl.Contains(TEXT("generativelanguage.googleapis.com"));
    FString Url = Settings->ApiBaseUrl;
    if (bIsGoogle && !Settings->ApiKey.IsEmpty())
    {
        // Append key as query param for Google Gemini API
        Url += (Url.Contains(TEXT("?")) ? TEXT("&") : TEXT("?"));
        Url += TEXT("key=") + FGenericPlatformHttp::UrlEncode(Settings->ApiKey);
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    if (!bIsGoogle && !Settings->ApiKey.IsEmpty())
    {
        // For custom providers, support Bearer header
        Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Settings->ApiKey));
    }

    FString ImageB64 = FBase64::Encode(InputPNG);
    FString BodyStr;
    const bool bUseGenerateContent = bIsGoogle && (Settings->bUseGenerateContent || Url.Contains(TEXT(":generateContent")));
    if (!bUseGenerateContent)
    {
        OnFailed.Broadcast(TEXT("Only Gemini 2.5 Flash via :generateContent is supported."));
        SetReadyToDestroy();
        return;
    }

    if (bUseGenerateContent)
    {
        // Google Gemini 2.5 Flash (generateContent) image-to-image with image_generation tool
        TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
        // tools: [ { image_generation: {} } ]
        {
            TArray<TSharedPtr<FJsonValue>> Tools;
            TSharedPtr<FJsonObject> ToolObj = MakeShared<FJsonObject>();
            ToolObj->SetObjectField(TEXT("image_generation"), MakeShared<FJsonObject>());
            Tools.Add(MakeShared<FJsonValueObject>(ToolObj));
            Root->SetArrayField(TEXT("tools"), Tools);
        }

        // contents: user parts: text + inline_data (PNG)
        TArray<TSharedPtr<FJsonValue>> Contents;
        TSharedPtr<FJsonObject> UserContent = MakeShared<FJsonObject>();
        UserContent->SetStringField(TEXT("role"), TEXT("user"));
        TArray<TSharedPtr<FJsonValue>> Parts;
        // Prompt text part
        {
            TSharedPtr<FJsonObject> PartText = MakeShared<FJsonObject>();
            PartText->SetStringField(TEXT("text"), Prompt);
            Parts.Add(MakeShared<FJsonValueObject>(PartText));
        }
        // Inline image part (source image for img2img)
        {
            TSharedPtr<FJsonObject> InlineData = MakeShared<FJsonObject>();
            InlineData->SetStringField(TEXT("mime_type"), TEXT("image/png"));
            InlineData->SetStringField(TEXT("data"), ImageB64);
            TSharedPtr<FJsonObject> PartImage = MakeShared<FJsonObject>();
            PartImage->SetObjectField(TEXT("inline_data"), InlineData);
            Parts.Add(MakeShared<FJsonValueObject>(PartImage));
        }
        UserContent->SetArrayField(TEXT("parts"), Parts);
        Contents.Add(MakeShared<FJsonValueObject>(UserContent));
        Root->SetArrayField(TEXT("contents"), Contents);

        // generationConfig: prefer PNG, candidate count as a hint (set both snake/camel for compatibility)
        {
            TSharedPtr<FJsonObject> Gen = MakeShared<FJsonObject>();
            Gen->SetStringField(TEXT("response_mime_type"), TEXT("image/png"));
            Gen->SetStringField(TEXT("responseMimeType"), TEXT("image/png"));
            Gen->SetNumberField(TEXT("candidate_count"), FMath::Max(1, NumImages));
            Gen->SetNumberField(TEXT("candidateCount"), FMath::Max(1, NumImages));
            Root->SetObjectField(TEXT("generationConfig"), Gen);
            Root->SetObjectField(TEXT("generation_config"), Gen);
        }

        // tool_config: image generation parameters (snake_case per docs)
        {
            TSharedPtr<FJsonObject> ImgGenCfg = MakeShared<FJsonObject>();
            ImgGenCfg->SetNumberField(TEXT("number_of_images"), FMath::Max(1, NumImages));
            // Dimensions
            const FString SizeStr = FString::Printf(TEXT("%dx%d"), FMath::Max(1, OutWidth), FMath::Max(1, OutHeight));
            ImgGenCfg->SetStringField(TEXT("size"), SizeStr); // Some examples accept a size string
            TSharedPtr<FJsonObject> DimObj = MakeShared<FJsonObject>();
            DimObj->SetNumberField(TEXT("width"), FMath::Max(1, OutWidth));
            DimObj->SetNumberField(TEXT("height"), FMath::Max(1, OutHeight));
            ImgGenCfg->SetObjectField(TEXT("image_dimensions"), DimObj);
            if (!NegativePrompt.IsEmpty())
            {
                ImgGenCfg->SetStringField(TEXT("negative_prompt"), NegativePrompt);
            }

            // Optional mask
            if (OptionalMask)
            {
                TArray<uint8> MaskPNG; int32 MW=0, MH=0; 
                if (UViewportCaptureLibrary::RenderTargetToPNG(OptionalMask, MaskPNG, MW, MH, true) && MaskPNG.Num() > 0)
                {
                    const FString MaskB64 = FBase64::Encode(MaskPNG);
                    TSharedPtr<FJsonObject> InlineMask = MakeShared<FJsonObject>();
                    InlineMask->SetStringField(TEXT("mime_type"), TEXT("image/png"));
                    InlineMask->SetStringField(TEXT("data"), MaskB64);
                    TSharedPtr<FJsonObject> MaskObj = MakeShared<FJsonObject>();
                    MaskObj->SetObjectField(TEXT("inline_data"), InlineMask);
                    ImgGenCfg->SetObjectField(TEXT("mask"), MaskObj);
                }
            }

            // Attach Image Generation config under documented snake_case key
            TSharedPtr<FJsonObject> ToolCfgSnake = MakeShared<FJsonObject>();
            ToolCfgSnake->SetObjectField(TEXT("image_generation"), ImgGenCfg);
            Root->SetObjectField(TEXT("tool_config"), ToolCfgSnake);
        }

        // Merge advanced JSON fragment from settings (top-level shallow merge)
        if (!Settings->AdvancedRequestJSON.IsEmpty())
        {
            TSharedPtr<FJsonObject> Extra;
            TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Settings->AdvancedRequestJSON);
            if (FJsonSerializer::Deserialize(R, Extra) && Extra.IsValid())
            {
                for (const auto& KVP : Extra->Values)
                {
                    Root->SetField(KVP.Key, KVP.Value);
                }
            }
        }

        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
        FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
        Req->SetContentAsString(BodyStr);
    }

    Req->OnRequestProgress().BindLambda([this](FHttpRequestPtr, int32 BytesSent, int32 BytesReceived)
    {
        // Simple heuristic progress up to 60%
        float P = FMath::Clamp(BytesSent / 1024.0f / 1024.0f, 0.0f, 1.0f);
        OnProgress.Broadcast(0.2f + 0.4f * P, TEXT("Uploading"));
    });

    Req->OnProcessRequestComplete().BindUObject(this, &UNanoBananaBridgeAsyncAction::HandleServiceResponse);
    Req->ProcessRequest();
}

void UNanoBananaBridgeAsyncAction::HandleServiceResponse(TSharedPtr<IHttpRequest> Req, TSharedPtr<IHttpResponse> Resp, bool bSucceeded)
{
    if (!bSucceeded || !Resp.IsValid())
    {
        OnFailed.Broadcast(TEXT("Nano Banana request failed."));
        SetReadyToDestroy();
        return;
    }

    const int32 Code = Resp->GetResponseCode();
    if (Code < 200 || Code >= 300)
    {
        OnFailed.Broadcast(FString::Printf(TEXT("Nano Banana HTTP %d"), Code));
        SetReadyToDestroy();
        return;
    }

    OnProgress.Broadcast(0.7f, TEXT("Processing response"));

    TArray<uint8> ResultPNG;
    TArray<TArray<uint8>> AllPNGs;

    FString CT = Resp->GetContentType();
    const FString RespStr = Resp->GetContentAsString();
    const bool bIsGoogle = Req->GetURL().Contains(TEXT("generativelanguage.googleapis.com"));

    if (CT.Contains(TEXT("image/")))
    {
        ResultPNG = Resp->GetContent();
        if (ResultPNG.Num() > 0) { AllPNGs.Add(ResultPNG); }
    }
    else if (bIsGoogle)
    {
        // Parse Google Gemini Images response: candidates[0].content.parts[*].inline_data.data
        TSharedPtr<FJsonObject> Json;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RespStr);
        if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
        {
            CollectInlineImagesFromJsonObject(Json, AllPNGs);
        }
    }
    else
    {
        // Fallback: JSON with a top-level 'image' base64 field
        TSharedPtr<FJsonObject> Json;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RespStr);
        if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
        {
            FString B64;
            if (Json->TryGetStringField(TEXT("image"), B64))
            {
                FBase64::Decode(B64, ResultPNG);
                if (ResultPNG.Num() > 0) { AllPNGs.Add(ResultPNG); }
            }
        }
    }

    if (AllPNGs.Num() == 0)
    {
        OnFailed.Broadcast(TEXT("Empty result image from Nano Banana."));
        SetReadyToDestroy();
        return;
    }

    UNanoBananaSettings* Settings = GetMutableDefault<UNanoBananaSettings>();
    FString BaseDir = Settings ? Settings->OutputDirectory : TEXT("Saved/NanoBanana");
    FString AbsBaseDir = FPaths::ConvertRelativePathToFull(BaseDir);

    // Save results (all). First image uses base path; others get an index suffix.
    FString BaseResultPath = ResultSavePath;
    if (BaseResultPath.IsEmpty())
    {
        BaseResultPath = MakeTimestampedPath(AbsBaseDir, TEXT("_Result.png"));
    }

    auto IndexedPath = [](const FString& BasePath, int32 Index) -> FString
    {
        if (Index == 0) return BasePath;
        const FString Dir = FPaths::GetPath(BasePath);
        const FString Name = FPaths::GetBaseFilename(BasePath, false);
        const FString Ext = FPaths::GetExtension(BasePath, true); // includes dot
        return Dir / FString::Printf(TEXT("%s_%02d%s"), *Name, Index+1, *Ext);
    };

    for (int32 i = 0; i < AllPNGs.Num(); ++i)
    {
        const FString Path = IndexedPath(BaseResultPath, i);
        if (i == 0) { ResultSavePath = Path; }
        FFileHelper::SaveArrayToFile(AllPNGs[i], *Path);
    }

    OnProgress.Broadcast(0.85f, TEXT("Composing side-by-side"));

    // Load input back if exists, otherwise skip composition
    TArray<uint8> InputPNG;
    if (!InputSavePath.IsEmpty())
    {
        FFileHelper::LoadFileToArray(InputPNG, *InputSavePath);
    }

    TArray<uint8> CompositePNG;
    if (InputPNG.Num() > 0 && AllPNGs.Num() > 0)
    {
        UImageComposerLibrary::ComposeSideBySidePNGs(InputPNG, AllPNGs[0], CompositePNG, 8);
        if (CompositePNG.Num() > 0)
        {
            CompositeSavePath = CompositeSavePath.IsEmpty() ? MakeTimestampedPath(AbsBaseDir, TEXT("_Composite.png")) : CompositeSavePath;
            FFileHelper::SaveArrayToFile(CompositePNG, *CompositeSavePath);
        }
    }

    UTexture2D* ResultTexture = FImageUtils::ImportBufferAsTexture2D(AllPNGs[0]);
    OnProgress.Broadcast(1.0f, TEXT("Completed"));
    OnCompleted.Broadcast(ResultTexture, ResultSavePath, CompositeSavePath);
    SetReadyToDestroy();
}

FString UNanoBananaBridgeAsyncAction::MakeTimestampedPath(const FString& BaseDir, const FString& Suffix) const
{
    const FString Stamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
    FString Dir = FPaths::ConvertRelativePathToFull(BaseDir);
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (!PF.DirectoryExists(*Dir))
    {
        PF.CreateDirectoryTree(*Dir);
    }
    return Dir / FString::Printf(TEXT("NanoBanana_%s%s"), *Stamp, *Suffix);
}


