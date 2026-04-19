// Static request-builder tests for the three providers.
// These exercise pure C++ helpers — no HTTP, no settings dependency.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "NanoBananaTypes.h"
#include "Providers/Google/GoogleGeminiProvider.h"
#include "Providers/Fal/FalAiProvider.h"
#include "Providers/Replicate/ReplicateProvider.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    static FNanoBananaRequest MakeBaseRequest()
    {
        FNanoBananaRequest R;
        R.Prompt = TEXT("a banana");
        R.NumImages = 2;
        R.Aspect = ENanoBananaAspect::R16x9;
        R.Resolution = ENanoBananaResolution::Res2K;
        R.Seed = 42;
        R.NegativePrompt = TEXT("blurry");
        R.OutputFormat = ENanoBananaOutputFormat::PNG;
        return R;
    }

    static TArray<TArray<uint8>> MakeFakePngs(int32 N)
    {
        TArray<TArray<uint8>> Out;
        for (int32 i = 0; i < N; ++i)
        {
            // Minimal "PNG" magic header + filler so SniffImageMimeType returns image/png.
            TArray<uint8> Bytes = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
                                    0x00, 0x00, 0x00, (uint8)(0x10 + i)};
            Out.Add(MoveTemp(Bytes));
        }
        return Out;
    }
}

