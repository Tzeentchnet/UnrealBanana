#include "FalAiProvider.h"
#include "../../Http/Base64Image.h"
#include "../../Http/PollLoop.h"
#include "NanoBananaSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FString FFalAiProvider::ResolveModelSlug(ENanoBananaModel Model, const FString& CustomModelId)
{
    switch (Model)
    {
    case ENanoBananaModel::NanoBanana:    return TEXT("fal-ai/nano-banana");
    case ENanoBananaModel::NanoBanana2:   return TEXT("fal-ai/nano-banana-2");
    case ENanoBananaModel::NanoBananaPro: return TEXT("fal-ai/nano-banana-pro");
    case ENanoBananaModel::Custom:        return CustomModelId.IsEmpty() ? TEXT("fal-ai/nano-banana") : CustomModelId;
    default:                              return TEXT("fal-ai/nano-banana");
    }
}

static FString TrimSlash(FString S)
{
    if (S.EndsWith(TEXT("/"))) S.LeftChopInline(1);
    return S;
}

FString FFalAiProvider::BuildSyncUrl(const FString& Override, const FString& Slug)
{
    const FString Base = Override.IsEmpty() ? TEXT("https://fal.run") : TrimSlash(Override);
    return FString::Printf(TEXT("%s/%s"), *Base, *Slug);
}

FString FFalAiProvider::BuildQueueSubmitUrl(const FString& Override, const FString& Slug)
{
    const FString Base = Override.IsEmpty() ? TEXT("https://queue.fal.run") : TrimSlash(Override);
    return FString::Printf(TEXT("%s/%s"), *Base, *Slug);
}

FString FFalAiProvider::BuildQueueStatusUrl(const FString& Override, const FString& Slug, const FString& RequestId)
{
    const FString Base = Override.IsEmpty() ? TEXT("https://queue.fal.run") : TrimSlash(Override);
    return FString::Printf(TEXT("%s/%s/requests/%s/status"), *Base, *Slug, *RequestId);
}

FString FFalAiProvider::BuildQueueResultUrl(const FString& Override, const FString& Slug, const FString& RequestId)
{
    const FString Base = Override.IsEmpty() ? TEXT("https://queue.fal.run") : TrimSlash(Override);
    return FString::Printf(TEXT("%s/%s/requests/%s"), *Base, *Slug, *RequestId);
}

FString FFalAiProvider::BuildRequestJson(const FNanoBananaRequest& Request, const TArray<TArray<uint8>>& EncodedReferences)
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();

    Root->SetStringField(TEXT("prompt"), Request.Prompt);
    Root->SetNumberField(TEXT("num_images"), FMath::Max(1, Request.NumImages));
    Root->SetStringField(TEXT("output_format"),
        Request.OutputFormat == ENanoBananaOutputFormat::JPEG ? TEXT("jpeg") :
        Request.OutputFormat == ENanoBananaOutputFormat::WebP ? TEXT("webp") : TEXT("png"));

    if (Request.Aspect != ENanoBananaAspect::Auto)
    {
        Root->SetStringField(TEXT("aspect_ratio"), FNanoBananaTypeUtils::AspectToString(Request.Aspect));
    }
    if (Request.Resolution != ENanoBananaResolution::Auto)
    {
        Root->SetStringField(TEXT("resolution"), FNanoBananaTypeUtils::ResolutionToString(Request.Resolution));
    }
    if (Request.Seed != 0)
    {
        Root->SetNumberField(TEXT("seed"), Request.Seed);
    }
    if (!Request.NegativePrompt.IsEmpty())
    {
        Root->SetStringField(TEXT("negative_prompt"), Request.NegativePrompt);
    }

    if (EncodedReferences.Num() > 0)
    {
        // FAL nano-banana edit endpoints accept image_urls (data URI strings work).
        TArray<TSharedPtr<FJsonValue>> Urls;
        for (const TArray<uint8>& Img : EncodedReferences)
        {
            if (Img.Num() == 0) continue;
            const FString Mime = NanoBanana::Image::SniffImageMimeType(Img);
            const FString Uri = NanoBanana::Image::PngBytesToDataUri(Img, Mime);
            Urls.Add(MakeShared<FJsonValueString>(Uri));
        }
        if (Urls.Num() > 0)
        {
            Root->SetArrayField(TEXT("image_urls"), Urls);
            // Some models also accept singular image_url for the first image.
            Root->SetStringField(TEXT("image_url"), Urls[0]->AsString());
        }
    }

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
    return Out;
}

