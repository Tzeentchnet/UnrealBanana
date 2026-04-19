#include "NanoBananaWidgetBase.h"
#include "Components/EditableTextBox.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "NanoBananaBridgeAsyncAction.h"
#include "NanoBananaSettings.h"
#include "Engine/Texture2D.h"

void UNanoBananaWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();
    PopulateCombos();
    if (CancelButton)
    {
        CancelButton->OnClicked.AddDynamic(this, &UNanoBananaWidgetBase::HandleCancelClicked);
    }
}

void UNanoBananaWidgetBase::PopulateCombos()
{
    const UNanoBananaSettings& S = UNanoBananaSettings::Get();

    if (VendorCombo)
    {
        VendorCombo->ClearOptions();
        VendorCombo->AddOption(TEXT("Google"));
        VendorCombo->AddOption(TEXT("Fal"));
        VendorCombo->AddOption(TEXT("Replicate"));
        VendorCombo->SetSelectedOption(FNanoBananaTypeUtils::VendorToString(S.DefaultVendor));
    }
    if (ModelCombo)
    {
        ModelCombo->ClearOptions();
        ModelCombo->AddOption(TEXT("NanoBanana"));
        ModelCombo->AddOption(TEXT("NanoBanana2"));
        ModelCombo->AddOption(TEXT("NanoBananaPro"));
        ModelCombo->AddOption(TEXT("Custom"));
        ModelCombo->SetSelectedOption(FNanoBananaTypeUtils::ModelToDisplayString(S.DefaultModel));
    }
}

ENanoBananaVendor UNanoBananaWidgetBase::GetSelectedVendor() const
{
    if (VendorCombo)
    {
        const FString V = VendorCombo->GetSelectedOption();
        if (V == TEXT("Google"))    return ENanoBananaVendor::Google;
        if (V == TEXT("Fal"))       return ENanoBananaVendor::Fal;
        if (V == TEXT("Replicate")) return ENanoBananaVendor::Replicate;
    }
    return UNanoBananaSettings::Get().DefaultVendor;
}

ENanoBananaModel UNanoBananaWidgetBase::GetSelectedModel() const
{
    if (ModelCombo)
    {
        const FString M = ModelCombo->GetSelectedOption();
        if (M == TEXT("NanoBanana"))    return ENanoBananaModel::NanoBanana;
        if (M == TEXT("NanoBanana2"))   return ENanoBananaModel::NanoBanana2;
        if (M == TEXT("NanoBananaPro")) return ENanoBananaModel::NanoBananaPro;
        if (M == TEXT("Custom"))        return ENanoBananaModel::Custom;
    }
    return UNanoBananaSettings::Get().DefaultModel;
}

void UNanoBananaWidgetBase::StartFromViewport(bool bShowUI)
{
    const FString Prompt = PromptTextBox ? PromptTextBox->GetText().ToString() : FString();
    StartWithPrompt(Prompt, bShowUI);
}

void UNanoBananaWidgetBase::StartWithPrompt(const FString& Prompt, bool bShowUI)
{
    ActiveAction = UNanoBananaBridgeAsyncAction::CaptureViewportAndGenerate(
        this, Prompt, GetSelectedVendor(), GetSelectedModel(), bShowUI, /*bAlsoSaveComposite*/ true);
    if (!ActiveAction) return;
    ActiveAction->OnProgress.AddDynamic(this, &UNanoBananaWidgetBase::HandleProgress);
    ActiveAction->OnCompleted.AddDynamic(this, &UNanoBananaWidgetBase::HandleCompleted);
    ActiveAction->OnFailed.AddDynamic(this, &UNanoBananaWidgetBase::HandleFailed);
    ActiveAction->Activate();
}

void UNanoBananaWidgetBase::CancelGeneration()
{
    if (ActiveAction)
    {
        ActiveAction->Cancel();
    }
}

void UNanoBananaWidgetBase::HandleCancelClicked()
{
    CancelGeneration();
}

void UNanoBananaWidgetBase::HandleProgress(float P, const FString& Stage)
{
    if (ProgressBar)
    {
        ProgressBar->SetPercent(P);
    }
    OnStageChanged(Stage);
}

void UNanoBananaWidgetBase::HandleCompleted(const TArray<FNanoBananaImageResult>& Results, const FString& CompositePath)
{
    if (ResultImage && Results.Num() > 0 && Results[0].Texture)
    {
        ResultImage->SetBrushFromTexture(Results[0].Texture);
    }
    ActiveAction = nullptr;
    OnCompleted(Results, CompositePath);
}

void UNanoBananaWidgetBase::HandleFailed(const FString& Error)
{
    ActiveAction = nullptr;
    OnStageChanged(FString::Printf(TEXT("Failed: %s"), *Error));
    OnFailed(Error);
}
