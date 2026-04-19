#include "JsonResponseScanner.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Base64.h"

namespace NanoBanana::Json
{
    static void TryDecodeInlineDataObject(const TSharedPtr<FJsonObject>& Inline, TArray<TArray<uint8>>& Out)
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
                    Out.Add(MoveTemp(Img));
                }
            }
        }
    }

    void CollectInlineImages(const TSharedPtr<FJsonObject>& Obj, TArray<TArray<uint8>>& Out)
    {
        if (!Obj.IsValid()) return;

        // Gemini snake_case + camelCase variants
        TryDecodeInlineDataObject(Obj->GetObjectField(TEXT("inline_data")), Out);
        TryDecodeInlineDataObject(Obj->GetObjectField(TEXT("inlineData")), Out);

        // OpenAI-style direct base64 fields
        FString B64;
        if (Obj->TryGetStringField(TEXT("b64_json"), B64) || Obj->TryGetStringField(TEXT("bytesBase64"), B64))
        {
            TArray<uint8> Img;
            if (FBase64::Decode(B64, Img) && Img.Num() > 0)
            {
                Out.Add(MoveTemp(Img));
            }
        }

        // Recurse
        for (const auto& KVP : Obj->Values)
        {
            const TSharedPtr<FJsonValue>& Val = KVP.Value;
            if (!Val.IsValid()) continue;
            if (Val->Type == EJson::Object)
            {
                CollectInlineImages(Val->AsObject(), Out);
            }
            else if (Val->Type == EJson::Array)
            {
                for (const TSharedPtr<FJsonValue>& Elem : Val->AsArray())
                {
                    if (Elem.IsValid() && Elem->Type == EJson::Object)
                    {
                        CollectInlineImages(Elem->AsObject(), Out);
                    }
                }
            }
        }
    }

    void CollectInlineImagesFromResponseBody(const FString& Body, TArray<TArray<uint8>>& Out)
    {
        TSharedPtr<FJsonObject> Json;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
        if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
        {
            CollectInlineImages(Json, Out);
        }
    }
}