void FFalAiProvider::Submit(const FNanoBananaRequest& Request, const FProviderCallbacks& Callbacks)
{
    const UNanoBananaSettings& S = UNanoBananaSettings::Get();
    const FString ApiKey = S.GetEffectiveApiKey(ENanoBananaVendor::Fal);
    if (ApiKey.IsEmpty())
    {
        if (Callbacks.OnFailure) Callbacks.OnFailure(TEXT("FAL: missing API key (set in Project Settings or FAL_KEY env var)."));
        return;
    }

    TArray<TArray<uint8>> Refs;
    NanoBanana::Image::ResolveAllReferences(Request, Refs);

    const FString Slug = ResolveModelSlug(Request.Model, Request.CustomModelId);
    const FString Body = BuildRequestJson(Request, Refs);

    if (Callbacks.OnRequestBuilt) Callbacks.OnRequestBuilt(Body);

    if (S.Fal.bAlwaysUseQueue)
    {
        SubmitQueue(Slug, Body, ApiKey, Callbacks);
    }
    else
    {
        const FString SyncUrl = BuildSyncUrl(S.Fal.SyncBaseUrlOverride, Slug);
        SubmitSync(SyncUrl, Body, ApiKey, Callbacks);
    }
}

void FFalAiProvider::SubmitSync(const FString& Url, const FString& Body, const FString& ApiKey, const FProviderCallbacks& Callbacks)
{
    if (Callbacks.OnProgress) Callbacks.OnProgress(0.2f, TEXT("FAL sync request"));
    const UNanoBananaSettings& S = UNanoBananaSettings::Get();

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
    InFlight = Req;
    Req->SetURL(Url);
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Key %s"), *ApiKey));
    Req->SetTimeout((float)FMath::Max(5, S.RequestTimeoutSeconds));
    Req->SetContentAsString(Body);

    TWeakPtr<FFalAiProvider, ESPMode::ThreadSafe> WeakThis = StaticCastSharedRef<FFalAiProvider>(AsShared());
    Req->OnProcessRequestComplete().BindLambda(
        [WeakThis, Callbacks, Body, ApiKey](FHttpRequestPtr ReqPtr, FHttpResponsePtr Resp, bool bSucceeded)
        {
            TSharedPtr<FFalAiProvider, ESPMode::ThreadSafe> Pinned = WeakThis.Pin();
            if (!Pinned.IsValid() || Pinned->bCanceled) return;
            Pinned->InFlight.Reset();

            const bool bTimeout = !bSucceeded || !Resp.IsValid();
            if (!bTimeout)
            {
                const int32 Code = Resp->GetResponseCode();
                const FString RespStr = Resp->GetContentAsString();
                if (Code >= 200 && Code < 300)
                {
                    Pinned->HandleResultPayload(RespStr, Callbacks);
                    return;
                }
                // Hard failure (auth, bad request) — don't bother queuing.
                if (Code == 401 || Code == 403 || Code == 422)
                {
                    if (Callbacks.OnFailure) Callbacks.OnFailure(FString::Printf(TEXT("FAL HTTP %d: %s"), Code, *RespStr.Left(512)));
                    return;
                }
            }

            // Soft failure (timeout, 5xx) — fall back to queue.
            if (Callbacks.OnProgress) Callbacks.OnProgress(0.25f, TEXT("FAL sync timed out — switching to queue"));
            // Re-derive slug from URL (last 2 path components).
            FString Url2 = ReqPtr.IsValid() ? ReqPtr->GetURL() : FString();
            const int32 SlashIdx = Url2.Find(TEXT("/fal-ai/"));
            FString Slug = (SlashIdx != INDEX_NONE) ? Url2.RightChop(SlashIdx + 1) : TEXT("fal-ai/nano-banana");
            // Strip query string if any.
            int32 Q = INDEX_NONE; if (Slug.FindChar('?', Q)) Slug.LeftInline(Q);
            Pinned->SubmitQueue(Slug, Body, ApiKey, Callbacks);
        });
    Req->ProcessRequest();
}

