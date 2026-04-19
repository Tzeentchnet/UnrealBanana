#include "NanoBananaTypes.h"

FString FNanoBananaTypeUtils::AspectToString(ENanoBananaAspect Aspect)
{
    switch (Aspect)
    {
    case ENanoBananaAspect::R1x1:  return TEXT("1:1");
    case ENanoBananaAspect::R16x9: return TEXT("16:9");
    case ENanoBananaAspect::R9x16: return TEXT("9:16");
    case ENanoBananaAspect::R4x3:  return TEXT("4:3");
    case ENanoBananaAspect::R3x4:  return TEXT("3:4");
    case ENanoBananaAspect::R3x2:  return TEXT("3:2");
    case ENanoBananaAspect::R2x3:  return TEXT("2:3");
    case ENanoBananaAspect::R21x9: return TEXT("21:9");
    case ENanoBananaAspect::Auto:
    default:                       return TEXT("auto");
    }
}

FString FNanoBananaTypeUtils::ResolutionToString(ENanoBananaResolution Res)
{
    switch (Res)
    {
    case ENanoBananaResolution::Res512: return TEXT("0.5K");
    case ENanoBananaResolution::Res1K:  return TEXT("1K");
    case ENanoBananaResolution::Res2K:  return TEXT("2K");
    case ENanoBananaResolution::Res4K:  return TEXT("4K");
    case ENanoBananaResolution::Auto:
    default:                            return TEXT("auto");
    }
}

int32 FNanoBananaTypeUtils::ResolutionToPixels(ENanoBananaResolution Res)
{
    switch (Res)
    {
    case ENanoBananaResolution::Res512: return 512;
    case ENanoBananaResolution::Res1K:  return 1024;
    case ENanoBananaResolution::Res2K:  return 2048;
    case ENanoBananaResolution::Res4K:  return 4096;
    case ENanoBananaResolution::Auto:
    default:                            return 0;
    }
}

FString FNanoBananaTypeUtils::OutputFormatToMime(ENanoBananaOutputFormat Fmt)
{
    switch (Fmt)
    {
    case ENanoBananaOutputFormat::JPEG: return TEXT("image/jpeg");
    case ENanoBananaOutputFormat::WebP: return TEXT("image/webp");
    case ENanoBananaOutputFormat::PNG:
    default:                            return TEXT("image/png");
    }
}

FString FNanoBananaTypeUtils::OutputFormatToExt(ENanoBananaOutputFormat Fmt)
{
    switch (Fmt)
    {
    case ENanoBananaOutputFormat::JPEG: return TEXT(".jpg");
    case ENanoBananaOutputFormat::WebP: return TEXT(".webp");
    case ENanoBananaOutputFormat::PNG:
    default:                            return TEXT(".png");
    }
}

FString FNanoBananaTypeUtils::VendorToString(ENanoBananaVendor V)
{
    switch (V)
    {
    case ENanoBananaVendor::Google:    return TEXT("Google");
    case ENanoBananaVendor::Fal:       return TEXT("Fal");
    case ENanoBananaVendor::Replicate: return TEXT("Replicate");
    default:                           return TEXT("Unknown");
    }
}

FString FNanoBananaTypeUtils::ModelToDisplayString(ENanoBananaModel M)
{
    switch (M)
    {
    case ENanoBananaModel::NanoBanana:    return TEXT("NanoBanana");
    case ENanoBananaModel::NanoBanana2:   return TEXT("NanoBanana2");
    case ENanoBananaModel::NanoBananaPro: return TEXT("NanoBananaPro");
    case ENanoBananaModel::Custom:        return TEXT("Custom");
    default:                              return TEXT("Unknown");
    }
}
