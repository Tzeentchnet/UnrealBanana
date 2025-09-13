#include "NanoBananaWidgetBase.h"
#include "Components/EditableTextBox.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "NanoBananaBridgeAsyncAction.h"

void UNanoBananaWidgetBase::StartFromViewport(bool bShowUI)
{
    FString Prompt = PromptTextBox ? PromptTextBox->GetText().ToString() : FString();
    StartWithPrompt(Prompt, bShowUI);
}

void UNanoBananaWidgetBase::StartWithPrompt(const FString& Prompt, bool bShowUI)
{
    UNanoBananaBridgeAsyncAction* Action = UNanoBananaBridgeAsyncAction::CaptureAndGenerate(this, Prompt, bShowUI);
    Action->OnProgress.AddDynamic(this, &UNanoBananaWidgetBase::HandleProgress);
    Action->OnCompleted.AddDynamic(this, &UNanoBananaWidgetBase::HandleCompleted);
    Action->OnFailed.AddDynamic(this, &UNanoBananaWidgetBase::HandleFailed);
    Action->Activate();
}

void UNanoBananaWidgetBase::HandleProgress(float P, const FString& Stage)
{
    if (ProgressBar)
    {
        ProgressBar->SetPercent(P);
    }
    OnStageChanged(Stage);
}

void UNanoBananaWidgetBase::HandleCompleted(UTexture2D* Texture, const FString& ResultPath, const FString& CompositePath)
{
    if (ResultImage && Texture)
    {
        ResultImage->SetBrushFromTexture(Texture);
    }
    OnCompleted(ResultPath, CompositePath);
}

void UNanoBananaWidgetBase::HandleFailed(const FString& Error)
{
    OnStageChanged(FString::Printf(TEXT("Failed: %s"), *Error));
}