void FFalAiProvider::SubmitQueue(const FString& Slug, const FString& Body, const FString& ApiKey, const FProviderCallbacks& Callbacks)
{
    const UNanoBananaSettings& S = UNanoBananaSettings::Get();
    if (Callbacks.OnProgress) Callbacks.OnProgress(0.3f, TEXT("FAL queue submit"));

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Submit = FHttpModule::Get().CreateRequest();
    InFlight = Submit;
    Submit->SetURL(BuildQueueSubmitUrl(S.Fal.QueueBaseUrlOverride, Slug));
    Submit->SetVerb(TEXT("POST"));
    Submit->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Submit->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Key %s"), *ApiKey));
    Submit->SetContentAsString(Body);

    TWeakPtr<FFalAiProvider, ESPMode::ThreadSafe> WeakThis = StaticCastSharedRef<FFalAiProvider>(AsShared());
    Submit->OnProcessRequestComplete().BindLambda(
        [WeakThis, Callbacks, Slug, ApiKey](FHttpRequestPtr, FHttpResponsePtr Resp, bool bSucceeded)
        {
            TSharedPtr<FFalAiProvider, ESPMode::ThreadSafe> Pinned = WeakThis.Pin();
            if (!Pinned.IsValid() || Pinned->bCanceled) return;
            Pinned->InFlight.Reset();
            if (!bSucceeded || !Resp.IsValid())
            {
                if (Callbacks.OnFailure) Callbacks.OnFailure(TEXT("FAL queue submit failed (network)."));
                return;
            }
            const int32 Code = Resp->GetResponseCode();
            const FString RespStr = Resp->GetContentAsString();
            if (Code < 200 || Code >= 300)
            {
                if (Callbacks.OnFailure) Callbacks.OnFailure(FString::Printf(TEXT("FAL queue HTTP %d: %s"), Code, *RespStr.Left(512)));
                return;
            }

            // Parse request_id and start polling status.
            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(RespStr);
            FString RequestId;
            if (FJsonSerializer::Deserialize(R, Json) && Json.IsValid())
            {
                Json->TryGetStringField(TEXT("request_id"), RequestId);
            }
            if (RequestId.IsEmpty())
            {
                if (Callbacks.OnFailure) Callbacks.OnFailure(TEXT("FAL queue: response missing request_id."));
                return;
            }

            const UNanoBananaSettings& S2 = UNanoBananaSettings::Get();
            const FString StatusUrl = BuildQueueStatusUrl(S2.Fal.QueueBaseUrlOverride, Slug, RequestId);
            const FString ResultUrl = BuildQueueResultUrl(S2.Fal.QueueBaseUrlOverride, Slug, RequestId);

            using namespace NanoBanana::Http;
            TSharedPtr<FPollLoop, ESPMode::ThreadSafe> Loop = MakeShared<FPollLoop, ESPMode::ThreadSafe>();
            Pinned->Poll = Loop;
            Loop->MaxTotalSeconds = (float)FMath::Max(10, S2.MaxPollSeconds);
            Loop->RequestFactory = [StatusUrl, ApiKey]()
            {
                TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Q = FHttpModule::Get().CreateRequest();
                Q->SetURL(StatusUrl);
                Q->SetVerb(TEXT("GET"));
                Q->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Key %s"), *ApiKey));
                return Q;
            };
            Loop->DecideFn = [](int32 HttpCode, const FString& Body, FString& OutErr) -> EPollDecision
            {
                if (HttpCode < 200 || HttpCode >= 300)
                {
                    OutErr = FString::Printf(TEXT("FAL status HTTP %d: %s"), HttpCode, *Body.Left(256));
                    return EPollDecision::Failed;
                }
                TSharedPtr<FJsonObject> J;
                TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Body);
                if (FJsonSerializer::Deserialize(R, J) && J.IsValid())
                {
                    FString St; J->TryGetStringField(TEXT("status"), St);
                    if (St.Equals(TEXT("COMPLETED"), ESearchCase::IgnoreCase)) return EPollDecision::Succeeded;
                    if (St.Equals(TEXT("FAILED"), ESearchCase::IgnoreCase) || St.Equals(TEXT("ERROR"), ESearchCase::IgnoreCase))
                    {
                        OutErr = FString::Printf(TEXT("FAL job failed: %s"), *Body.Left(512));
                        return EPollDecision::Failed;
                    }
                }
                return EPollDecision::Continue;
            };
            Loop->OnProgress = [Callbacks](float F)
            {
                if (Callbacks.OnProgress) Callbacks.OnProgress(0.3f + 0.5f * F, TEXT("FAL polling..."));
            };
            Loop->OnFailed = [Callbacks](const FString& E)
            {
                if (Callbacks.OnFailure) Callbacks.OnFailure(E);
            };
            Loop->OnSucceeded = [WeakThis, Callbacks, ResultUrl, ApiKey](const FString& /*StatusBody*/)
            {
                TSharedPtr<FFalAiProvider, ESPMode::ThreadSafe> P = WeakThis.Pin();
                if (!P.IsValid() || P->bCanceled) return;
                if (Callbacks.OnProgress) Callbacks.OnProgress(0.85f, TEXT("FAL fetching result"));
                TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Get = FHttpModule::Get().CreateRequest();
                P->InFlight = Get;
                Get->SetURL(ResultUrl);
                Get->SetVerb(TEXT("GET"));
                Get->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Key %s"), *ApiKey));
                TWeakPtr<FFalAiProvider, ESPMode::ThreadSafe> Wk = WeakThis;
                Get->OnProcessRequestComplete().BindLambda(
                    [Wk, Callbacks](FHttpRequestPtr, FHttpResponsePtr Resp2, bool bOK)
                    {
                        TSharedPtr<FFalAiProvider, ESPMode::ThreadSafe> P2 = Wk.Pin();
                        if (!P2.IsValid() || P2->bCanceled) return;
                        P2->InFlight.Reset();
                        if (!bOK || !Resp2.IsValid())
                        {
                            if (Callbacks.OnFailure) Callbacks.OnFailure(TEXT("FAL result fetch failed."));
                            return;
                        }
                        P2->HandleResultPayload(Resp2->GetContentAsString(), Callbacks);
                    });
                Get->ProcessRequest();
            };
            Loop->Start();
        });
    Submit->ProcessRequest();
}

