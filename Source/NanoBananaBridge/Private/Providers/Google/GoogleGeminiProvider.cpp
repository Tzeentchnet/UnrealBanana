#include "GoogleGeminiProvider.h"
#include "../../Http/Base64Image.h"
#include "../../Http/JsonResponseScanner.h"
#include "NanoBananaSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Base64.h"
#include "GenericPlatform/GenericPlatformHttp.h"

FString FGoogleGeminiProvider::ResolveModelId(ENanoBananaModel Model, const FString& CustomModelId)
{
    switch (Model)
    {
    case ENanoBananaModel::NanoBanana:    return TEXT("gemini-2.5-flash-image");
    case ENanoBananaModel::NanoBanana2:   return TEXT("gemini-3.1-flash-image-preview");
    case ENanoBananaModel::NanoBananaPro: return TEXT("gemini-3-pro-image-preview");
    case ENanoBananaModel::Custom:        return CustomModelId.IsEmpty() ? TEXT("gemini-2.5-flash-image") : CustomModelId;
    default:                              return TEXT("gemini-2.5-flash-image");
    }
}

FString FGoogleGeminiProvider::BuildEndpointUrl(const FString& BaseUrlOverride, const FString& ModelId, const FString& ApiKey)
{
    FString Base = BaseUrlOverride.IsEmpty()
        ? TEXT("https://generativelanguage.googleapis.com/v1beta")
        : BaseUrlOverride.TrimEnd();
    if (Base.EndsWith(TEXT("/"))) Base.LeftChopInline(1);

    FString Url = FString::Printf(TEXT("%s/models/%s:generateContent"), *Base, *ModelId);
    if (!ApiKey.IsEmpty())
    {
        Url += TEXT("?key=") + FGenericPlatformHttp::UrlEncode(ApiKey);
    }
    return Url;
}

FString FGoogleGeminiProvider::BuildRequestJson(const FNanoBananaRequest& Request, const TArray<TArray<uint8>>& EncodedReferences)
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();

    // contents: [ { role: "user", parts: [ {text}, {inlineData}*N ] } ]
    TArray<TSharedPtr<FJsonValue>> Parts;
    {
        TSharedPtr<FJsonObject> TextPart = MakeShared<FJsonObject>();
        // Always include prompt text — combine with negative if present.
        FString FullPrompt = Request.Prompt;
        if (!Request.NegativePrompt.IsEmpty())
        {
            FullPrompt += FString::Printf(TEXT("\n\nNegative: %s"), *Request.NegativePrompt);
        }
        TextPart->SetStringField(TEXT("text"), FullPrompt);
        Parts.Add(MakeShared<FJsonValueObject>(TextPart));
    }
    for (const TArray<uint8>& Img : EncodedReferences)
    {
        if (Img.Num() == 0) continue;
        const FString Mime = NanoBanana::Image::SniffImageMimeType(Img);
        TSharedPtr<FJsonObject> Inline = MakeShared<FJsonObject>();
        Inline->SetStringField(TEXT("mimeType"), Mime);
        Inline->SetStringField(TEXT("mime_type"), Mime); // tolerate both casings server-side
        Inline->SetStringField(TEXT("data"), FBase64::Encode(Img));
        TSharedPtr<FJsonObject> Part = MakeShared<FJsonObject>();
        Part->SetObjectField(TEXT("inlineData"), Inline);
        Part->SetObjectField(TEXT("inline_data"), Inline);
        Parts.Add(MakeShared<FJsonValueObject>(Part));
    }

    TSharedPtr<FJsonObject> UserContent = MakeShared<FJsonObject>();
    UserContent->SetStringField(TEXT("role"), TEXT("user"));
    UserContent->SetArrayField(TEXT("parts"), Parts);
    TArray<TSharedPtr<FJsonValue>> Contents;
    Contents.Add(MakeShared<FJsonValueObject>(UserContent));
    Root->SetArrayField(TEXT("contents"), Contents);

    // generationConfig
    {
        TSharedPtr<FJsonObject> Gen = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> Modalities;
        Modalities.Add(MakeShared<FJsonValueString>(TEXT("IMAGE")));
        Modalities.Add(MakeShared<FJsonValueString>(TEXT("TEXT")));
        Gen->SetArrayField(TEXT("responseModalities"), Modalities);
        Gen->SetNumberField(TEXT("candidateCount"), FMath::Max(1, Request.NumImages));
        if (Request.Seed != 0)
        {
            Gen->SetNumberField(TEXT("seed"), Request.Seed);
        }
        Root->SetObjectField(TEXT("generationConfig"), Gen);
    }

    // imageConfig (Gemini 3+)
    {
        TSharedPtr<FJsonObject> ImgCfg = MakeShared<FJsonObject>();
        if (Request.Aspect != ENanoBananaAspect::Auto)
        {
            ImgCfg->SetStringField(TEXT("aspectRatio"), FNanoBananaTypeUtils::AspectToString(Request.Aspect));
        }
        if (Request.Resolution != ENanoBananaResolution::Auto)
        {
            ImgCfg->SetStringField(TEXT("imageSize"), FNanoBananaTypeUtils::ResolutionToString(Request.Resolution));
        }
        if (ImgCfg->Values.Num() > 0)
        {
            Root->SetObjectField(TEXT("imageConfig"), ImgCfg);
        }
    }

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
    return Out;
}

