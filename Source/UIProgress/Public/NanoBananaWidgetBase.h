// Minimal widget base for prompt input, progress, and result display
#pragma once

#include "Blueprint/UserWidget.h"
#include "NanoBananaWidgetBase.generated.h"

class UEditableTextBox;
class UProgressBar;
class UImage;

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

    UFUNCTION(BlueprintCallable, Category="Nano Banana")
    void StartFromViewport(bool bShowUI = true);

    UFUNCTION(BlueprintCallable, Category="Nano Banana")
    void StartWithPrompt(const FString& Prompt, bool bShowUI = true);

protected:
    UFUNCTION(BlueprintImplementableEvent, Category="Nano Banana")
    void OnStageChanged(const FString& Stage);

    UFUNCTION(BlueprintImplementableEvent, Category="Nano Banana")
    void OnCompleted(const FString& ResultPath, const FString& CompositePath);

protected:
    UFUNCTION()
    void HandleProgress(float P, const FString& Stage);

    UFUNCTION()
    void HandleCompleted(UTexture2D* Texture, const FString& ResultPath, const FString& CompositePath);

    UFUNCTION()
    void HandleFailed(const FString& Error);
};