void FFalAiProvider::HandleResultPayload(const FString& Body, const FProviderCallbacks& Callbacks)
{
    TSharedPtr<FJsonObject> Json;
    TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Body);
    if (!FJsonSerializer::Deserialize(R, Json) || !Json.IsValid())
    {
        if (Callbacks.OnFailure) Callbacks.OnFailure(TEXT("FAL: failed to parse result JSON."));
        return;
    }

    TArray<FString> Urls;
    const TArray<TSharedPtr<FJsonValue>>* ImagesArr = nullptr;
    if (Json->TryGetArrayField(TEXT("images"), ImagesArr) && ImagesArr)
    {
        for (const TSharedPtr<FJsonValue>& V : *ImagesArr)
        {
            if (V.IsValid() && V->Type == EJson::Object)
            {
                FString Url; V->AsObject()->TryGetStringField(TEXT("url"), Url);
                if (!Url.IsEmpty()) Urls.Add(Url);
            }
        }
    }
    if (Urls.Num() == 0)
    {
        // Some FAL responses nest under "data" or expose "image" singular.
        const TSharedPtr<FJsonObject>* Data = nullptr;
        if (Json->TryGetObjectField(TEXT("data"), Data) && Data && Data->IsValid())
        {
            const TArray<TSharedPtr<FJsonValue>>* Arr2 = nullptr;
            if ((*Data)->TryGetArrayField(TEXT("images"), Arr2) && Arr2)
            {
                for (const TSharedPtr<FJsonValue>& V : *Arr2)
                {
                    if (V.IsValid() && V->Type == EJson::Object)
                    {
                        FString Url; V->AsObject()->TryGetStringField(TEXT("url"), Url);
                        if (!Url.IsEmpty()) Urls.Add(Url);
                    }
                }
            }
        }
    }

    if (Urls.Num() == 0)
    {
        if (Callbacks.OnFailure) Callbacks.OnFailure(FString::Printf(TEXT("FAL: result had no image URLs. Body: %s"), *Body.Left(512)));
        return;
    }

    FetchImageUrls(Urls, Callbacks, Body);
}

void FFalAiProvider::FetchImageUrls(const TArray<FString>& Urls, const FProviderCallbacks& Callbacks, const FString& RawResponse)
{
    // Issue parallel GETs and assemble in order.
    TSharedRef<TArray<TArray<uint8>>> Bucket = MakeShared<TArray<TArray<uint8>>>();
    Bucket->SetNum(Urls.Num());
    TSharedRef<int32> Pending = MakeShared<int32>(Urls.Num());
    TSharedRef<bool> Failed = MakeShared<bool>(false);

    TWeakPtr<FFalAiProvider, ESPMode::ThreadSafe> WeakThis = StaticCastSharedRef<FFalAiProvider>(AsShared());

    for (int32 i = 0; i < Urls.Num(); ++i)
    {
        const FString Url = Urls[i];
        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Get = FHttpModule::Get().CreateRequest();
        Get->SetURL(Url);
        Get->SetVerb(TEXT("GET"));
        const int32 Index = i;
        Get->OnProcessRequestComplete().BindLambda(
            [WeakThis, Callbacks, Bucket, Pending, Failed, Index, RawResponse]
            (FHttpRequestPtr, FHttpResponsePtr Resp, bool bOK)
            {
                TSharedPtr<FFalAiProvider, ESPMode::ThreadSafe> P = WeakThis.Pin();
                if (!P.IsValid() || P->bCanceled) return;
                if (*Failed) return;
                if (!bOK || !Resp.IsValid() || Resp->GetResponseCode() < 200 || Resp->GetResponseCode() >= 300)
                {
                    *Failed = true;
                    if (Callbacks.OnFailure) Callbacks.OnFailure(FString::Printf(TEXT("FAL image download failed (index %d)."), Index));
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

void FFalAiProvider::Cancel()
{
    bCanceled = true;
    if (InFlight.IsValid()) { InFlight->CancelRequest(); InFlight.Reset(); }
    if (Poll.IsValid()) { Poll->Cancel(); Poll.Reset(); }
}
