#include "ReplicateProvider.h"
#include "../../Http/Base64Image.h"
#include "../../Http/PollLoop.h"
#include "NanoBananaSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FString FReplicateProvider::ResolveModelSlug(ENanoBananaModel Model, const FString& CustomModelId)
{
    switch (Model)
    {
    case ENanoBananaModel::NanoBanana:    return TEXT("google/nano-banana");
    case ENanoBananaModel::NanoBanana2:   return TEXT("google/nano-banana"); // Replicate currently maps both fast tiers to nano-banana
    case ENanoBananaModel::NanoBananaPro: return TEXT("google/nano-banana-pro");
    case ENanoBananaModel::Custom:        return CustomModelId.IsEmpty() ? TEXT("google/nano-banana") : CustomModelId;
    default:                              return TEXT("google/nano-banana");
    }
}

FString FReplicateProvider::ResolveVersionOverride(ENanoBananaModel Model)
{
    const UNanoBananaSettings& S = UNanoBananaSettings::Get();
    switch (Model)
    {
    case ENanoBananaModel::NanoBanana:
    case ENanoBananaModel::NanoBanana2:
        return S.Replicate.NanoBananaVersionHash;
    case ENanoBananaModel::NanoBananaPro:
        return S.Replicate.NanoBananaProVersionHash;
    default:
        return FString();
    }
}

FString FReplicateProvider::BuildPredictionsUrl(const FString& Override)
{
    FString Base = Override.IsEmpty() ? TEXT("https://api.replicate.com/v1") : Override;
    if (Base.EndsWith(TEXT("/"))) Base.LeftChopInline(1);
    return Base + TEXT("/predictions");
}

FString FReplicateProvider::BuildRequestJson(const FNanoBananaRequest& Request, const TArray<TArray<uint8>>& EncodedReferences)
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();

    const FString Slug = ResolveModelSlug(Request.Model, Request.CustomModelId);
    const FString VersionHash = ResolveVersionOverride(Request.Model);
    if (!VersionHash.IsEmpty())
    {
        Root->SetStringField(TEXT("version"), VersionHash);
    }
    else
    {
        Root->SetStringField(TEXT("model"), Slug);
    }

    TSharedPtr<FJsonObject> Input = MakeShared<FJsonObject>();
    Input->SetStringField(TEXT("prompt"), Request.Prompt);
    if (Request.NumImages > 1)
    {
        Input->SetNumberField(TEXT("num_outputs"), Request.NumImages);
    }
    if (Request.Aspect != ENanoBananaAspect::Auto)
    {
        Input->SetStringField(TEXT("aspect_ratio"), FNanoBananaTypeUtils::AspectToString(Request.Aspect));
    }
    if (Request.Resolution != ENanoBananaResolution::Auto)
    {
        const int32 Px = FNanoBananaTypeUtils::ResolutionToPixels(Request.Resolution);
        if (Px > 0)
        {
            Input->SetNumberField(TEXT("width"), Px);
            Input->SetNumberField(TEXT("height"), Px);
        }
    }
    if (Request.Seed != 0)
    {
        Input->SetNumberField(TEXT("seed"), Request.Seed);
    }
    if (!Request.NegativePrompt.IsEmpty())
    {
        Input->SetStringField(TEXT("negative_prompt"), Request.NegativePrompt);
    }
    Input->SetStringField(TEXT("output_format"),
        Request.OutputFormat == ENanoBananaOutputFormat::JPEG ? TEXT("jpg") :
        Request.OutputFormat == ENanoBananaOutputFormat::WebP ? TEXT("webp") : TEXT("png"));

    if (EncodedReferences.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> Imgs;
        for (const TArray<uint8>& Img : EncodedReferences)
        {
            if (Img.Num() == 0) continue;
            const FString Mime = NanoBanana::Image::SniffImageMimeType(Img);
            const FString Uri = NanoBanana::Image::PngBytesToDataUri(Img, Mime);
            Imgs.Add(MakeShared<FJsonValueString>(Uri));
        }
        if (Imgs.Num() > 0)
        {
            Input->SetArrayField(TEXT("image_input"), Imgs);
            Input->SetStringField(TEXT("image"), Imgs[0]->AsString()); // alias for single-image models
        }
    }

    Root->SetObjectField(TEXT("input"), Input);

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
    return Out;
}