void FGoogleGeminiProvider::Submit(const FNanoBananaRequest& Request, const FProviderCallbacks& Callbacks)
{
    const UNanoBananaSettings& S = UNanoBananaSettings::Get();
    const FString ApiKey = S.GetEffectiveApiKey(ENanoBananaVendor::Google);
    if (ApiKey.IsEmpty())
    {
        if (Callbacks.OnFailure) Callbacks.OnFailure(TEXT("Google: missing API key (set in Project Settings or GEMINI_API_KEY env var)."));
        return;
    }

    // Resolve all references to image bytes (texture / RT / file).
    TArray<TArray<uint8>> Refs;
    NanoBanana::Image::ResolveAllReferences(Request, Refs);

    const FString ModelId = ResolveModelId(Request.Model, Request.CustomModelId);
    const FString Url = BuildEndpointUrl(S.Google.BaseUrlOverride, ModelId, ApiKey);
    const FString Body = BuildRequestJson(Request, Refs);

    if (Callbacks.OnRequestBuilt) Callbacks.OnRequestBuilt(Body);
    if (Callbacks.OnProgress) Callbacks.OnProgress(0.2f, FString::Printf(TEXT("Calling Gemini %s"), *ModelId));

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
    InFlight = Req;
    Req->SetURL(Url);
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Req->SetTimeout((float)FMath::Max(5, S.RequestTimeoutSeconds));
    Req->SetContentAsString(Body);

    TWeakPtr<FGoogleGeminiProvider, ESPMode::ThreadSafe> WeakThis = StaticCastSharedRef<FGoogleGeminiProvider>(AsShared());
    Req->OnProcessRequestComplete().BindLambda(
        [WeakThis, Callbacks](FHttpRequestPtr, FHttpResponsePtr Resp, bool bSucceeded)
        {
            TSharedPtr<FGoogleGeminiProvider, ESPMode::ThreadSafe> Pinned = WeakThis.Pin();
            if (!Pinned.IsValid() || Pinned->bCanceled) return;
            Pinned->InFlight.Reset();

            if (!bSucceeded || !Resp.IsValid())
            {
                if (Callbacks.OnFailure) Callbacks.OnFailure(TEXT("Gemini request failed (network)."));
                return;
            }
            const int32 Code = Resp->GetResponseCode();
            const FString RespStr = Resp->GetContentAsString();
            if (Code < 200 || Code >= 300)
            {
                if (Callbacks.OnFailure) Callbacks.OnFailure(FString::Printf(TEXT("Gemini HTTP %d: %s"), Code, *RespStr.Left(512)));
                return;
            }

            if (Callbacks.OnProgress) Callbacks.OnProgress(0.85f, TEXT("Decoding Gemini response"));

            TArray<TArray<uint8>> Images;
            NanoBanana::Json::CollectInlineImagesFromResponseBody(RespStr, Images);
            if (Images.Num() == 0)
            {
                if (Callbacks.OnFailure) Callbacks.OnFailure(FString::Printf(TEXT("Gemini returned no images. Body: %s"), *RespStr.Left(512)));
                return;
            }
            if (Callbacks.OnSuccess) Callbacks.OnSuccess(MoveTemp(Images), RespStr);
        });
    Req->ProcessRequest();
}

void FGoogleGeminiProvider::Cancel()
{
    bCanceled = true;
    if (InFlight.IsValid())
    {
        InFlight->CancelRequest();
        InFlight.Reset();
    }
}
