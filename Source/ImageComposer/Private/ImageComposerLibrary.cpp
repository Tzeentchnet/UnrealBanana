#include "ImageComposerLibrary.h"
#include "Modules/ModuleManager.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"

static bool DecodePNGToRaw(const TArray<uint8>& PNG, TArray<uint8>& OutRawBGRA, int32& OutW, int32& OutH)
{
    if (PNG.Num() == 0) return false;
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    TSharedPtr<IImageWrapper> Wrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
    if (!Wrapper->SetCompressed(PNG.GetData(), PNG.Num())) return false;
    OutW = Wrapper->GetWidth();
    OutH = Wrapper->GetHeight();
    const TArray<uint8>* Raw = nullptr;
    Wrapper->GetRaw(ERGBFormat::BGRA, 8, OutRawBGRA);
    return OutRawBGRA.Num() > 0;
}

static void EncodeRawToPNG(const TArray<uint8>& RawBGRA, int32 W, int32 H, TArray<uint8>& OutPNG)
{
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    TSharedPtr<IImageWrapper> Wrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
    Wrapper->SetRaw(RawBGRA.GetData(), RawBGRA.Num(), W, H, ERGBFormat::BGRA, 8);
    OutPNG = Wrapper->GetCompressed(100);
}

bool UImageComposerLibrary::ComposeSideBySidePNGs(const TArray<uint8>& LeftPNG, const TArray<uint8>& RightPNG, TArray<uint8>& OutCompositePNG, int32 Padding)
{
    OutCompositePNG.Reset();

    int32 LW = 0, LH = 0, RW = 0, RH = 0;
    TArray<uint8> LRaw, RRaw;
    if (!DecodePNGToRaw(LeftPNG, LRaw, LW, LH)) return false;
    if (!DecodePNGToRaw(RightPNG, RRaw, RW, RH)) return false;

    const int32 OutW = LW + Padding + RW;
    const int32 OutH = FMath::Max(LH, RH);

    TArray<uint8> OutRaw;
    OutRaw.SetNumZeroed(OutW * OutH * 4);

    auto CopyRow = [](const uint8* Src, uint8* Dst, int32 Pixels)
    {
        FMemory::Memcpy(Dst, Src, Pixels * 4);
    };

    for (int32 Y = 0; Y < OutH; ++Y)
    {
        uint8* OutRow = &OutRaw[Y * OutW * 4];
        if (Y < LH)
        {
            const uint8* LRow = &LRaw[Y * LW * 4];
            CopyRow(LRow, OutRow, LW);
        }
        if (Y < RH)
        {
            const uint8* RRow = &RRaw[Y * RW * 4];
            CopyRow(RRow, OutRow + (LW + Padding) * 4, RW);
        }
        // Padding gap is left black (zeros)
    }

    EncodeRawToPNG(OutRaw, OutW, OutH, OutCompositePNG);
    return OutCompositePNG.Num() > 0;
}