void FReplicateProvider::Submit(const FNanoBananaRequest& Request, const FProviderCallbacks& Callbacks)
{
    const UNanoBananaSettings& S = UNanoBananaSettings::Get();
    const FString ApiKey = S.GetEffectiveApiKey(ENanoBananaVendor::Replicate);
    if (ApiKey.IsEmpty())
    {
        if (Callbacks.OnFailure) Callbacks.OnFailure(TEXT("Replicate: missing API key (set in Project Settings or REPLICATE_API_TOKEN env var)."));
        return;
    }

    TArray<TArray<uint8>> Refs;
    NanoBanana::Image::ResolveAllReferences(Request, Refs);

    const FString Body = BuildRequestJson(Request, Refs);
    if (Callbacks.OnRequestBuilt) Callbacks.OnRequestBuilt(Body);

    if (Callbacks.OnProgress) Callbacks.OnProgress(0.2f, TEXT("Replicate submit"));

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
    InFlight = Req;
    Req->SetURL(BuildPredictionsUrl(S.Replicate.BaseUrlOverride));
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
    if (S.Replicate.bPreferSyncWait)
    {
        Req->SetHeader(TEXT("Prefer"), TEXT("wait"));
    }
    Req->SetTimeout((float)FMath::Max(5, S.RequestTimeoutSeconds + 5));
    Req->SetContentAsString(Body);

    TWeakPtr<FReplicateProvider, ESPMode::ThreadSafe> WeakThis = StaticCastSharedRef<FReplicateProvider>(AsShared());
    Req->OnProcessRequestComplete().BindLambda(
        [WeakThis, Callbacks, ApiKey](FHttpRequestPtr, FHttpResponsePtr Resp, bool bSucceeded)
        {
            TSharedPtr<FReplicateProvider, ESPMode::ThreadSafe> P = WeakThis.Pin();
            if (!P.IsValid() || P->bCanceled) return;
            P->InFlight.Reset();
            if (!bSucceeded || !Resp.IsValid())
            {
                if (Callbacks.OnFailure) Callbacks.OnFailure(TEXT("Replicate request failed (network)."));
                return;
            }
            const int32 Code = Resp->GetResponseCode();
            const FString RespStr = Resp->GetContentAsString();
            if (Code < 200 || Code >= 300)
            {
                if (Callbacks.OnFailure) Callbacks.OnFailure(FString::Printf(TEXT("Replicate HTTP %d: %s"), Code, *RespStr.Left(512)));
                return;
            }
            P->HandleInitialResponse(RespStr, ApiKey, Callbacks);
        });
    Req->ProcessRequest();
}

