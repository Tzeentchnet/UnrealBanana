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

UNanoBananaBridgeAsyncAction* UNanoBananaBridgeAsyncAction::CaptureAndGenerate(UObject* InWorldContextObject, const FString& InPrompt, bool bInShowUI)
{
    UNanoBananaBridgeAsyncAction* Action = NewObject<UNanoBananaBridgeAsyncAction>();
    Action->WorldContextObject = InWorldContextObject;
    Action->Prompt = InPrompt;
    Action->bShowUI = bInShowUI;
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
    if (bIsGoogle)
    {
        // Google Gemini Images style: contents.parts with text + inline_data
        TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
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
        // Inline image part (reference / guidance)
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

        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
        FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
        Req->SetContentAsString(BodyStr);
    }
    else
    {
        // Custom provider contract (prompt + image fields)
        TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
        Body->SetStringField(TEXT("prompt"), Prompt);
        Body->SetStringField(TEXT("image"), ImageB64);
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
        FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);
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

    FString CT = Resp->GetContentType();
    const FString RespStr = Resp->GetContentAsString();
    const bool bIsGoogle = Req->GetURL().Contains(TEXT("generativelanguage.googleapis.com"));

    if (CT.Contains(TEXT("image/")))
    {
        ResultPNG = Resp->GetContent();
    }
    else if (bIsGoogle)
    {
        // Parse Google Gemini Images response: candidates[0].content.parts[*].inline_data.data
        TSharedPtr<FJsonObject> Json;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RespStr);
        if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
        {
            const TArray<TSharedPtr<FJsonValue>>* Candidates;
            if (Json->TryGetArrayField(TEXT("candidates"), Candidates) && Candidates->Num() > 0)
            {
                TSharedPtr<FJsonObject> CandObj = (*Candidates)[0]->AsObject();
                if (CandObj.IsValid())
                {
                    TSharedPtr<FJsonObject> ContentObj = CandObj->GetObjectField(TEXT("content"));
                    if (ContentObj.IsValid())
                    {
                        const TArray<TSharedPtr<FJsonValue>>* Parts;
                        if (ContentObj->TryGetArrayField(TEXT("parts"), Parts))
                        {
                            for (const TSharedPtr<FJsonValue>& Val : *Parts)
                            {
                                TSharedPtr<FJsonObject> PartObj = Val->AsObject();
                                if (!PartObj.IsValid()) continue;
                                TSharedPtr<FJsonObject> InlineObj = PartObj->GetObjectField(TEXT("inline_data"));
                                if (!InlineObj.IsValid()) InlineObj = PartObj->GetObjectField(TEXT("inlineData"));
                                if (InlineObj.IsValid())
                                {
                                    FString Mime;
                                    InlineObj->TryGetStringField(TEXT("mime_type"), Mime);
                                    if (Mime.IsEmpty()) InlineObj->TryGetStringField(TEXT("mimeType"), Mime);
                                    if (Mime.StartsWith(TEXT("image/")))
                                    {
                                        FString DataB64;
                                        if (InlineObj->TryGetStringField(TEXT("data"), DataB64))
                                        {
                                            FBase64::Decode(DataB64, ResultPNG);
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
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
            }
        }
    }

    if (ResultPNG.Num() == 0)
    {
        OnFailed.Broadcast(TEXT("Empty result image from Nano Banana."));
        SetReadyToDestroy();
        return;
    }

    UNanoBananaSettings* Settings = GetMutableDefault<UNanoBananaSettings>();
    FString BaseDir = Settings ? Settings->OutputDirectory : TEXT("Saved/NanoBanana");
    FString AbsBaseDir = FPaths::ConvertRelativePathToFull(BaseDir);

    // Save result
    ResultSavePath = ResultSavePath.IsEmpty() ? MakeTimestampedPath(AbsBaseDir, TEXT("_Result.png")) : ResultSavePath;
    FFileHelper::SaveArrayToFile(ResultPNG, *ResultSavePath);

    OnProgress.Broadcast(0.85f, TEXT("Composing side-by-side"));

    // Load input back if exists, otherwise skip composition
    TArray<uint8> InputPNG;
    if (!InputSavePath.IsEmpty())
    {
        FFileHelper::LoadFileToArray(InputPNG, *InputSavePath);
    }

    TArray<uint8> CompositePNG;
    if (InputPNG.Num() > 0)
    {
        UImageComposerLibrary::ComposeSideBySidePNGs(InputPNG, ResultPNG, CompositePNG, 8);
        if (CompositePNG.Num() > 0)
        {
            CompositeSavePath = CompositeSavePath.IsEmpty() ? MakeTimestampedPath(AbsBaseDir, TEXT("_Composite.png")) : CompositeSavePath;
            FFileHelper::SaveArrayToFile(CompositePNG, *CompositeSavePath);
        }
    }

    UTexture2D* ResultTexture = FImageUtils::ImportBufferAsTexture2D(ResultPNG);
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
