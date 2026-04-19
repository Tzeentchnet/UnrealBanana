// Minimal widget base for prompt input, vendor/model selection, progress, cancel, and result display.
#pragma once

#include "Blueprint/UserWidget.h"
#include "NanoBananaTypes.h"
#include "NanoBananaWidgetBase.generated.h"

class UEditableTextBox;
class UProgressBar;
class UImage;
class UButton;
class UComboBoxString;
class UNanoBananaBridgeAsyncAction;

UCLASS(Abstract)
class UIPROGRESS_API UNanoBananaWidgetBase : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(meta=(BindWidgetOptional))
    UEditableTextBox* PromptTextBox = nullptr;

    UPROPERTY(meta=(BindWidgetOptional))
    UProgressBar* ProgressBar = nullptr;

    UPROPERTY(meta=(BindWidgetOptional))
    UImage* ResultImage = nullptr;

    UPROPERTY(meta=(BindWidgetOptional))
    UComboBoxString* VendorCombo = nullptr;

    UPROPERTY(meta=(BindWidgetOptional))
    UComboBoxString* ModelCombo = nullptr;

    UPROPERTY(meta=(BindWidgetOptional))
    UButton* CancelButton = nullptr;

    /** Captures the viewport, then submits using the prompt + vendor/model combo selections. */
    UFUNCTION(BlueprintCallable, Category="Nano Banana")
    void StartFromViewport(bool bShowUI = true);

    /** Submits with the given prompt + vendor/model combo selections (or settings defaults if combos unbound). */
    UFUNCTION(BlueprintCallable, Category="Nano Banana")
    void StartWithPrompt(const FString& Prompt, bool bShowUI = true);

    /** Cancels any in-flight request. */
    UFUNCTION(BlueprintCallable, Category="Nano Banana")
    void CancelGeneration();

protected:
    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintImplementableEvent, Category="Nano Banana")
    void OnStageChanged(const FString& Stage);

    UFUNCTION(BlueprintImplementableEvent, Category="Nano Banana")
    void OnCompleted(const TArray<FNanoBananaImageResult>& Results, const FString& CompositePath);

    UFUNCTION(BlueprintImplementableEvent, Category="Nano Banana")
    void OnFailed(const FString& Error);

    /** Read the current vendor/model selections from the combos (falls back to settings defaults). */
    ENanoBananaVendor GetSelectedVendor() const;
    ENanoBananaModel  GetSelectedModel() const;

    /** Populates the vendor/model combo boxes from enum reflection. */
    void PopulateCombos();

private:
    UPROPERTY()
    TObjectPtr<UNanoBananaBridgeAsyncAction> ActiveAction;

    UFUNCTION()
    void HandleProgress(float P, const FString& Stage);

    UFUNCTION()
    void HandleCompleted(const TArray<FNanoBananaImageResult>& Results, const FString& CompositePath);

    UFUNCTION()
    void HandleFailed(const FString& Error);

    UFUNCTION()
    void HandleCancelClicked();
};