void FReplicateProvider::HandleInitialResponse(const FString& Body, const FString& ApiKey, const FProviderCallbacks& Callbacks)
{
    TSharedPtr<FJsonObject> Json;
    TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Body);
    if (!FJsonSerializer::Deserialize(R, Json) || !Json.IsValid())
    {
        if (Callbacks.OnFailure) Callbacks.OnFailure(TEXT("Replicate: failed to parse response JSON."));
        return;
    }

    FString Status; Json->TryGetStringField(TEXT("status"), Status);
    const bool bTerminalSuccess = Status.Equals(TEXT("succeeded"), ESearchCase::IgnoreCase);
    const bool bTerminalFail    = Status.Equals(TEXT("failed"), ESearchCase::IgnoreCase) || Status.Equals(TEXT("canceled"), ESearchCase::IgnoreCase);

    if (bTerminalSuccess)
    {
        HandleTerminalPrediction(Body, Callbacks);
        return;
    }
    if (bTerminalFail)
    {
        FString Err; Json->TryGetStringField(TEXT("error"), Err);
        if (Callbacks.OnFailure) Callbacks.OnFailure(FString::Printf(TEXT("Replicate prediction %s: %s"), *Status, *Err.Left(256)));
        return;
    }

    // Need to poll urls.get
    const TSharedPtr<FJsonObject>* UrlsObj = nullptr;
    FString GetUrl;
    if (Json->TryGetObjectField(TEXT("urls"), UrlsObj) && UrlsObj && (*UrlsObj).IsValid())
    {
        (*UrlsObj)->TryGetStringField(TEXT("get"), GetUrl);
    }
    if (GetUrl.IsEmpty())
    {
        FString Id; Json->TryGetStringField(TEXT("id"), Id);
        if (Id.IsEmpty())
        {
            if (Callbacks.OnFailure) Callbacks.OnFailure(TEXT("Replicate: missing urls.get and id in response."));
            return;
        }
        const UNanoBananaSettings& S = UNanoBananaSettings::Get();
        FString Base = S.Replicate.BaseUrlOverride.IsEmpty() ? TEXT("https://api.replicate.com/v1") : S.Replicate.BaseUrlOverride;
        if (Base.EndsWith(TEXT("/"))) Base.LeftChopInline(1);
        GetUrl = FString::Printf(TEXT("%s/predictions/%s"), *Base, *Id);
    }

    PollPrediction(GetUrl, ApiKey, Callbacks);
}

void FReplicateProvider::PollPrediction(const FString& GetUrl, const FString& ApiKey, const FProviderCallbacks& Callbacks)
{
    using namespace NanoBanana::Http;
    const UNanoBananaSettings& S = UNanoBananaSettings::Get();

    TSharedPtr<FPollLoop, ESPMode::ThreadSafe> Loop = MakeShared<FPollLoop, ESPMode::ThreadSafe>();
    Poll = Loop;
    Loop->MaxTotalSeconds = (float)FMath::Max(10, S.MaxPollSeconds);
    Loop->RequestFactory = [GetUrl, ApiKey]()
    {
        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Q = FHttpModule::Get().CreateRequest();
        Q->SetURL(GetUrl);
        Q->SetVerb(TEXT("GET"));
        Q->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
        return Q;
    };
    Loop->DecideFn = [](int32 Code, const FString& Body, FString& OutErr) -> EPollDecision
    {
        if (Code < 200 || Code >= 300)
        {
            OutErr = FString::Printf(TEXT("Replicate poll HTTP %d"), Code);
            return EPollDecision::Failed;
        }
        TSharedPtr<FJsonObject> J;
        TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Body);
        if (FJsonSerializer::Deserialize(R, J) && J.IsValid())
        {
            FString St; J->TryGetStringField(TEXT("status"), St);
            if (St.Equals(TEXT("succeeded"), ESearchCase::IgnoreCase)) return EPollDecision::Succeeded;
            if (St.Equals(TEXT("failed"), ESearchCase::IgnoreCase) || St.Equals(TEXT("canceled"), ESearchCase::IgnoreCase))
            {
                FString Err; J->TryGetStringField(TEXT("error"), Err);
                OutErr = FString::Printf(TEXT("Replicate prediction %s: %s"), *St, *Err.Left(256));
                return EPollDecision::Failed;
            }
        }
        return EPollDecision::Continue;
    };
    Loop->OnProgress = [Callbacks](float F)
    {
        if (Callbacks.OnProgress) Callbacks.OnProgress(0.3f + 0.5f * F, TEXT("Replicate polling..."));
    };
    Loop->OnFailed = [Callbacks](const FString& E)
    {
        if (Callbacks.OnFailure) Callbacks.OnFailure(E);
    };
    TWeakPtr<FReplicateProvider, ESPMode::ThreadSafe> WeakThis = StaticCastSharedRef<FReplicateProvider>(AsShared());
    Loop->OnSucceeded = [WeakThis, Callbacks](const FString& Body)
    {
        TSharedPtr<FReplicateProvider, ESPMode::ThreadSafe> P = WeakThis.Pin();
        if (!P.IsValid() || P->bCanceled) return;
        P->HandleTerminalPrediction(Body, Callbacks);
    };
    Loop->Start();
}