// ============================================================
// Google Gemini
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoogleGeminiProvider_ResolveModelId_Test,
    "UnrealBanana.Providers.Google.ResolveModelId",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGoogleGeminiProvider_ResolveModelId_Test::RunTest(const FString&)
{
    TestEqual(TEXT("NanoBanana    -> 2.5-flash-image"),
        FGoogleGeminiProvider::ResolveModelId(ENanoBananaModel::NanoBanana, FString()),
        FString(TEXT("gemini-2.5-flash-image")));
    TestEqual(TEXT("NanoBanana2   -> 3.1-flash-image-preview"),
        FGoogleGeminiProvider::ResolveModelId(ENanoBananaModel::NanoBanana2, FString()),
        FString(TEXT("gemini-3.1-flash-image-preview")));
    TestEqual(TEXT("NanoBananaPro -> 3-pro-image-preview"),
        FGoogleGeminiProvider::ResolveModelId(ENanoBananaModel::NanoBananaPro, FString()),
        FString(TEXT("gemini-3-pro-image-preview")));
    TestEqual(TEXT("Custom uses CustomModelId"),
        FGoogleGeminiProvider::ResolveModelId(ENanoBananaModel::Custom, TEXT("custom-id")),
        FString(TEXT("custom-id")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoogleGeminiProvider_BuildEndpointUrl_Test,
    "UnrealBanana.Providers.Google.BuildEndpointUrl",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGoogleGeminiProvider_BuildEndpointUrl_Test::RunTest(const FString&)
{
    const FString Url = FGoogleGeminiProvider::BuildEndpointUrl(FString(), TEXT("gemini-2.5-flash-image"), TEXT("KEY123"));
    TestTrue(TEXT("default base"), Url.StartsWith(TEXT("https://generativelanguage.googleapis.com/v1beta/models/")));
    TestTrue(TEXT("model in path"), Url.Contains(TEXT("/gemini-2.5-flash-image:generateContent")));
    TestTrue(TEXT("api key as query"), Url.Contains(TEXT("?key=KEY123")));

    const FString UrlOverride = FGoogleGeminiProvider::BuildEndpointUrl(TEXT("https://my-proxy.example.com/gemini/"), TEXT("m"), TEXT(""));
    TestTrue(TEXT("override + trailing slash trimmed"), UrlOverride.StartsWith(TEXT("https://my-proxy.example.com/gemini/models/m:generateContent")));
    TestFalse(TEXT("no key -> no query"), UrlOverride.Contains(TEXT("?key=")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoogleGeminiProvider_BuildRequestJson_Test,
    "UnrealBanana.Providers.Google.BuildRequestJson",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGoogleGeminiProvider_BuildRequestJson_Test::RunTest(const FString&)
{
    const FNanoBananaRequest R = MakeBaseRequest();
    const TArray<TArray<uint8>> Refs = MakeFakePngs(2);
    const FString Json = FGoogleGeminiProvider::BuildRequestJson(R, Refs);

    TestTrue(TEXT("contains contents"), Json.Contains(TEXT("\"contents\"")));
    TestTrue(TEXT("contains role user"), Json.Contains(TEXT("\"user\"")));
    TestTrue(TEXT("contains prompt text"), Json.Contains(TEXT("a banana")));
    TestTrue(TEXT("contains negative inline"), Json.Contains(TEXT("Negative: blurry")));
    TestTrue(TEXT("contains inlineData"), Json.Contains(TEXT("\"inlineData\"")));
    TestTrue(TEXT("contains responseModalities IMAGE"), Json.Contains(TEXT("\"IMAGE\"")));
    TestTrue(TEXT("contains aspectRatio 16:9"), Json.Contains(TEXT("\"16:9\"")));
    TestTrue(TEXT("contains imageSize 2K"), Json.Contains(TEXT("\"2K\"")));
    TestTrue(TEXT("contains seed"), Json.Contains(TEXT("\"seed\"")));
    TestTrue(TEXT("contains candidateCount 2"), Json.Contains(TEXT("\"candidateCount\"")) && Json.Contains(TEXT(":2")));
    return true;
}

// ============================================================
// FAL.ai
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFalAiProvider_ResolveModelSlug_Test,
    "UnrealBanana.Providers.Fal.ResolveModelSlug",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFalAiProvider_ResolveModelSlug_Test::RunTest(const FString&)
{
    TestEqual(TEXT("NanoBanana"),    FFalAiProvider::ResolveModelSlug(ENanoBananaModel::NanoBanana, FString()),    FString(TEXT("fal-ai/nano-banana")));
    TestEqual(TEXT("NanoBanana2"),   FFalAiProvider::ResolveModelSlug(ENanoBananaModel::NanoBanana2, FString()),   FString(TEXT("fal-ai/nano-banana-2")));
    TestEqual(TEXT("NanoBananaPro"), FFalAiProvider::ResolveModelSlug(ENanoBananaModel::NanoBananaPro, FString()), FString(TEXT("fal-ai/nano-banana-pro")));
    TestEqual(TEXT("Custom"),        FFalAiProvider::ResolveModelSlug(ENanoBananaModel::Custom, TEXT("foo/bar")),  FString(TEXT("foo/bar")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFalAiProvider_BuildUrls_Test,
    "UnrealBanana.Providers.Fal.BuildUrls",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFalAiProvider_BuildUrls_Test::RunTest(const FString&)
{
    TestEqual(TEXT("sync"),
        FFalAiProvider::BuildSyncUrl(FString(), TEXT("fal-ai/nano-banana-2")),
        FString(TEXT("https://fal.run/fal-ai/nano-banana-2")));
    TestEqual(TEXT("queue submit"),
        FFalAiProvider::BuildQueueSubmitUrl(FString(), TEXT("fal-ai/nano-banana-2")),
        FString(TEXT("https://queue.fal.run/fal-ai/nano-banana-2")));
    TestEqual(TEXT("queue status"),
        FFalAiProvider::BuildQueueStatusUrl(FString(), TEXT("fal-ai/nano-banana-2"), TEXT("REQ123")),
        FString(TEXT("https://queue.fal.run/fal-ai/nano-banana-2/requests/REQ123/status")));
    TestEqual(TEXT("queue result"),
        FFalAiProvider::BuildQueueResultUrl(FString(), TEXT("fal-ai/nano-banana-2"), TEXT("REQ123")),
        FString(TEXT("https://queue.fal.run/fal-ai/nano-banana-2/requests/REQ123")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFalAiProvider_BuildRequestJson_Test,
    "UnrealBanana.Providers.Fal.BuildRequestJson",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFalAiProvider_BuildRequestJson_Test::RunTest(const FString&)
{
    const FNanoBananaRequest R = MakeBaseRequest();
    const TArray<TArray<uint8>> Refs = MakeFakePngs(1);
    const FString Json = FFalAiProvider::BuildRequestJson(R, Refs);

    TestTrue(TEXT("prompt"),         Json.Contains(TEXT("\"prompt\"")));
    TestTrue(TEXT("num_images 2"),   Json.Contains(TEXT("\"num_images\"")) && Json.Contains(TEXT(":2")));
    TestTrue(TEXT("aspect_ratio"),   Json.Contains(TEXT("\"aspect_ratio\"")) && Json.Contains(TEXT("\"16:9\"")));
    TestTrue(TEXT("resolution 2K"),  Json.Contains(TEXT("\"resolution\"")) && Json.Contains(TEXT("\"2K\"")));
    TestTrue(TEXT("output_format"),  Json.Contains(TEXT("\"output_format\"")) && Json.Contains(TEXT("\"png\"")));
    TestTrue(TEXT("seed"),           Json.Contains(TEXT("\"seed\"")));
    TestTrue(TEXT("negative_prompt"), Json.Contains(TEXT("\"negative_prompt\"")));
    TestTrue(TEXT("image_urls data URI"), Json.Contains(TEXT("\"image_urls\"")) && Json.Contains(TEXT("data:image/png;base64,")));
    return true;
}

// ============================================================
// Replicate
// ============================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReplicateProvider_ResolveModelSlug_Test,
    "UnrealBanana.Providers.Replicate.ResolveModelSlug",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FReplicateProvider_ResolveModelSlug_Test::RunTest(const FString&)
{
    TestEqual(TEXT("NanoBanana"),    FReplicateProvider::ResolveModelSlug(ENanoBananaModel::NanoBanana, FString()),    FString(TEXT("google/nano-banana")));
    TestEqual(TEXT("NanoBananaPro"), FReplicateProvider::ResolveModelSlug(ENanoBananaModel::NanoBananaPro, FString()), FString(TEXT("google/nano-banana-pro")));
    TestEqual(TEXT("Custom"),        FReplicateProvider::ResolveModelSlug(ENanoBananaModel::Custom, TEXT("user/model")), FString(TEXT("user/model")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReplicateProvider_BuildPredictionsUrl_Test,
    "UnrealBanana.Providers.Replicate.BuildPredictionsUrl",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FReplicateProvider_BuildPredictionsUrl_Test::RunTest(const FString&)
{
    TestEqual(TEXT("default"), FReplicateProvider::BuildPredictionsUrl(FString()), FString(TEXT("https://api.replicate.com/v1/predictions")));
    TestEqual(TEXT("override trim"), FReplicateProvider::BuildPredictionsUrl(TEXT("https://x.example.com/api/")), FString(TEXT("https://x.example.com/api/predictions")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReplicateProvider_BuildRequestJson_Test,
    "UnrealBanana.Providers.Replicate.BuildRequestJson",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FReplicateProvider_BuildRequestJson_Test::RunTest(const FString&)
{
    const FNanoBananaRequest R = MakeBaseRequest();
    const TArray<TArray<uint8>> Refs = MakeFakePngs(1);
    const FString Json = FReplicateProvider::BuildRequestJson(R, Refs);

    TestTrue(TEXT("model field"),    Json.Contains(TEXT("\"model\"")) && Json.Contains(TEXT("google/nano-banana")));
    TestTrue(TEXT("nested input"),   Json.Contains(TEXT("\"input\"")));
    TestTrue(TEXT("prompt"),         Json.Contains(TEXT("\"prompt\"")) && Json.Contains(TEXT("a banana")));
    TestTrue(TEXT("aspect_ratio"),   Json.Contains(TEXT("\"aspect_ratio\"")) && Json.Contains(TEXT("\"16:9\"")));
    TestTrue(TEXT("width 2048"),     Json.Contains(TEXT("\"width\"")) && Json.Contains(TEXT(":2048")));
    TestTrue(TEXT("seed"),           Json.Contains(TEXT("\"seed\"")));
    TestTrue(TEXT("output_format"),  Json.Contains(TEXT("\"output_format\"")) && Json.Contains(TEXT("\"png\"")));
    TestTrue(TEXT("image_input data URI"), Json.Contains(TEXT("\"image_input\"")) && Json.Contains(TEXT("data:image/png;base64,")));
    TestTrue(TEXT("num_outputs"),    Json.Contains(TEXT("\"num_outputs\"")) && Json.Contains(TEXT(":2")));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