void FReplicateProvider::HandleTerminalPrediction(const FString& Body, const FProviderCallbacks& Callbacks)
{
    TSharedPtr<FJsonObject> Json;
    TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Body);
    if (!FJsonSerializer::Deserialize(R, Json) || !Json.IsValid())
    {
        if (Callbacks.OnFailure) Callbacks.OnFailure(TEXT("Replicate: failed to parse terminal prediction JSON."));
        return;
    }

    TArray<FString> Urls;
    const TArray<TSharedPtr<FJsonValue>>* OutArr = nullptr;
    if (Json->TryGetArrayField(TEXT("output"), OutArr) && OutArr)
    {
        for (const TSharedPtr<FJsonValue>& V : *OutArr)
        {
            if (V.IsValid() && V->Type == EJson::String)
            {
                Urls.Add(V->AsString());
            }
        }
    }
    else
    {
        // Some models return output as a single string
        FString Single;
        if (Json->TryGetStringField(TEXT("output"), Single) && !Single.IsEmpty())
        {
            Urls.Add(Single);
        }
    }

    if (Urls.Num() == 0)
    {
        if (Callbacks.OnFailure) Callbacks.OnFailure(FString::Printf(TEXT("Replicate: no output URLs. Body: %s"), *Body.Left(512)));
        return;
    }

    if (Callbacks.OnProgress) Callbacks.OnProgress(0.85f, TEXT("Replicate fetching images"));
    FetchImageUrls(Urls, Callbacks, Body);
}

void FReplicateProvider::FetchImageUrls(const TArray<FString>& Urls, const FProviderCallbacks& Callbacks, const FString& RawResponse)
{
    const UNanoBananaSettings& S = UNanoBananaSettings::Get();
    const FString ApiKey = S.GetEffectiveApiKey(ENanoBananaVendor::Replicate);

    TSharedRef<TArray<TArray<uint8>>> Bucket = MakeShared<TArray<TArray<uint8>>>();
    Bucket->SetNum(Urls.Num());
    TSharedRef<int32> Pending = MakeShared<int32>(Urls.Num());
    TSharedRef<bool> Failed = MakeShared<bool>(false);

    TWeakPtr<FReplicateProvider, ESPMode::ThreadSafe> WeakThis = StaticCastSharedRef<FReplicateProvider>(AsShared());

    for (int32 i = 0; i < Urls.Num(); ++i)
    {
        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Get = FHttpModule::Get().CreateRequest();
        Get->SetURL(Urls[i]);
        Get->SetVerb(TEXT("GET"));
        if (!ApiKey.IsEmpty())
        {
            Get->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
        }
        const int32 Index = i;
        Get->OnProcessRequestComplete().BindLambda(
            [WeakThis, Callbacks, Bucket, Pending, Failed, Index, RawResponse]
            (FHttpRequestPtr, FHttpResponsePtr Resp, bool bOK)
            {
                TSharedPtr<FReplicateProvider, ESPMode::ThreadSafe> P = WeakThis.Pin();
                if (!P.IsValid() || P->bCanceled) return;
                if (*Failed) return;
                if (!bOK || !Resp.IsValid() || Resp->GetResponseCode() < 200 || Resp->GetResponseCode() >= 300)
                {
                    *Failed = true;
                    if (Callbacks.OnFailure) Callbacks.OnFailure(FString::Printf(TEXT("Replicate image download failed (index %d)."), Index));
                    return;
                }
                (*Bucket)[Index] = Resp->GetContent();
                if (--(*Pending) == 0)
                {
                    if (Callbacks.OnSuccess) Callbacks.OnSuccess(MoveTemp(*Bucket), RawResponse);
                }
            });
        Get->ProcessRequest();
    }
}

void FReplicateProvider::Cancel()
{
    bCanceled = true;
    if (InFlight.IsValid()) { InFlight->CancelRequest(); InFlight.Reset(); }
    if (Poll.IsValid()) { Poll->Cancel(); Poll.Reset(); }
}
